// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_host.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/shared_worker/shared_worker_content_settings_proxy_impl.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "third_party/WebKit/public/platform/web_feature.mojom.h"
#include "third_party/WebKit/public/web/worker_content_settings_proxy.mojom.h"

namespace content {
namespace {

void NotifyWorkerReadyForInspection(int worker_process_id,
                                    int worker_route_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(NotifyWorkerReadyForInspection,
                                           worker_process_id, worker_route_id));
    return;
  }
  SharedWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
      worker_process_id, worker_route_id);
}

void NotifyWorkerDestroyed(int worker_process_id, int worker_route_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(NotifyWorkerDestroyed,
                                           worker_process_id, worker_route_id));
    return;
  }
  SharedWorkerDevToolsManager::GetInstance()->WorkerDestroyed(
      worker_process_id, worker_route_id);
}

}  // namespace

SharedWorkerHost::SharedWorkerHost(SharedWorkerInstance* instance,
                                   SharedWorkerMessageFilter* filter,
                                   int worker_route_id)
    : instance_(instance),
      worker_render_filter_(filter),
      worker_process_id_(filter->render_process_id()),
      worker_route_id_(worker_route_id),
      next_connection_request_id_(1),
      creation_time_(base::TimeTicks::Now()),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(instance_);
  DCHECK(worker_render_filter_);
}

SharedWorkerHost::~SharedWorkerHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UMA_HISTOGRAM_LONG_TIMES("SharedWorker.TimeToDeleted",
                           base::TimeTicks::Now() - creation_time_);
  if (!closed_ && !termination_message_sent_)
    NotifyWorkerDestroyed(worker_process_id_, worker_route_id_);
  SharedWorkerServiceImpl::GetInstance()->NotifyWorkerDestroyed(
      worker_process_id_, worker_route_id_);
}

void SharedWorkerHost::Start(bool pause_on_start) {
  blink::mojom::WorkerContentSettingsProxyPtrInfo content_settings;
  content_settings_ = base::MakeUnique<SharedWorkerContentSettingsProxyImpl>(
      instance_->url(), this, mojo::MakeRequest(&content_settings));

  WorkerProcessMsg_CreateWorker_Params params;
  params.url = instance_->url();
  params.name = instance_->name();
  params.content_security_policy = instance_->content_security_policy();
  params.security_policy_type = instance_->security_policy_type();
  params.creation_address_space = instance_->creation_address_space();
  params.pause_on_start = pause_on_start;
  params.route_id = worker_route_id_;
  params.data_saver_enabled = instance_->data_saver_enabled();
  params.content_settings_handle = content_settings.PassHandle().release();
  Send(new WorkerProcessMsg_CreateWorker(params));
}

void SharedWorkerHost::CountFeature(blink::mojom::WebFeature feature) {
  // Avoid reporting a feature more than once, and enable any new clients to
  // observe features that were historically used.
  if (!used_features_.insert(feature).second)
    return;
  for (const ClientInfo& info : clients_)
    info.client->OnFeatureUsed(feature);
}

void SharedWorkerHost::WorkerContextClosed() {
  // Set the closed flag - this will stop any further messages from
  // being sent to the worker (messages can still be sent from the worker,
  // for exception reporting, etc).
  closed_ = true;
  if (!termination_message_sent_)
    NotifyWorkerDestroyed(worker_process_id_, worker_route_id_);
}

void SharedWorkerHost::WorkerContextDestroyed() {
  // Disconnect all clients to signal destruction of the worker.
  clients_.clear();
}

void SharedWorkerHost::WorkerReadyForInspection() {
  NotifyWorkerReadyForInspection(worker_process_id_, worker_route_id_);
}

void SharedWorkerHost::WorkerScriptLoaded() {
  UMA_HISTOGRAM_TIMES("SharedWorker.TimeToScriptLoaded",
                      base::TimeTicks::Now() - creation_time_);
}

void SharedWorkerHost::WorkerScriptLoadFailed() {
  UMA_HISTOGRAM_TIMES("SharedWorker.TimeToScriptLoadFailed",
                      base::TimeTicks::Now() - creation_time_);
  for (const ClientInfo& info : clients_)
    info.client->OnScriptLoadFailed();
}

void SharedWorkerHost::WorkerConnected(int connection_request_id) {
  if (!instance_)
    return;
  for (const ClientInfo& info : clients_) {
    if (info.connection_request_id != connection_request_id)
      continue;
    info.client->OnConnected(std::vector<blink::mojom::WebFeature>(
        used_features_.begin(), used_features_.end()));
    return;
  }
}

void SharedWorkerHost::AllowFileSystem(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  GetContentClient()->browser()->AllowWorkerFileSystem(
      url, instance_->resource_context(), GetRenderFrameIDsForWorker(),
      base::Bind(&SharedWorkerHost::AllowFileSystemResponse,
                 weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void SharedWorkerHost::AllowFileSystemResponse(
    base::OnceCallback<void(bool)> callback,
    bool allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(allowed);
}

bool SharedWorkerHost::AllowIndexedDB(const GURL& url,
                                      const base::string16& name) {
  return GetContentClient()->browser()->AllowWorkerIndexedDB(
      url, name, instance_->resource_context(), GetRenderFrameIDsForWorker());
}

void SharedWorkerHost::TerminateWorker() {
  termination_message_sent_ = true;
  if (!closed_)
    NotifyWorkerDestroyed(worker_process_id_, worker_route_id_);
  Send(new WorkerMsg_TerminateWorkerContext(worker_route_id_));
}

SharedWorkerHost::ClientInfo::ClientInfo(mojom::SharedWorkerClientPtr client,
                                         int connection_request_id,
                                         int process_id,
                                         int frame_id)
    : client(std::move(client)),
      connection_request_id(connection_request_id),
      process_id(process_id),
      frame_id(frame_id) {}

SharedWorkerHost::ClientInfo::~ClientInfo() {}

std::vector<std::pair<int, int>>
SharedWorkerHost::GetRenderFrameIDsForWorker() {
  std::vector<std::pair<int, int>> result;
  for (const ClientInfo& info : clients_)
    result.push_back(std::make_pair(info.process_id, info.frame_id));
  return result;
}

bool SharedWorkerHost::IsAvailable() const {
  return !termination_message_sent_ && !closed_;
}

void SharedWorkerHost::AddClient(mojom::SharedWorkerClientPtr client,
                                 int process_id,
                                 int frame_id,
                                 const MessagePort& port) {
  clients_.emplace_back(std::move(client), next_connection_request_id_++,
                        process_id, frame_id);
  ClientInfo& info = clients_.back();

  // Observe when the client goes away.
  info.client.set_connection_error_handler(base::BindOnce(
      &SharedWorkerHost::OnClientConnectionLost, weak_factory_.GetWeakPtr()));

  Send(new WorkerMsg_Connect(worker_route_id_, info.connection_request_id,
                             port));
}

bool SharedWorkerHost::ServesExternalClient() {
  for (const ClientInfo& info : clients_) {
    if (info.process_id != worker_process_id_)
      return true;
  }
  return false;
}

void SharedWorkerHost::OnClientConnectionLost() {
  // We'll get a notification for each dropped connection.
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (it->client.encountered_error()) {
      clients_.erase(it);
      break;
    }
  }
  // If there are no clients left, then it's cleanup time.
  if (clients_.empty())
    TerminateWorker();
}

bool SharedWorkerHost::Send(IPC::Message* message) {
  return worker_render_filter_->Send(message);
}

}  // namespace content
