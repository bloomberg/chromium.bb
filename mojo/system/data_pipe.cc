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
  // TODO(vtl): FIXME -- "cancel" any two-phase write (do we need to do this?)
  ProducerCloseImplNoLock();
}

MojoResult DataPipe::ProducerWriteData(const void* elements,
                                       uint32_t* num_bytes,
                                       MojoWriteDataFlags flags) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if (producer_in_two_phase_write_)
    return MOJO_RESULT_BUSY;

  // Returning "busy" takes priority over "invalid argument".
  if (*num_bytes % element_num_bytes_ != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (*num_bytes == 0)
    return MOJO_RESULT_OK;  // Nothing to do.

  return ProducerWriteDataImplNoLock(elements, num_bytes, flags);
}

MojoResult DataPipe::ProducerBeginWriteData(void** buffer,
                                            uint32_t* buffer_num_bytes,
                                            MojoWriteDataFlags flags) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if (producer_in_two_phase_write_)
    return MOJO_RESULT_BUSY;

  MojoResult rv = ProducerBeginWriteDataImplNoLock(buffer,
                                                   buffer_num_bytes,
                                                   flags);
  if (rv != MOJO_RESULT_OK)
    return rv;

  producer_in_two_phase_write_ = true;
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::ProducerEndWriteData(uint32_t num_bytes_written) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if (!producer_in_two_phase_write_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  MojoResult rv = ProducerEndWriteDataImplNoLock(num_bytes_written);
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
  // TODO(vtl): FIXME -- "cancel" any two-phase read (do we need to do this?)
  ConsumerCloseImplNoLock();
}

MojoResult DataPipe::ConsumerReadData(void* elements,
                                      uint32_t* num_bytes,
                                      MojoReadDataFlags flags) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (consumer_in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  // Note: Don't need to validate |*num_bytes| for query.
  if ((flags & MOJO_READ_DATA_FLAG_QUERY))
    return ConsumerQueryDataNoLock(num_bytes);

  if (*num_bytes % element_num_bytes_ != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (*num_bytes == 0)
    return MOJO_RESULT_OK;  // Nothing to do (for read or for discard).

  if ((flags & MOJO_READ_DATA_FLAG_DISCARD)) {
    return ConsumerDiscardDataNoLock(num_bytes,
                                     (flags & MOJO_READ_DATA_FLAG_ALL_OR_NONE));
  }

  return ConsumerReadDataImplNoLock(elements, num_bytes, flags);
}

MojoResult DataPipe::ConsumerBeginReadData(const void** buffer,
                                           uint32_t* buffer_num_bytes,
                                           MojoReadDataFlags flags) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (consumer_in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  MojoResult rv = ConsumerBeginReadDataImplNoLock(buffer,
                                                  buffer_num_bytes,
                                                  flags);
  if (rv != MOJO_RESULT_OK)
    return rv;

  consumer_in_two_phase_read_ = true;
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::ConsumerEndReadData(uint32_t num_bytes_read) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (!consumer_in_two_phase_read_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  MojoResult rv = ConsumerEndReadDataImplNoLock(num_bytes_read);
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
    : element_num_bytes_(0),
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
                          size_t element_num_bytes,
                          size_t capacity_num_bytes) {
  // No need to lock: This method is not thread-safe.

  if (element_num_bytes == 0)
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (capacity_num_bytes == 0) {
    // Round the default capacity down to a multiple of the element size.
    capacity_num_bytes = kDefaultDataPipeCapacityBytes -
        (kDefaultDataPipeCapacityBytes % element_num_bytes);
    // But make sure that the capacity is still at least one.
    if (capacity_num_bytes == 0)
      capacity_num_bytes = element_num_bytes;
  }
  if (capacity_num_bytes > kMaxDataPipeCapacityBytes)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  may_discard_ = may_discard;
  element_num_bytes_ = element_num_bytes;
  capacity_num_bytes_ = capacity_num_bytes;
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
