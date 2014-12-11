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
#include "content/browser/service_worker/service_worker_registration_handle.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/resource_request_body.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/child_process_host.h"

namespace content {

static const int kDocumentMainThreadId = 0;

namespace {

void FocusOnUIThread(int render_process_id,
                     int render_frame_id,
                     const ServiceWorkerProviderHost::FocusCallback& callback) {
  WebContents* web_contents = WebContents::FromRenderFrameHost(
      RenderFrameHost::FromID(render_process_id, render_frame_id));

  bool result = false;

  if (web_contents && web_contents->GetDelegate()) {
    result = true;
    web_contents->GetDelegate()->ActivateContents(web_contents);
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, result));
}

}  // anonymous namespace

ServiceWorkerProviderHost::ServiceWorkerProviderHost(
    int render_process_id, int render_frame_id, int provider_id,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerDispatcherHost* dispatcher_host)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      provider_id_(provider_id),
      context_(context),
      dispatcher_host_(dispatcher_host),
      allow_association_(true) {
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, render_process_id_);
}

ServiceWorkerProviderHost::~ServiceWorkerProviderHost() {
  // Clear docurl so the deferred activation of a waiting worker
  // won't associate the new version with a provider being destroyed.
  document_url_ = GURL();
  if (controlling_version_.get())
    controlling_version_->RemoveControllee(this);
  if (associated_registration_.get()) {
    DecreaseProcessReference(associated_registration_->pattern());
    associated_registration_->RemoveListener(this);
  }

  for (const GURL& pattern : associated_patterns_)
    DecreaseProcessReference(pattern);
}

void ServiceWorkerProviderHost::OnRegistrationFailed(
    ServiceWorkerRegistration* registration) {
  DCHECK_EQ(associated_registration_.get(), registration);
  DisassociateRegistration();
}

void ServiceWorkerProviderHost::SetDocumentUrl(const GURL& url) {
  DCHECK(!url.has_ref());
  document_url_ = url;
}

void ServiceWorkerProviderHost::SetTopmostFrameUrl(const GURL& url) {
  topmost_frame_url_ = url;
}

void ServiceWorkerProviderHost::SetControllerVersionAttribute(
    ServiceWorkerVersion* version) {
  if (version == controlling_version_.get())
    return;

  scoped_refptr<ServiceWorkerVersion> previous_version = controlling_version_;
  controlling_version_ = version;
  if (version)
    version->AddControllee(this);
  if (previous_version.get())
    previous_version->RemoveControllee(this);

  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  dispatcher_host_->Send(new ServiceWorkerMsg_SetControllerServiceWorker(
      kDocumentMainThreadId, provider_id(), CreateHandleAndPass(version)));
}

bool ServiceWorkerProviderHost::SetHostedVersionId(int64 version_id) {
  if (!context_)
    return true;  // System is shutting down.
  if (active_version())
    return false;  // Unexpected bad message.

  ServiceWorkerVersion* live_version = context_->GetLiveVersion(version_id);
  if (!live_version)
    return true;  // Was deleted before it got started.

  ServiceWorkerVersionInfo info = live_version->GetInfo();
  if (info.running_status != ServiceWorkerVersion::STARTING ||
      info.process_id != render_process_id_) {
    // If we aren't trying to start this version in our process
    // something is amiss.
    return false;
  }

  running_hosted_version_ = live_version;
  return true;
}

void ServiceWorkerProviderHost::AssociateRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(CanAssociateRegistration(registration));
  if (associated_registration_.get())
    DecreaseProcessReference(associated_registration_->pattern());
  IncreaseProcessReference(registration->pattern());

  associated_registration_ = registration;
  associated_registration_->AddListener(this);
  SendAssociateRegistrationMessage();
  SetControllerVersionAttribute(registration->active_version());
}

void ServiceWorkerProviderHost::DisassociateRegistration() {
  if (!associated_registration_.get())
    return;
  DecreaseProcessReference(associated_registration_->pattern());
  associated_registration_->RemoveListener(this);
  associated_registration_ = NULL;
  SetControllerVersionAttribute(NULL);

  if (dispatcher_host_) {
    dispatcher_host_->Send(new ServiceWorkerMsg_DisassociateRegistration(
        kDocumentMainThreadId, provider_id()));
  }
}

scoped_ptr<ServiceWorkerRequestHandler>
ServiceWorkerProviderHost::CreateRequestHandler(
    FetchRequestMode request_mode,
    FetchCredentialsMode credentials_mode,
    ResourceType resource_type,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    scoped_refptr<ResourceRequestBody> body) {
  if (IsHostToRunningServiceWorker()) {
    return scoped_ptr<ServiceWorkerRequestHandler>(
        new ServiceWorkerContextRequestHandler(
            context_, AsWeakPtr(), blob_storage_context, resource_type));
  }
  if (ServiceWorkerUtils::IsMainResourceType(resource_type) ||
      controlling_version()) {
    return scoped_ptr<ServiceWorkerRequestHandler>(
        new ServiceWorkerControlleeRequestHandler(context_,
                                                  AsWeakPtr(),
                                                  blob_storage_context,
                                                  request_mode,
                                                  credentials_mode,
                                                  resource_type,
                                                  request_context_type,
                                                  frame_type,
                                                  body));
  }
  return scoped_ptr<ServiceWorkerRequestHandler>();
}

