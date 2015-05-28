// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/list_container.h"

#include <algorithm>
#include <vector>

#include "cc/base/scoped_ptr_vector.h"

namespace {
const size_t kDefaultNumElementTypesToReserve = 32;
}  // namespace

namespace cc {

// ListContainerCharAllocator
////////////////////////////////////////////////////
// This class deals only with char* and void*. It does allocation and passing
// out raw pointers, as well as memory deallocation when being destroyed.
class ListContainerBase::ListContainerCharAllocator {
 public:
  // ListContainerCharAllocator::InnerList
  /////////////////////////////////////////////
  // This class holds the raw memory chunk, as well as information about its
  // size and availability.
  struct InnerList {
    scoped_ptr<char[]> data;
    // The number of elements in total the memory can hold. The difference
    // between capacity and size is the how many more elements this list can
    // hold.
    size_t capacity;
    // The number of elements have been put into this list.
    size_t size;
    // The size of each element is in bytes. This is used to move from between
    // elements' memory locations.
    size_t step;

    InnerList() : capacity(0), size(0), step(0) {}

    void Erase(char* position) {
      // Confident that destructor is called by caller of this function. Since
      // ListContainerCharAllocator does not handle construction after
      // allocation, it doesn't handle desctrution before deallocation.
      DCHECK_LE(position, LastElement());
      DCHECK_GE(position, Begin());
      char* start = position + step;
      std::copy(start, End(), position);

      --size;
      // Decrease capacity to avoid creating not full not last InnerList.
      --capacity;
    }

    bool IsFull() { return capacity == size; }
    size_t NumElementsAvailable() const { return capacity - size; }

    void* AddElement() {
      DCHECK_LT(size, capacity);
      ++size;
      return LastElement();
    }

    char* Begin() const { return data.get(); }
    char* End() const { return data.get() + size * step; }
    char* LastElement() const { return data.get() + (size - 1) * step; }
    char* ElementAt(size_t index) const { return data.get() + index * step; }

   private:
    DISALLOW_COPY_AND_ASSIGN(InnerList);
  };

  explicit ListContainerCharAllocator(size_t element_size)
      : element_size_(element_size),
        size_(0),
        list_count_(0),
        last_list_(NULL) {
    AllocateNewList(kDefaultNumElementTypesToReserve);
  }

  ListContainerCharAllocator(size_t element_size, size_t element_count)
      : element_size_(element_size),
        size_(0),
        list_count_(0),
        last_list_(NULL) {
    AllocateNewList(element_count > 0 ? element_count
                                      : kDefaultNumElementTypesToReserve);
  }

  ~ListContainerCharAllocator() {}

  void* Allocate() {
    if (last_list_->IsFull())
      AllocateNewList(last_list_->capacity * 2);

    ++size_;
    return last_list_->AddElement();
  }

  size_t element_size() const { return element_size_; }
  size_t list_count() const { return list_count_; }
  size_t size() const { return size_; }
  bool IsEmpty() const { return size() == 0; }

  size_t Capacity() const {
    size_t capacity_sum = 0;
    for (ScopedPtrVector<InnerList>::const_iterator iter = storage_.begin();
         iter != storage_.end(); ++iter) {
      capacity_sum += (*iter)->capacity;
    }
    return capacity_sum;
  }

  void Clear() {
    size_t initial_allocation_size = storage_.front()->capacity;
    storage_.clear();
    list_count_ = 0;
    last_list_ = NULL;
    size_ = 0;
    AllocateNewList(initial_allocation_size);
  }

  void Erase(PositionInListContainerCharAllocator position) {
    DCHECK_EQ(this, position.ptr_to_container);
    storage_[position.vector_index]->Erase(position.item_iterator);
    // TODO(weiliangc): Free the InnerList if it is empty.
    --size_;
  }

  InnerList* InnerListById(size_t id) const {
    DCHECK_LT(id, list_count_);
    return storage_[id];
  }

  size_t FirstInnerListId() const {
    // |size_| > 0 means that at least one vector in |storage_| will be
    // non-empty.
    DCHECK_GT(size_, 0u);
    size_t id = 0;
    while (storage_[id]->size == 0)
      ++id;
    return id;
  }

  size_t LastInnerListId() const {
    // |size_| > 0 means that at least one vector in |storage_| will be
    // non-empty.
    DCHECK_GT(size_, 0u);
    size_t id = list_count_ - 1;
    while (storage_[id]->size == 0)
      --id;
    return id;
  }

  void AllocateNewList(size_t list_size) {
    ++list_count_;
    scoped_ptr<InnerList> new_list(new InnerList);
    storage_.push_back(new_list.Pass());
    last_list_ = storage_.back();
    InnerList* list = last_list_;
    list->capacity = list_size;
    list->size = 0;
    list->step = element_size_;
    list->data.reset(new char[list->capacity * list->step]);
  }

