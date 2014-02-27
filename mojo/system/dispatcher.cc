// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/dispatcher.h"

#include "base/logging.h"
#include "mojo/system/constants.h"

namespace mojo {
namespace system {

// Dispatcher ------------------------------------------------------------------

// static
DispatcherTransport Dispatcher::CoreImplAccess::TryStartTransport(
    Dispatcher* dispatcher) {
  DCHECK(dispatcher);

  if (!dispatcher->lock_.Try())
    return DispatcherTransport();

  // We shouldn't race with things that close dispatchers, since closing can
  // only take place either under |handle_table_lock_| or when the handle is
  // marked as busy.
  DCHECK(!dispatcher->is_closed_);

  return DispatcherTransport(dispatcher);
}

// static
size_t Dispatcher::MessageInTransitAccess::GetMaximumSerializedSize(
    const Dispatcher* dispatcher,
    const Channel* channel) {
  DCHECK(dispatcher);
  return dispatcher->GetMaximumSerializedSize(channel);
}

// static
size_t Dispatcher::MessageInTransitAccess::SerializeAndClose(
    Dispatcher* dispatcher,
    void* destination,
    Channel* channel) {
  DCHECK(dispatcher);
  return dispatcher->SerializeAndClose(destination, channel);
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
  DCHECK(!transports || (transports->size() > 0 &&
                         transports->size() < kMaxMessageNumHandles));

  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return WriteMessageImplNoLock(bytes, num_bytes, transports, flags);
}

MojoResult Dispatcher::ReadMessage(
    void* bytes,
    uint32_t* num_bytes,
    std::vector<scoped_refptr<Dispatcher> >* dispatchers,
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

MojoResult Dispatcher::MapBuffer(uint64_t offset,
                                 uint64_t num_bytes,
                                 void** buffer,
                                 MojoMapBufferFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return MapBufferImplNoLock(offset, num_bytes, buffer, flags);
}

MojoResult Dispatcher::AddWaiter(Waiter* waiter,
                                 MojoWaitFlags flags,
                                 MojoResult wake_result) {
  DCHECK_GE(wake_result, 0);

  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return AddWaiterImplNoLock(waiter, flags, wake_result);
}

void Dispatcher::RemoveWaiter(Waiter* waiter) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return;
  RemoveWaiterImplNoLock(waiter);
}

Dispatcher::Dispatcher()
    : is_closed_(false) {
}

Dispatcher::~Dispatcher() {
  // Make sure that |Close()| was called.
  DCHECK(is_closed_);
}

void Dispatcher::CancelAllWaitersNoLock() {
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
    std::vector<scoped_refptr<Dispatcher> >* /*dispatchers*/,
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

MojoResult Dispatcher::BeginWriteDataImplNoLock(void** /*buffer*/,
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

MojoResult Dispatcher::BeginReadDataImplNoLock(const void** /*buffer*/,
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

MojoResult Dispatcher::MapBufferImplNoLock(uint64_t /*offset*/,
                                           uint64_t /*num_bytes*/,
                                           void** /*buffer*/,
                                           MojoMapBufferFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for buffer dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::AddWaiterImplNoLock(Waiter* /*waiter*/,
                                           MojoWaitFlags /*flags*/,
                                           MojoResult /*wake_result*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, waiting isn't supported. Only dispatchers that can be waited on
  // will do something nontrivial.
  return MOJO_RESULT_FAILED_PRECONDITION;
}

void Dispatcher::RemoveWaiterImplNoLock(Waiter* /*waiter*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, waiting isn't supported. Only dispatchers that can be waited on
  // will do something nontrivial.
}

size_t Dispatcher::GetMaximumSerializedSizeImplNoLock(
      const Channel* /*channel*/) const {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, serializing isn't supported.
  return 0;
}

size_t Dispatcher::SerializeAndCloseImplNoLock(void* /*destination*/,
                                               Channel* /*channel*/) {
  lock_.AssertAcquired();
  DCHECK(is_closed_);
  // By default, serializing isn't supported, so just close.
  CloseImplNoLock();
  return 0;
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
  CancelAllWaitersNoLock();
  CloseImplNoLock();
}

scoped_refptr<Dispatcher>
Dispatcher::CreateEquivalentDispatcherAndCloseNoLock() {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);

  is_closed_ = true;
  CancelAllWaitersNoLock();
  return CreateEquivalentDispatcherAndCloseImplNoLock();
}

size_t Dispatcher::GetMaximumSerializedSize(const Channel* channel) const {
  DCHECK(channel);
  DCHECK(HasOneRef());

  base::AutoLock locker(lock_);
  DCHECK(!is_closed_);
  return GetMaximumSerializedSizeImplNoLock(channel);
}

size_t Dispatcher::SerializeAndClose(void* destination, Channel* channel) {
  DCHECK(destination);
  DCHECK(channel);
  DCHECK(HasOneRef());

  base::AutoLock locker(lock_);
  DCHECK(!is_closed_);

  // We have to call |GetMaximumSerializedSizeImplNoLock()| first, because we
  // leave it to |SerializeAndCloseImplNoLock()| to close the thing.
  size_t max_size = DCHECK_IS_ON() ?
      GetMaximumSerializedSizeImplNoLock(channel) : static_cast<size_t>(-1);

  // Like other |...Close()| methods, we mark ourselves as closed before calling
  // the impl.
  is_closed_ = true;
  // No need to cancel waiters: we shouldn't have any (and shouldn't be in
  // |Core|'s handle table.

  size_t size = SerializeAndCloseImplNoLock(destination, channel);
  DCHECK_LE(size, max_size);
  return size;
}

// DispatcherTransport ---------------------------------------------------------

void DispatcherTransport::End() {
  DCHECK(dispatcher_);
  dispatcher_->lock_.Release();
  dispatcher_ = NULL;
}

}  // namespace system
}  // namespace mojo