bool ServiceWorkerProviderHost::CanAssociateRegistration(
    ServiceWorkerRegistration* registration) {
  if (!context_)
    return false;
  if (running_hosted_version_.get())
    return false;
  if (!registration || associated_registration_.get() || !allow_association_)
    return false;
  return true;
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

void ServiceWorkerProviderHost::Focus(const FocusCallback& callback) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&FocusOnUIThread,
                 render_process_id_,
                 render_frame_id_,
                 callback));
}

void ServiceWorkerProviderHost::GetClientInfo(
    int embedded_worker_id,
    int request_id) {
  dispatcher_host_->Send(new ServiceWorkerMsg_GetClientInfo(
      kDocumentMainThreadId, embedded_worker_id, request_id, provider_id()));
}

void ServiceWorkerProviderHost::AddScopedProcessReferenceToPattern(
    const GURL& pattern) {
  associated_patterns_.push_back(pattern);
  IncreaseProcessReference(pattern);
}

void ServiceWorkerProviderHost::PrepareForCrossSiteTransfer() {
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, render_process_id_);

  for (const GURL& pattern : associated_patterns_)
    DecreaseProcessReference(pattern);

  if (associated_registration_.get())
    DecreaseProcessReference(associated_registration_->pattern());

  render_process_id_ = ChildProcessHost::kInvalidUniqueID;
  render_frame_id_ = MSG_ROUTING_NONE;
  provider_id_ = kInvalidServiceWorkerProviderId;
  dispatcher_host_ = nullptr;
}

void ServiceWorkerProviderHost::CompleteCrossSiteTransfer(
    int new_process_id,
    int new_frame_id,
    int new_provider_id,
    ServiceWorkerDispatcherHost* new_dispatcher_host) {
  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, render_process_id_);
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, new_process_id);

  render_process_id_ = new_process_id;
  render_frame_id_ = new_frame_id;
  provider_id_ = new_provider_id;
  dispatcher_host_ = new_dispatcher_host;

  for (const GURL& pattern : associated_patterns_)
    IncreaseProcessReference(pattern);

  if (associated_registration_.get()) {
    IncreaseProcessReference(associated_registration_->pattern());
    SendAssociateRegistrationMessage();
    if (dispatcher_host_ && associated_registration_->active_version()) {
      dispatcher_host_->Send(new ServiceWorkerMsg_SetControllerServiceWorker(
          kDocumentMainThreadId, provider_id(),
          CreateHandleAndPass(associated_registration_->active_version())));
    }
  }
}

void ServiceWorkerProviderHost::SendAssociateRegistrationMessage() {
  if (!dispatcher_host_)
    return;

  ServiceWorkerRegistrationHandle* handle =
      dispatcher_host_->GetOrCreateRegistrationHandle(
          provider_id(), associated_registration_.get());

  ServiceWorkerVersionAttributes attrs;
  attrs.installing = handle->CreateServiceWorkerHandleAndPass(
      associated_registration_->installing_version());
  attrs.waiting = handle->CreateServiceWorkerHandleAndPass(
      associated_registration_->waiting_version());
  attrs.active = handle->CreateServiceWorkerHandleAndPass(
      associated_registration_->active_version());

  dispatcher_host_->Send(new ServiceWorkerMsg_AssociateRegistration(
      kDocumentMainThreadId, provider_id(), handle->GetObjectInfo(), attrs));
}

ServiceWorkerObjectInfo ServiceWorkerProviderHost::CreateHandleAndPass(
    ServiceWorkerVersion* version) {
  ServiceWorkerObjectInfo info;
  if (context_ && version) {
    scoped_ptr<ServiceWorkerHandle> handle =
        ServiceWorkerHandle::Create(context_,
                                    dispatcher_host_,
                                    kDocumentMainThreadId,
                                    provider_id_,
                                    version);
    info = handle->GetObjectInfo();
    dispatcher_host_->RegisterServiceWorkerHandle(handle.Pass());
  }
  return info;
}

void ServiceWorkerProviderHost::IncreaseProcessReference(
    const GURL& pattern) {
  if (context_ && context_->process_manager()) {
    context_->process_manager()->AddProcessReferenceToPattern(
        pattern, render_process_id_);
  }
}

void ServiceWorkerProviderHost::DecreaseProcessReference(
    const GURL& pattern) {
  if (context_ && context_->process_manager()) {
    context_->process_manager()->RemoveProcessReferenceFromPattern(
        pattern, render_process_id_);
  }
}

bool ServiceWorkerProviderHost::IsContextAlive() {
  return context_ != NULL;
}

}  // namespace content
