// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/contiguous_container.h"

#include <stddef.h>
#include <algorithm>
#include <utility>

namespace cc {

// Default number of max-sized elements to allocate space for, if there is no
// initial buffer.
static const unsigned kDefaultInitialBufferSize = 32;

class ContiguousContainerBase::Buffer {
 public:
  explicit Buffer(size_t buffer_size) : capacity_(buffer_size) {}
  ~Buffer() = default;

  Buffer(Buffer&&) = default;
  Buffer& operator=(Buffer&&) = default;

  size_t Capacity() const { return capacity_; }
  size_t UsedCapacity() const { return end_ - data_.get(); }
  size_t UnusedCapacity() const { return Capacity() - UsedCapacity(); }
  size_t MemoryUsage() const { return data_ ? capacity_ : 0; }
  bool empty() const { return UsedCapacity() == 0; }

  void* Allocate(size_t object_size) {
    DCHECK_GE(UnusedCapacity(), object_size);
    if (!data_) {
      data_.reset(new char[capacity_]);
      end_ = data_.get();
    }
    void* result = end_;
    end_ += object_size;
    return result;
  }

 private:
  size_t capacity_;
  std::unique_ptr<char[]> data_;
  // begin() <= end_ <= begin() + capacity_
  char* end_ = nullptr;
};

ContiguousContainerBase::ContiguousContainerBase(size_t max_object_size)
    : max_object_size_(max_object_size) {}

ContiguousContainerBase::ContiguousContainerBase(size_t max_object_size,
                                                 size_t initial_size_bytes)
    : max_object_size_(max_object_size) {
  buffers_.emplace_back(std::max(max_object_size_, initial_size_bytes));
}

ContiguousContainerBase::~ContiguousContainerBase() {}

size_t ContiguousContainerBase::GetCapacityInBytes() const {
  size_t capacity = 0;
  for (const auto& buffer : buffers_)
    capacity += buffer.Capacity();
  return capacity;
}

size_t ContiguousContainerBase::UsedCapacityInBytes() const {
  size_t used_capacity = 0;
  for (const auto& buffer : buffers_)
    used_capacity += buffer.UsedCapacity();
  return used_capacity;
}

size_t ContiguousContainerBase::MemoryUsageInBytes() const {
  size_t memory_usage = 0;
  for (const auto& buffer : buffers_)
    memory_usage += buffer.MemoryUsage();
  return sizeof(*this) + memory_usage +
         elements_.capacity() * sizeof(elements_[0]);
}

void* ContiguousContainerBase::Allocate(size_t object_size) {
  DCHECK_LE(object_size, max_object_size_);

  if (buffers_.empty())
    buffers_.emplace_back(kDefaultInitialBufferSize * max_object_size_);
  else if (buffers_.back().UnusedCapacity() < object_size)
    buffers_.emplace_back(2 * buffers_.back().Capacity());

  void* element = buffers_.back().Allocate(object_size);
  elements_.push_back(element);
  return element;
}

}  // namespace cc
