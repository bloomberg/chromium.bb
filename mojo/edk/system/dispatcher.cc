// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/data_pipe_consumer_dispatcher.h"
#include "mojo/edk/system/data_pipe_producer_dispatcher.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"
#include "mojo/edk/system/platform_handle_dispatcher.h"
#include "mojo/edk/system/shared_buffer_dispatcher.h"

namespace mojo {
namespace edk {

namespace test {

// TODO(vtl): Maybe this should be defined in a test-only file instead.
DispatcherTransport DispatcherTryStartTransport(Dispatcher* dispatcher) {
  return Dispatcher::HandleTableAccess::TryStartTransport(dispatcher);
}

}  // namespace test

// Dispatcher ------------------------------------------------------------------

// static
DispatcherTransport Dispatcher::HandleTableAccess::TryStartTransport(
    Dispatcher* dispatcher) {
  DCHECK(dispatcher);

  // Our dispatcher implementations hop to IO thread on initialization, so it's
  // valid that while their RawChannel os being initialized on IO thread, the
  // dispatcher is being sent. We handle this by just acquiring the lock.

  // See comment in header for why we need this.
  dispatcher->TransportStarted();

  dispatcher->lock_.Acquire();

  // We shouldn't race with things that close dispatchers, since closing can
  // only take place either under |handle_table_lock_| or when the handle is
  // marked as busy.
  DCHECK(!dispatcher->is_closed_);

  return DispatcherTransport(dispatcher);
}

// static
void Dispatcher::TransportDataAccess::StartSerialize(
    Dispatcher* dispatcher,
    size_t* max_size,
    size_t* max_platform_handles) {
  DCHECK(dispatcher);
  dispatcher->StartSerialize(max_size, max_platform_handles);
}

// static
bool Dispatcher::TransportDataAccess::EndSerializeAndClose(
    Dispatcher* dispatcher,
    void* destination,
    size_t* actual_size,
    PlatformHandleVector* platform_handles) {
  DCHECK(dispatcher);
  return dispatcher->EndSerializeAndClose(destination, actual_size,
                                          platform_handles);
}

// static
scoped_refptr<Dispatcher> Dispatcher::TransportDataAccess::Deserialize(
    int32_t type,
    const void* source,
    size_t size,
    PlatformHandleVector* platform_handles) {
  switch (static_cast<Dispatcher::Type>(type)) {
    case Type::UNKNOWN:
    case Type::WAIT_SET:
      DVLOG(2) << "Deserializing invalid handle";
      return nullptr;
    case Type::MESSAGE_PIPE:
      return scoped_refptr<Dispatcher>(MessagePipeDispatcher::Deserialize(
          source, size, platform_handles));
    case Type::DATA_PIPE_PRODUCER:
      return scoped_refptr<Dispatcher>(
          DataPipeProducerDispatcher::Deserialize(
              source, size, platform_handles));
    case Type::DATA_PIPE_CONSUMER:
      return scoped_refptr<Dispatcher>(
          DataPipeConsumerDispatcher::Deserialize(
              source, size, platform_handles));
    case Type::SHARED_BUFFER:
      return scoped_refptr<Dispatcher>(SharedBufferDispatcher::Deserialize(
          source, size, platform_handles));
    case Type::PLATFORM_HANDLE:
      return scoped_refptr<Dispatcher>(PlatformHandleDispatcher::Deserialize(
          source, size, platform_handles));
  }
  LOG(WARNING) << "Unknown dispatcher type " << type;
  return nullptr;
}

MojoResult Dispatcher::Close() {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  CloseNoLock();
  return MOJO_RESULT_OK;
}

MojoResult Dispatcher::WriteMessage(
    const void* bytes,
    uint32_t num_bytes,
    std::vector<DispatcherTransport>* transports,
    MojoWriteMessageFlags flags) {
  DCHECK(!transports ||
         (transports->size() > 0 &&
          transports->size() < GetConfiguration().max_message_num_handles));

  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return WriteMessageImplNoLock(bytes, num_bytes, transports, flags);
}

MojoResult Dispatcher::ReadMessage(void* bytes,
                                   uint32_t* num_bytes,
                                   DispatcherVector* dispatchers,
                                   uint32_t* num_dispatchers,
                                   MojoReadMessageFlags flags) {
  DCHECK(!num_dispatchers || *num_dispatchers == 0 ||
         (dispatchers && dispatchers->empty()));

  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return ReadMessageImplNoLock(bytes, num_bytes, dispatchers, num_dispatchers,
                               flags);
}

MojoResult Dispatcher::WriteData(const void* elements,
                                 uint32_t* num_bytes,
                                 MojoWriteDataFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return WriteDataImplNoLock(elements, num_bytes, flags);
}

MojoResult Dispatcher::BeginWriteData(void** buffer,
                                      uint32_t* buffer_num_bytes,
                                      MojoWriteDataFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return BeginWriteDataImplNoLock(buffer, buffer_num_bytes, flags);
}

MojoResult Dispatcher::EndWriteData(uint32_t num_bytes_written) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return EndWriteDataImplNoLock(num_bytes_written);
}

MojoResult Dispatcher::ReadData(void* elements,
                                uint32_t* num_bytes,
                                MojoReadDataFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return ReadDataImplNoLock(elements, num_bytes, flags);
}

MojoResult Dispatcher::BeginReadData(const void** buffer,
                                     uint32_t* buffer_num_bytes,
                                     MojoReadDataFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return BeginReadDataImplNoLock(buffer, buffer_num_bytes, flags);
}

MojoResult Dispatcher::EndReadData(uint32_t num_bytes_read) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return EndReadDataImplNoLock(num_bytes_read);
}

