// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/worker_thread_dispatcher.h"

#include "base/memory/ptr_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local.h"
#include "base/values.h"
#include "content/public/child/worker_thread.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/feature_switch.h"
#include "extensions/renderer/extension_bindings_system.h"
#include "extensions/renderer/js_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/service_worker_data.h"

namespace extensions {

namespace {

base::LazyInstance<WorkerThreadDispatcher>::DestructorAtExit g_instance =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::ThreadLocalPointer<extensions::ServiceWorkerData>>::
    DestructorAtExit g_data_tls = LAZY_INSTANCE_INITIALIZER;

ServiceWorkerData* GetServiceWorkerData() {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  DCHECK(data);
  return data;
}

// Handler for sending IPCs with native extension bindings.
void SendRequestIPC(ScriptContext* context,
                    const ExtensionHostMsg_Request_Params& params,
                    binding::RequestThread thread) {
  // TODO(devlin): This won't handle incrementing/decrementing service worker
  // lifetime.
  WorkerThreadDispatcher::Get()->Send(
      new ExtensionHostMsg_RequestWorker(params));
}

void SendEventListenersIPC(binding::EventListenersChanged changed,
                           ScriptContext* context,
                           const std::string& event_name,
                           const base::DictionaryValue* filter,
                           bool was_manual) {
  // TODO(devlin/lazyboy): Wire this up once extension workers support events.
}

}  // namespace

WorkerThreadDispatcher::WorkerThreadDispatcher() {}
WorkerThreadDispatcher::~WorkerThreadDispatcher() {}

WorkerThreadDispatcher* WorkerThreadDispatcher::Get() {
  return g_instance.Pointer();
}

void WorkerThreadDispatcher::Init(content::RenderThread* render_thread) {
  DCHECK(render_thread);
  DCHECK_EQ(content::RenderThread::Get(), render_thread);
  DCHECK(!message_filter_);
  message_filter_ = render_thread->GetSyncMessageFilter();
  render_thread->AddObserver(this);
}

// static
ExtensionBindingsSystem* WorkerThreadDispatcher::GetBindingsSystem() {
  return GetServiceWorkerData()->bindings_system();
}

// static
ServiceWorkerRequestSender* WorkerThreadDispatcher::GetRequestSender() {
  return static_cast<ServiceWorkerRequestSender*>(
      GetBindingsSystem()->GetRequestSender());
}

// static
V8SchemaRegistry* WorkerThreadDispatcher::GetV8SchemaRegistry() {
  return GetServiceWorkerData()->v8_schema_registry();
}

// static
bool WorkerThreadDispatcher::HandlesMessageOnWorkerThread(
    const IPC::Message& message) {
  return message.type() == ExtensionMsg_ResponseWorker::ID;
}

// static
void WorkerThreadDispatcher::ForwardIPC(int worker_thread_id,
                                        const IPC::Message& message) {
  WorkerThreadDispatcher::Get()->OnMessageReceivedOnWorkerThread(
      worker_thread_id, message);
}

bool WorkerThreadDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  if (HandlesMessageOnWorkerThread(message)) {
    int worker_thread_id = base::kInvalidThreadId;
    bool found = base::PickleIterator(message).ReadInt(&worker_thread_id);
    CHECK(found && worker_thread_id > 0);
    base::TaskRunner* runner = GetTaskRunnerFor(worker_thread_id);
    bool task_posted = runner->PostTask(
        FROM_HERE, base::Bind(&WorkerThreadDispatcher::ForwardIPC,
                              worker_thread_id, message));
    DCHECK(task_posted) << "Could not PostTask IPC to worker thread.";
    return true;
  }
  return false;
}

void WorkerThreadDispatcher::OnMessageReceivedOnWorkerThread(
    int worker_thread_id,
    const IPC::Message& message) {
  CHECK_EQ(content::WorkerThread::GetCurrentId(), worker_thread_id);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerThreadDispatcher, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ResponseWorker, OnResponseWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  CHECK(handled);
}

base::TaskRunner* WorkerThreadDispatcher::GetTaskRunnerFor(
    int worker_thread_id) {
  base::AutoLock lock(task_runner_map_lock_);
  return task_runner_map_[worker_thread_id];
}

bool WorkerThreadDispatcher::Send(IPC::Message* message) {
  return message_filter_->Send(message);
}

void WorkerThreadDispatcher::OnResponseWorker(int worker_thread_id,
                                              int request_id,
                                              bool succeeded,
                                              const base::ListValue& response,
                                              const std::string& error) {
  // TODO(devlin): Using the RequestSender directly here won't work with
  // NativeExtensionBindingsSystem (since there is no associated RequestSender
  // in that case). We should instead be going
  // ExtensionBindingsSystem::HandleResponse().
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  WorkerThreadDispatcher::GetRequestSender()->HandleWorkerResponse(
      request_id, data->service_worker_version_id(), succeeded, response,
      error);
}

void WorkerThreadDispatcher::AddWorkerData(
    int64_t service_worker_version_id,
    ResourceBundleSourceMap* source_map) {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  if (!data) {
    std::unique_ptr<ExtensionBindingsSystem> bindings_system;
    if (FeatureSwitch::native_crx_bindings()->IsEnabled()) {
      bindings_system = base::MakeUnique<NativeExtensionBindingsSystem>(
          base::Bind(&SendRequestIPC), base::Bind(&SendEventListenersIPC));
    } else {
      bindings_system = base::MakeUnique<JsExtensionBindingsSystem>(
          source_map, base::MakeUnique<ServiceWorkerRequestSender>(
                          this, service_worker_version_id));
    }
    ServiceWorkerData* new_data = new ServiceWorkerData(
        service_worker_version_id, std::move(bindings_system));
    g_data_tls.Pointer()->Set(new_data);
  }

  int worker_thread_id = base::PlatformThread::CurrentId();
  DCHECK_EQ(content::WorkerThread::GetCurrentId(), worker_thread_id);
  {
    base::AutoLock lock(task_runner_map_lock_);
    auto* task_runner = base::ThreadTaskRunnerHandle::Get().get();
    CHECK(task_runner);
    task_runner_map_[worker_thread_id] = task_runner;
  }
}

void WorkerThreadDispatcher::RemoveWorkerData(
    int64_t service_worker_version_id) {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  if (data) {
    DCHECK_EQ(service_worker_version_id, data->service_worker_version_id());
    delete data;
    g_data_tls.Pointer()->Set(nullptr);
  }

  int worker_thread_id = base::PlatformThread::CurrentId();
  DCHECK_EQ(content::WorkerThread::GetCurrentId(), worker_thread_id);
  {
    base::AutoLock lock(task_runner_map_lock_);
    task_runner_map_.erase(worker_thread_id);
  }
}

}  // namespace extensions
