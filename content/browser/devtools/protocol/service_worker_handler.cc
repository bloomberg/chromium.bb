// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/service_worker_handler.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/containers/flat_set.h"
#include "content/browser/background_sync/background_sync_context.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_watcher.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/push_event_payload.h"
#include "content/public/common/push_messaging_status.mojom.h"
#include "url/gurl.h"

namespace content {
namespace protocol {

namespace {

void ResultNoOp(bool success) {
}

void StatusNoOp(ServiceWorkerStatusCode status) {
}

void StatusNoOpKeepingRegistration(
    scoped_refptr<content::ServiceWorkerRegistration> protect,
    ServiceWorkerStatusCode status) {
}

void PushDeliveryNoOp(mojom::PushDeliveryStatus status) {}

const std::string GetVersionRunningStatusString(
    EmbeddedWorkerStatus running_status) {
  switch (running_status) {
    case EmbeddedWorkerStatus::STOPPED:
      return ServiceWorker::ServiceWorkerVersionRunningStatusEnum::Stopped;
    case EmbeddedWorkerStatus::STARTING:
      return ServiceWorker::ServiceWorkerVersionRunningStatusEnum::Starting;
    case EmbeddedWorkerStatus::RUNNING:
      return ServiceWorker::ServiceWorkerVersionRunningStatusEnum::Running;
    case EmbeddedWorkerStatus::STOPPING:
      return ServiceWorker::ServiceWorkerVersionRunningStatusEnum::Stopping;
    default:
      NOTREACHED();
  }
  return std::string();
}

const std::string GetVersionStatusString(
    content::ServiceWorkerVersion::Status status) {
  switch (status) {
    case content::ServiceWorkerVersion::NEW:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::New;
    case content::ServiceWorkerVersion::INSTALLING:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Installing;
    case content::ServiceWorkerVersion::INSTALLED:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Installed;
    case content::ServiceWorkerVersion::ACTIVATING:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Activating;
    case content::ServiceWorkerVersion::ACTIVATED:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Activated;
    case content::ServiceWorkerVersion::REDUNDANT:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Redundant;
    default:
      NOTREACHED();
  }
  return std::string();
}

void StopServiceWorkerOnIO(scoped_refptr<ServiceWorkerContextWrapper> context,
                           int64_t version_id) {
  if (content::ServiceWorkerVersion* version =
          context->GetLiveVersion(version_id)) {
    version->StopWorker(base::Bind(&StatusNoOp));
  }
}

void GetDevToolsRouteInfoOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    int64_t version_id,
    const base::Callback<void(int, int)>& callback) {
  if (content::ServiceWorkerVersion* version =
          context->GetLiveVersion(version_id)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            callback, version->embedded_worker()->process_id(),
            version->embedded_worker()->worker_devtools_agent_route_id()));
  }
}

Response CreateContextErrorResponse() {
  return Response::Error("Could not connect to the context");
}

Response CreateInvalidVersionIdErrorResponse() {
  return Response::InvalidParams("Invalid version ID");
}

void DidFindRegistrationForDispatchSyncEventOnIO(
    scoped_refptr<BackgroundSyncContext> sync_context,
    const std::string& tag,
    bool last_chance,
    ServiceWorkerStatusCode status,
    scoped_refptr<content::ServiceWorkerRegistration> registration) {
  if (status != SERVICE_WORKER_OK || !registration->active_version())
    return;
  BackgroundSyncManager* background_sync_manager =
      sync_context->background_sync_manager();
  scoped_refptr<content::ServiceWorkerVersion> version(
      registration->active_version());
  // Keep the registration while dispatching the sync event.
  background_sync_manager->EmulateDispatchSyncEvent(
      tag, std::move(version), last_chance,
      base::Bind(&StatusNoOpKeepingRegistration, std::move(registration)));
}

void DispatchSyncEventOnIO(scoped_refptr<ServiceWorkerContextWrapper> context,
                           scoped_refptr<BackgroundSyncContext> sync_context,
                           const GURL& origin,
                           int64_t registration_id,
                           const std::string& tag,
                           bool last_chance) {
  context->FindReadyRegistrationForId(
      registration_id, origin,
      base::Bind(&DidFindRegistrationForDispatchSyncEventOnIO, sync_context,
                 tag, last_chance));
}

}  // namespace

ServiceWorkerHandler::ServiceWorkerHandler()
    : DevToolsDomainHandler(ServiceWorker::Metainfo::domainName),
      enabled_(false),
      render_frame_host_(nullptr),
      weak_factory_(this) {
}

ServiceWorkerHandler::~ServiceWorkerHandler() {
}

