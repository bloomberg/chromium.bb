// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_LIST_CONTAINER_H_
#define CC_BASE_LIST_CONTAINER_H_

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"

namespace cc {

// ListContainer is a container type that handles allocating contiguous memory
// for new elements and traversing through elements with either iterator or
// reverse iterator. Since this container hands out raw pointers of its
// elements, it is very important that this container never reallocate its
// memory so those raw pointer will continue to be valid.  This class is used to
// contain SharedQuadState or DrawQuad. Since the size of each DrawQuad varies,
// to hold DrawQuads, the allocations size of each element in this class is
// LargestDrawQuadSize while BaseElementType is DrawQuad.

// Base class for non-templated logic. All methods are protected, and only
// exposed by ListContainer<BaseElementType>.
// For usage, see comments in ListContainer.
class CC_EXPORT ListContainerBase {
 protected:
  explicit ListContainerBase(size_t max_size_for_derived_class);
  ListContainerBase(size_t max_size_for_derived_class,
                    size_t num_of_elements_to_reserve_for);
  ~ListContainerBase();

  // This class deals only with char* and void*. It does allocation and passing
  // out raw pointers, as well as memory deallocation when being destroyed.
  class ListContainerCharAllocator;

  // This class points to a certain position inside memory of
  // ListContainerCharAllocator. It is a base class for ListContainer iterators.
  struct CC_EXPORT PositionInListContainerCharAllocator {
    ListContainerCharAllocator* ptr_to_container;
    size_t vector_index;
    char* item_iterator;

    PositionInListContainerCharAllocator(
        const PositionInListContainerCharAllocator& other);

    PositionInListContainerCharAllocator(ListContainerCharAllocator* container,
                                         size_t vector_ind,
                                         char* item_iter);

    bool operator==(const PositionInListContainerCharAllocator& other) const;
    bool operator!=(const PositionInListContainerCharAllocator& other) const;

    PositionInListContainerCharAllocator Increment();
    PositionInListContainerCharAllocator ReverseIncrement();
  };

  // Iterator classes that can be used to access data.
  /////////////////////////////////////////////////////////////////
  class CC_EXPORT Iterator : public PositionInListContainerCharAllocator {
    // This class is only defined to forward iterate through
    // ListContainerCharAllocator.
   public:
    Iterator(ListContainerCharAllocator* container,
             size_t vector_ind,
             char* item_iter,
             size_t index);
    ~Iterator();

    size_t index() const;

   protected:
    // This is used to track how many increment has happened since begin(). It
    // is used to avoid double increment at places an index reference is
    // needed. For iterator this means begin() corresponds to index 0 and end()
    // corresponds to index |size|.
    size_t index_;
  };

  class CC_EXPORT ConstIterator : public PositionInListContainerCharAllocator {
    // This class is only defined to forward iterate through
    // ListContainerCharAllocator.
   public:
    ConstIterator(ListContainerCharAllocator* container,
                  size_t vector_ind,
                  char* item_iter,
                  size_t index);
    ConstIterator(const Iterator& other);  // NOLINT
    ~ConstIterator();

    size_t index() const;

   protected:
    // This is used to track how many increment has happened since begin(). It
    // is used to avoid double increment at places an index reference is
    // needed. For iterator this means begin() corresponds to index 0 and end()
    // corresponds to index |size|.
    size_t index_;
  };

  class CC_EXPORT ReverseIterator
      : public PositionInListContainerCharAllocator {
    // This class is only defined to reverse iterate through
    // ListContainerCharAllocator.
   public:
    ReverseIterator(ListContainerCharAllocator* container,
                    size_t vector_ind,
                    char* item_iter,
                    size_t index);
    ~ReverseIterator();

    size_t index() const;

   protected:
    // This is used to track how many increment has happened since rbegin(). It
    // is used to avoid double increment at places an index reference is
    // needed. For reverse iterator this means rbegin() corresponds to index 0
    // and rend() corresponds to index |size|.
    size_t index_;
  };

