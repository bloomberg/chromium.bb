// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/service_worker_handler.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

namespace content {
namespace devtools {
namespace service_worker {

namespace {

const std::string GetVersionRunningStatusString(
    content::ServiceWorkerVersion::RunningStatus running_status) {
  switch (running_status) {
    case content::ServiceWorkerVersion::STOPPED:
      return service_worker_version::kRunningStatusStopped;
    case content::ServiceWorkerVersion::STARTING:
      return service_worker_version::kRunningStatusStarting;
    case content::ServiceWorkerVersion::RUNNING:
      return service_worker_version::kRunningStatusRunning;
    case content::ServiceWorkerVersion::STOPPING:
      return service_worker_version::kRunningStatusStopping;
  }
  return "";
}

const std::string GetVersionStatusString(
    content::ServiceWorkerVersion::Status status) {
  switch (status) {
    case content::ServiceWorkerVersion::NEW:
      return service_worker_version::kStatusNew;
    case content::ServiceWorkerVersion::INSTALLING:
      return service_worker_version::kStatusInstalling;
    case content::ServiceWorkerVersion::INSTALLED:
      return service_worker_version::kStatusInstalled;
    case content::ServiceWorkerVersion::ACTIVATING:
      return service_worker_version::kStatusActivating;
    case content::ServiceWorkerVersion::ACTIVATED:
      return service_worker_version::kStatusActivated;
    case content::ServiceWorkerVersion::REDUNDANT:
      return service_worker_version::kStatusRedundant;
  }
  return "";
}

scoped_refptr<ServiceWorkerVersion> CreateVersionDictionaryValue(
    const ServiceWorkerVersionInfo& version_info) {
  scoped_refptr<ServiceWorkerVersion> version(
      ServiceWorkerVersion::Create()
          ->set_version_id(base::Int64ToString(version_info.version_id))
          ->set_registration_id(
                base::Int64ToString(version_info.registration_id))
          ->set_script_url(version_info.script_url.spec())
          ->set_running_status(
                GetVersionRunningStatusString(version_info.running_status))
          ->set_status(GetVersionStatusString(version_info.status)));
  return version;
}

scoped_refptr<ServiceWorkerRegistration> CreateRegistrationDictionaryValue(
    const ServiceWorkerRegistrationInfo& registration_info) {
  scoped_refptr<ServiceWorkerRegistration> registration(
      ServiceWorkerRegistration::Create()
          ->set_registration_id(
                base::Int64ToString(registration_info.registration_id))
          ->set_scope_url(registration_info.pattern.spec()));
  if (registration_info.active_version.version_id !=
      kInvalidServiceWorkerVersionId) {
    registration->set_active_version(
        CreateVersionDictionaryValue(registration_info.active_version));
  }
  if (registration_info.waiting_version.version_id !=
      kInvalidServiceWorkerVersionId) {
    registration->set_waiting_version(
        CreateVersionDictionaryValue(registration_info.waiting_version));
  }
  if (registration_info.installing_version.version_id !=
      kInvalidServiceWorkerVersionId) {
    registration->set_installing_version(
        CreateVersionDictionaryValue(registration_info.installing_version));
  }
  return registration;
}

}  // namespace

using Response = DevToolsProtocolClient::Response;

class ServiceWorkerHandler::ContextObserver
    : public ServiceWorkerContextObserver,
      public base::RefCountedThreadSafe<ContextObserver> {
 public:
  ContextObserver(scoped_refptr<ServiceWorkerContextWrapper> context,
                  base::WeakPtr<ServiceWorkerHandler> handler);
  void Start();
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<ContextObserver>;
  ~ContextObserver() override;
  void GetStoredRegistrationsOnIOThread();
  void OnStoredRegistrationsOnIOThread(
      const std::vector<ServiceWorkerRegistrationInfo>& registrations);
  void StopOnIOThread();

  void OnVersionUpdated(int64 version_id);
  void OnRegistrationUpdated(int64 registration_id);

  // ServiceWorkerContextObserver implements
  void OnRunningStateChanged(int64 version_id) override;
  void OnVersionStateChanged(int64 version_id) override;
  void OnRegistrationStored(int64 registration_id,
                            const GURL& pattern) override;
  void OnRegistrationDeleted(int64 registration_id,
                             const GURL& pattern) override;

  scoped_refptr<ServiceWorkerContextWrapper> context_;
  base::WeakPtr<ServiceWorkerHandler> handler_;
};

ServiceWorkerHandler::ContextObserver::ContextObserver(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    base::WeakPtr<ServiceWorkerHandler> handler)
    : context_(context), handler_(handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ServiceWorkerHandler::ContextObserver::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&ServiceWorkerHandler::ContextObserver::
                                         GetStoredRegistrationsOnIOThread,
                                     this));
}

void ServiceWorkerHandler::ContextObserver::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ServiceWorkerHandler::ContextObserver::StopOnIOThread, this));
}

