// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/ipc_message_sender.h"

#include <map>

#include "base/guid.h"
#include "content/public/child/worker_thread.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/worker_thread_dispatcher.h"

namespace extensions {

namespace {

const int kMainThreadId = 0;

class MainThreadIPCMessageSender : public IPCMessageSender {
 public:
  MainThreadIPCMessageSender() {}
  ~MainThreadIPCMessageSender() override {}

  void SendRequestIPC(ScriptContext* context,
                      std::unique_ptr<ExtensionHostMsg_Request_Params> params,
                      binding::RequestThread thread) override {
    content::RenderFrame* frame = context->GetRenderFrame();
    if (!frame)
      return;

    switch (thread) {
      case binding::RequestThread::UI:
        frame->Send(
            new ExtensionHostMsg_Request(frame->GetRoutingID(), *params));
        break;
      case binding::RequestThread::IO:
        frame->Send(new ExtensionHostMsg_RequestForIOThread(
            frame->GetRoutingID(), *params));
        break;
    }
  }

  void SendOnRequestResponseReceivedIPC(int request_id) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MainThreadIPCMessageSender);
};

class WorkerThreadIPCMessageSender : public IPCMessageSender {
 public:
  WorkerThreadIPCMessageSender(WorkerThreadDispatcher* dispatcher,
                               int64_t service_worker_version_id)
      : dispatcher_(dispatcher),
        service_worker_version_id_(service_worker_version_id) {}
  ~WorkerThreadIPCMessageSender() override {}

  void SendRequestIPC(ScriptContext* context,
                      std::unique_ptr<ExtensionHostMsg_Request_Params> params,
                      binding::RequestThread thread) override {
    DCHECK(!context->GetRenderFrame());
    DCHECK_EQ(Feature::SERVICE_WORKER_CONTEXT, context->context_type());
    DCHECK_EQ(binding::RequestThread::UI, thread);
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());

    int worker_thread_id = content::WorkerThread::GetCurrentId();
    params->worker_thread_id = worker_thread_id;
    params->service_worker_version_id = service_worker_version_id_;

    std::string guid = base::GenerateGUID();
    request_id_to_guid_[params->request_id] = guid;

    // Keeps the worker alive during extension function call. Balanced in
    // HandleWorkerResponse().
    dispatcher_->Send(new ExtensionHostMsg_IncrementServiceWorkerActivity(
        service_worker_version_id_, guid));

    dispatcher_->Send(new ExtensionHostMsg_RequestWorker(*params));
  }

  void SendOnRequestResponseReceivedIPC(int request_id) override {
    std::map<int, std::string>::iterator iter =
        request_id_to_guid_.find(request_id);
    DCHECK(iter != request_id_to_guid_.end());
    dispatcher_->Send(new ExtensionHostMsg_DecrementServiceWorkerActivity(
        service_worker_version_id_, iter->second));
    request_id_to_guid_.erase(iter);
  }

 private:
  WorkerThreadDispatcher* const dispatcher_;
  const int64_t service_worker_version_id_;

  // request id -> GUID map for each outstanding requests.
  std::map<int, std::string> request_id_to_guid_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThreadIPCMessageSender);
};

}  // namespace

IPCMessageSender::IPCMessageSender() {}
IPCMessageSender::~IPCMessageSender() = default;

// static
std::unique_ptr<IPCMessageSender>
IPCMessageSender::CreateMainThreadIPCMessageSender() {
  return base::MakeUnique<MainThreadIPCMessageSender>();
}

// static
std::unique_ptr<IPCMessageSender>
IPCMessageSender::CreateWorkerThreadIPCMessageSender(
    WorkerThreadDispatcher* dispatcher,
    int64_t service_worker_version_id) {
  return base::MakeUnique<WorkerThreadIPCMessageSender>(
      dispatcher, service_worker_version_id);
}

}  // namespace extensions
