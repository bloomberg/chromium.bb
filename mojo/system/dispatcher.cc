// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/dispatcher.h"

#include "base/logging.h"
#include "mojo/system/constants.h"

namespace mojo {
namespace system {

MojoResult Dispatcher::Close() {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  is_closed_ = true;
  CancelAllWaitersNoLock();
  return CloseImplNoLock();
}

MojoResult Dispatcher::WriteMessage(const void* bytes,
                                    uint32_t num_bytes,
                                    const std::vector<Dispatcher*>* dispatchers,
                                    MojoWriteMessageFlags flags) {
  DCHECK(!dispatchers || (dispatchers->size() > 0 &&
                          dispatchers->size() < kMaxMessageNumHandles));

  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return WriteMessageImplNoLock(bytes, num_bytes, dispatchers, flags);
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
                                 uint32_t* num_elements,
                                 MojoWriteDataFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return WriteDataImplNoLock(elements, num_elements, flags);
}

MojoResult Dispatcher::BeginWriteData(void** buffer,
                                      uint32_t* buffer_num_elements,
                                      MojoWriteDataFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return BeginWriteDataImplNoLock(buffer, buffer_num_elements, flags);
}

MojoResult Dispatcher::EndWriteData(uint32_t num_elements_written) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return EndWriteDataImplNoLock(num_elements_written);
}

MojoResult Dispatcher::ReadData(void* elements,
                                uint32_t* num_elements,
                                MojoReadDataFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return ReadDataImplNoLock(elements, num_elements, flags);
}

MojoResult Dispatcher::BeginReadData(const void** buffer,
                                     uint32_t* buffer_num_elements,
                                     MojoReadDataFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return BeginReadDataImplNoLock(buffer, buffer_num_elements, flags);
}

MojoResult Dispatcher::EndReadData(uint32_t num_elements_read) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return EndReadDataImplNoLock(num_elements_read);
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

scoped_refptr<Dispatcher>
Dispatcher::CreateEquivalentDispatcherAndCloseNoLock() {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);

  is_closed_ = true;
  CancelAllWaitersNoLock();
  return CreateEquivalentDispatcherAndCloseImplNoLock();
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

MojoResult Dispatcher::CloseImplNoLock() {
  lock_.AssertAcquired();
  DCHECK(is_closed_);
  // This may not need to do anything. Dispatchers should override this to do
  // any actual close-time cleanup necessary.
  return MOJO_RESULT_OK;
}

MojoResult Dispatcher::WriteMessageImplNoLock(
    const void* bytes,
    uint32_t num_bytes,
    const std::vector<Dispatcher*>* dispatchers,
    MojoWriteMessageFlags flags) {
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
                                           uint32_t* /*num_elements*/,
                                           MojoWriteDataFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::BeginWriteDataImplNoLock(
    void** /*buffer*/,
    uint32_t* /*buffer_num_elements*/,
    MojoWriteDataFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::EndWriteDataImplNoLock(
    uint32_t /*num_elements_written*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::ReadDataImplNoLock(void* /*elements*/,
                                          uint32_t* /*num_elements*/,
                                          MojoReadDataFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::BeginReadDataImplNoLock(
    const void** /*buffer*/,
    uint32_t* /*buffer_num_elements*/,
    MojoReadDataFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::EndReadDataImplNoLock(uint32_t /*num_elements_read*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
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

}  // namespace system
}  // namespace mojo
