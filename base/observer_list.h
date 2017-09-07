// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OBSERVER_LIST_H_
#define BASE_OBSERVER_LIST_H_

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"

///////////////////////////////////////////////////////////////////////////////
//
// OVERVIEW:
//
//   A container for a list of observers.  Unlike a normal STL vector or list,
//   this container can be modified during iteration without invalidating the
//   iterator.  So, it safely handles the case of an observer removing itself
//   or other observers from the list while observers are being notified.
//
// TYPICAL USAGE:
//
//   class MyWidget {
//    public:
//     ...
//
//     class Observer {
//      public:
//       virtual void OnFoo(MyWidget* w) = 0;
//       virtual void OnBar(MyWidget* w, int x, int y) = 0;
//     };
//
//     void AddObserver(Observer* obs) {
//       observer_list_.AddObserver(obs);
//     }
//
//     void RemoveObserver(Observer* obs) {
//       observer_list_.RemoveObserver(obs);
//     }
//
//     void NotifyFoo() {
//       for (auto& observer : observer_list_)
//         observer.OnFoo(this);
//     }
//
//     void NotifyBar(int x, int y) {
//       for (FooList::iterator i = observer_list.begin(),
//           e = observer_list.end(); i != e; ++i)
//        i->OnBar(this, x, y);
//     }
//
//    private:
//     base::ObserverList<Observer> observer_list_;
//   };
//
//
///////////////////////////////////////////////////////////////////////////////