void ServiceWorkerHandler::Wire(UberDispatcher* dispatcher) {
  frontend_.reset(new ServiceWorker::Frontend(dispatcher->channel()));
  ServiceWorker::Dispatcher::wire(dispatcher, this);
}

void ServiceWorkerHandler::SetRenderFrameHost(
    RenderFrameHostImpl* render_frame_host) {
  render_frame_host_ = render_frame_host;
  // Do not call UpdateHosts yet, wait for load to commit.
  if (!render_frame_host) {
    ClearForceUpdate();
    context_ = nullptr;
    return;
  }
  StoragePartition* partition = BrowserContext::GetStoragePartition(
      render_frame_host->GetProcess()->GetBrowserContext(),
      render_frame_host->GetSiteInstance());
  DCHECK(partition);
  context_ = static_cast<ServiceWorkerContextWrapper*>(
      partition->GetServiceWorkerContext());
}

Response ServiceWorkerHandler::Enable() {
  if (enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResponse();
  enabled_ = true;

  context_watcher_ = new ServiceWorkerContextWatcher(
      context_, base::Bind(&ServiceWorkerHandler::OnWorkerRegistrationUpdated,
                           weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceWorkerHandler::OnWorkerVersionUpdated,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceWorkerHandler::OnErrorReported,
                 weak_factory_.GetWeakPtr()));
  context_watcher_->Start();

  return Response::OK();
}

Response ServiceWorkerHandler::Disable() {
  if (!enabled_)
    return Response::OK();
  enabled_ = false;

  ClearForceUpdate();
  DCHECK(context_watcher_);
  context_watcher_->Stop();
  context_watcher_ = nullptr;
  return Response::OK();
}

Response ServiceWorkerHandler::Unregister(const std::string& scope_url) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResponse();
  context_->UnregisterServiceWorker(GURL(scope_url), base::Bind(&ResultNoOp));
  return Response::OK();
}

Response ServiceWorkerHandler::StartWorker(const std::string& scope_url) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResponse();
  context_->StartServiceWorker(GURL(scope_url), base::Bind(&StatusNoOp));
  return Response::OK();
}

Response ServiceWorkerHandler::SkipWaiting(const std::string& scope_url) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResponse();
  context_->SkipWaitingWorker(GURL(scope_url));
  return Response::OK();
}

Response ServiceWorkerHandler::StopWorker(const std::string& version_id) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(version_id, &id))
    return CreateInvalidVersionIdErrorResponse();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&StopServiceWorkerOnIO, context_, id));
  return Response::OK();
}

Response ServiceWorkerHandler::UpdateRegistration(
    const std::string& scope_url) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResponse();
  context_->UpdateRegistration(GURL(scope_url));
  return Response::OK();
}

Response ServiceWorkerHandler::InspectWorker(const std::string& version_id) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResponse();

  int64_t id = kInvalidServiceWorkerVersionId;
  if (!base::StringToInt64(version_id, &id))
    return CreateInvalidVersionIdErrorResponse();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetDevToolsRouteInfoOnIO, context_, id,
                 base::Bind(&ServiceWorkerHandler::OpenNewDevToolsWindow,
                            weak_factory_.GetWeakPtr())));
  return Response::OK();
}

Response ServiceWorkerHandler::SetForceUpdateOnPageLoad(
    bool force_update_on_page_load) {
  if (!context_)
    return CreateContextErrorResponse();
  context_->SetForceUpdateOnPageLoad(force_update_on_page_load);
  return Response::OK();
}

Response ServiceWorkerHandler::DeliverPushMessage(
    const std::string& origin,
    const std::string& registration_id,
    const std::string& data) {
  if (!enabled_)
    return Response::OK();
  if (!render_frame_host_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(registration_id, &id))
    return CreateInvalidVersionIdErrorResponse();
  PushEventPayload payload;
  if (data.size() > 0)
    payload.setData(data);
  BrowserContext::DeliverPushMessage(
      render_frame_host_->GetProcess()->GetBrowserContext(), GURL(origin), id,
      payload, base::Bind(&PushDeliveryNoOp));
  return Response::OK();
}

Response ServiceWorkerHandler::DispatchSyncEvent(
    const std::string& origin,
    const std::string& registration_id,
    const std::string& tag,
    bool last_chance) {
  if (!enabled_)
    return Response::OK();
  if (!render_frame_host_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(registration_id, &id))
    return CreateInvalidVersionIdErrorResponse();

  StoragePartitionImpl* partition =
      static_cast<StoragePartitionImpl*>(BrowserContext::GetStoragePartition(
          render_frame_host_->GetProcess()->GetBrowserContext(),
          render_frame_host_->GetSiteInstance()));
  BackgroundSyncContext* sync_context = partition->GetBackgroundSyncContext();

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&DispatchSyncEventOnIO, context_,
                                     make_scoped_refptr(sync_context),
                                     GURL(origin), id, tag, last_chance));
  return Response::OK();
}

