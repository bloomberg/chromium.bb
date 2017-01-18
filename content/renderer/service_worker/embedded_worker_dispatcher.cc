// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_dispatcher.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_thread_registry.h"
#include "content/common/devtools_messages.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/public/common/content_client.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/embedded_worker_devtools_agent.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorker.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorkerStartData.h"

namespace content {

EmbeddedWorkerDispatcher::WorkerWrapper::WorkerWrapper(
    blink::WebEmbeddedWorker* worker,
    int devtools_agent_route_id)
    : worker_(worker),
      devtools_agent_(
          new EmbeddedWorkerDevToolsAgent(worker, devtools_agent_route_id)) {}

EmbeddedWorkerDispatcher::WorkerWrapper::~WorkerWrapper() {}

EmbeddedWorkerDispatcher::EmbeddedWorkerDispatcher() : weak_factory_(this) {}

EmbeddedWorkerDispatcher::~EmbeddedWorkerDispatcher() {}

bool EmbeddedWorkerDispatcher::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerDispatcher, message)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerMsg_StopWorker, OnStopWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void EmbeddedWorkerDispatcher::WorkerContextDestroyed(
    int embedded_worker_id) {
  RenderThreadImpl::current()->thread_safe_sender()->Send(
      new EmbeddedWorkerHostMsg_WorkerStopped(embedded_worker_id));
  UnregisterWorker(embedded_worker_id);
}

void EmbeddedWorkerDispatcher::OnStopWorker(int embedded_worker_id) {
  TRACE_EVENT0("ServiceWorker", "EmbeddedWorkerDispatcher::OnStopWorker");
  WorkerWrapper* wrapper = workers_.Lookup(embedded_worker_id);
  // OnStopWorker is possible to be called twice.
  if (!wrapper) {
    LOG(WARNING) << "Got OnStopWorker for nonexistent worker";
    return;
  }
  // This should eventually call WorkerContextDestroyed. (We may need to post
  // a delayed task to forcibly abort the worker context if we find it
  // necessary)
  stop_worker_times_[embedded_worker_id] = base::TimeTicks::Now();
  wrapper->worker()->terminateWorkerContext();
}

std::unique_ptr<EmbeddedWorkerDispatcher::WorkerWrapper>
EmbeddedWorkerDispatcher::StartWorkerContext(
    const EmbeddedWorkerStartParams& params,
    std::unique_ptr<ServiceWorkerContextClient> context_client) {
  std::unique_ptr<WorkerWrapper> wrapper(new WorkerWrapper(
      blink::WebEmbeddedWorker::create(context_client.release(), nullptr),
      params.worker_devtools_agent_route_id));

  blink::WebEmbeddedWorkerStartData start_data;
  start_data.scriptURL = params.script_url;
  start_data.userAgent =
      blink::WebString::fromUTF8(GetContentClient()->GetUserAgent());
  start_data.waitForDebuggerMode =
      params.wait_for_debugger
          ? blink::WebEmbeddedWorkerStartData::WaitForDebugger
          : blink::WebEmbeddedWorkerStartData::DontWaitForDebugger;
  start_data.v8CacheOptions = static_cast<blink::WebSettings::V8CacheOptions>(
      params.settings.v8_cache_options);
  start_data.dataSaverEnabled = params.settings.data_saver_enabled;
  start_data.pauseAfterDownloadMode =
      params.pause_after_download
          ? blink::WebEmbeddedWorkerStartData::PauseAfterDownload
          : blink::WebEmbeddedWorkerStartData::DontPauseAfterDownload;

  wrapper->worker()->startWorkerContext(start_data);
  return wrapper;
}

void EmbeddedWorkerDispatcher::RegisterWorker(
    int embedded_worker_id,
    std::unique_ptr<WorkerWrapper> wrapper) {
  workers_.AddWithID(std::move(wrapper), embedded_worker_id);
}

void EmbeddedWorkerDispatcher::UnregisterWorker(int embedded_worker_id) {
  if (ContainsKey(stop_worker_times_, embedded_worker_id)) {
    base::TimeTicks stop_time = stop_worker_times_[embedded_worker_id];
    UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.TerminateThread.Time",
                               base::TimeTicks::Now() - stop_time);
    stop_worker_times_.erase(embedded_worker_id);
  }
  workers_.Remove(embedded_worker_id);
}

void EmbeddedWorkerDispatcher::RecordStopWorkerTimer(int embedded_worker_id) {
  WorkerWrapper* wrapper = workers_.Lookup(embedded_worker_id);
  DCHECK(wrapper);
  // This should eventually call WorkerContextDestroyed. (We may need to post
  // a delayed task to forcibly abort the worker context if we find it
  // necessary)
  stop_worker_times_[embedded_worker_id] = base::TimeTicks::Now();
}

}  // namespace content
