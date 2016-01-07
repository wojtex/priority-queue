#ifndef _JNP1_PRIORITYQUEUE_HH_
#define _JNP1_PRIORITYQUEUE_HH_

#include <cassert>
#include <iostream>
#include <exception>
#include <map>
#include <memory>
#include <set>

// TODO: Add some messages in ctors to exceptions objects

// Założenia:
// 1. Nie obsługujemy wyjątku std::bad_alloc, który może zostać rzucony przez
// std::make_shared()
// 2.

class PriorityQueueEmptyException : public std::exception {
   public:
    PriorityQueueEmptyException() = default;
    virtual const char* what() const noexcept(true) {
        return "Priority queue is empty.";
    }
};

class PriorityQueueNotFoundException : public std::exception {
   public:
    PriorityQueueNotFoundException() = default;
    virtual const char* what() const noexcept(true) {
        return "Could not find element in priority queue with specified key.";
    }
};

class PriorityQueueInsertionException : public std::exception {
   public:
    PriorityQueueInsertionException() = default;
    virtual const char* what() const noexcept(true) {
        return "Could not insert key-value pair";
    }
};

template <typename K, typename V>
class PriorityQueue {
   public:
    using key_type = K;
    using value_type = V;

   protected:
    using key_ptr = std::shared_ptr<K>;
    using value_ptr = std::shared_ptr<V>;
    using element = std::pair<key_ptr, value_ptr>;

   public:
    using size_type = typename std::multiset<element>::size_type;

   protected:
    // Sorter classes
    // Konstruktory i operatory przypisania muszą być nothrow
    class KeyComparer {
       public:
        bool operator()(const key_ptr& lhs, const key_ptr& rhs) {
            return *lhs < *rhs;
        }
    };

    class ValueComparer {
       public:
        bool operator()(const value_ptr& lhs, const value_ptr& rhs) {
            return *lhs < *rhs;
        }
    };

    class ValueKeyComparer {
       public:
        bool operator()(const element& lhs, const element& rhs) {
            if (*(lhs.second) < *(rhs.second)) return true;
            if (*(rhs.second) < *(lhs.second)) return false;
            return *(lhs.first) < *(rhs.first);
        }
    };

   protected:
    // sortowanie po wartości, a potem po kluczu
    std::multiset<element, ValueKeyComparer> sorted_by_value;
    // sortowanie po kluczu, a potem po wartości, a na koniec po adresach
    // (domyślnie)
    std::map<key_ptr,
             std::map<value_ptr, std::multiset<element>, ValueComparer>,
             KeyComparer> sorted_by_key;

   public:
    // TODO: czy konstruktory na prawdę potrzebują jakiegoś exception-safety?

    // Konstruktor bezparametrowy tworzący pustą kolejkę [O(1)]
    PriorityQueue() = default;

    // Konstruktor kopiujący [O(queue.size())]
    // konstruktory kontenerów zapewniają silną gwarancję
    // jak coś się rzuci, to i tak PriorityQueue się nie utworzy
    // TODO: zweryfikować
    PriorityQueue(const PriorityQueue<K, V>& queue) = default;

    // Konstruktor przenoszący [O(1)]
    PriorityQueue(PriorityQueue<K, V>&& queue) noexcept
        : sorted_by_value(std::move(queue.sorted_by_value)),
          sorted_by_key(std::move(queue.sorted_by_key)) {}

    // Operator przypisania [O(queue.size()) dla użycia P = Q, a O(1) dla użycia
    // P = move(Q)]
    // TODO: w treści była inna definicja operatora przypisania,
    // ale chyba trzeba rozbić ją na dwa konstruktory
    PriorityQueue<K, V>& operator=(const PriorityQueue<K, V>& queue) {
        if (this == &queue) return *this;
        PriorityQueue<K, V> tmp(queue);  // może rzucać tylko bad_alloc
        this->swap(tmp);                 // powinno być noexcept
        return *this;
    }

    PriorityQueue<K, V>& operator=(PriorityQueue<K, V>&& queue) noexcept(true) {
        this->sorted_by_value =
            std::move(queue.sorted_by_value);  // powinno być noexcept (move)
        this->sorted_by_key =
            std::move(queue.sorted_by_key);  // powinno być noexcept (move)
        // jest noexcept bo kopiowanie shared_ptr jest noexcept
        return *this;
    }

    // Metoda zwracająca true wtedy i tylko wtedy, gdy kolejka jest pusta [O(1)]
    bool empty() const noexcept(true) { return sorted_by_value.empty(); }

    // Metoda zwracająca liczbę par (klucz, wartość) przechowywanych w kolejce
    // [O(1)]
    size_type size() const noexcept(true) { return sorted_by_value.size(); }

