// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/data_pipe.h"

#include <string.h>

#include <algorithm>
#include <limits>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "mojo/system/constants.h"
#include "mojo/system/memory.h"
#include "mojo/system/options_validation.h"
#include "mojo/system/waiter_list.h"

namespace mojo {
namespace system {

// static
const MojoCreateDataPipeOptions DataPipe::kDefaultCreateOptions = {
    static_cast<uint32_t>(sizeof(MojoCreateDataPipeOptions)),
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE, 1u,
    static_cast<uint32_t>(kDefaultDataPipeCapacityBytes)};

// static
MojoResult DataPipe::ValidateCreateOptions(
    UserPointer<const MojoCreateDataPipeOptions> in_options,
    MojoCreateDataPipeOptions* out_options) {
  const MojoCreateDataPipeOptionsFlags kKnownFlags =
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD;

  *out_options = kDefaultCreateOptions;
  if (in_options.IsNull())
    return MOJO_RESULT_OK;

  UserOptionsReader<MojoCreateDataPipeOptions> reader(in_options);
  if (!reader.is_valid())
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (!OPTIONS_STRUCT_HAS_MEMBER(MojoCreateDataPipeOptions, flags, reader))
    return MOJO_RESULT_OK;
  if ((reader.options().flags & ~kKnownFlags))
    return MOJO_RESULT_UNIMPLEMENTED;
  out_options->flags = reader.options().flags;

  // Checks for fields beyond |flags|:

  if (!OPTIONS_STRUCT_HAS_MEMBER(
          MojoCreateDataPipeOptions, element_num_bytes, reader))
    return MOJO_RESULT_OK;
  if (reader.options().element_num_bytes == 0)
    return MOJO_RESULT_INVALID_ARGUMENT;
  out_options->element_num_bytes = reader.options().element_num_bytes;

  if (!OPTIONS_STRUCT_HAS_MEMBER(
          MojoCreateDataPipeOptions, capacity_num_bytes, reader) ||
      reader.options().capacity_num_bytes == 0) {
    // Round the default capacity down to a multiple of the element size (but at
    // least one element).
    out_options->capacity_num_bytes =
        std::max(static_cast<uint32_t>(kDefaultDataPipeCapacityBytes -
                                       (kDefaultDataPipeCapacityBytes %
                                        out_options->element_num_bytes)),
                 out_options->element_num_bytes);
    return MOJO_RESULT_OK;
  }
  if (reader.options().capacity_num_bytes % out_options->element_num_bytes != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (reader.options().capacity_num_bytes > kMaxDataPipeCapacityBytes)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  out_options->capacity_num_bytes = reader.options().capacity_num_bytes;

  return MOJO_RESULT_OK;
}

void DataPipe::ProducerCancelAllWaiters() {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());
  producer_waiter_list_->CancelAllWaiters();
}

void DataPipe::ProducerClose() {
  base::AutoLock locker(lock_);
  DCHECK(producer_open_);
  producer_open_ = false;
  DCHECK(has_local_producer_no_lock());
  producer_waiter_list_.reset();
  // Not a bug, except possibly in "user" code.
  DVLOG_IF(2, producer_in_two_phase_write_no_lock())
      << "Producer closed with active two-phase write";
  producer_two_phase_max_num_bytes_written_ = 0;
  ProducerCloseImplNoLock();
  AwakeConsumerWaitersForStateChangeNoLock(
      ConsumerGetHandleSignalsStateImplNoLock());
}

MojoResult DataPipe::ProducerWriteData(UserPointer<const void> elements,
                                       UserPointer<uint32_t> num_bytes,
                                       bool all_or_none) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if (producer_in_two_phase_write_no_lock())
    return MOJO_RESULT_BUSY;
  if (!consumer_open_no_lock())
    return MOJO_RESULT_FAILED_PRECONDITION;