MojoResult Dispatcher::DuplicateBufferHandle(
    const MojoDuplicateBufferHandleOptions* options,
    scoped_refptr<Dispatcher>* new_dispatcher) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return DuplicateBufferHandleImplNoLock(options, new_dispatcher);
}

MojoResult Dispatcher::MapBuffer(
    uint64_t offset,
    uint64_t num_bytes,
    MojoMapBufferFlags flags,
    scoped_ptr<PlatformSharedBufferMapping>* mapping) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return MapBufferImplNoLock(offset, num_bytes, flags, mapping);
}

HandleSignalsState Dispatcher::GetHandleSignalsState() const {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return HandleSignalsState();

  return GetHandleSignalsStateImplNoLock();
}

MojoResult Dispatcher::AddAwakable(Awakable* awakable,
                                   MojoHandleSignals signals,
                                   uintptr_t context,
                                   HandleSignalsState* signals_state) {
  base::AutoLock locker(lock_);
  if (is_closed_) {
    if (signals_state)
      *signals_state = HandleSignalsState();
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  return AddAwakableImplNoLock(awakable, signals, context, signals_state);
}

void Dispatcher::RemoveAwakable(Awakable* awakable,
                                HandleSignalsState* handle_signals_state) {
  base::AutoLock locker(lock_);
  if (is_closed_) {
    if (handle_signals_state)
      *handle_signals_state = HandleSignalsState();
    return;
  }

  RemoveAwakableImplNoLock(awakable, handle_signals_state);
}

MojoResult Dispatcher::AddWaitingDispatcher(
    const scoped_refptr<Dispatcher>& dispatcher,
    MojoHandleSignals signals,
    uintptr_t context) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return AddWaitingDispatcherImplNoLock(dispatcher, signals, context);
}

MojoResult Dispatcher::RemoveWaitingDispatcher(
    const scoped_refptr<Dispatcher>& dispatcher) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return RemoveWaitingDispatcherImplNoLock(dispatcher);
}

MojoResult Dispatcher::GetReadyDispatchers(uint32_t* count,
                                           DispatcherVector* dispatchers,
                                           MojoResult* results,
                                           uintptr_t* contexts) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return GetReadyDispatchersImplNoLock(count, dispatchers, results, contexts);
}

Dispatcher::Dispatcher() : is_closed_(false) {
}

Dispatcher::~Dispatcher() {
  // Make sure that |Close()| was called.
  DCHECK(is_closed_);
}

void Dispatcher::CancelAllAwakablesNoLock() {
  lock_.AssertAcquired();
  DCHECK(is_closed_);
  // By default, waiting isn't supported. Only dispatchers that can be waited on
  // will do something nontrivial.
}

void Dispatcher::CloseImplNoLock() {
  lock_.AssertAcquired();
  DCHECK(is_closed_);
  // This may not need to do anything. Dispatchers should override this to do
  // any actual close-time cleanup necessary.
}

