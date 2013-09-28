// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/core_test_base.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "mojo/system/core_impl.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/memory.h"

namespace mojo {
namespace system {
namespace test {

namespace {

// MockDispatcher --------------------------------------------------------------

class MockDispatcher : public Dispatcher {
 public:
  explicit MockDispatcher(CoreTestBase::MockHandleInfo* info)
      : info_(info) {
    CHECK(info_);
    info_->IncrementCtorCallCount();
  }

 private:
  friend class base::RefCountedThreadSafe<MockDispatcher>;
  virtual ~MockDispatcher() {
    info_->IncrementDtorCallCount();
  }

  // |Dispatcher| implementation/overrides:
  virtual MojoResult CloseImplNoLock() OVERRIDE {
    info_->IncrementCloseCallCount();
    lock().AssertAcquired();
    return MOJO_RESULT_OK;
  }

  virtual MojoResult WriteMessageImplNoLock(
      const void* bytes,
      uint32_t num_bytes,
      const MojoHandle* handles,
      uint32_t num_handles,
      MojoWriteMessageFlags /*flags*/) OVERRIDE {
    info_->IncrementWriteMessageCallCount();
    lock().AssertAcquired();

    if (!VerifyUserPointer(bytes, num_bytes, 1))
      return MOJO_RESULT_INVALID_ARGUMENT;
    if (!VerifyUserPointer(handles, num_handles, sizeof(handles[0])))
      return MOJO_RESULT_INVALID_ARGUMENT;

    return MOJO_RESULT_OK;
  }

  virtual MojoResult ReadMessageImplNoLock(
      void* bytes,
      uint32_t* num_bytes,
      MojoHandle* handles,
      uint32_t* num_handles,
      MojoReadMessageFlags /*flags*/) OVERRIDE {
    info_->IncrementReadMessageCallCount();
    lock().AssertAcquired();

    if (num_bytes && !VerifyUserPointer(bytes, *num_bytes, 1))
      return MOJO_RESULT_INVALID_ARGUMENT;
    if (num_handles &&
        !VerifyUserPointer(handles, *num_handles, sizeof(handles[0])))
      return MOJO_RESULT_INVALID_ARGUMENT;

    return MOJO_RESULT_OK;
  }

  virtual MojoResult AddWaiterImplNoLock(Waiter* /*waiter*/,
                                         MojoWaitFlags /*flags*/,
                                         MojoResult /*wake_result*/) OVERRIDE {
    info_->IncrementAddWaiterCallCount();
    lock().AssertAcquired();
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  virtual void RemoveWaiterImplNoLock(Waiter* /*waiter*/) OVERRIDE {
    info_->IncrementRemoveWaiterCallCount();
    lock().AssertAcquired();
  }

  virtual void CancelAllWaitersNoLock() OVERRIDE {
    info_->IncrementCancelAllWaitersCallCount();
    lock().AssertAcquired();
  }

  CoreTestBase::MockHandleInfo* const info_;

  DISALLOW_COPY_AND_ASSIGN(MockDispatcher);
};

}  // namespace

// CoreTestBase ----------------------------------------------------------------

CoreTestBase::CoreTestBase() {
}

CoreTestBase::~CoreTestBase() {
}

void CoreTestBase::SetUp() {
  core_ = new CoreImpl();
}

void CoreTestBase::TearDown() {
  delete core_;
  core_ = NULL;
}

MojoHandle CoreTestBase::CreateMockHandle(CoreTestBase::MockHandleInfo* info) {
  CHECK(core_);
  scoped_refptr<MockDispatcher> dispatcher(new MockDispatcher(info));
  base::AutoLock locker(core_->handle_table_lock_);
  return core_->AddDispatcherNoLock(dispatcher);
}

// CoreTestBase_MockHandleInfo -------------------------------------------------

CoreTestBase_MockHandleInfo::CoreTestBase_MockHandleInfo()
    : ctor_call_count_(0),
      dtor_call_count_(0),
      close_call_count_(0),
      write_message_call_count_(0),
      read_message_call_count_(0),
      add_waiter_call_count_(0),
      remove_waiter_call_count_(0),
      cancel_all_waiters_call_count_(0) {
}

CoreTestBase_MockHandleInfo::~CoreTestBase_MockHandleInfo() {
}

unsigned CoreTestBase_MockHandleInfo::GetCtorCallCount() const {
  base::AutoLock locker(lock_);
  return ctor_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetDtorCallCount() const {
  base::AutoLock locker(lock_);
  return dtor_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetCloseCallCount() const {
  base::AutoLock locker(lock_);
  return close_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetWriteMessageCallCount() const {
  base::AutoLock locker(lock_);
  return write_message_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetReadMessageCallCount() const {
  base::AutoLock locker(lock_);
  return read_message_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetAddWaiterCallCount() const {
  base::AutoLock locker(lock_);
  return add_waiter_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetRemoveWaiterCallCount() const {
  base::AutoLock locker(lock_);
  return remove_waiter_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetCancelAllWaitersCallCount() const {
  base::AutoLock locker(lock_);
  return cancel_all_waiters_call_count_;
}

void CoreTestBase_MockHandleInfo::IncrementCtorCallCount() {
  base::AutoLock locker(lock_);
  ctor_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementDtorCallCount() {
  base::AutoLock locker(lock_);
  dtor_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementCloseCallCount() {
  base::AutoLock locker(lock_);
  close_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementWriteMessageCallCount() {
  base::AutoLock locker(lock_);
  write_message_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementReadMessageCallCount() {
  base::AutoLock locker(lock_);
  read_message_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementAddWaiterCallCount() {
  base::AutoLock locker(lock_);
  add_waiter_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementRemoveWaiterCallCount() {
  base::AutoLock locker(lock_);
  remove_waiter_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementCancelAllWaitersCallCount() {
  base::AutoLock locker(lock_);
  cancel_all_waiters_call_count_++;
}

}  // namespace test
}  // namespace system
}  // namespace mojo
