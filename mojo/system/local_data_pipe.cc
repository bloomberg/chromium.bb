// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(vtl): I currently potentially overflow in doing index calculations.
// E.g., |buffer_first_element_index_| and |buffer_current_num_elements_| fit
// into a |uint32_t|, but their sum may not. This is bad and poses a security
// risk. (We're currently saved by the limit on capacity -- the maximum size of
// the buffer, checked in |DataPipe::Init()|, is currently sufficiently small.

#include "mojo/system/local_data_pipe.h"

#include <algorithm>

#include "base/logging.h"
#include "mojo/system/limits.h"

namespace mojo {
namespace system {

LocalDataPipe::LocalDataPipe()
    : DataPipe(true, true),
      producer_open_(true),
      consumer_open_(true),
      buffer_first_element_index_(0),
      buffer_current_num_elements_(0),
      two_phase_max_elements_written_(0),
      two_phase_max_elements_read_(0) {
}

MojoResult LocalDataPipe::Init(const MojoCreateDataPipeOptions* options) {
  static const MojoCreateDataPipeOptions kDefaultOptions = {
    sizeof(MojoCreateDataPipeOptions),  // |struct_size|.
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,  // |flags|.
    1u,  // |element_size|.
    static_cast<uint32_t>(kDefaultDataPipeCapacityBytes)
  };
  if (!options)
    options = &kDefaultOptions;

  if (options->struct_size < sizeof(*options))
    return MOJO_RESULT_INVALID_ARGUMENT;

  // Note: lazily allocate memory, since a common case will be that one of the
  // handles is immediately passed off to another process.
  return DataPipe::Init(
      (options->flags & MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD),
      static_cast<size_t>(options->element_size),
      static_cast<size_t>(options->capacity_num_elements));
}

LocalDataPipe::~LocalDataPipe() {
  DCHECK(!producer_open_);
  DCHECK(!consumer_open_);
}

void LocalDataPipe::ProducerCloseImplNoLock() {
  DCHECK(producer_open_);
  producer_open_ = false;
  if (!buffer_current_num_elements_) {
    buffer_.reset();
    buffer_current_num_elements_ = 0;
  }
  AwakeConsumerWaitersForStateChangeNoLock();
}

MojoResult LocalDataPipe::ProducerBeginWriteDataImplNoLock(
    void** buffer,
    uint32_t* buffer_num_elements,
    MojoWriteDataFlags flags) {
  size_t max_elements_to_write = GetMaxElementsToWriteNoLock();
  // TODO(vtl): Consider this return value.
  if ((flags & MOJO_WRITE_DATA_FLAG_ALL_OR_NONE) &&
      *buffer_num_elements < max_elements_to_write)
    return MOJO_RESULT_OUT_OF_RANGE;

  size_t next_index = (buffer_first_element_index_ +
                       buffer_current_num_elements_) % capacity_num_elements();
  EnsureBufferNoLock();
  *buffer = buffer_.get() + next_index * element_size();
  *buffer_num_elements = static_cast<uint32_t>(max_elements_to_write);
  two_phase_max_elements_written_ =
      static_cast<uint32_t>(max_elements_to_write);
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipe::ProducerEndWriteDataImplNoLock(
    uint32_t num_elements_written) {
  if (num_elements_written > two_phase_max_elements_written_) {
    // Note: The two-phase write ends here even on failure.
    two_phase_max_elements_written_ = 0;  // For safety.
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  buffer_current_num_elements_ += num_elements_written;
  DCHECK_LE(buffer_current_num_elements_, capacity_num_elements());
  two_phase_max_elements_written_ = 0;  // For safety.
  return MOJO_RESULT_OK;
}

MojoWaitFlags LocalDataPipe::ProducerSatisfiedFlagsNoLock() {
  MojoWaitFlags rv = MOJO_WAIT_FLAG_NONE;
  if (consumer_open_ && buffer_current_num_elements_ < capacity_num_elements())
    rv |= MOJO_WAIT_FLAG_WRITABLE;
  return rv;
}

MojoWaitFlags LocalDataPipe::ProducerSatisfiableFlagsNoLock() {
  MojoWaitFlags rv = MOJO_WAIT_FLAG_NONE;
  if (consumer_open_)
    rv |= MOJO_WAIT_FLAG_WRITABLE;
  return rv;
}

void LocalDataPipe::ConsumerCloseImplNoLock() {
  DCHECK(consumer_open_);
  consumer_open_ = false;
  buffer_.reset();
  buffer_current_num_elements_ = 0;
  AwakeProducerWaitersForStateChangeNoLock();
}

MojoResult LocalDataPipe::ConsumerDiscardDataNoLock(uint32_t* num_elements,
                                                    bool all_or_none) {
  // TODO(vtl): Think about the error code in this case.
  if (all_or_none && *num_elements > buffer_current_num_elements_)
    return MOJO_RESULT_OUT_OF_RANGE;

  size_t num_elements_to_discard =
      std::min(static_cast<size_t>(*num_elements),
               buffer_current_num_elements_);
  buffer_first_element_index_ =
      (buffer_first_element_index_ + num_elements_to_discard) %
          capacity_num_elements();
  buffer_current_num_elements_ -= num_elements_to_discard;

  AwakeProducerWaitersForStateChangeNoLock();
  AwakeConsumerWaitersForStateChangeNoLock();

  *num_elements = static_cast<uint32_t>(num_elements_to_discard);
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipe::ConsumerQueryDataNoLock(uint32_t* num_elements) {
  // Note: This cast is safe, since the capacity fits into a |uint32_t|.
  *num_elements = static_cast<uint32_t>(buffer_current_num_elements_);
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipe::ConsumerBeginReadDataImplNoLock(
    const void** buffer,
    uint32_t* buffer_num_elements,
    MojoReadDataFlags flags) {
  size_t max_elements_to_read = GetMaxElementsToReadNoLock();
  // TODO(vtl): Consider this return value.
  if ((flags & MOJO_READ_DATA_FLAG_ALL_OR_NONE) &&
      *buffer_num_elements < max_elements_to_read)
    return MOJO_RESULT_OUT_OF_RANGE;
  // Note: This works even if |buffer_| is null.
  *buffer = buffer_.get() + buffer_first_element_index_ * element_size();
  *buffer_num_elements = static_cast<uint32_t>(max_elements_to_read);
  two_phase_max_elements_read_ = static_cast<uint32_t>(max_elements_to_read);
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipe::ConsumerEndReadDataImplNoLock(
    uint32_t num_elements_read) {
  if (num_elements_read > two_phase_max_elements_read_) {
    // Note: The two-phase read ends here even on failure.
    two_phase_max_elements_read_ = 0;  // For safety.
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  buffer_first_element_index_ += num_elements_read;
  DCHECK_LE(buffer_first_element_index_, capacity_num_elements());
  buffer_first_element_index_ %= capacity_num_elements();
  DCHECK_LE(num_elements_read, buffer_current_num_elements_);
  buffer_current_num_elements_ -= num_elements_read;
  two_phase_max_elements_read_ = 0;  // For safety.
  return MOJO_RESULT_OK;
}

MojoWaitFlags LocalDataPipe::ConsumerSatisfiedFlagsNoLock() {
  MojoWaitFlags rv = MOJO_WAIT_FLAG_NONE;
  if (buffer_current_num_elements_ > 0)
    rv |= MOJO_WAIT_FLAG_READABLE;
  return rv;
}

MojoWaitFlags LocalDataPipe::ConsumerSatisfiableFlagsNoLock() {
  MojoWaitFlags rv = MOJO_WAIT_FLAG_NONE;
  if (buffer_current_num_elements_ > 0 || producer_open_)
    rv |= MOJO_WAIT_FLAG_READABLE;
  return rv;
}

void LocalDataPipe::EnsureBufferNoLock() {
  DCHECK(producer_open_);
  if (buffer_.get())
    return;
  buffer_.reset(static_cast<char*>(
      base::AlignedAlloc(static_cast<size_t>(capacity_num_elements()) *
                             element_size(),
                         kDataPipeBufferAlignmentBytes)));
}

size_t LocalDataPipe::GetMaxElementsToWriteNoLock() {
  size_t next_index = buffer_first_element_index_ +
                      buffer_current_num_elements_;
  if (next_index >= capacity_num_elements()) {
    next_index %= capacity_num_elements();
    DCHECK_GE(buffer_first_element_index_, next_index);
    return buffer_first_element_index_ - next_index;
  }
  return capacity_num_elements() - next_index;
}

size_t LocalDataPipe::GetMaxElementsToReadNoLock() {
  if (buffer_first_element_index_ + buffer_current_num_elements_ >
          capacity_num_elements())
    return capacity_num_elements() - buffer_first_element_index_;
  return buffer_current_num_elements_;
}

}  // namespace system
}  // namespace mojo