  class CC_EXPORT ConstReverseIterator
      : public PositionInListContainerCharAllocator {
    // This class is only defined to reverse iterate through
    // ListContainerCharAllocator.
   public:
    ConstReverseIterator(ListContainerCharAllocator* container,
                         size_t vector_ind,
                         char* item_iter,
                         size_t index);
    ConstReverseIterator(const ReverseIterator& other);  // NOLINT
    ~ConstReverseIterator();

    size_t index() const;

   protected:
    // This is used to track how many increment has happened since rbegin(). It
    // is used to avoid double increment at places an index reference is
    // needed. For reverse iterator this means rbegin() corresponds to index 0
    // and rend() corresponds to index |size|.
    size_t index_;
  };

  // Unlike the ListContainer methods, these do not invoke element destructors.
  void RemoveLast();
  void EraseAndInvalidateAllPointers(Iterator position);

  ConstReverseIterator crbegin() const;
  ConstReverseIterator crend() const;
  ReverseIterator rbegin();
  ReverseIterator rend();
  ConstIterator cbegin() const;
  ConstIterator cend() const;
  Iterator begin();
  Iterator end();

  Iterator IteratorAt(size_t index);
  ConstIterator IteratorAt(size_t index) const;

  size_t size() const;
  bool empty() const;

  size_t MaxSizeForDerivedClass() const;

  // Unlike the ListContainer method, this one does not invoke element
  // destructors.
  void clear();

  size_t AvailableSizeWithoutAnotherAllocationForTesting() const;

  // Hands out memory location for an element at the end of data structure.
  void* Allocate(size_t size_of_actual_element_in_bytes);

  scoped_ptr<ListContainerCharAllocator> data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ListContainerBase);
};

template <class BaseElementType>
class ListContainer : public ListContainerBase {
 public:
  // BaseElementType is the type of raw pointers this class hands out; however,
  // its derived classes might require different memory sizes.
  // max_size_for_derived_class the largest memory size required for all the
  // derived classes to use for allocation.
  explicit ListContainer(size_t max_size_for_derived_class)
      : ListContainerBase(max_size_for_derived_class) {}

  // This constructor omits input variable for max_size_for_derived_class. This
  // is used when there is no derived classes from BaseElementType we need to
  // worry about, and allocation size is just sizeof(BaseElementType).
  ListContainer() : ListContainerBase(sizeof(BaseElementType)) {}

  // This constructor reserves the requested memory up front so only single
  // allocation is needed. When num_of_elements_to_reserve_for is zero, use the
  // default size.
  ListContainer(size_t max_size_for_derived_class,
                size_t num_of_elements_to_reserve_for)
      : ListContainerBase(max_size_for_derived_class,
                          num_of_elements_to_reserve_for) {}

  ~ListContainer() {
    for (Iterator i = begin(); i != end(); ++i) {
      i->~BaseElementType();
    }
  }

  class Iterator;
  class ConstIterator;
  class ReverseIterator;
  class ConstReverseIterator;

  // Removes the last element of the list and makes its space available for
  // allocation.
  void RemoveLast() {
    DCHECK(!empty());
    back()->~BaseElementType();
    ListContainerBase::RemoveLast();
  }

  // When called, all raw pointers that have been handed out are no longer
  // valid. Use with caution.
  // This function does not deallocate memory.
  void EraseAndInvalidateAllPointers(Iterator position) {
    BaseElementType* item = *position;
    item->~BaseElementType();
    ListContainerBase::EraseAndInvalidateAllPointers(position);
  }

  ConstReverseIterator crbegin() const {
    return ConstReverseIterator(ListContainerBase::crbegin());
  }
  ConstReverseIterator crend() const {
    return ConstReverseIterator(ListContainerBase::crend());
  }
  ConstReverseIterator rbegin() const { return crbegin(); }
  ConstReverseIterator rend() const { return crend(); }
  ReverseIterator rbegin() {
    return ReverseIterator(ListContainerBase::rbegin());
  }
  ReverseIterator rend() { return ReverseIterator(ListContainerBase::rend()); }
  ConstIterator cbegin() const {
    return ConstIterator(ListContainerBase::cbegin());
  }
  ConstIterator cend() const {
    return ConstIterator(ListContainerBase::cend());
  }
  ConstIterator begin() const { return cbegin(); }
  ConstIterator end() const { return cend(); }
  Iterator begin() { return Iterator(ListContainerBase::begin()); }
  Iterator end() { return Iterator(ListContainerBase::end()); }

