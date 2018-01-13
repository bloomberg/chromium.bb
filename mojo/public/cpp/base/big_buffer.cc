// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/big_buffer.h"

#include "base/logging.h"

namespace mojo_base {

namespace internal {

BigBufferSharedMemoryRegion::BigBufferSharedMemoryRegion() = default;

BigBufferSharedMemoryRegion::BigBufferSharedMemoryRegion(
    mojo::ScopedSharedBufferHandle buffer_handle,
    size_t size)
    : size_(size),
      buffer_handle_(std::move(buffer_handle)),
      buffer_mapping_(buffer_handle_->Map(size)) {}

BigBufferSharedMemoryRegion::BigBufferSharedMemoryRegion(
    BigBufferSharedMemoryRegion&& other) = default;

BigBufferSharedMemoryRegion::~BigBufferSharedMemoryRegion() = default;

BigBufferSharedMemoryRegion& BigBufferSharedMemoryRegion::operator=(
    BigBufferSharedMemoryRegion&& other) = default;

mojo::ScopedSharedBufferHandle BigBufferSharedMemoryRegion::TakeBufferHandle() {
  DCHECK(buffer_handle_.is_valid());
  buffer_mapping_.reset();
  return std::move(buffer_handle_);
}

}  // namespace internal

// static
constexpr size_t BigBuffer::kMaxInlineBytes;

BigBuffer::BigBuffer() : storage_type_(StorageType::kBytes) {}

BigBuffer::BigBuffer(BigBuffer&& other) = default;

BigBuffer::BigBuffer(base::span<const uint8_t> data) {
  if (data.size() > kMaxInlineBytes) {
    auto buffer = mojo::SharedBufferHandle::Create(data.size());
    if (buffer.is_valid()) {
      storage_type_ = StorageType::kSharedMemory;
      shared_memory_.emplace(std::move(buffer), data.size());
      std::copy(data.begin(), data.end(),
                static_cast<uint8_t*>(shared_memory_->buffer_mapping_.get()));
      return;
    }
  }

  // Either the data is small enough that we're happy to carry it in an array,
  // or shared buffer allocation failed and we fall back on this as a best-
  // effort alternative.
  storage_type_ = StorageType::kBytes;
  bytes_.resize(data.size());
  std::copy(data.begin(), data.end(), bytes_.begin());
}

BigBuffer::BigBuffer(const std::vector<uint8_t>& data)
    : BigBuffer(base::make_span(data)) {}

BigBuffer::BigBuffer(internal::BigBufferSharedMemoryRegion shared_memory)
    : storage_type_(StorageType::kSharedMemory),
      shared_memory_(std::move(shared_memory)) {}

BigBuffer::~BigBuffer() = default;

BigBuffer& BigBuffer::operator=(BigBuffer&& other) = default;

uint8_t* BigBuffer::data() {
  return const_cast<uint8_t*>(const_cast<const BigBuffer*>(this)->data());
}

const uint8_t* BigBuffer::data() const {
  switch (storage_type_) {
    case StorageType::kBytes:
      return bytes_.data();
    case StorageType::kSharedMemory:
      DCHECK(shared_memory_->buffer_mapping_);
      return static_cast<const uint8_t*>(
          const_cast<const void*>(shared_memory_->buffer_mapping_.get()));
    default:
      NOTREACHED();
      return nullptr;
  }
}

size_t BigBuffer::size() const {
  switch (storage_type_) {
    case StorageType::kBytes:
      return bytes_.size();
    case StorageType::kSharedMemory:
      return shared_memory_->size();
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace mojo_base