  // Returning "busy" takes priority over "invalid argument".
  uint32_t max_num_bytes_to_write = num_bytes.Get();
  if (max_num_bytes_to_write % element_num_bytes_ != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (max_num_bytes_to_write == 0)
    return MOJO_RESULT_OK;  // Nothing to do.

  uint32_t min_num_bytes_to_write = all_or_none ? max_num_bytes_to_write : 0;

  HandleSignalsState old_consumer_state =
      ConsumerGetHandleSignalsStateImplNoLock();
  MojoResult rv = ProducerWriteDataImplNoLock(
      elements, num_bytes, max_num_bytes_to_write, min_num_bytes_to_write);
  HandleSignalsState new_consumer_state =
      ConsumerGetHandleSignalsStateImplNoLock();
  if (!new_consumer_state.equals(old_consumer_state))
    AwakeConsumerWaitersForStateChangeNoLock(new_consumer_state);
  return rv;
}

MojoResult DataPipe::ProducerBeginWriteData(
    UserPointer<void*> buffer,
    UserPointer<uint32_t> buffer_num_bytes,
    bool all_or_none) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if (producer_in_two_phase_write_no_lock())
    return MOJO_RESULT_BUSY;
  if (!consumer_open_no_lock())
    return MOJO_RESULT_FAILED_PRECONDITION;

  uint32_t min_num_bytes_to_write = 0;
  if (all_or_none) {
    min_num_bytes_to_write = buffer_num_bytes.Get();
    if (min_num_bytes_to_write % element_num_bytes_ != 0)
      return MOJO_RESULT_INVALID_ARGUMENT;
  }

  MojoResult rv = ProducerBeginWriteDataImplNoLock(
      buffer, buffer_num_bytes, min_num_bytes_to_write);
  if (rv != MOJO_RESULT_OK)
    return rv;
  // Note: No need to awake producer waiters, even though we're going from
  // writable to non-writable (since you can't wait on non-writability).
  // Similarly, though this may have discarded data (in "may discard" mode),
  // making it non-readable, there's still no need to awake consumer waiters.
  DCHECK(producer_in_two_phase_write_no_lock());
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::ProducerEndWriteData(uint32_t num_bytes_written) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if (!producer_in_two_phase_write_no_lock())
    return MOJO_RESULT_FAILED_PRECONDITION;
  // Note: Allow successful completion of the two-phase write even if the
  // consumer has been closed.

  HandleSignalsState old_consumer_state =
      ConsumerGetHandleSignalsStateImplNoLock();
  MojoResult rv;
  if (num_bytes_written > producer_two_phase_max_num_bytes_written_ ||
      num_bytes_written % element_num_bytes_ != 0) {
    rv = MOJO_RESULT_INVALID_ARGUMENT;
    producer_two_phase_max_num_bytes_written_ = 0;
  } else {
    rv = ProducerEndWriteDataImplNoLock(num_bytes_written);
  }
  // Two-phase write ended even on failure.
  DCHECK(!producer_in_two_phase_write_no_lock());
  // If we're now writable, we *became* writable (since we weren't writable
  // during the two-phase write), so awake producer waiters.
  HandleSignalsState new_producer_state =
      ProducerGetHandleSignalsStateImplNoLock();
  if (new_producer_state.satisfies(MOJO_HANDLE_SIGNAL_WRITABLE))
    AwakeProducerWaitersForStateChangeNoLock(new_producer_state);
  HandleSignalsState new_consumer_state =
      ConsumerGetHandleSignalsStateImplNoLock();
  if (!new_consumer_state.equals(old_consumer_state))
    AwakeConsumerWaitersForStateChangeNoLock(new_consumer_state);
  return rv;
}

HandleSignalsState DataPipe::ProducerGetHandleSignalsState() {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());
  return ProducerGetHandleSignalsStateImplNoLock();
}

MojoResult DataPipe::ProducerAddWaiter(Waiter* waiter,
                                       MojoHandleSignals signals,
                                       uint32_t context,
                                       HandleSignalsState* signals_state) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  HandleSignalsState producer_state = ProducerGetHandleSignalsStateImplNoLock();
  if (producer_state.satisfies(signals)) {
    if (signals_state)
      *signals_state = producer_state;
    return MOJO_RESULT_ALREADY_EXISTS;
  }
  if (!producer_state.can_satisfy(signals)) {
    if (signals_state)
      *signals_state = producer_state;
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  producer_waiter_list_->AddWaiter(waiter, signals, context);
  return MOJO_RESULT_OK;
}

void DataPipe::ProducerRemoveWaiter(Waiter* waiter,
                                    HandleSignalsState* signals_state) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());
  producer_waiter_list_->RemoveWaiter(waiter);
  if (signals_state)
    *signals_state = ProducerGetHandleSignalsStateImplNoLock();
}

bool DataPipe::ProducerIsBusy() const {
  base::AutoLock locker(lock_);
  return producer_in_two_phase_write_no_lock();
}