  // TODO(weiliangc): front(), back() and ElementAt() function should return
  // reference, consistent with container-of-object.
  BaseElementType* front() { return *begin(); }
  BaseElementType* back() { return *rbegin(); }
  const BaseElementType* front() const { return *begin(); }
  const BaseElementType* back() const { return *rbegin(); }

  BaseElementType* ElementAt(size_t index) {
    return *Iterator(IteratorAt(index));
  }
  const BaseElementType* ElementAt(size_t index) const {
    return *ConstIterator(IteratorAt(index));
  }

  // Take in derived element type and construct it at location generated by
  // Allocate().
  template <typename DerivedElementType>
  DerivedElementType* AllocateAndConstruct() {
    return new (Allocate(sizeof(DerivedElementType))) DerivedElementType;
  }

  // Take in derived element type and copy construct it at location generated by
  // Allocate().
  template <typename DerivedElementType>
  DerivedElementType* AllocateAndCopyFrom(const DerivedElementType* source) {
    return new (Allocate(sizeof(DerivedElementType)))
        DerivedElementType(*source);
  }

  // Construct a new element on top of an existing one.
  template <typename DerivedElementType>
  DerivedElementType* ReplaceExistingElement(Iterator at) {
    at->~BaseElementType();
    return new (*at) DerivedElementType();
  }

  template <typename DerivedElementType>
  void swap(ListContainer<DerivedElementType>& other) {
    data_.swap(other.data_);
  }

  // Appends a new item without copying. The original item will not be
  // destructed and will be replaced with a new DerivedElementType. The
  // DerivedElementType does not have to match the moved type as a full block
  // of memory will be moved (up to MaxSizeForDerivedClass()). A pointer to
  // the moved element is returned.
  template <typename DerivedElementType>
  DerivedElementType* AppendByMoving(DerivedElementType* item) {
    size_t max_size_for_derived_class = MaxSizeForDerivedClass();
    void* new_item = Allocate(max_size_for_derived_class);
    memcpy(new_item, static_cast<void*>(item), max_size_for_derived_class);
    // Construct a new element in-place so it can be destructed safely.
    new (item) DerivedElementType;
    return static_cast<DerivedElementType*>(new_item);
  }

  using ListContainerBase::size;
  using ListContainerBase::empty;

  void clear() {
    for (Iterator i = begin(); i != end(); ++i) {
      i->~BaseElementType();
    }
    ListContainerBase::clear();
  }

  using ListContainerBase::AvailableSizeWithoutAnotherAllocationForTesting;

  // Iterator classes that can be used to access data.
  /////////////////////////////////////////////////////////////////
  class Iterator : public ListContainerBase::Iterator {
    // This class is only defined to forward iterate through
    // ListContainerCharAllocator.
   public:
    Iterator(ListContainerCharAllocator* container,
             size_t vector_ind,
             char* item_iter,
             size_t index)
        : ListContainerBase::Iterator(container, vector_ind, item_iter, index) {
    }
    BaseElementType* operator->() const {
      return reinterpret_cast<BaseElementType*>(item_iterator);
    }
    BaseElementType* operator*() const {
      return reinterpret_cast<BaseElementType*>(item_iterator);
    }
    Iterator operator++(int unused_post_increment) {
      Iterator tmp = *this;
      operator++();
      return tmp;
    }
    Iterator& operator++() {
      Increment();
      ++index_;
      return *this;
    }

   private:
    explicit Iterator(const ListContainerBase::Iterator& base_iterator)
        : ListContainerBase::Iterator(base_iterator) {}
    friend Iterator ListContainer<BaseElementType>::begin();
    friend Iterator ListContainer<BaseElementType>::end();
    friend BaseElementType* ListContainer<BaseElementType>::ElementAt(
        size_t index);
  };