MojoResult Dispatcher::WriteMessageImplNoLock(
    const void* /*bytes*/,
    uint32_t /*num_bytes*/,
    std::vector<DispatcherTransport>* /*transports*/,
    MojoWriteMessageFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for message pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::ReadMessageImplNoLock(
    void* /*bytes*/,
    uint32_t* /*num_bytes*/,
    DispatcherVector* /*dispatchers*/,
    uint32_t* /*num_dispatchers*/,
    MojoReadMessageFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for message pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::WriteDataImplNoLock(const void* /*elements*/,
                                           uint32_t* /*num_bytes*/,
                                           MojoWriteDataFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::BeginWriteDataImplNoLock(
    void** /*buffer*/,
    uint32_t* /*buffer_num_bytes*/,
    MojoWriteDataFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::EndWriteDataImplNoLock(uint32_t /*num_bytes_written*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::ReadDataImplNoLock(void* /*elements*/,
                                          uint32_t* /*num_bytes*/,
                                          MojoReadDataFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::BeginReadDataImplNoLock(
    const void** /*buffer*/,
    uint32_t* /*buffer_num_bytes*/,
    MojoReadDataFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::EndReadDataImplNoLock(uint32_t /*num_bytes_read*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::DuplicateBufferHandleImplNoLock(
    const MojoDuplicateBufferHandleOptions* /*options*/,
    scoped_refptr<Dispatcher>* /*new_dispatcher*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for buffer dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::MapBufferImplNoLock(
    uint64_t /*offset*/,
    uint64_t /*num_bytes*/,
    MojoMapBufferFlags /*flags*/,
    scoped_ptr<PlatformSharedBufferMapping>* /*mapping*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for buffer dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

HandleSignalsState Dispatcher::GetHandleSignalsStateImplNoLock() const {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, waiting isn't supported. Only dispatchers that can be waited on
  // will do something nontrivial.
  return HandleSignalsState();
}

MojoResult Dispatcher::AddAwakableImplNoLock(
    Awakable* /*awakable*/,
    MojoHandleSignals /*signals*/,
    uintptr_t /*context*/,
    HandleSignalsState* signals_state) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, waiting isn't supported. Only dispatchers that can be waited on
  // will do something nontrivial.
  if (signals_state)
    *signals_state = HandleSignalsState();
  return MOJO_RESULT_FAILED_PRECONDITION;
}

MojoResult Dispatcher::AddWaitingDispatcherImplNoLock(
    const scoped_refptr<Dispatcher>& /*dispatcher*/,
    MojoHandleSignals /*signals*/,
    uintptr_t /*context*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for wait set dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::RemoveWaitingDispatcherImplNoLock(
    const scoped_refptr<Dispatcher>& /*dispatcher*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for wait set dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::GetReadyDispatchersImplNoLock(
    uint32_t* /*count*/,
    DispatcherVector* /*dispatchers*/,
    MojoResult* /*results*/,
    uintptr_t* /*contexts*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for wait set dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

void Dispatcher::RemoveAwakableImplNoLock(Awakable* /*awakable*/,
                                          HandleSignalsState* signals_state) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, waiting isn't supported. Only dispatchers that can be waited on
  // will do something nontrivial.
  if (signals_state)
    *signals_state = HandleSignalsState();
}

void Dispatcher::StartSerializeImplNoLock(size_t* max_size,
                                          size_t* max_platform_handles) {
  DCHECK(!is_closed_);
  *max_size = 0;
  *max_platform_handles = 0;
}

bool Dispatcher::EndSerializeAndCloseImplNoLock(
    void* /*destination*/,
    size_t* /*actual_size*/,
    PlatformHandleVector* /*platform_handles*/) {
  DCHECK(is_closed_);
  // By default, serializing isn't supported, so just close.
  CloseImplNoLock();
  return false;
}

bool Dispatcher::IsBusyNoLock() const {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // Most dispatchers support only "atomic" operations, so they are never busy
  // (in this sense).
  return false;
}

void Dispatcher::CloseNoLock() {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);

  is_closed_ = true;
  CancelAllAwakablesNoLock();
  CloseImplNoLock();
}

scoped_refptr<Dispatcher>
Dispatcher::CreateEquivalentDispatcherAndCloseNoLock() {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);

  is_closed_ = true;
  CancelAllAwakablesNoLock();
  return CreateEquivalentDispatcherAndCloseImplNoLock();
}

void Dispatcher::StartSerialize(size_t* max_size,
                                size_t* max_platform_handles) {
  DCHECK(max_size);
  DCHECK(max_platform_handles);
  DCHECK(!is_closed_);
  base::AutoLock locker(lock_);
  StartSerializeImplNoLock(max_size, max_platform_handles);
}

bool Dispatcher::EndSerializeAndClose(void* destination,
                                      size_t* actual_size,
                                      PlatformHandleVector* platform_handles) {
  DCHECK(actual_size);
  DCHECK(!is_closed_);

  // Like other |...Close()| methods, we mark ourselves as closed before calling
  // the impl. But there's no need to cancel waiters: we shouldn't have any (and
  // shouldn't be in |Core|'s handle table.
  is_closed_ = true;

  base::AutoLock locker(lock_);
  return EndSerializeAndCloseImplNoLock(destination, actual_size,
                                        platform_handles);
}

// DispatcherTransport ---------------------------------------------------------

void DispatcherTransport::End() {
  DCHECK(dispatcher_);
  dispatcher_->lock_.Release();

  dispatcher_->TransportEnded();

  dispatcher_ = nullptr;
}

}  // namespace edk
}  // namespace mojo
