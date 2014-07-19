// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include "base/stl_util.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_request_handler.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_messages.h"

namespace content {

static const int kDocumentMainThreadId = 0;

ServiceWorkerProviderHost::ServiceWorkerProviderHost(
    int process_id, int provider_id,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerDispatcherHost* dispatcher_host)
    : process_id_(process_id),
      provider_id_(provider_id),
      context_(context),
      dispatcher_host_(dispatcher_host) {
}

ServiceWorkerProviderHost::~ServiceWorkerProviderHost() {
  // Clear docurl so the deferred activation of a waiting worker
  // won't associate the new version with a provider being destroyed.
  document_url_ = GURL();
  if (controlling_version_)
    controlling_version_->RemoveControllee(this);
  if (active_version_)
    active_version_->RemovePotentialControllee(this);
  if (waiting_version_)
    waiting_version_->RemovePotentialControllee(this);
  if (installing_version_)
    installing_version_->RemovePotentialControllee(this);
}

void ServiceWorkerProviderHost::SetDocumentUrl(const GURL& url) {
  DCHECK(!url.has_ref());
  document_url_ = url;
}

void ServiceWorkerProviderHost::SetControllerVersion(
    ServiceWorkerVersion* version) {
  DCHECK(CanAssociateVersion(version));
  if (version == controlling_version_)
    return;
  scoped_refptr<ServiceWorkerVersion> previous_version = controlling_version_;
  controlling_version_ = version;
  if (version)
    version->AddControllee(this);
  if (previous_version)
    previous_version->RemoveControllee(this);

  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  dispatcher_host_->Send(new ServiceWorkerMsg_SetControllerServiceWorker(
      kDocumentMainThreadId, provider_id(), CreateHandleAndPass(version)));
}

void ServiceWorkerProviderHost::SetActiveVersion(
    ServiceWorkerVersion* version) {
  DCHECK(CanAssociateVersion(version));
  if (version == active_version_)
    return;
  scoped_refptr<ServiceWorkerVersion> previous_version = active_version_;
  active_version_ = version;
  if (version)
    version->AddPotentialControllee(this);
  if (previous_version)
    previous_version->RemovePotentialControllee(this);

  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  dispatcher_host_->Send(new ServiceWorkerMsg_SetActiveServiceWorker(
      kDocumentMainThreadId, provider_id(), CreateHandleAndPass(version)));
}

void ServiceWorkerProviderHost::SetWaitingVersion(
    ServiceWorkerVersion* version) {
  DCHECK(CanAssociateVersion(version));
  if (version == waiting_version_)
    return;
  scoped_refptr<ServiceWorkerVersion> previous_version = waiting_version_;
  waiting_version_ = version;
  if (version)
    version->AddPotentialControllee(this);
  if (previous_version)
    previous_version->RemovePotentialControllee(this);

  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  dispatcher_host_->Send(new ServiceWorkerMsg_SetWaitingServiceWorker(
      kDocumentMainThreadId, provider_id(), CreateHandleAndPass(version)));
}

void ServiceWorkerProviderHost::SetInstallingVersion(
    ServiceWorkerVersion* version) {
  DCHECK(CanAssociateVersion(version));
  if (version == installing_version_)
    return;
  scoped_refptr<ServiceWorkerVersion> previous_version = installing_version_;
  installing_version_ = version;
  if (version)
    version->AddPotentialControllee(this);
  if (previous_version)
    previous_version->RemovePotentialControllee(this);

  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  dispatcher_host_->Send(new ServiceWorkerMsg_SetInstallingServiceWorker(
      kDocumentMainThreadId, provider_id(), CreateHandleAndPass(version)));
}

void ServiceWorkerProviderHost::UnsetVersion(ServiceWorkerVersion* version) {
  if (!version)
    return;
  if (installing_version_ == version)
    SetInstallingVersion(NULL);
  else if (waiting_version_  == version)
    SetWaitingVersion(NULL);
  else if (active_version_ == version)
    SetActiveVersion(NULL);
  else if (controlling_version_ == version)
    SetControllerVersion(NULL);
}

bool ServiceWorkerProviderHost::SetHostedVersionId(int64 version_id) {
  if (!context_)
    return true;  // System is shutting down.
  if (active_version_)
    return false;  // Unexpected bad message.

  ServiceWorkerVersion* live_version = context_->GetLiveVersion(version_id);
  if (!live_version)
    return true;  // Was deleted before it got started.

  ServiceWorkerVersionInfo info = live_version->GetInfo();
  if (info.running_status != ServiceWorkerVersion::STARTING ||
      info.process_id != process_id_) {
    // If we aren't trying to start this version in our process
    // something is amiss.
    return false;
  }

  running_hosted_version_ = live_version;
  return true;
}

scoped_ptr<ServiceWorkerRequestHandler>
ServiceWorkerProviderHost::CreateRequestHandler(
    ResourceType::Type resource_type,
    base::WeakPtr<webkit_blob::BlobStorageContext> blob_storage_context) {
  if (IsHostToRunningServiceWorker()) {
    return scoped_ptr<ServiceWorkerRequestHandler>(
        new ServiceWorkerContextRequestHandler(
            context_, AsWeakPtr(), blob_storage_context, resource_type));
  }
  if (ServiceWorkerUtils::IsMainResourceType(resource_type) ||
      active_version()) {
    return scoped_ptr<ServiceWorkerRequestHandler>(
        new ServiceWorkerControlleeRequestHandler(
            context_, AsWeakPtr(), blob_storage_context, resource_type));
  }
  return scoped_ptr<ServiceWorkerRequestHandler>();
}

bool ServiceWorkerProviderHost::CanAssociateVersion(
    ServiceWorkerVersion* version) {
  if (!context_)
    return false;
  if (running_hosted_version_)
    return false;
  if (!version)
    return true;

  ServiceWorkerVersion* already_associated_version = NULL;
  if (controlling_version_)
    already_associated_version = controlling_version_;
  if (active_version_)
    already_associated_version = active_version_;
  else if (waiting_version_)
    already_associated_version = waiting_version_;
  else if (installing_version_)
    already_associated_version = installing_version_;

  return !already_associated_version ||
         already_associated_version->registration_id() ==
              version->registration_id();
}

void ServiceWorkerProviderHost::PostMessage(
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids) {
  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  std::vector<int> new_routing_ids;
  dispatcher_host_->message_port_message_filter()->
      UpdateMessagePortsWithNewRoutes(sent_message_port_ids,
                                      &new_routing_ids);

  dispatcher_host_->Send(
      new ServiceWorkerMsg_MessageToDocument(
          kDocumentMainThreadId, provider_id(),
          message,
          sent_message_port_ids,
          new_routing_ids));
}

ServiceWorkerObjectInfo ServiceWorkerProviderHost::CreateHandleAndPass(
    ServiceWorkerVersion* version) {
  ServiceWorkerObjectInfo info;
  if (context_ && version) {
    scoped_ptr<ServiceWorkerHandle> handle =
        ServiceWorkerHandle::Create(context_, dispatcher_host_,
                                    kDocumentMainThreadId, version);
    info = handle->GetObjectInfo();
    dispatcher_host_->RegisterServiceWorkerHandle(handle.Pass());
  }
  return info;
}

bool ServiceWorkerProviderHost::IsContextAlive() {
  return context_ != NULL;
}

}  // namespace content