namespace base {

template <typename ObserverType>
class ObserverListThreadSafe;

template <class ObserverType>
class ObserverListBase
    : public SupportsWeakPtr<ObserverListBase<ObserverType>> {
 public:
  // Enumeration of which observers are notified.
  enum NotificationType {
    // Specifies that any observers added during notification are notified.
    // This is the default type if no type is provided to the constructor.
    NOTIFY_ALL,

    // Specifies that observers added while sending out notification are not
    // notified.
    NOTIFY_EXISTING_ONLY
  };

  // An iterator class that can be used to access the list of observers.
  template <class ContainerType>
  class Iter {
   public:
    Iter();
    explicit Iter(ContainerType* list);
    ~Iter();

    // A workaround for C2244. MSVC requires fully qualified type name for
    // return type on a function definition to match a function declaration.
    using ThisType =
        typename ObserverListBase<ObserverType>::template Iter<ContainerType>;

    bool operator==(const Iter& other) const;
    bool operator!=(const Iter& other) const;
    ThisType& operator++();
    ObserverType* operator->() const;
    ObserverType& operator*() const;

   private:
    FRIEND_TEST_ALL_PREFIXES(ObserverListTest, BasicStdIterator);
    FRIEND_TEST_ALL_PREFIXES(ObserverListTest, StdIteratorRemoveFront);

    ObserverType* GetCurrent() const;
    void EnsureValidIndex();

    size_t clamped_max_index() const {
      return std::min(max_index_, list_->observers_.size());
    }

    bool is_end() const { return !list_ || index_ == clamped_max_index(); }

    WeakPtr<ObserverListBase<ObserverType>> list_;
    // When initially constructed and each time the iterator is incremented,
    // |index_| is guaranteed to point to a non-null index if the iterator
    // has not reached the end of the ObserverList.
    size_t index_;
    size_t max_index_;
  };

  using Iterator = Iter<ObserverListBase<ObserverType>>;

  using iterator = Iter<ObserverListBase<ObserverType>>;
  iterator begin() {
    // An optimization: do not involve weak pointers for empty list.
    // Note: can't use ?: operator here due to some MSVC bug (unit tests fail)
    if (observers_.empty())
      return iterator();
    return iterator(this);
  }
  iterator end() { return iterator(); }

  using const_iterator = Iter<const ObserverListBase<ObserverType>>;
  const_iterator begin() const {
    if (observers_.empty())
      return const_iterator();
    return const_iterator(this);
  }
  const_iterator end() const { return const_iterator(); }

  ObserverListBase() : notify_depth_(0), type_(NOTIFY_ALL) {}
  explicit ObserverListBase(NotificationType type)
      : notify_depth_(0), type_(type) {}

  // Add an observer to the list.  An observer should not be added to
  // the same list more than once.
  void AddObserver(ObserverType* obs);

  // Remove an observer from the list if it is in the list.
  void RemoveObserver(ObserverType* obs);

  // Determine whether a particular observer is in the list.
  bool HasObserver(const ObserverType* observer) const;

  void Clear();

 protected:
  size_t size() const { return observers_.size(); }

  void Compact();

 private:
  friend class ObserverListThreadSafe<ObserverType>;

  typedef std::vector<ObserverType*> ListType;

  ListType observers_;
  int notify_depth_;
  NotificationType type_;

  template <class ContainerType>
  friend class Iter;

  DISALLOW_COPY_AND_ASSIGN(ObserverListBase);
};

template <class ObserverType>
template <class ContainerType>
ObserverListBase<ObserverType>::Iter<ContainerType>::Iter()
    : index_(0), max_index_(0) {}

template <class ObserverType>
template <class ContainerType>
ObserverListBase<ObserverType>::Iter<ContainerType>::Iter(ContainerType* list)
    : list_(const_cast<ObserverListBase<ObserverType>*>(list)->AsWeakPtr()),
      index_(0),
      max_index_(list->type_ == NOTIFY_ALL ? std::numeric_limits<size_t>::max()
                                           : list->observers_.size()) {
  EnsureValidIndex();
  DCHECK(list_);
  ++list_->notify_depth_;
}

template <class ObserverType>
template <class ContainerType>
ObserverListBase<ObserverType>::Iter<ContainerType>::~Iter() {
  if (list_ && --list_->notify_depth_ == 0)
    list_->Compact();
}

template <class ObserverType>
template <class ContainerType>
bool ObserverListBase<ObserverType>::Iter<ContainerType>::operator==(
    const Iter& other) const {
  if (is_end() && other.is_end())
    return true;
  return list_.get() == other.list_.get() && index_ == other.index_;
}

template <class ObserverType>
template <class ContainerType>
bool ObserverListBase<ObserverType>::Iter<ContainerType>::operator!=(
    const Iter& other) const {
  return !operator==(other);
}

template <class ObserverType>
template <class ContainerType>
typename ObserverListBase<ObserverType>::template Iter<ContainerType>&
    ObserverListBase<ObserverType>::Iter<ContainerType>::operator++() {
  if (list_) {
    ++index_;
    EnsureValidIndex();
  }
  return *this;
}

template <class ObserverType>
template <class ContainerType>
ObserverType* ObserverListBase<ObserverType>::Iter<ContainerType>::operator->()
    const {
  ObserverType* current = GetCurrent();
  DCHECK(current);
  return current;
}

template <class ObserverType>
template <class ContainerType>
ObserverType& ObserverListBase<ObserverType>::Iter<ContainerType>::operator*()
    const {
  ObserverType* current = GetCurrent();
  DCHECK(current);
  return *current;
}

template <class ObserverType>
template <class ContainerType>
ObserverType* ObserverListBase<ObserverType>::Iter<ContainerType>::GetCurrent()
    const {
  if (!list_)
    return nullptr;
  return index_ < clamped_max_index() ? list_->observers_[index_] : nullptr;
}

template <class ObserverType>
template <class ContainerType>
void ObserverListBase<ObserverType>::Iter<ContainerType>::EnsureValidIndex() {
  if (!list_)
    return;

  size_t max_index = clamped_max_index();
  while (index_ < max_index && !list_->observers_[index_])
    ++index_;
}

template <class ObserverType>
void ObserverListBase<ObserverType>::AddObserver(ObserverType* obs) {
  DCHECK(obs);
  if (ContainsValue(observers_, obs)) {
    NOTREACHED() << "Observers can only be added once!";
    return;
  }
  observers_.push_back(obs);
}

template <class ObserverType>
void ObserverListBase<ObserverType>::RemoveObserver(ObserverType* obs) {
  DCHECK(obs);
  typename ListType::iterator it =
    std::find(observers_.begin(), observers_.end(), obs);
  if (it != observers_.end()) {
    if (notify_depth_) {
      *it = nullptr;
    } else {
      observers_.erase(it);
    }
  }
}

template <class ObserverType>
bool ObserverListBase<ObserverType>::HasObserver(
    const ObserverType* observer) const {
  for (size_t i = 0; i < observers_.size(); ++i) {
    if (observers_[i] == observer)
      return true;
  }
  return false;
}

template <class ObserverType>
void ObserverListBase<ObserverType>::Clear() {
  if (notify_depth_) {
    for (typename ListType::iterator it = observers_.begin();
      it != observers_.end(); ++it) {
      *it = nullptr;
    }
  } else {
    observers_.clear();
  }
}

template <class ObserverType>
void ObserverListBase<ObserverType>::Compact() {
  observers_.erase(std::remove(observers_.begin(), observers_.end(), nullptr),
                   observers_.end());
}

template <class ObserverType, bool check_empty = false>
class ObserverList : public ObserverListBase<ObserverType> {
 public:
  typedef typename ObserverListBase<ObserverType>::NotificationType
      NotificationType;

  ObserverList() {}
  explicit ObserverList(NotificationType type)
      : ObserverListBase<ObserverType>(type) {}

  ~ObserverList() {
    // When check_empty is true, assert that the list is empty on destruction.
    if (check_empty) {
      ObserverListBase<ObserverType>::Compact();
      DCHECK_EQ(ObserverListBase<ObserverType>::size(), 0U);
    }
  }

  bool might_have_observers() const {
    return ObserverListBase<ObserverType>::size() != 0;
  }
};

}  // namespace base

#endif  // BASE_OBSERVER_LIST_H_
