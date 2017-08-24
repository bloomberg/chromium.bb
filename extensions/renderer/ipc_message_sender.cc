// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/ipc_message_sender.h"

#include <map>

#include "base/guid.h"
#include "content/public/child/worker_thread.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/worker_thread_dispatcher.h"

namespace extensions {

namespace {

const int kMainThreadId = 0;

class MainThreadIPCMessageSender : public IPCMessageSender {
 public:
  MainThreadIPCMessageSender() : render_thread_(content::RenderThread::Get()) {}
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

  void SendAddUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK_NE(Feature::SERVICE_WORKER_CONTEXT, context->context_type());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    render_thread_->Send(new ExtensionHostMsg_AddListener(
        context->GetExtensionID(), context->url(), event_name, kMainThreadId));
  }

  void SendRemoveUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK_NE(Feature::SERVICE_WORKER_CONTEXT, context->context_type());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    render_thread_->Send(new ExtensionHostMsg_RemoveListener(
        context->GetExtensionID(), context->url(), event_name, kMainThreadId));
  }

  void SendAddUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK_NE(Feature::SERVICE_WORKER_CONTEXT, context->context_type());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    render_thread_->Send(new ExtensionHostMsg_AddLazyListener(
        context->GetExtensionID(), event_name));
  }

  void SendRemoveUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK_NE(Feature::SERVICE_WORKER_CONTEXT, context->context_type());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    render_thread_->Send(new ExtensionHostMsg_RemoveLazyListener(
        context->GetExtensionID(), event_name));
  }

  void SendAddFilteredEventListenerIPC(ScriptContext* context,
                                       const std::string& event_name,
                                       const base::DictionaryValue& filter,
                                       bool is_lazy) override {
    DCHECK_NE(Feature::SERVICE_WORKER_CONTEXT, context->context_type());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    render_thread_->Send(new ExtensionHostMsg_AddFilteredListener(
        context->GetExtensionID(), event_name, filter, is_lazy));
  }

  void SendRemoveFilteredEventListenerIPC(ScriptContext* context,
                                          const std::string& event_name,
                                          const base::DictionaryValue& filter,
                                          bool remove_lazy_listener) override {
    DCHECK_NE(Feature::SERVICE_WORKER_CONTEXT, context->context_type());
    DCHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());

    render_thread_->Send(new ExtensionHostMsg_RemoveFilteredListener(
        context->GetExtensionID(), event_name, filter, remove_lazy_listener));
  }

 private:
  content::RenderThread* const render_thread_;

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

  void SendAddUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK_EQ(Feature::SERVICE_WORKER_CONTEXT, context->context_type());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());

    dispatcher_->Send(new ExtensionHostMsg_AddListener(
        context->GetExtensionID(), context->service_worker_scope(), event_name,
        content::WorkerThread::GetCurrentId()));
  }

  void SendRemoveUnfilteredEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK_EQ(Feature::SERVICE_WORKER_CONTEXT, context->context_type());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());

    dispatcher_->Send(new ExtensionHostMsg_RemoveListener(
        context->GetExtensionID(), context->service_worker_scope(), event_name,
        content::WorkerThread::GetCurrentId()));
  }

  void SendAddUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK_EQ(Feature::SERVICE_WORKER_CONTEXT, context->context_type());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());

    dispatcher_->Send(new ExtensionHostMsg_AddLazyServiceWorkerListener(
        context->GetExtensionID(), event_name,
        context->service_worker_scope()));
  }

  void SendRemoveUnfilteredLazyEventListenerIPC(
      ScriptContext* context,
      const std::string& event_name) override {
    DCHECK_EQ(Feature::SERVICE_WORKER_CONTEXT, context->context_type());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());

    dispatcher_->Send(new ExtensionHostMsg_RemoveLazyServiceWorkerListener(
        context->GetExtensionID(), event_name,
        context->service_worker_scope()));
  }

  void SendAddFilteredEventListenerIPC(ScriptContext* context,
                                       const std::string& event_name,
                                       const base::DictionaryValue& filter,
                                       bool is_lazy) override {
    DCHECK_EQ(Feature::SERVICE_WORKER_CONTEXT, context->context_type());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());
    NOTIMPLEMENTED();
  }

  void SendRemoveFilteredEventListenerIPC(ScriptContext* context,
                                          const std::string& event_name,
                                          const base::DictionaryValue& filter,
                                          bool remove_lazy_listener) override {
    DCHECK_EQ(Feature::SERVICE_WORKER_CONTEXT, context->context_type());
    DCHECK_NE(kMainThreadId, content::WorkerThread::GetCurrentId());
    NOTIMPLEMENTED();
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
  return std::make_unique<MainThreadIPCMessageSender>();
}

// static
std::unique_ptr<IPCMessageSender>
IPCMessageSender::CreateWorkerThreadIPCMessageSender(
    WorkerThreadDispatcher* dispatcher,
    int64_t service_worker_version_id) {
  return std::make_unique<WorkerThreadIPCMessageSender>(
      dispatcher, service_worker_version_id);
}

}  // namespace extensions