    // Metoda wstawiająca do kolejki parę o kluczu key i wartości value
    // [O(log size())] (dopuszczamy możliwość występowania w kolejce wielu
    // par o tym samym kluczu)
    void insert(const K& key, const V& value) {
        auto k = std::make_shared<K>(key);
        auto v = std::make_shared<V>(value);

        auto pair_by_value = make_pair(k, v);

        // Iterators
        decltype(sorted_by_value)::iterator it1;
        decltype(sorted_by_key)::iterator it2;
        std::map<value_ptr, std::multiset<element>, ValueComparer>::iterator
            it3;
        std::multiset<element>::iterator it4;
        // If we have to remove them on fail.
        bool al1 = false, al2 = false, al3 = false, al4 = false;
        // Polegamy na silnej gwarancji kontenerów STL (map, set)
        try {
            it1 = sorted_by_value.insert(pair_by_value);
            al1 = true;

            std::tie(it2, al2) = sorted_by_key.try_emplace(k);

            std::tie(it3, al3) = it2->second.try_emplace(v);

            it4 = it3->insert(pair_by_value);
            al4 = true;
        } catch (...) {
            if (al4) it3->second.erase(it4);
            if (al3) it2->second.erase(it3);
            if (al2) sorted_by_key.erase(it2);
            if (al1) sorted_by_value.erase(it1);
            throw PriorityQueueInsertionException();
        }
    }

    // Metody zwracające odpowiednio najmniejszą i największą wartość
    // przechowywaną
    // w kolejce [O(1)]; w przypadku wywołania którejś z tych metod na pustej
    // strukturze powinien zostać zgłoszony wyjątek PriorityQueueEmptyException
    const V& minValue() const {
        if (empty()) throw PriorityQueueEmptyException();
        // begin() i * - noexcept(true)
        return *(sorted_by_value.begin()->second);
    }
    const V& maxValue() const {
        if (empty()) throw PriorityQueueEmptyException();
        // rbegin() i * - noexcept(true)
        return *(sorted_by_value.rbegin()->second);
    }

    // Metody zwracające klucz o przypisanej odpowiednio najmniejszej lub
    // największej wartości [O(1)]; w przypadku wywołania którejś z tych metod
    // na pustej strukturze powinien zostać zgłoszony wyjątek
    // PriorityQueueEmptyException
    const K& minKey() const {
        if (empty()) throw PriorityQueueEmptyException();
        // begin() i * - noexcept(true)
        return *(sorted_by_value.begin()->first);
    }
    const K& maxKey() const {
        if (empty()) throw PriorityQueueEmptyException();
        // rbegin() i * - noexcept(true)
        return *(sorted_by_value.rbegin()->first);
    }

    // Metody usuwające z kolejki jedną parę o odpowiednio najmniejszej lub
    // największej wartości [O(log size())]
    void deleteMin() {
        if (empty()) return;                            // noexcept
        const element& e = *(sorted_by_value.begin());  // noexcept

        auto kit =
            sorted_by_key.find(e.first);  // może rzucać operator porównania
        assert(kit != sorted_by_key.end());
        auto vit =
            kit->second.find(e.second);  // może rzucać operator porównania
        assert(vit != kit->second.end());
        auto ait =  // nie rzuca
            vit->second.find(e);
        assert(ait != vit->second.end());

        // Modyfikacje
        vit->second.erase(ait);                             // noexcept
        if (vit->second.empty()) kit->second.erase(vit);    // noexcept
        if (kit->second.empty()) sorted_by_key.erase(kit);  // noexcept
        sorted_by_value.erase(sorted_by_value.begin());     // noexcept
    }
    void deleteMax() {
        if (empty()) return;                              // noexcept
        const element& e = *prev(sorted_by_value.end());  // noexcept

        auto kit =
            sorted_by_key.find(e.first);  // może rzucać operator porównania
        assert(kit != sorted_by_key.end());
        auto vit =
            kit->second.find(e.second);  // może rzucać operator porównania
        assert(vit != kit->second.end());
        auto ait =  // nie rzuca
            vit->second.find(e);
        assert(ait != vit->second.end());

        // Modyfikacje
        vit->second.erase(ait);                              // noexcept
        if (vit->second.empty()) kit->second.erase(vit);     // noexcept
        if (kit->second.empty()) sorted_by_key.erase(kit);   // noexcept
        sorted_by_value.erase(prev(sorted_by_value.end()));  // noexcept
    }

    // Metoda zmieniająca dotychczasową wartość przypisaną kluczowi key na nową
    // wartość value [O(log size())]; w przypadku gdy w kolejce nie ma pary
    // o kluczu key, powinien zostać zgłoszony wyjątek
    // PriorityQueueNotFoundException(); w przypadku kiedy w kolejce jest kilka
    // par
    // o kluczu key, zmienia wartość w dowolnie wybranej parze o podanym kluczu