void ServiceWorkerHandler::ContextObserver::GetStoredRegistrationsOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  context_->context()->storage()->GetAllRegistrations(base::Bind(
      &ServiceWorkerHandler::ContextObserver::OnStoredRegistrationsOnIOThread,
      this));
}

void ServiceWorkerHandler::ContextObserver::OnStoredRegistrationsOnIOThread(
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  context_->AddObserver(this);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ServiceWorkerHandler::OnWorkerRegistrationUpdated, handler_,
                 registrations));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ServiceWorkerHandler::OnWorkerRegistrationUpdated, handler_,
                 context_->context()->GetAllLiveRegistrationInfo()));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ServiceWorkerHandler::OnWorkerVersionUpdated, handler_,
                 context_->context()->GetAllLiveVersionInfo()));
}

void ServiceWorkerHandler::ContextObserver::StopOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  context_->RemoveObserver(this);
}

ServiceWorkerHandler::ContextObserver::~ContextObserver() {
}

void ServiceWorkerHandler::ContextObserver::OnVersionUpdated(int64 version_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  content::ServiceWorkerVersion* version =
      context_->context()->GetLiveVersion(version_id);
  if (!version)
    return;
  OnRegistrationUpdated(version->registration_id());
  std::vector<ServiceWorkerVersionInfo> versions;
  versions.push_back(version->GetInfo());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ServiceWorkerHandler::OnWorkerVersionUpdated, handler_,
                 versions));
}

void ServiceWorkerHandler::ContextObserver::OnRegistrationUpdated(
    int64 registration_id) {
  content::ServiceWorkerRegistration* registration =
      context_->context()->GetLiveRegistration(registration_id);
  if (!registration)
    return;
  std::vector<ServiceWorkerRegistrationInfo> registrations;
  registrations.push_back(registration->GetInfo());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ServiceWorkerHandler::OnWorkerRegistrationUpdated, handler_,
                 registrations));
}

void ServiceWorkerHandler::ContextObserver::OnRunningStateChanged(
    int64 version_id) {
  OnVersionUpdated(version_id);
}

void ServiceWorkerHandler::ContextObserver::OnVersionStateChanged(
    int64 version_id) {
  OnVersionUpdated(version_id);
}

void ServiceWorkerHandler::ContextObserver::OnRegistrationStored(
    int64 registration_id,
    const GURL& pattern) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  content::ServiceWorkerRegistration* registration =
      context_->context()->GetLiveRegistration(registration_id);
  DCHECK(registration);
  std::vector<ServiceWorkerRegistrationInfo> registrations;
  registrations.push_back(registration->GetInfo());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ServiceWorkerHandler::OnWorkerRegistrationUpdated, handler_,
                 registrations));
}

void ServiceWorkerHandler::ContextObserver::OnRegistrationDeleted(
    int64 registration_id,
    const GURL& pattern) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ServiceWorkerHandler::OnWorkerRegistrationDeleted, handler_,
                 registration_id));
}

ServiceWorkerHandler::ServiceWorkerHandler()
    : enabled_(false), weak_factory_(this) {
}

ServiceWorkerHandler::~ServiceWorkerHandler() {
  Disable();
}

