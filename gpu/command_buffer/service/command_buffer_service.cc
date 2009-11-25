// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/command_buffer_service.h"

using ::base::SharedMemory;

namespace command_buffer {

CommandBufferService::CommandBufferService()
    : size_(0),
      get_offset_(0),
      put_offset_(0),
      token_(0),
      parse_error_(0),
      error_status_(false) {
  // Element zero is always NULL.
  registered_objects_.push_back(linked_ptr<SharedMemory>());
}

CommandBufferService::~CommandBufferService() {
}

bool CommandBufferService::Initialize(::base::SharedMemory* ring_buffer) {
  DCHECK(ring_buffer);

  // Fail if already initialized.
  if (ring_buffer_.get())
    return false;

  size_t size_in_bytes = ring_buffer->max_size();
  size_ = size_in_bytes / sizeof(int32);
  ring_buffer_.reset(ring_buffer);

  return true;
}

SharedMemory* CommandBufferService::GetRingBuffer() {
  return ring_buffer_.get();
}

int32 CommandBufferService::GetSize() {
  return size_;
}

int32 CommandBufferService::SyncOffsets(int32 put_offset) {
  if (put_offset < 0 || put_offset >= size_)
    return -1;

  put_offset_ = put_offset;

  if (put_offset_change_callback_.get()) {
    put_offset_change_callback_->Run();
  }

  return get_offset_;
}

int32 CommandBufferService::GetGetOffset() {
  return get_offset_;
}

void CommandBufferService::SetGetOffset(int32 get_offset) {
  DCHECK(get_offset >= 0 && get_offset < size_);
  get_offset_ = get_offset;
}

int32 CommandBufferService::GetPutOffset() {
  return put_offset_;
}

void CommandBufferService::SetPutOffsetChangeCallback(
    Callback0::Type* callback) {
  put_offset_change_callback_.reset(callback);
}

int32 CommandBufferService::CreateTransferBuffer(size_t size) {
  linked_ptr<SharedMemory> buffer(new SharedMemory);
  if (!buffer->Create(std::wstring(), false, false, size))
    return -1;

  if (unused_registered_object_elements_.empty()) {
    // Check we haven't exceeded the range that fits in a 32-bit integer.
    int32 handle = static_cast<int32>(registered_objects_.size());
    if (handle != registered_objects_.size())
      return -1;

    registered_objects_.push_back(buffer);
    return handle;
  }

  int32 handle = *unused_registered_object_elements_.begin();
  unused_registered_object_elements_.erase(
      unused_registered_object_elements_.begin());
  DCHECK(!registered_objects_[handle].get());
  registered_objects_[handle] = buffer;
  return handle;
}

void CommandBufferService::DestroyTransferBuffer(int32 handle) {
  if (handle <= 0)
    return;

  if (static_cast<size_t>(handle) >= registered_objects_.size())
    return;

  registered_objects_[handle].reset();
  unused_registered_object_elements_.insert(handle);

  // Remove all null objects from the end of the vector. This allows the vector
  // to shrink when, for example, all objects are unregistered. Note that this
  // loop never removes element zero, which is always NULL.
  while (registered_objects_.size() > 1 && !registered_objects_.back().get()) {
    registered_objects_.pop_back();
    unused_registered_object_elements_.erase(
        static_cast<int32>(registered_objects_.size()));
  }
}

::base::SharedMemory* CommandBufferService::GetTransferBuffer(int32 handle) {
  if (handle < 0)
    return NULL;

  if (static_cast<size_t>(handle) >= registered_objects_.size())
    return NULL;

  return registered_objects_[handle].get();
}

int32 CommandBufferService::GetToken() {
  return token_;
}

void CommandBufferService::SetToken(int32 token) {
  token_ = token;
}

int32 CommandBufferService::ResetParseError() {
  int32 last_error = parse_error_;
  parse_error_ = 0;
  return last_error;
}

void CommandBufferService::SetParseError(int32 parse_error) {
  if (parse_error_ == 0) {
    parse_error_ = parse_error;
  }
}

bool CommandBufferService::GetErrorStatus() {
  return error_status_;
}

void CommandBufferService::RaiseErrorStatus() {
  error_status_ = true;
}

}  // namespace command_buffer