void DataPipe::ConsumerCancelAllWaiters() {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());
  consumer_waiter_list_->CancelAllWaiters();
}

void DataPipe::ConsumerClose() {
  base::AutoLock locker(lock_);
  DCHECK(consumer_open_);
  consumer_open_ = false;
  DCHECK(has_local_consumer_no_lock());
  consumer_waiter_list_.reset();
  // Not a bug, except possibly in "user" code.
  DVLOG_IF(2, consumer_in_two_phase_read_no_lock())
      << "Consumer closed with active two-phase read";
  consumer_two_phase_max_num_bytes_read_ = 0;
  ConsumerCloseImplNoLock();
  AwakeProducerWaitersForStateChangeNoLock(
      ProducerGetHandleSignalsStateImplNoLock());
}

MojoResult DataPipe::ConsumerReadData(UserPointer<void> elements,
                                      UserPointer<uint32_t> num_bytes,
                                      bool all_or_none) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (consumer_in_two_phase_read_no_lock())
    return MOJO_RESULT_BUSY;

  uint32_t max_num_bytes_to_read = num_bytes.Get();
  if (max_num_bytes_to_read % element_num_bytes_ != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (max_num_bytes_to_read == 0)
    return MOJO_RESULT_OK;  // Nothing to do.

  uint32_t min_num_bytes_to_read = all_or_none ? max_num_bytes_to_read : 0;

  HandleSignalsState old_producer_state =
      ProducerGetHandleSignalsStateImplNoLock();
  MojoResult rv = ConsumerReadDataImplNoLock(
      elements, num_bytes, max_num_bytes_to_read, min_num_bytes_to_read);
  HandleSignalsState new_producer_state =
      ProducerGetHandleSignalsStateImplNoLock();
  if (!new_producer_state.equals(old_producer_state))
    AwakeProducerWaitersForStateChangeNoLock(new_producer_state);
  return rv;
}

MojoResult DataPipe::ConsumerDiscardData(UserPointer<uint32_t> num_bytes,
                                         bool all_or_none) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (consumer_in_two_phase_read_no_lock())
    return MOJO_RESULT_BUSY;

  uint32_t max_num_bytes_to_discard = num_bytes.Get();
  if (max_num_bytes_to_discard % element_num_bytes_ != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (max_num_bytes_to_discard == 0)
    return MOJO_RESULT_OK;  // Nothing to do.

  uint32_t min_num_bytes_to_discard =
      all_or_none ? max_num_bytes_to_discard : 0;

  HandleSignalsState old_producer_state =
      ProducerGetHandleSignalsStateImplNoLock();
  MojoResult rv = ConsumerDiscardDataImplNoLock(
      num_bytes, max_num_bytes_to_discard, min_num_bytes_to_discard);
  HandleSignalsState new_producer_state =
      ProducerGetHandleSignalsStateImplNoLock();
  if (!new_producer_state.equals(old_producer_state))
    AwakeProducerWaitersForStateChangeNoLock(new_producer_state);
  return rv;
}

MojoResult DataPipe::ConsumerQueryData(UserPointer<uint32_t> num_bytes) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (consumer_in_two_phase_read_no_lock())
    return MOJO_RESULT_BUSY;

  // Note: Don't need to validate |*num_bytes| for query.
  return ConsumerQueryDataImplNoLock(num_bytes);
}