    // sprawdza, czy do danej pary odwołuje się tylko jeden zestaw wskaźników
    // inaczej musi zaalokować nową parę (by modyfikacja nie dosięgła innych
    // par)
    // TODO: zportować do nowych struktur danych
    void changeValue(const K& key, const V& value) {
        auto k = std::make_shared<K>(key);

        auto es_it = sorted_by_key.find(k);
        if (es_it == sorted_by_key.end())
            throw PriorityQueueNotFoundException();

        assert(!es_it->second.empty());
        auto ov = *(es_it->second.begin());
        es_it->second.erase(es_it->second.begin());
        auto deleted_pair = make_pair(k, ov);
        sorted_by_value.erase(deleted_pair);

        auto nv = std::make_shared<V>(value);

        // Polegamy na silnej gwarancji kontenerów STL (map, set)
        try {
            es_it->second.insert(nv);
            try {
                sorted_by_value.insert(make_pair(k, nv));
            } catch (...) {
                // Usuwamy poprzednio dodaną parę i rzucamy dalej
                es_it->second.erase(nv);
                throw;
            }
        } catch (...) {
            // Dodajemy usunięte wcześniej elementy
            es_it->second.insert(ov);
            sorted_by_value.insert(deleted_pair);

            throw PriorityQueueInsertionException();
        }
    }

    // Metoda scalająca zawartość kolejki z podaną kolejką queue; ta operacja
    // usuwa
    // wszystkie elementy z kolejki queue i wstawia je do kolejki *this
    // [O(size() + queue.size() * log (queue.size() + size()))]
    // TODO: sportować do nowych struktur danych
    void merge(PriorityQueue<K, V>& queue) {
        if (this == &queue) return;

        std::multiset<element, ValueKeyComparer> sorted_by_value_rollback;
        std::map<key_ptr,
                 std::map<value_ptr, std::multiset<element>, ValueComparer>,
                 KeyComparer> sorted_by_key_rollback;

        try {
            for (element e : queue.sorted_by_value) {
                key_ptr k = e.first;
                value_ptr v = e.second;

                auto by_value = make_pair(k, v);

                sorted_by_value.insert(by_value);
                sorted_by_value_rollback.insert(by_value);

                std::map<value_ptr, std::multiset<element>, ValueComparer>
                    key_map;

                auto insert_by_key = sorted_by_key.insert(k, key_map);

                std::multiset<element> elem_set;
                auto insert_key_map = sorted_by_key[k].insert(v, elem_set);

                auto new_elem = make_pair(k, v);
                sorted_by_key[k][v].insert(new_elem);
                // TODO: Wg. mnie tu (przy rollbackach) nie trzeba rozpisywac [] 
                // na inserty
                sorted_by_key_rollback[k][v].insert(new_elem);
            }
            queue.sorted_by_value.clear();
            queue.sorted_by_key.clear();

        } catch (...) {
            // Wyczyść nowododane elementy w celu powrotu do stanu przed
            // wykonaniem operacji merge()
            for (auto& elem : sorted_by_value_rollback) {
                sorted_by_value.erase(elem);
            }
            for (auto elem : sorted_by_key_rollback) {
                // if(sorted_by_key.find(elem.first) != sorted_by_key.end()) {
                // }
                // TODO: Sprawdzic czy usuwane sa poprawne elementy/nie ma
                // leakow
                // TODO: Check if contains
                sorted_by_key[elem.first].erase(elem.second.begin());
            }

            throw PriorityQueueEmptyException();
        }
    }

    // Metoda zamieniającą zawartość kolejki z podaną kolejką queue (tak jak
    // większość kontenerów w bibliotece standardowej) [O(1)]
    // Gwarancja no-throw
    void swap(PriorityQueue<K, V>& queue) noexcept {
        if (this == &queue) return;
        this->sorted_by_value.swap(queue.sorted_by_value);
        this->sorted_by_key.swap(queue.sorted_by_key);
    }

    friend void swap(PriorityQueue<K, V>& lhs,
                     PriorityQueue<K, V>& rhs) noexcept {
        lhs.swap(rhs);
    }

    friend bool operator==(const PriorityQueue<K, V>& lhs,
                           const PriorityQueue<K, V>& rhs) {
        return lhs.sorted_by_value == rhs.sorted_by_value;
    }
    friend bool operator!=(const PriorityQueue<K, V>& lhs,
                           const PriorityQueue<K, V>& rhs) {
        return lhs.sorted_by_value != rhs.sorted_by_value;
    }
    friend bool operator<(const PriorityQueue<K, V>& lhs,
                          const PriorityQueue<K, V>& rhs) {
        return lhs.sorted_by_value < rhs.sorted_by_value;
    }
    friend bool operator>(const PriorityQueue<K, V>& lhs,
                          const PriorityQueue<K, V>& rhs) {
        return rhs < lhs;
    }
    friend bool operator<=(const PriorityQueue<K, V>& lhs,
                           const PriorityQueue<K, V>& rhs) {
        return !(lhs > rhs);
    }
    friend bool operator>=(const PriorityQueue<K, V>& lhs,
                           const PriorityQueue<K, V>& rhs) {
        return !(lhs < rhs);
    }
};

#endif /* end of include guard: _JNP1_PRIORITYQUEUE_HH_ */
