// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_shared_memory.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

#include <algorithm>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/numerics/safe_math.h"

namespace base {
namespace {

// Use a machine-sized pointer as atomic type. It will use the Atomic32 or
// Atomic64 routines, depending on the architecture.
typedef intptr_t AtomicType;
typedef uintptr_t UAtomicType;

// Template specialization for timestamp serialization/deserialization. This
// is used to serialize timestamps using Unix time on systems where AtomicType
// does not have enough precision to contain a timestamp in the standard
// serialized format.
template <int>
Time TimeFromWireFormat(int64 value);
template <int>
int64 TimeToWireFormat(Time time);

// Serialize to Unix time when using 4-byte wire format.
// Note: 19 January 2038, this will cease to work.
template <>
Time ALLOW_UNUSED_TYPE TimeFromWireFormat<4>(int64 value) {
  return value ? Time::UnixEpoch() + TimeDelta::FromSeconds(value) : Time();
}
template <>
int64 ALLOW_UNUSED_TYPE TimeToWireFormat<4>(Time time) {
  return time > Time::UnixEpoch() ? (time - Time::UnixEpoch()).InSeconds() : 0;
}

// Standard serialization format when using 8-byte wire format.
template <>
Time ALLOW_UNUSED_TYPE TimeFromWireFormat<8>(int64 value) {
  return Time::FromInternalValue(value);
}
template <>
int64 ALLOW_UNUSED_TYPE TimeToWireFormat<8>(Time time) {
  return time.ToInternalValue();
}

struct SharedState {
  enum LockState { UNLOCKED = 0, LOCKED = 1 };

  explicit SharedState(AtomicType ivalue) { value.i = ivalue; }
  SharedState(LockState lock_state, Time timestamp) {
    int64 wire_timestamp = TimeToWireFormat<sizeof(AtomicType)>(timestamp);
    DCHECK_GE(wire_timestamp, 0);
    DCHECK((lock_state & ~1) == 0);
    value.u = (static_cast<UAtomicType>(wire_timestamp) << 1) | lock_state;
  }

  LockState GetLockState() const { return static_cast<LockState>(value.u & 1); }

  Time GetTimestamp() const {
    return TimeFromWireFormat<sizeof(AtomicType)>(value.u >> 1);
  }