MojoResult DataPipe::ConsumerBeginReadData(
    UserPointer<const void*> buffer,
    UserPointer<uint32_t> buffer_num_bytes,
    bool all_or_none) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (consumer_in_two_phase_read_no_lock())
    return MOJO_RESULT_BUSY;

  uint32_t min_num_bytes_to_read = 0;
  if (all_or_none) {
    min_num_bytes_to_read = buffer_num_bytes.Get();
    if (min_num_bytes_to_read % element_num_bytes_ != 0)
      return MOJO_RESULT_INVALID_ARGUMENT;
  }

  MojoResult rv = ConsumerBeginReadDataImplNoLock(
      buffer, buffer_num_bytes, min_num_bytes_to_read);
  if (rv != MOJO_RESULT_OK)
    return rv;
  DCHECK(consumer_in_two_phase_read_no_lock());
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::ConsumerEndReadData(uint32_t num_bytes_read) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (!consumer_in_two_phase_read_no_lock())
    return MOJO_RESULT_FAILED_PRECONDITION;

  HandleSignalsState old_producer_state =
      ProducerGetHandleSignalsStateImplNoLock();
  MojoResult rv;
  if (num_bytes_read > consumer_two_phase_max_num_bytes_read_ ||
      num_bytes_read % element_num_bytes_ != 0) {
    rv = MOJO_RESULT_INVALID_ARGUMENT;
    consumer_two_phase_max_num_bytes_read_ = 0;
  } else {
    rv = ConsumerEndReadDataImplNoLock(num_bytes_read);
  }
  // Two-phase read ended even on failure.
  DCHECK(!consumer_in_two_phase_read_no_lock());
  // If we're now readable, we *became* readable (since we weren't readable
  // during the two-phase read), so awake consumer waiters.
  HandleSignalsState new_consumer_state =
      ConsumerGetHandleSignalsStateImplNoLock();
  if (new_consumer_state.satisfies(MOJO_HANDLE_SIGNAL_READABLE))
    AwakeConsumerWaitersForStateChangeNoLock(new_consumer_state);
  HandleSignalsState new_producer_state =
      ProducerGetHandleSignalsStateImplNoLock();
  if (!new_producer_state.equals(old_producer_state))
    AwakeProducerWaitersForStateChangeNoLock(new_producer_state);
  return rv;
}

HandleSignalsState DataPipe::ConsumerGetHandleSignalsState() {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());
  return ConsumerGetHandleSignalsStateImplNoLock();
}

MojoResult DataPipe::ConsumerAddWaiter(Waiter* waiter,
                                       MojoHandleSignals signals,
                                       uint32_t context,
                                       HandleSignalsState* signals_state) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  HandleSignalsState consumer_state = ConsumerGetHandleSignalsStateImplNoLock();
  if (consumer_state.satisfies(signals)) {
    if (signals_state)
      *signals_state = consumer_state;
    return MOJO_RESULT_ALREADY_EXISTS;
  }
  if (!consumer_state.can_satisfy(signals)) {
    if (signals_state)
      *signals_state = consumer_state;
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  consumer_waiter_list_->AddWaiter(waiter, signals, context);
  return MOJO_RESULT_OK;
}

void DataPipe::ConsumerRemoveWaiter(Waiter* waiter,
                                    HandleSignalsState* signals_state) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());
  consumer_waiter_list_->RemoveWaiter(waiter);
  if (signals_state)
    *signals_state = ConsumerGetHandleSignalsStateImplNoLock();
}

bool DataPipe::ConsumerIsBusy() const {
  base::AutoLock locker(lock_);
  return consumer_in_two_phase_read_no_lock();
}

DataPipe::DataPipe(bool has_local_producer,
                   bool has_local_consumer,
                   const MojoCreateDataPipeOptions& validated_options)
    : may_discard_((validated_options.flags &
                    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD)),
      element_num_bytes_(validated_options.element_num_bytes),
      capacity_num_bytes_(validated_options.capacity_num_bytes),
      producer_open_(true),
      consumer_open_(true),
      producer_waiter_list_(has_local_producer ? new WaiterList() : NULL),
      consumer_waiter_list_(has_local_consumer ? new WaiterList() : NULL),
      producer_two_phase_max_num_bytes_written_(0),
      consumer_two_phase_max_num_bytes_read_(0) {
  // Check that the passed in options actually are validated.
  MojoCreateDataPipeOptions unused ALLOW_UNUSED = {0};
  DCHECK_EQ(ValidateCreateOptions(MakeUserPointer(&validated_options), &unused),
            MOJO_RESULT_OK);
}

DataPipe::~DataPipe() {
  DCHECK(!producer_open_);
  DCHECK(!consumer_open_);
  DCHECK(!producer_waiter_list_);
  DCHECK(!consumer_waiter_list_);
}

void DataPipe::AwakeProducerWaitersForStateChangeNoLock(
    const HandleSignalsState& new_producer_state) {
  lock_.AssertAcquired();
  if (!has_local_producer_no_lock())
    return;
  producer_waiter_list_->AwakeWaitersForStateChange(new_producer_state);
}

void DataPipe::AwakeConsumerWaitersForStateChangeNoLock(
    const HandleSignalsState& new_consumer_state) {
  lock_.AssertAcquired();
  if (!has_local_consumer_no_lock())
    return;
  consumer_waiter_list_->AwakeWaitersForStateChange(new_consumer_state);
}

}  // namespace system
}  // namespace mojo
