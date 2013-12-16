// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/data_pipe.h"

#include <string.h>

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "mojo/system/constants.h"
#include "mojo/system/memory.h"
#include "mojo/system/waiter_list.h"

namespace mojo {
namespace system {

void DataPipe::ProducerCancelAllWaiters() {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());
  producer_waiter_list_->CancelAllWaiters();
}

void DataPipe::ProducerClose() {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());
  producer_waiter_list_.reset();
  ProducerCloseImplNoLock();
}

MojoResult DataPipe::ProducerWriteData(const void* elements,
                                       uint32_t* num_elements,
                                       MojoWriteDataFlags flags) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if (producer_in_two_phase_write_)
    return MOJO_RESULT_BUSY;

  // TODO(vtl): This implementation may write less than requested, even if room
  // is available. Fix this. (Probably make a subclass-specific impl.)
  void* buffer = NULL;
  uint32_t buffer_num_elements = *num_elements;
  MojoResult rv = ProducerBeginWriteDataImplNoLock(&buffer,
                                                   &buffer_num_elements,
                                                   flags);
  if (rv != MOJO_RESULT_OK)
    return rv;

  uint32_t num_elements_to_write = std::min(*num_elements, buffer_num_elements);
  memcpy(buffer, elements, num_elements_to_write * element_size_);

  rv = ProducerEndWriteDataImplNoLock(num_elements_to_write);
  if (rv != MOJO_RESULT_OK)
    return rv;

  *num_elements = num_elements_to_write;
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::ProducerBeginWriteData(void** buffer,
                                            uint32_t* buffer_num_elements,
                                            MojoWriteDataFlags flags) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if (producer_in_two_phase_write_)
    return MOJO_RESULT_BUSY;

  MojoResult rv = ProducerBeginWriteDataImplNoLock(buffer,
                                                   buffer_num_elements,
                                                   flags);
  if (rv != MOJO_RESULT_OK)
    return rv;

  producer_in_two_phase_write_ = true;
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::ProducerEndWriteData(uint32_t num_elements_written) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if (!producer_in_two_phase_write_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  MojoResult rv = ProducerEndWriteDataImplNoLock(num_elements_written);
  producer_in_two_phase_write_ = false;  // End two-phase write even on failure.
  return rv;
}

MojoResult DataPipe::ProducerAddWaiter(Waiter* waiter,
                                       MojoWaitFlags flags,
                                       MojoResult wake_result) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if ((flags & ProducerSatisfiedFlagsNoLock()))
    return MOJO_RESULT_ALREADY_EXISTS;
  if (!(flags & ProducerSatisfiableFlagsNoLock()))
    return MOJO_RESULT_FAILED_PRECONDITION;

  producer_waiter_list_->AddWaiter(waiter, flags, wake_result);
  return MOJO_RESULT_OK;
}

void DataPipe::ProducerRemoveWaiter(Waiter* waiter) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());
  producer_waiter_list_->RemoveWaiter(waiter);
}

void DataPipe::ConsumerCancelAllWaiters() {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());
  consumer_waiter_list_->CancelAllWaiters();
}

void DataPipe::ConsumerClose() {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());
  consumer_waiter_list_.reset();
  ConsumerCloseImplNoLock();
}