  class ConstIterator : public ListContainerBase::ConstIterator {
    // This class is only defined to forward iterate through
    // ListContainerCharAllocator.
   public:
    ConstIterator(ListContainerCharAllocator* container,
                  size_t vector_ind,
                  char* item_iter,
                  size_t index)
        : ListContainerBase::ConstIterator(container,
                                           vector_ind,
                                           item_iter,
                                           index) {}
    ConstIterator(const Iterator& other)  // NOLINT
        : ListContainerBase::ConstIterator(other) {}
    const BaseElementType* operator->() const {
      return reinterpret_cast<const BaseElementType*>(item_iterator);
    }
    const BaseElementType* operator*() const {
      return reinterpret_cast<const BaseElementType*>(item_iterator);
    }
    ConstIterator operator++(int unused_post_increment) {
      ConstIterator tmp = *this;
      operator++();
      return tmp;
    }
    ConstIterator& operator++() {
      Increment();
      ++index_;
      return *this;
    }

   private:
    explicit ConstIterator(
        const ListContainerBase::ConstIterator& base_iterator)
        : ListContainerBase::ConstIterator(base_iterator) {}
    friend ConstIterator ListContainer<BaseElementType>::cbegin() const;
    friend ConstIterator ListContainer<BaseElementType>::cend() const;
    friend const BaseElementType* ListContainer<BaseElementType>::ElementAt(
        size_t index) const;
  };

  class ReverseIterator : public ListContainerBase::ReverseIterator {
    // This class is only defined to reverse iterate through
    // ListContainerCharAllocator.
   public:
    ReverseIterator(ListContainerCharAllocator* container,
                    size_t vector_ind,
                    char* item_iter,
                    size_t index)
        : ListContainerBase::ReverseIterator(container,
                                             vector_ind,
                                             item_iter,
                                             index) {}
    BaseElementType* operator->() const {
      return reinterpret_cast<BaseElementType*>(item_iterator);
    }
    BaseElementType* operator*() const {
      return reinterpret_cast<BaseElementType*>(item_iterator);
    }
    ReverseIterator operator++(int unused_post_increment) {
      ReverseIterator tmp = *this;
      operator++();
      return tmp;
    }
    ReverseIterator& operator++() {
      ReverseIncrement();
      ++index_;
      return *this;
    }

   private:
    explicit ReverseIterator(ListContainerBase::ReverseIterator base_iterator)
        : ListContainerBase::ReverseIterator(base_iterator) {}
    friend ReverseIterator ListContainer<BaseElementType>::rbegin();
    friend ReverseIterator ListContainer<BaseElementType>::rend();
  };

  class ConstReverseIterator : public ListContainerBase::ConstReverseIterator {
    // This class is only defined to reverse iterate through
    // ListContainerCharAllocator.
   public:
    ConstReverseIterator(ListContainerCharAllocator* container,
                         size_t vector_ind,
                         char* item_iter,
                         size_t index)
        : ListContainerBase::ConstReverseIterator(container,
                                                  vector_ind,
                                                  item_iter,
                                                  index) {}
    ConstReverseIterator(const ReverseIterator& other)  // NOLINT
        : ListContainerBase::ConstReverseIterator(other) {}
    const BaseElementType* operator->() const {
      return reinterpret_cast<const BaseElementType*>(item_iterator);
    }
    const BaseElementType* operator*() const {
      return reinterpret_cast<const BaseElementType*>(item_iterator);
    }
    ConstReverseIterator operator++(int unused_post_increment) {
      ConstReverseIterator tmp = *this;
      operator++();
      return tmp;
    }
    ConstReverseIterator& operator++() {
      ReverseIncrement();
      ++index_;
      return *this;
    }

   private:
    explicit ConstReverseIterator(
        ListContainerBase::ConstReverseIterator base_iterator)
        : ListContainerBase::ConstReverseIterator(base_iterator) {}
    friend ConstReverseIterator ListContainer<BaseElementType>::crbegin() const;
    friend ConstReverseIterator ListContainer<BaseElementType>::crend() const;
  };
};

}  // namespace cc

#endif  // CC_BASE_LIST_CONTAINER_H_