  size_t NumAvailableElementsInLastList() const {
    return last_list_->NumElementsAvailable();
  }

 private:
  ScopedPtrVector<InnerList> storage_;
  const size_t element_size_;
  size_t size_;
  size_t list_count_;
  InnerList* last_list_;

  DISALLOW_COPY_AND_ASSIGN(ListContainerCharAllocator);
};

// PositionInListContainerCharAllocator
//////////////////////////////////////////////////////
ListContainerBase::PositionInListContainerCharAllocator::
    PositionInListContainerCharAllocator(
        const ListContainerBase::PositionInListContainerCharAllocator& other)
    : ptr_to_container(other.ptr_to_container),
      vector_index(other.vector_index),
      item_iterator(other.item_iterator) {
}

ListContainerBase::PositionInListContainerCharAllocator::
    PositionInListContainerCharAllocator(
        ListContainerBase::ListContainerCharAllocator* container,
        size_t vector_ind,
        char* item_iter)
    : ptr_to_container(container),
      vector_index(vector_ind),
      item_iterator(item_iter) {
}

bool ListContainerBase::PositionInListContainerCharAllocator::operator==(
    const ListContainerBase::PositionInListContainerCharAllocator& other)
    const {
  DCHECK_EQ(ptr_to_container, other.ptr_to_container);
  return vector_index == other.vector_index &&
         item_iterator == other.item_iterator;
}

bool ListContainerBase::PositionInListContainerCharAllocator::operator!=(
    const ListContainerBase::PositionInListContainerCharAllocator& other)
    const {
  return !(*this == other);
}

ListContainerBase::PositionInListContainerCharAllocator
ListContainerBase::PositionInListContainerCharAllocator::Increment() {
  ListContainerCharAllocator::InnerList* list =
      ptr_to_container->InnerListById(vector_index);
  if (item_iterator == list->LastElement()) {
    ++vector_index;
    while (vector_index < ptr_to_container->list_count()) {
      if (ptr_to_container->InnerListById(vector_index)->size != 0)
        break;
      ++vector_index;
    }
    if (vector_index < ptr_to_container->list_count())
      item_iterator = ptr_to_container->InnerListById(vector_index)->Begin();
    else
      item_iterator = NULL;
  } else {
    item_iterator += list->step;
  }
  return *this;
}

ListContainerBase::PositionInListContainerCharAllocator
ListContainerBase::PositionInListContainerCharAllocator::ReverseIncrement() {
  ListContainerCharAllocator::InnerList* list =
      ptr_to_container->InnerListById(vector_index);
  if (item_iterator == list->Begin()) {
    --vector_index;
    // Since |vector_index| is unsigned, we compare < list_count() instead of
    // comparing >= 0, as the variable will wrap around when it goes out of
    // range (below 0).
    while (vector_index < ptr_to_container->list_count()) {
      if (ptr_to_container->InnerListById(vector_index)->size != 0)
        break;
      --vector_index;
    }
    if (vector_index < ptr_to_container->list_count()) {
      item_iterator =
          ptr_to_container->InnerListById(vector_index)->LastElement();
    } else {
      item_iterator = NULL;
    }
  } else {
    item_iterator -= list->step;
  }
  return *this;
}

// ListContainerBase
////////////////////////////////////////////
ListContainerBase::ListContainerBase(size_t max_size_for_derived_class)
    : data_(new ListContainerCharAllocator(max_size_for_derived_class)) {
}

ListContainerBase::ListContainerBase(size_t max_size_for_derived_class,
                                     size_t num_of_elements_to_reserve_for)
    : data_(new ListContainerCharAllocator(max_size_for_derived_class,
                                           num_of_elements_to_reserve_for)) {
}

ListContainerBase::~ListContainerBase() {
}

void ListContainerBase::EraseAndInvalidateAllPointers(
    ListContainerBase::Iterator position) {
  data_->Erase(position);
}

ListContainerBase::ConstReverseIterator ListContainerBase::crbegin() const {
  if (data_->IsEmpty())
    return crend();

  size_t id = data_->LastInnerListId();
  return ConstReverseIterator(data_.get(), id,
                              data_->InnerListById(id)->LastElement(), 0);
}

ListContainerBase::ConstReverseIterator ListContainerBase::crend() const {
  return ConstReverseIterator(data_.get(), static_cast<size_t>(-1), NULL,
                              size());
}

ListContainerBase::ReverseIterator ListContainerBase::rbegin() {
  if (data_->IsEmpty())
    return rend();

  size_t id = data_->LastInnerListId();
  return ReverseIterator(data_.get(), id,
                         data_->InnerListById(id)->LastElement(), 0);
}

ListContainerBase::ReverseIterator ListContainerBase::rend() {
  return ReverseIterator(data_.get(), static_cast<size_t>(-1), NULL, size());
}

ListContainerBase::ConstIterator ListContainerBase::cbegin() const {
  if (data_->IsEmpty())
    return cend();

  size_t id = data_->FirstInnerListId();
  return ConstIterator(data_.get(), id, data_->InnerListById(id)->Begin(), 0);
}

ListContainerBase::ConstIterator ListContainerBase::cend() const {
  if (data_->IsEmpty())
    return ConstIterator(data_.get(), 0, NULL, size());

  size_t id = data_->list_count();
  return ConstIterator(data_.get(), id, NULL, size());
}

ListContainerBase::Iterator ListContainerBase::begin() {
  if (data_->IsEmpty())
    return end();

  size_t id = data_->FirstInnerListId();
  return Iterator(data_.get(), id, data_->InnerListById(id)->Begin(), 0);
}

ListContainerBase::Iterator ListContainerBase::end() {
  if (data_->IsEmpty())
    return Iterator(data_.get(), 0, NULL, size());

  size_t id = data_->list_count();
  return Iterator(data_.get(), id, NULL, size());
}

ListContainerBase::ConstIterator ListContainerBase::IteratorAt(
    size_t index) const {
  DCHECK_LT(index, size());
  size_t original_index = index;
  size_t list_index;
  for (list_index = 0; list_index < data_->list_count(); ++list_index) {
    size_t current_size = data_->InnerListById(list_index)->size;
    if (index < current_size)
      break;
    index -= current_size;
  }
  return ConstIterator(data_.get(), list_index,
                       data_->InnerListById(list_index)->ElementAt(index),
                       original_index);
}

ListContainerBase::Iterator ListContainerBase::IteratorAt(size_t index) {
  DCHECK_LT(index, size());
  size_t original_index = index;
  size_t list_index;
  for (list_index = 0; list_index < data_->list_count(); ++list_index) {
    size_t current_size = data_->InnerListById(list_index)->size;
    if (index < current_size)
      break;
    index -= current_size;
  }
  return Iterator(data_.get(), list_index,
                  data_->InnerListById(list_index)->ElementAt(index),
                  original_index);
}

void* ListContainerBase::Allocate(size_t size_of_actual_element_in_bytes) {
  DCHECK_LE(size_of_actual_element_in_bytes, data_->element_size());
  return data_->Allocate();
}

size_t ListContainerBase::size() const {
  return data_->size();
}

bool ListContainerBase::empty() const {
  return data_->IsEmpty();
}

void ListContainerBase::clear() {
  data_->Clear();
}

size_t ListContainerBase::AvailableSizeWithoutAnotherAllocationForTesting()
    const {
  return data_->NumAvailableElementsInLastList();
}

// ListContainerBase::Iterator
/////////////////////////////////////////////////
ListContainerBase::Iterator::Iterator(ListContainerCharAllocator* container,
                                      size_t vector_ind,
                                      char* item_iter,
                                      size_t index)
    : PositionInListContainerCharAllocator(container, vector_ind, item_iter),
      index_(index) {
}

ListContainerBase::Iterator::~Iterator() {
}

size_t ListContainerBase::Iterator::index() const {
  return index_;
}

// ListContainerBase::ConstIterator
/////////////////////////////////////////////////
ListContainerBase::ConstIterator::ConstIterator(
    const ListContainerBase::Iterator& other)
    : PositionInListContainerCharAllocator(other), index_(other.index()) {
}

ListContainerBase::ConstIterator::ConstIterator(
    ListContainerCharAllocator* container,
    size_t vector_ind,
    char* item_iter,
    size_t index)
    : PositionInListContainerCharAllocator(container, vector_ind, item_iter),
      index_(index) {
}

ListContainerBase::ConstIterator::~ConstIterator() {
}

size_t ListContainerBase::ConstIterator::index() const {
  return index_;
}

// ListContainerBase::ReverseIterator
/////////////////////////////////////////////////
ListContainerBase::ReverseIterator::ReverseIterator(
    ListContainerCharAllocator* container,
    size_t vector_ind,
    char* item_iter,
    size_t index)
    : PositionInListContainerCharAllocator(container, vector_ind, item_iter),
      index_(index) {
}

ListContainerBase::ReverseIterator::~ReverseIterator() {
}

size_t ListContainerBase::ReverseIterator::index() const {
  return index_;
}

// ListContainerBase::ConstReverseIterator
/////////////////////////////////////////////////
ListContainerBase::ConstReverseIterator::ConstReverseIterator(
    const ListContainerBase::ReverseIterator& other)
    : PositionInListContainerCharAllocator(other), index_(other.index()) {
}

ListContainerBase::ConstReverseIterator::ConstReverseIterator(
    ListContainerCharAllocator* container,
    size_t vector_ind,
    char* item_iter,
    size_t index)
    : PositionInListContainerCharAllocator(container, vector_ind, item_iter),
      index_(index) {
}

ListContainerBase::ConstReverseIterator::~ConstReverseIterator() {
}

size_t ListContainerBase::ConstReverseIterator::index() const {
  return index_;
}

}  // namespace cc