void ServiceWorkerHandler::SetRenderFrameHost(
    RenderFrameHost* render_frame_host) {
  if (!render_frame_host) {
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

void ServiceWorkerHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

void ServiceWorkerHandler::SetURL(const GURL& url) {
  url_ = url;
  if (enabled_) {
    for (auto pair : attached_hosts_) {
      if (!MatchesInspectedPage(pair.second.get()))
        WorkerDestroyed(pair.second.get());
    }

    ServiceWorkerDevToolsAgentHost::List agent_hosts;
    ServiceWorkerDevToolsManager::GetInstance()->
        AddAllAgentHosts(&agent_hosts);
    for (auto host : agent_hosts) {
      if (!MatchesInspectedPage(host.get()))
        continue;
      if (attached_hosts_.find(host->GetId()) != attached_hosts_.end())
        continue;
      // TODO(pfeldman): workers are created concurrently, we need
      // to get notification earlier to go through the Created/Ready
      // lifecycle.
      WorkerReadyForInspection(host.get());
    }
  }
}

void ServiceWorkerHandler::Detached() {
  Disable();
}

Response ServiceWorkerHandler::Enable() {
  if (enabled_)
    return Response::OK();
  if (!context_)
    return Response::InternalError("Could not connect to the context");
  enabled_ = true;

  ServiceWorkerDevToolsManager::GetInstance()->AddObserver(this);

  ServiceWorkerDevToolsAgentHost::List agent_hosts;
  ServiceWorkerDevToolsManager::GetInstance()->AddAllAgentHosts(&agent_hosts);
  for (auto host : agent_hosts)
    WorkerReadyForInspection(host.get());

  context_observer_ = new ContextObserver(context_, weak_factory_.GetWeakPtr());
  context_observer_->Start();
  return Response::OK();
}

Response ServiceWorkerHandler::Disable() {
  if (!enabled_)
    return Response::OK();
  enabled_ = false;

  ServiceWorkerDevToolsManager::GetInstance()->RemoveObserver(this);
  for (const auto& pair : attached_hosts_)
    pair.second->DetachClient();
  attached_hosts_.clear();
  DCHECK(context_observer_);
  context_observer_->Stop();
  context_observer_ = nullptr;
  return Response::OK();
}

Response ServiceWorkerHandler::SendMessage(
    const std::string& worker_id,
    const std::string& message) {
  auto it = attached_hosts_.find(worker_id);
  if (it == attached_hosts_.end())
    return Response::InternalError("Not connected to the worker");
  it->second->DispatchProtocolMessage(message);
  return Response::OK();
}

Response ServiceWorkerHandler::Stop(
    const std::string& worker_id) {
  auto it = attached_hosts_.find(worker_id);
  if (it == attached_hosts_.end())
    return Response::InternalError("Not connected to the worker");
  it->second->Close();
  return Response::OK();
}

void ServiceWorkerHandler::OnWorkerRegistrationUpdated(
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registration_values;
  for (const auto& registration : registrations) {
    registration_values.push_back(
        CreateRegistrationDictionaryValue(registration));
  }
  client_->WorkerRegistrationUpdated(
      WorkerRegistrationUpdatedParams::Create()->set_registrations(
          registration_values));
}

void ServiceWorkerHandler::OnWorkerVersionUpdated(
    const std::vector<ServiceWorkerVersionInfo>& versions) {
  std::vector<scoped_refptr<ServiceWorkerVersion>> version_values;
  for (const auto& version : versions) {
    version_values.push_back(CreateVersionDictionaryValue(version));
  }
  client_->WorkerVersionUpdated(
      WorkerVersionUpdatedParams::Create()->set_versions(version_values));
}

void ServiceWorkerHandler::OnWorkerRegistrationDeleted(int64 registration_id) {
  client_->WorkerRegistrationDeleted(
      WorkerRegistrationDeletedParams::Create()->set_registration_id(
          base::Int64ToString(registration_id)));
}

void ServiceWorkerHandler::DispatchProtocolMessage(
    DevToolsAgentHost* host,
    const std::string& message) {

  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;  // Already disconnected.

  client_->DispatchMessage(
      DispatchMessageParams::Create()->
          set_worker_id(host->GetId())->
          set_message(message));
}

void ServiceWorkerHandler::AgentHostClosed(
    DevToolsAgentHost* host,
    bool replaced_with_another_client) {
  WorkerDestroyed(static_cast<ServiceWorkerDevToolsAgentHost*>(host));
}

void ServiceWorkerHandler::WorkerCreated(
    ServiceWorkerDevToolsAgentHost* host) {
  if (!MatchesInspectedPage(host))
    return;
  host->PauseForDebugOnStart();
}

void ServiceWorkerHandler::WorkerReadyForInspection(
    ServiceWorkerDevToolsAgentHost* host) {
  if (host->IsAttached() || !MatchesInspectedPage(host))
    return;

  attached_hosts_[host->GetId()] = host;
  host->AttachClient(this);
  client_->WorkerCreated(WorkerCreatedParams::Create()->
      set_worker_id(host->GetId())->
      set_url(host->GetURL().spec()));
}

void ServiceWorkerHandler::WorkerDestroyed(
    ServiceWorkerDevToolsAgentHost* host) {
  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;
  it->second->DetachClient();
  client_->WorkerTerminated(WorkerTerminatedParams::Create()->
      set_worker_id(host->GetId()));
  attached_hosts_.erase(it);
}

bool ServiceWorkerHandler::MatchesInspectedPage(
    ServiceWorkerDevToolsAgentHost* host) {
  // TODO(pfeldman): match based on scope.
  // TODO(pfeldman): match iframes.
  return host->GetURL().host() == url_.host();
}

}  // namespace service_worker
}  // namespace devtools
}  // namespace content
