// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/attachments/attachment_service_proxy_for_test.h"

#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/model/attachments/attachment_service.h"

namespace syncer {

AttachmentServiceProxyForTest::OwningCore::OwningCore(
    std::unique_ptr<AttachmentService> wrapped,
    std::unique_ptr<base::WeakPtrFactory<AttachmentService>> weak_ptr_factory)
    : Core(weak_ptr_factory->GetWeakPtr()),
      wrapped_(std::move(wrapped)),
      weak_ptr_factory_(std::move(weak_ptr_factory)) {
  DCHECK(wrapped_);
}

AttachmentServiceProxyForTest::OwningCore::~OwningCore() {}

// Static.
AttachmentServiceProxy AttachmentServiceProxyForTest::Create() {
  std::unique_ptr<AttachmentService> wrapped(
      AttachmentService::CreateForTest());
  // This class's base class, AttachmentServiceProxy, must be initialized with a
  // WeakPtr to an AttachmentService.  Because the base class ctor must be
  // invoked before any of this class's members are initialized, we create the
  // WeakPtrFactory here and pass it to the ctor so that it may initialize its
  // base class and own the WeakPtrFactory.
  //
  // We must pass by unique_ptr because WeakPtrFactory has no copy constructor.
  std::unique_ptr<base::WeakPtrFactory<AttachmentService>> weak_ptr_factory(
      new base::WeakPtrFactory<AttachmentService>(wrapped.get()));

  scoped_refptr<Core> core_for_test(
      new OwningCore(std::move(wrapped), std::move(weak_ptr_factory)));

  scoped_refptr<base::SequencedTaskRunner> runner;
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    runner = base::ThreadTaskRunnerHandle::Get();
  } else {
    // Dummy runner for tests that don't have MessageLoop.
    DVLOG(1) << "Creating dummy MessageLoop for AttachmentServiceProxy.";
    base::MessageLoop loop;
    // This works because |runner| takes a ref to the proxy.
    runner = base::ThreadTaskRunnerHandle::Get();
  }
  return AttachmentServiceProxyForTest(runner, core_for_test);
}

AttachmentServiceProxyForTest::~AttachmentServiceProxyForTest() {}

AttachmentServiceProxyForTest::AttachmentServiceProxyForTest(
    const scoped_refptr<base::SequencedTaskRunner>& wrapped_task_runner,
    const scoped_refptr<Core>& core)
    : AttachmentServiceProxy(wrapped_task_runner, core) {}

}  // namespace syncer
