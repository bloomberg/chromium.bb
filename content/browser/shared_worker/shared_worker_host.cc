// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_host.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/shared_worker/shared_worker_content_settings_proxy_impl.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "third_party/WebKit/common/message_port/message_port_channel.h"
#include "third_party/WebKit/public/platform/web_feature.mojom.h"
#include "third_party/WebKit/public/web/worker_content_settings_proxy.mojom.h"

namespace content {
namespace {

void NotifyWorkerReadyForInspection(int process_id, int route_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(NotifyWorkerReadyForInspection, process_id, route_id));
    return;
  }
  SharedWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
      process_id, route_id);
}

void NotifyWorkerDestroyed(int process_id, int route_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(NotifyWorkerDestroyed, process_id, route_id));
    return;
  }
  SharedWorkerDevToolsManager::GetInstance()->WorkerDestroyed(process_id,
                                                              route_id);
}

}  // namespace

SharedWorkerHost::SharedWorkerHost(SharedWorkerInstance* instance,
                                   int process_id,
                                   int route_id)
    : binding_(this),
      instance_(instance),
      process_id_(process_id),
      route_id_(route_id),
      next_connection_request_id_(1),
      creation_time_(base::TimeTicks::Now()),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(instance_);
}

SharedWorkerHost::~SharedWorkerHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UMA_HISTOGRAM_LONG_TIMES("SharedWorker.TimeToDeleted",
                           base::TimeTicks::Now() - creation_time_);
  if (!closed_ && !termination_message_sent_)
    NotifyWorkerDestroyed(process_id_, route_id_);
  SharedWorkerServiceImpl::GetInstance()->NotifyWorkerDestroyed(process_id_,
                                                                route_id_);
}

void SharedWorkerHost::Start(mojom::SharedWorkerFactoryPtr factory,
                             bool pause_on_start) {
  blink::mojom::WorkerContentSettingsProxyPtr content_settings;
  content_settings_ = std::make_unique<SharedWorkerContentSettingsProxyImpl>(
      instance_->url(), this, mojo::MakeRequest(&content_settings));

  mojom::SharedWorkerHostPtr host;
  binding_.Bind(mojo::MakeRequest(&host));

  mojom::SharedWorkerInfoPtr info(mojom::SharedWorkerInfo::New(
      instance_->url(), instance_->name(), instance_->content_security_policy(),
      instance_->content_security_policy_type(),
      instance_->creation_address_space(), instance_->data_saver_enabled()));

  factory->CreateSharedWorker(std::move(info), pause_on_start, route_id_,
                              std::move(content_settings), std::move(host),
                              mojo::MakeRequest(&worker_));

  // Monitor the lifetime of the worker.
  worker_.set_connection_error_handler(base::BindOnce(
      &SharedWorkerHost::OnWorkerConnectionLost, weak_factory_.GetWeakPtr()));
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
    NotifyWorkerDestroyed(process_id_, route_id_);
  worker_->Terminate();
  // Now, we wait to observe OnWorkerConnectionLost.
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

void SharedWorkerHost::OnConnected(int connection_request_id) {
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

void SharedWorkerHost::OnContextClosed() {
  // Set the closed flag - this will stop any further messages from
  // being sent to the worker (messages can still be sent from the worker,
  // for exception reporting, etc).
  closed_ = true;
  if (!termination_message_sent_)
    NotifyWorkerDestroyed(process_id_, route_id_);
}

void SharedWorkerHost::OnReadyForInspection() {
  NotifyWorkerReadyForInspection(process_id_, route_id_);
}

void SharedWorkerHost::OnScriptLoaded() {
  UMA_HISTOGRAM_TIMES("SharedWorker.TimeToScriptLoaded",
                      base::TimeTicks::Now() - creation_time_);
}

void SharedWorkerHost::OnScriptLoadFailed() {
  UMA_HISTOGRAM_TIMES("SharedWorker.TimeToScriptLoadFailed",
                      base::TimeTicks::Now() - creation_time_);
  for (const ClientInfo& info : clients_)
    info.client->OnScriptLoadFailed();
}

void SharedWorkerHost::OnFeatureUsed(blink::mojom::WebFeature feature) {
  // Avoid reporting a feature more than once, and enable any new clients to
  // observe features that were historically used.
  if (!used_features_.insert(feature).second)
    return;
  for (const ClientInfo& info : clients_)
    info.client->OnFeatureUsed(feature);
}

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
                                 const blink::MessagePortChannel& port) {
  clients_.emplace_back(std::move(client), next_connection_request_id_++,
                        process_id, frame_id);
  ClientInfo& info = clients_.back();

  // Observe when the client goes away.
  info.client.set_connection_error_handler(base::BindOnce(
      &SharedWorkerHost::OnClientConnectionLost, weak_factory_.GetWeakPtr()));

  worker_->Connect(info.connection_request_id, port.ReleaseHandle());
}

bool SharedWorkerHost::ServesExternalClient() {
  for (const ClientInfo& info : clients_) {
    if (info.process_id != process_id_)
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

void SharedWorkerHost::OnWorkerConnectionLost() {
  // This will destroy |this| resulting in client's observing their mojo
  // connection being dropped.
  SharedWorkerServiceImpl::GetInstance()->DestroyHost(process_id_, route_id_);
}

}  // namespace content
