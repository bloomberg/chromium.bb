// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/list_container.h"

#include <algorithm>
#include <vector>

#include "cc/base/scoped_ptr_vector.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/shared_quad_state.h"

namespace {
const size_t kDefaultNumElementTypesToReserve = 32;
}  // namespace

namespace cc {

// ListContainerCharAllocator
////////////////////////////////////////////////////
// This class deals only with char* and void*. It does allocation and passing
// out raw pointers, as well as memory deallocation when being destroyed.
template <typename BaseElementType>
class ListContainer<BaseElementType>::ListContainerCharAllocator {
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
    DCHECK_NE(0u, element_count);
    AllocateNewList(element_count);
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
    for (typename ScopedPtrVector<InnerList>::const_iterator iter =
             storage_.begin();
         iter != storage_.end();
         ++iter) {
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
template <typename BaseElementType>
ListContainer<BaseElementType>::PositionInListContainerCharAllocator::
    PositionInListContainerCharAllocator(const typename ListContainer<
        BaseElementType>::PositionInListContainerCharAllocator& other)
    : ptr_to_container(other.ptr_to_container),
      vector_index(other.vector_index),
      item_iterator(other.item_iterator) {
}

template <typename BaseElementType>
ListContainer<BaseElementType>::PositionInListContainerCharAllocator::
    PositionInListContainerCharAllocator(
        typename ListContainer<BaseElementType>::ListContainerCharAllocator*
            container,
        size_t vector_ind,
        char* item_iter)
    : ptr_to_container(container),
      vector_index(vector_ind),
      item_iterator(item_iter) {
}

template <typename BaseElementType>
bool ListContainer<BaseElementType>::PositionInListContainerCharAllocator::
operator==(const typename ListContainer<
    BaseElementType>::PositionInListContainerCharAllocator& other) const {
  DCHECK_EQ(ptr_to_container, other.ptr_to_container);
  return vector_index == other.vector_index &&
         item_iterator == other.item_iterator;
}

template <typename BaseElementType>
bool ListContainer<BaseElementType>::PositionInListContainerCharAllocator::
operator!=(const typename ListContainer<
    BaseElementType>::PositionInListContainerCharAllocator& other) const {
  return !(*this == other);
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::PositionInListContainerCharAllocator
ListContainer<
    BaseElementType>::PositionInListContainerCharAllocator::Increment() {
  typename ListContainerCharAllocator::InnerList* list =
      ptr_to_container->InnerListById(vector_index);
  if (item_iterator == list->LastElement()) {
    if (vector_index < ptr_to_container->list_count() - 1) {
      ++vector_index;
      item_iterator = ptr_to_container->InnerListById(vector_index)->Begin();
    } else {
      item_iterator = NULL;
    }
  } else {
    item_iterator += list->step;
  }
  return *this;
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::PositionInListContainerCharAllocator
ListContainer<
    BaseElementType>::PositionInListContainerCharAllocator::ReverseIncrement() {
  typename ListContainerCharAllocator::InnerList* list =
      ptr_to_container->InnerListById(vector_index);
  if (item_iterator == list->Begin()) {
    if (vector_index > 0) {
      --vector_index;
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

// ListContainer
////////////////////////////////////////////
template <typename BaseElementType>
ListContainer<BaseElementType>::ListContainer(size_t max_size_for_derived_class)
    : data_(new ListContainerCharAllocator(max_size_for_derived_class)) {
}

template <typename BaseElementType>
ListContainer<BaseElementType>::ListContainer(
    size_t max_size_for_derived_class,
    size_t num_of_elements_to_reserve_for)
    : data_(new ListContainerCharAllocator(max_size_for_derived_class,
                                           num_of_elements_to_reserve_for)) {
}

template <typename BaseElementType>
ListContainer<BaseElementType>::ListContainer()
    : data_(new ListContainerCharAllocator(sizeof(BaseElementType))) {
}

template <typename BaseElementType>
ListContainer<BaseElementType>::~ListContainer() {
  for (Iterator i = begin(); i != end(); ++i) {
    i->~BaseElementType();
  }
}

template <typename BaseElementType>
void ListContainer<BaseElementType>::EraseAndInvalidateAllPointers(
    typename ListContainer<BaseElementType>::Iterator position) {
  BaseElementType* item = &*position;
  item->~BaseElementType();
  data_->Erase(position);
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::ConstReverseIterator
ListContainer<BaseElementType>::rbegin() const {
  if (data_->IsEmpty())
    return ConstReverseIterator(data_.get(), 0, NULL);

  size_t last_id = data_->list_count() - 1;
  return ConstReverseIterator(
      data_.get(), last_id, data_->InnerListById(last_id)->LastElement());
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::ConstReverseIterator
ListContainer<BaseElementType>::rend() const {
  return ConstReverseIterator(data_.get(), 0, NULL);
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::ReverseIterator
ListContainer<BaseElementType>::rbegin() {
  if (data_->IsEmpty())
    return ReverseIterator(data_.get(), 0, NULL);

  size_t last_id = data_->list_count() - 1;
  return ReverseIterator(
      data_.get(), last_id, data_->InnerListById(last_id)->LastElement());
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::ReverseIterator
ListContainer<BaseElementType>::rend() {
  return ReverseIterator(data_.get(), 0, NULL);
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::ConstIterator
ListContainer<BaseElementType>::begin() const {
  if (data_->IsEmpty())
    return ConstIterator(data_.get(), 0, NULL);

  return ConstIterator(data_.get(), 0, data_->InnerListById(0)->Begin());
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::ConstIterator
ListContainer<BaseElementType>::end() const {
  if (data_->IsEmpty())
    return ConstIterator(data_.get(), 0, NULL);

  size_t last_id = data_->list_count() - 1;
  return ConstIterator(data_.get(), last_id, NULL);
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::Iterator
ListContainer<BaseElementType>::begin() {
  if (data_->IsEmpty())
    return Iterator(data_.get(), 0, NULL);

  return Iterator(data_.get(), 0, data_->InnerListById(0)->Begin());
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::Iterator
ListContainer<BaseElementType>::end() {
  if (data_->IsEmpty())
    return Iterator(data_.get(), 0, NULL);

  size_t last_id = data_->list_count() - 1;
  return Iterator(data_.get(), last_id, NULL);
}

template <typename BaseElementType>
BaseElementType* ListContainer<BaseElementType>::front() {
  Iterator iter = begin();
  return &*iter;
}

template <typename BaseElementType>
BaseElementType* ListContainer<BaseElementType>::back() {
  ReverseIterator iter = rbegin();
  return &*iter;
}

template <typename BaseElementType>
const BaseElementType* ListContainer<BaseElementType>::front() const {
  ConstIterator iter = begin();
  return &*iter;
}

template <typename BaseElementType>
const BaseElementType* ListContainer<BaseElementType>::back() const {
  ConstReverseIterator iter = rbegin();
  return &*iter;
}

template <typename BaseElementType>
const BaseElementType* ListContainer<BaseElementType>::ElementAt(
    size_t index) const {
  DCHECK_LT(index, size());
  size_t list_index;
  for (list_index = 0; list_index < data_->list_count(); ++list_index) {
    size_t current_size = data_->InnerListById(list_index)->size;
    if (index < current_size)
      break;
    index -= current_size;
  }
  return &*ConstIterator(data_.get(),
                         list_index,
                         data_->InnerListById(list_index)->ElementAt(index));
}

template <typename BaseElementType>
BaseElementType* ListContainer<BaseElementType>::ElementAt(size_t index) {
  DCHECK_LT(index, size());
  size_t list_index;
  for (list_index = 0; list_index < data_->list_count(); ++list_index) {
    size_t current_size = data_->InnerListById(list_index)->size;
    if (index < current_size)
      break;
    index -= current_size;
  }
  return &*Iterator(data_.get(),
                    list_index,
                    data_->InnerListById(list_index)->ElementAt(index));
}

template <typename BaseElementType>
BaseElementType* ListContainer<BaseElementType>::Allocate(
    size_t size_of_actual_element_in_bytes) {
  DCHECK_LE(size_of_actual_element_in_bytes, data_->element_size());
  void* result = data_->Allocate();
  return static_cast<BaseElementType*>(result);
}

template <typename BaseElementType>
size_t ListContainer<BaseElementType>::size() const {
  return data_->size();
}

template <typename BaseElementType>
bool ListContainer<BaseElementType>::empty() const {
  return data_->IsEmpty();
}

template <typename BaseElementType>
void ListContainer<BaseElementType>::clear() {
  for (Iterator i = begin(); i != end(); ++i) {
    i->~BaseElementType();
  }
  data_->Clear();
}

template <typename BaseElementType>
size_t ListContainer<
    BaseElementType>::AvailableSizeWithoutAnotherAllocationForTesting() const {
  return data_->NumAvailableElementsInLastList();
}

// ListContainer::Iterator
/////////////////////////////////////////////////
template <typename BaseElementType>
ListContainer<BaseElementType>::Iterator::Iterator(
    ListContainerCharAllocator* container,
    size_t vector_ind,
    char* item_iter)
    : PositionInListContainerCharAllocator(container, vector_ind, item_iter) {
}

template <typename BaseElementType>
ListContainer<BaseElementType>::Iterator::~Iterator() {
}

template <typename BaseElementType>
BaseElementType* ListContainer<BaseElementType>::Iterator::operator->() const {
  return reinterpret_cast<BaseElementType*>(this->item_iterator);
}

template <typename BaseElementType>
BaseElementType& ListContainer<BaseElementType>::Iterator::operator*() const {
  return *(reinterpret_cast<BaseElementType*>(this->item_iterator));
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::Iterator
ListContainer<BaseElementType>::Iterator::
operator++(int unused_post_increment) {
  Iterator tmp = *this;
  operator++();
  return tmp;
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::Iterator
ListContainer<BaseElementType>::Iterator::
operator++() {
  this->Increment();
  return *this;
}

// ListContainer::ConstIterator
/////////////////////////////////////////////////
template <typename BaseElementType>
ListContainer<BaseElementType>::ConstIterator::ConstIterator(
    const typename ListContainer<BaseElementType>::Iterator& other)
    : PositionInListContainerCharAllocator(other) {
}

template <typename BaseElementType>
ListContainer<BaseElementType>::ConstIterator::ConstIterator(
    ListContainerCharAllocator* container,
    size_t vector_ind,
    char* item_iter)
    : PositionInListContainerCharAllocator(container, vector_ind, item_iter) {
}

template <typename BaseElementType>
ListContainer<BaseElementType>::ConstIterator::~ConstIterator() {
}

template <typename BaseElementType>
const BaseElementType* ListContainer<BaseElementType>::ConstIterator::
operator->() const {
  return reinterpret_cast<const BaseElementType*>(this->item_iterator);
}

template <typename BaseElementType>
const BaseElementType& ListContainer<BaseElementType>::ConstIterator::
operator*() const {
  return *(reinterpret_cast<const BaseElementType*>(this->item_iterator));
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::ConstIterator
ListContainer<BaseElementType>::ConstIterator::
operator++(int unused_post_increment) {
  ConstIterator tmp = *this;
  operator++();
  return tmp;
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::ConstIterator
ListContainer<BaseElementType>::ConstIterator::
operator++() {
  this->Increment();
  return *this;
}

// ListContainer::ReverseIterator
/////////////////////////////////////////////////
template <typename BaseElementType>
ListContainer<BaseElementType>::ReverseIterator::ReverseIterator(
    ListContainerCharAllocator* container,
    size_t vector_ind,
    char* item_iter)
    : PositionInListContainerCharAllocator(container, vector_ind, item_iter) {
}

template <typename BaseElementType>
ListContainer<BaseElementType>::ReverseIterator::~ReverseIterator() {
}

template <typename BaseElementType>
BaseElementType* ListContainer<BaseElementType>::ReverseIterator::operator->()
    const {
  return reinterpret_cast<BaseElementType*>(this->item_iterator);
}

template <typename BaseElementType>
BaseElementType& ListContainer<BaseElementType>::ReverseIterator::operator*()
    const {
  return *(reinterpret_cast<BaseElementType*>(this->item_iterator));
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::ReverseIterator
ListContainer<BaseElementType>::ReverseIterator::
operator++(int unused_post_increment) {
  ReverseIterator tmp = *this;
  operator++();
  return tmp;
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::ReverseIterator
ListContainer<BaseElementType>::ReverseIterator::
operator++() {
  this->ReverseIncrement();
  return *this;
}

// ListContainer::ConstReverseIterator
/////////////////////////////////////////////////
template <typename BaseElementType>
ListContainer<BaseElementType>::ConstReverseIterator::ConstReverseIterator(
    const typename ListContainer<BaseElementType>::ReverseIterator& other)
    : PositionInListContainerCharAllocator(other) {
}

template <typename BaseElementType>
ListContainer<BaseElementType>::ConstReverseIterator::ConstReverseIterator(
    ListContainerCharAllocator* container,
    size_t vector_ind,
    char* item_iter)
    : PositionInListContainerCharAllocator(container, vector_ind, item_iter) {
}

template <typename BaseElementType>
ListContainer<BaseElementType>::ConstReverseIterator::~ConstReverseIterator() {
}

template <typename BaseElementType>
const BaseElementType* ListContainer<BaseElementType>::ConstReverseIterator::
operator->() const {
  return reinterpret_cast<const BaseElementType*>(this->item_iterator);
}

template <typename BaseElementType>
const BaseElementType& ListContainer<BaseElementType>::ConstReverseIterator::
operator*() const {
  return *(reinterpret_cast<const BaseElementType*>(this->item_iterator));
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::ConstReverseIterator
ListContainer<BaseElementType>::ConstReverseIterator::
operator++(int unused_post_increment) {
  ConstReverseIterator tmp = *this;
  operator++();
  return tmp;
}

template <typename BaseElementType>
typename ListContainer<BaseElementType>::ConstReverseIterator
ListContainer<BaseElementType>::ConstReverseIterator::
operator++() {
  this->ReverseIncrement();
  return *this;
}

template class ListContainer<SharedQuadState>;
template class ListContainer<DrawQuad>;

}  // namespace cc