  // Bit 1: Lock state. Bit is set when locked.
  // Bit 2..sizeof(AtomicType)*8: Usage timestamp. NULL time when locked or
  // purged.
  union {
    AtomicType i;
    UAtomicType u;
  } value;
};

// Shared state is stored at offset 0 in shared memory segments.
SharedState* SharedStateFromSharedMemory(const SharedMemory& shared_memory) {
  DCHECK(shared_memory.memory());
  return static_cast<SharedState*>(shared_memory.memory());
}

}  // namespace

DiscardableSharedMemory::DiscardableSharedMemory() {
}

DiscardableSharedMemory::DiscardableSharedMemory(
    SharedMemoryHandle shared_memory_handle)
    : shared_memory_(shared_memory_handle, false) {
}

DiscardableSharedMemory::~DiscardableSharedMemory() {
}

bool DiscardableSharedMemory::CreateAndMap(size_t size) {
  CheckedNumeric<size_t> checked_size = size;
  checked_size += sizeof(SharedState);
  if (!checked_size.IsValid())
    return false;

  if (!shared_memory_.CreateAndMapAnonymous(checked_size.ValueOrDie()))
    return false;

  DCHECK(last_known_usage_.is_null());
  SharedState new_state(SharedState::LOCKED, Time());
  subtle::Release_Store(&SharedStateFromSharedMemory(shared_memory_)->value.i,
                        new_state.value.i);
  return true;
}

bool DiscardableSharedMemory::Map(size_t size) {
  return shared_memory_.Map(sizeof(SharedState) + size);
}

bool DiscardableSharedMemory::Lock() {
  DCHECK(shared_memory_.memory());

  // Return false when instance has been purged or not initialized properly by
  // checking if |last_known_usage_| is NULL.
  if (last_known_usage_.is_null())
    return false;

  SharedState old_state(SharedState::UNLOCKED, last_known_usage_);
  SharedState new_state(SharedState::LOCKED, Time());
  SharedState result(subtle::Acquire_CompareAndSwap(
      &SharedStateFromSharedMemory(shared_memory_)->value.i,
      old_state.value.i,
      new_state.value.i));
  if (result.value.u == old_state.value.u)
    return true;

  // Update |last_known_usage_| in case the above CAS failed because of
  // an incorrect timestamp.
  last_known_usage_ = result.GetTimestamp();
  return false;
}

void DiscardableSharedMemory::Unlock() {
  DCHECK(shared_memory_.memory());

  Time current_time = Now();
  DCHECK(!current_time.is_null());

  SharedState old_state(SharedState::LOCKED, Time());
  SharedState new_state(SharedState::UNLOCKED, current_time);
  // Note: timestamp cannot be NULL as that is a unique value used when
  // locked or purged.
  DCHECK(!new_state.GetTimestamp().is_null());
  // Timestamps precision should at least be accurate to the second.
  DCHECK_EQ((new_state.GetTimestamp() - Time::UnixEpoch()).InSeconds(),
            (current_time - Time::UnixEpoch()).InSeconds());
  SharedState result(subtle::Release_CompareAndSwap(
      &SharedStateFromSharedMemory(shared_memory_)->value.i,
      old_state.value.i,
      new_state.value.i));

  DCHECK_EQ(old_state.value.u, result.value.u);

  last_known_usage_ = current_time;
}

void* DiscardableSharedMemory::memory() const {
  return SharedStateFromSharedMemory(shared_memory_) + 1;
}

bool DiscardableSharedMemory::Purge(Time current_time) {
  // Early out if not mapped. This can happen if the segment was previously
  // unmapped using a call to Close().
  if (!shared_memory_.memory())
    return true;

  SharedState old_state(SharedState::UNLOCKED, last_known_usage_);
  SharedState new_state(SharedState::UNLOCKED, Time());
  SharedState result(subtle::Acquire_CompareAndSwap(
      &SharedStateFromSharedMemory(shared_memory_)->value.i,
      old_state.value.i,
      new_state.value.i));

  // Update |last_known_usage_| to |current_time| if the memory is locked. This
  // allows the caller to determine if purging failed because last known usage
  // was incorrect or memory was locked. In the second case, the caller should
  // most likely wait for some amount of time before attempting to purge the
  // the memory again.
  if (result.value.u != old_state.value.u) {
    last_known_usage_ = result.GetLockState() == SharedState::LOCKED
                            ? current_time
                            : result.GetTimestamp();
    return false;
  }

  last_known_usage_ = Time();
  return true;
}

bool DiscardableSharedMemory::PurgeAndTruncate(Time current_time) {
  if (!Purge(current_time))
    return false;

#if defined(OS_POSIX)
  // Truncate shared memory to size of SharedState.
  SharedMemoryHandle handle = shared_memory_.handle();
  if (SharedMemory::IsHandleValid(handle)) {
    if (HANDLE_EINTR(ftruncate(handle.fd, sizeof(SharedState))) != 0)
      DPLOG(ERROR) << "ftruncate() failed";
  }
#endif

  return true;
}

bool DiscardableSharedMemory::IsMemoryResident() const {
  DCHECK(shared_memory_.memory());

  SharedState result(subtle::NoBarrier_Load(
      &SharedStateFromSharedMemory(shared_memory_)->value.i));

  return result.GetLockState() == SharedState::LOCKED ||
         !result.GetTimestamp().is_null();
}

void DiscardableSharedMemory::Close() {
  shared_memory_.Close();
}

Time DiscardableSharedMemory::Now() const {
  return Time::Now();
}

}  // namespace base
