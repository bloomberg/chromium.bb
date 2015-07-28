// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/child_memory_dump_manager_delegate_impl.h"

#include "base/single_thread_task_runner.h"
#include "components/tracing/child_trace_message_filter.h"

namespace tracing {

// static
ChildMemoryDumpManagerDelegateImpl*
ChildMemoryDumpManagerDelegateImpl::GetInstance() {
  return Singleton<
      ChildMemoryDumpManagerDelegateImpl,
      LeakySingletonTraits<ChildMemoryDumpManagerDelegateImpl>>::get();
}

ChildMemoryDumpManagerDelegateImpl::ChildMemoryDumpManagerDelegateImpl()
    : ctmf_(nullptr),
      tracing_process_id_(
          base::trace_event::MemoryDumpManager::kInvalidTracingProcessId) {
  base::trace_event::MemoryDumpManager::GetInstance()->SetDelegate(this);
}

ChildMemoryDumpManagerDelegateImpl::~ChildMemoryDumpManagerDelegateImpl() {
}

void ChildMemoryDumpManagerDelegateImpl::SetChildTraceMessageFilter(
    ChildTraceMessageFilter* ctmf) {
  // Check that we are either registering the CTMF or tearing it down, but not
  // replacing a valid instance with another one (should never happen).
  DCHECK(ctmf_ == nullptr || (ctmf == nullptr && ctmf_task_runner_ != nullptr));
  ctmf_ = ctmf;
  ctmf_task_runner_ = ctmf ? (ctmf->ipc_task_runner()) : nullptr;
}

// Invoked in child processes by the MemoryDumpManager.
void ChildMemoryDumpManagerDelegateImpl::RequestGlobalMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::trace_event::MemoryDumpCallback& callback) {
  // Bail out if we receive a dump request from the manager before the
  // ChildTraceMessageFilter has been initialized.
  if (!ctmf_task_runner_) {
    if (!callback.is_null())
      callback.Run(args.dump_guid, false /* success */);
    return;
  }

  // Make sure we access |ctmf_| only on the thread where it lives to avoid
  // races on shutdown.
  if (!ctmf_task_runner_->BelongsToCurrentThread()) {
    ctmf_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ChildMemoryDumpManagerDelegateImpl::RequestGlobalMemoryDump,
                   base::Unretained(this), args, callback));
    return;
  }

  // The ChildTraceMessageFilter could have been destroyed while hopping on the
  // right thread. If this is the case, bail out.
  if (!ctmf_) {
    if (!callback.is_null())
      callback.Run(args.dump_guid, false /* success */);
    return;
  }

  // Send the request up to the browser process' MessageDumpmanager.
  ctmf_->SendGlobalMemoryDumpRequest(args, callback);
}

bool ChildMemoryDumpManagerDelegateImpl::IsCoordinatorProcess() const {
  return false;
}

uint64 ChildMemoryDumpManagerDelegateImpl::GetTracingProcessId() const {
  return tracing_process_id_;
}

}  // namespace tracing
