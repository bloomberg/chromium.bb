// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(vtl): I currently potentially overflow in doing index calculations.
// E.g., |start_index_| and |current_num_bytes_| fit into a |uint32_t|, but
// their sum may not. This is bad and poses a security risk. (We're currently
// saved by the limit on capacity -- the maximum size of the buffer, checked in
// |DataPipe::ValidateOptions()|, is currently sufficiently small.

#include "mojo/system/local_data_pipe.h"

#include <string.h>

#include <algorithm>

#include "base/logging.h"
#include "mojo/system/constants.h"

namespace mojo {
namespace system {

LocalDataPipe::LocalDataPipe(const MojoCreateDataPipeOptions& options)
    : DataPipe(true, true, options),
      start_index_(0),
      current_num_bytes_(0) {
  // Note: |buffer_| is lazily allocated, since a common case will be that one
  // of the handles is immediately passed off to another process.
}

LocalDataPipe::~LocalDataPipe() {
}

void LocalDataPipe::ProducerCloseImplNoLock() {
  // If the consumer is still open and we still have data, we have to keep the
  // buffer around. Currently, we won't free it even if it empties later. (We
  // could do this -- requiring a check on every read -- but that seems to be
  // optimizing for the uncommon case.)
  if (!consumer_open_no_lock() || !current_num_bytes_) {
    // Note: There can only be a two-phase *read* (by the consumer) if we still
    // have data.
    DCHECK(!consumer_in_two_phase_read_no_lock());
    DestroyBufferNoLock();
  }
  AwakeConsumerWaitersForStateChangeNoLock();
}

MojoResult LocalDataPipe::ProducerWriteDataImplNoLock(const void* elements,
                                                      uint32_t* num_bytes,
                                                      bool all_or_none) {
  DCHECK_EQ(*num_bytes % element_num_bytes(), 0u);
  DCHECK_GT(*num_bytes, 0u);

  // TODO(vtl): Consider this return value.
  if (all_or_none && *num_bytes > capacity_num_bytes() - current_num_bytes_)
    return MOJO_RESULT_OUT_OF_RANGE;

  size_t num_bytes_to_write =
      std::min(static_cast<size_t>(*num_bytes),
               capacity_num_bytes() - current_num_bytes_);
  if (num_bytes_to_write == 0)
    return MOJO_RESULT_SHOULD_WAIT;

  // The amount we can write in our first |memcpy()|.
  size_t num_bytes_to_write_first =
      std::min(num_bytes_to_write, GetMaxNumBytesToWriteNoLock());
  // Do the first (and possibly only) |memcpy()|.
  size_t first_write_index =
      (start_index_ + current_num_bytes_) % capacity_num_bytes();
  EnsureBufferNoLock();
  memcpy(buffer_.get() + first_write_index, elements, num_bytes_to_write_first);

  if (num_bytes_to_write_first < num_bytes_to_write) {
    // The "second write index" is zero.
    memcpy(buffer_.get(),
           static_cast<const char*>(elements) + num_bytes_to_write_first,
           num_bytes_to_write - num_bytes_to_write_first);
  }

  bool was_empty = (current_num_bytes_ == 0);

  current_num_bytes_ += num_bytes_to_write;
  DCHECK_LE(current_num_bytes_, capacity_num_bytes());

  if (was_empty && num_bytes_to_write > 0)
    AwakeConsumerWaitersForStateChangeNoLock();

  *num_bytes = static_cast<uint32_t>(num_bytes_to_write);
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipe::ProducerBeginWriteDataImplNoLock(
    void** buffer,
    uint32_t* buffer_num_bytes,
    bool all_or_none) {
  size_t max_num_bytes_to_write = GetMaxNumBytesToWriteNoLock();
  // TODO(vtl): Consider this return value.
  if (all_or_none && *buffer_num_bytes > max_num_bytes_to_write)
    return MOJO_RESULT_OUT_OF_RANGE;

  // Don't go into a two-phase write if there's no room.
  if (max_num_bytes_to_write == 0)
    return MOJO_RESULT_SHOULD_WAIT;

  size_t write_index =
      (start_index_ + current_num_bytes_) % capacity_num_bytes();
  EnsureBufferNoLock();
  *buffer = buffer_.get() + write_index;
  *buffer_num_bytes = static_cast<uint32_t>(max_num_bytes_to_write);
  set_producer_two_phase_max_num_bytes_written_no_lock(
      static_cast<uint32_t>(max_num_bytes_to_write));
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipe::ProducerEndWriteDataImplNoLock(
    uint32_t num_bytes_written) {
  if (num_bytes_written > producer_two_phase_max_num_bytes_written_no_lock()) {
    // Note: The two-phase write ends here even on failure.
    set_producer_two_phase_max_num_bytes_written_no_lock(0);
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  bool was_empty = (current_num_bytes_ == 0);

  current_num_bytes_ += num_bytes_written;
  DCHECK_LE(current_num_bytes_, capacity_num_bytes());
  set_producer_two_phase_max_num_bytes_written_no_lock(0);

  if (was_empty && num_bytes_written > 0)
    AwakeConsumerWaitersForStateChangeNoLock();

  return MOJO_RESULT_OK;
}

MojoWaitFlags LocalDataPipe::ProducerSatisfiedFlagsNoLock() {
  MojoWaitFlags rv = MOJO_WAIT_FLAG_NONE;
  if (consumer_open_no_lock() && current_num_bytes_ < capacity_num_bytes())
    rv |= MOJO_WAIT_FLAG_WRITABLE;
  return rv;
}

MojoWaitFlags LocalDataPipe::ProducerSatisfiableFlagsNoLock() {
  MojoWaitFlags rv = MOJO_WAIT_FLAG_NONE;
  if (consumer_open_no_lock())
    rv |= MOJO_WAIT_FLAG_WRITABLE;
  return rv;
}

void LocalDataPipe::ConsumerCloseImplNoLock() {
  // If the producer is around and in a two-phase write, we have to keep the
  // buffer around. (We then don't free it until the producer is closed. This
  // could be rectified, but again seems like optimizing for the uncommon case.)
  if (!producer_open_no_lock() || !producer_in_two_phase_write_no_lock())
    DestroyBufferNoLock();
  current_num_bytes_ = 0;
  AwakeProducerWaitersForStateChangeNoLock();
}

MojoResult LocalDataPipe::ConsumerReadDataImplNoLock(void* elements,
                                                     uint32_t* num_bytes,
                                                     bool all_or_none) {
  DCHECK_EQ(*num_bytes % element_num_bytes(), 0u);
  DCHECK_GT(*num_bytes, 0u);

  // TODO(vtl): Think about the error code in this case.
  if (all_or_none && *num_bytes > current_num_bytes_)
    return MOJO_RESULT_OUT_OF_RANGE;

  size_t num_bytes_to_read =
      std::min(static_cast<size_t>(*num_bytes), current_num_bytes_);
  if (num_bytes_to_read == 0) {
    return producer_open_no_lock() ? MOJO_RESULT_SHOULD_WAIT :
                                     MOJO_RESULT_FAILED_PRECONDITION;
  }

  // The amount we can read in our first |memcpy()|.
  size_t num_bytes_to_read_first =
      std::min(num_bytes_to_read, GetMaxNumBytesToReadNoLock());
  memcpy(elements, buffer_.get() + start_index_, num_bytes_to_read_first);

  if (num_bytes_to_read_first < num_bytes_to_read) {
    // The "second read index" is zero.
    memcpy(static_cast<char*>(elements) + num_bytes_to_read_first,
           buffer_.get(),
           num_bytes_to_read - num_bytes_to_read_first);
  }

  bool was_full = (current_num_bytes_ == capacity_num_bytes());

  start_index_ += num_bytes_to_read;
  start_index_ %= capacity_num_bytes();
  current_num_bytes_ -= num_bytes_to_read;

  if (was_full && num_bytes_to_read > 0)
    AwakeProducerWaitersForStateChangeNoLock();

  *num_bytes = static_cast<uint32_t>(num_bytes_to_read);
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipe::ConsumerDiscardDataImplNoLock(uint32_t* num_bytes,
                                                        bool all_or_none) {
  DCHECK_EQ(*num_bytes % element_num_bytes(), 0u);
  DCHECK_GT(*num_bytes, 0u);

  // TODO(vtl): Think about the error code in this case.
  if (all_or_none && *num_bytes > current_num_bytes_)
    return MOJO_RESULT_OUT_OF_RANGE;

  // Be consistent with other operations; error if no data available.
  if (current_num_bytes_ == 0) {
    return producer_open_no_lock() ? MOJO_RESULT_SHOULD_WAIT :
                                     MOJO_RESULT_FAILED_PRECONDITION;
  }

  bool was_full = (current_num_bytes_ == capacity_num_bytes());

  size_t num_bytes_to_discard =
      std::min(static_cast<size_t>(*num_bytes), current_num_bytes_);
  start_index_ = (start_index_ + num_bytes_to_discard) % capacity_num_bytes();
  current_num_bytes_ -= num_bytes_to_discard;

  if (was_full && num_bytes_to_discard > 0)
    AwakeProducerWaitersForStateChangeNoLock();

  *num_bytes = static_cast<uint32_t>(num_bytes_to_discard);
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipe::ConsumerQueryDataImplNoLock(uint32_t* num_bytes) {
  // Note: This cast is safe, since the capacity fits into a |uint32_t|.
  *num_bytes = static_cast<uint32_t>(current_num_bytes_);
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipe::ConsumerBeginReadDataImplNoLock(
    const void** buffer,
    uint32_t* buffer_num_bytes,
    bool all_or_none) {
  size_t max_num_bytes_to_read = GetMaxNumBytesToReadNoLock();
  // TODO(vtl): Consider this return value.
  if (all_or_none && *buffer_num_bytes > max_num_bytes_to_read)
    return MOJO_RESULT_OUT_OF_RANGE;

  // Don't go into a two-phase read if there's no data.
  if (max_num_bytes_to_read == 0) {
    return producer_open_no_lock() ? MOJO_RESULT_SHOULD_WAIT :
                                     MOJO_RESULT_FAILED_PRECONDITION;
  }

  *buffer = buffer_.get() + start_index_;
  *buffer_num_bytes = static_cast<uint32_t>(max_num_bytes_to_read);
  set_consumer_two_phase_max_num_bytes_read_no_lock(
      static_cast<uint32_t>(max_num_bytes_to_read));
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipe::ConsumerEndReadDataImplNoLock(
    uint32_t num_bytes_read) {
  if (num_bytes_read > consumer_two_phase_max_num_bytes_read_no_lock()) {
    // Note: The two-phase read ends here even on failure.
    set_consumer_two_phase_max_num_bytes_read_no_lock(0);
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  bool was_full = (current_num_bytes_ == capacity_num_bytes());

  start_index_ += num_bytes_read;
  DCHECK_LE(start_index_, capacity_num_bytes());
  start_index_ %= capacity_num_bytes();
  DCHECK_LE(num_bytes_read, current_num_bytes_);
  current_num_bytes_ -= num_bytes_read;
  set_consumer_two_phase_max_num_bytes_read_no_lock(0);

  if (was_full && num_bytes_read > 0)
    AwakeProducerWaitersForStateChangeNoLock();

  return MOJO_RESULT_OK;
}

MojoWaitFlags LocalDataPipe::ConsumerSatisfiedFlagsNoLock() {
  MojoWaitFlags rv = MOJO_WAIT_FLAG_NONE;
  if (current_num_bytes_ > 0)
    rv |= MOJO_WAIT_FLAG_READABLE;
  return rv;
}

MojoWaitFlags LocalDataPipe::ConsumerSatisfiableFlagsNoLock() {
  MojoWaitFlags rv = MOJO_WAIT_FLAG_NONE;
  if (current_num_bytes_ > 0 || producer_open_no_lock())
    rv |= MOJO_WAIT_FLAG_READABLE;
  return rv;
}

void LocalDataPipe::EnsureBufferNoLock() {
  DCHECK(producer_open_no_lock());
  if (buffer_.get())
    return;
  buffer_.reset(static_cast<char*>(
      base::AlignedAlloc(capacity_num_bytes(), kDataPipeBufferAlignmentBytes)));
}

void LocalDataPipe::DestroyBufferNoLock() {
#ifndef NDEBUG
  // Scribble on the buffer to help detect use-after-frees. (This also helps the
  // unit test detect certain bugs without needing ASAN or similar.)
  if (buffer_.get())
    memset(buffer_.get(), 0xcd, capacity_num_bytes());
#endif
  buffer_.reset();
}

size_t LocalDataPipe::GetMaxNumBytesToWriteNoLock() {
  size_t next_index = start_index_ + current_num_bytes_;
  if (next_index >= capacity_num_bytes()) {
    next_index %= capacity_num_bytes();
    DCHECK_GE(start_index_, next_index);
    DCHECK_EQ(start_index_ - next_index,
              capacity_num_bytes() - current_num_bytes_);
    return start_index_ - next_index;
  }
  return capacity_num_bytes() - next_index;
}

size_t LocalDataPipe::GetMaxNumBytesToReadNoLock() {
  if (start_index_ + current_num_bytes_ > capacity_num_bytes())
    return capacity_num_bytes() - start_index_;
  return current_num_bytes_;
}

}  // namespace system
}  // namespace mojo