MojoResult DataPipe::ConsumerReadData(void* elements,
                                      uint32_t* num_elements,
                                      MojoReadDataFlags flags) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (consumer_in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  if ((flags & MOJO_READ_DATA_FLAG_DISCARD)) {
    return ConsumerDiscardDataNoLock(num_elements,
                                     (flags & MOJO_READ_DATA_FLAG_ALL_OR_NONE));
  }
  if ((flags & MOJO_READ_DATA_FLAG_QUERY))
    return ConsumerQueryDataNoLock(num_elements);

  // TODO(vtl): This implementation may write less than requested, even if room
  // is available. Fix this. (Probably make a subclass-specific impl.)
  const void* buffer = NULL;
  uint32_t buffer_num_elements = 0;
  MojoResult rv = ConsumerBeginReadDataImplNoLock(&buffer,
                                                  &buffer_num_elements,
                                                  flags);
  if (rv != MOJO_RESULT_OK)
    return rv;

  uint32_t num_elements_to_read = std::min(*num_elements, buffer_num_elements);
  memcpy(elements, buffer, num_elements_to_read * element_size_);

  rv = ConsumerEndReadDataImplNoLock(num_elements_to_read);
  if (rv != MOJO_RESULT_OK)
    return rv;

  *num_elements = num_elements_to_read;
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::ConsumerBeginReadData(const void** buffer,
                                           uint32_t* buffer_num_elements,
                                           MojoReadDataFlags flags) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (consumer_in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  MojoResult rv = ConsumerBeginReadDataImplNoLock(buffer,
                                                  buffer_num_elements,
                                                  flags);
  if (rv != MOJO_RESULT_OK)
    return rv;

  consumer_in_two_phase_read_ = true;
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::ConsumerEndReadData(uint32_t num_elements_read) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (!consumer_in_two_phase_read_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  MojoResult rv = ConsumerEndReadDataImplNoLock(num_elements_read);
  consumer_in_two_phase_read_ = false;  // End two-phase read even on failure.
  return rv;
}

MojoResult DataPipe::ConsumerAddWaiter(Waiter* waiter,
                                       MojoWaitFlags flags,
                                       MojoResult wake_result) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if ((flags & ConsumerSatisfiedFlagsNoLock()))
    return MOJO_RESULT_ALREADY_EXISTS;
  if (!(flags & ConsumerSatisfiableFlagsNoLock()))
    return MOJO_RESULT_FAILED_PRECONDITION;

  consumer_waiter_list_->AddWaiter(waiter, flags, wake_result);
  return MOJO_RESULT_OK;
}

void DataPipe::ConsumerRemoveWaiter(Waiter* waiter) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());
  consumer_waiter_list_->RemoveWaiter(waiter);
}

DataPipe::DataPipe(bool has_local_producer, bool has_local_consumer)
    : element_size_(0),
      producer_waiter_list_(has_local_producer ? new WaiterList() : NULL),
      consumer_waiter_list_(has_local_consumer ? new WaiterList() : NULL),
      producer_in_two_phase_write_(false),
      consumer_in_two_phase_read_(false) {
  DCHECK(has_local_producer || has_local_consumer);
}

DataPipe::~DataPipe() {
  DCHECK(!has_local_producer_no_lock());
  DCHECK(!has_local_consumer_no_lock());
}

MojoResult DataPipe::Init(bool may_discard,
                          size_t element_size,
                          size_t capacity_num_elements) {
  // No need to lock: This method is not thread-safe.

  if (element_size == 0)
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (!capacity_num_elements) {
    // Set the capacity to the default (rounded down by element size, but always
    // at least one element).
    capacity_num_elements =
        std::max(static_cast<size_t>(1),
                 kDefaultDataPipeCapacityBytes / element_size);
  }
  if (capacity_num_elements >
          std::numeric_limits<uint32_t>::max() / element_size)
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (capacity_num_elements * element_size > kMaxDataPipeCapacityBytes)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  may_discard_ = may_discard;
  element_size_ = element_size;
  capacity_num_elements_ = capacity_num_elements;
  return MOJO_RESULT_OK;
}

void DataPipe::AwakeProducerWaitersForStateChangeNoLock() {
  lock_.AssertAcquired();
  if (!has_local_producer_no_lock())
    return;
  producer_waiter_list_->AwakeWaitersForStateChange(
      ProducerSatisfiedFlagsNoLock(), ProducerSatisfiableFlagsNoLock());
}

void DataPipe::AwakeConsumerWaitersForStateChangeNoLock() {
  lock_.AssertAcquired();
  if (!has_local_consumer_no_lock())
    return;
  consumer_waiter_list_->AwakeWaitersForStateChange(
      ConsumerSatisfiedFlagsNoLock(), ConsumerSatisfiableFlagsNoLock());
}

}  // namespace system
}  // namespace mojo