void ServiceWorkerHandler::OpenNewDevToolsWindow(int process_id,
                                                 int devtools_agent_route_id) {
  scoped_refptr<DevToolsAgentHostImpl> agent_host(
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForWorker(process_id, devtools_agent_route_id));
  if (!agent_host.get())
    return;
  agent_host->Inspect();
}

void ServiceWorkerHandler::OnWorkerRegistrationUpdated(
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  using Registration = ServiceWorker::ServiceWorkerRegistration;
  std::unique_ptr<protocol::Array<Registration>> result =
      protocol::Array<Registration>::create();
  for (const auto& registration : registrations) {
    result->addItem(Registration::Create()
        .SetRegistrationId(
            base::Int64ToString(registration.registration_id))
        .SetScopeURL(registration.pattern.spec())
        .SetIsDeleted(registration.delete_flag ==
                      ServiceWorkerRegistrationInfo::IS_DELETED)
        .Build());
  }
  frontend_->WorkerRegistrationUpdated(std::move(result));
}

void ServiceWorkerHandler::OnWorkerVersionUpdated(
    const std::vector<ServiceWorkerVersionInfo>& versions) {
  using Version = ServiceWorker::ServiceWorkerVersion;
  std::unique_ptr<protocol::Array<Version>> result =
      protocol::Array<Version>::create();
  for (const auto& version : versions) {
    base::flat_set<std::string> client_set;

    for (const auto& client : version.clients) {
      if (client.second.type == SERVICE_WORKER_PROVIDER_FOR_WINDOW) {
        // PlzNavigate: a navigation may not yet be associated with a
        // RenderFrameHost. Use the |web_contents_getter| instead.
        WebContents* web_contents =
            client.second.web_contents_getter
                ? client.second.web_contents_getter.Run()
                : WebContents::FromRenderFrameHost(RenderFrameHostImpl::FromID(
                      client.second.process_id, client.second.route_id));
        // There is a possibility that the frame is already deleted
        // because of the thread hopping.
        if (!web_contents)
          continue;
        client_set.insert(
            DevToolsAgentHost::GetOrCreateFor(web_contents)->GetId());
      } else if (client.second.type ==
                 SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER) {
        scoped_refptr<DevToolsAgentHost> agent_host(
            DevToolsAgentHost::GetForWorker(client.second.process_id,
                                            client.second.route_id));
        if (agent_host)
          client_set.insert(agent_host->GetId());
      }
    }
    std::unique_ptr<protocol::Array<std::string>> clients =
        protocol::Array<std::string>::create();
    for (auto& c : client_set)
      clients->addItem(c);

    std::unique_ptr<Version> version_value = Version::Create()
        .SetVersionId(base::Int64ToString(version.version_id))
        .SetRegistrationId(
            base::Int64ToString(version.registration_id))
        .SetScriptURL(version.script_url.spec())
        .SetRunningStatus(
            GetVersionRunningStatusString(version.running_status))
        .SetStatus(GetVersionStatusString(version.status))
        .SetScriptLastModified(
            version.script_last_modified.ToDoubleT())
        .SetScriptResponseTime(
            version.script_response_time.ToDoubleT())
        .SetControlledClients(std::move(clients))
        .Build();
    scoped_refptr<DevToolsAgentHostImpl> host(
        ServiceWorkerDevToolsManager::GetInstance()
            ->GetDevToolsAgentHostForWorker(
                version.process_id,
                version.devtools_agent_route_id));
    if (host)
      version_value->SetTargetId(host->GetId());
    result->addItem(std::move(version_value));
  }
  frontend_->WorkerVersionUpdated(std::move(result));
}

void ServiceWorkerHandler::OnErrorReported(
    int64_t registration_id,
    int64_t version_id,
    const ServiceWorkerContextCoreObserver::ErrorInfo& info) {
  frontend_->WorkerErrorReported(
      ServiceWorker::ServiceWorkerErrorMessage::Create()
          .SetErrorMessage(base::UTF16ToUTF8(info.error_message))
          .SetRegistrationId(base::Int64ToString(registration_id))
          .SetVersionId(base::Int64ToString(version_id))
          .SetSourceURL(info.source_url.spec())
          .SetLineNumber(info.line_number)
          .SetColumnNumber(info.column_number)
          .Build());
}

void ServiceWorkerHandler::ClearForceUpdate() {
  if (context_)
    context_->SetForceUpdateOnPageLoad(false);
}

}  // namespace protocol
}  // namespace content
