// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_manager.h"

#include "base/utf_string_conversions.h"
#include "content/browser/browser_thread.h"
#include "content/browser/load_from_memory_cache_details.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/browser/renderer_host/resource_request_details.h"
#include "content/browser/ssl/ssl_cert_error_handler.h"
#include "content/browser/ssl/ssl_policy.h"
#include "content/browser/ssl/ssl_request_info.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/provisional_load_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "net/base/cert_status_flags.h"

// static
void SSLManager::OnSSLCertificateError(ResourceDispatcherHost* rdh,
                                       net::URLRequest* request,
                                       const net::SSLInfo& ssl_info,
                                       bool is_hsts_host) {
  DVLOG(1) << "OnSSLCertificateError() cert_error: "
           << net::MapCertStatusToNetError(ssl_info.cert_status)
           << " url: " << request->url().spec()
           << " cert_status: " << std::hex << ssl_info.cert_status;

  ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);

  // A certificate error occurred.  Construct a SSLCertErrorHandler object and
  // hand it over to the UI thread for processing.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(new SSLCertErrorHandler(rdh,
                                                request,
                                                info->resource_type(),
                                                ssl_info,
                                                is_hsts_host),
                        &SSLCertErrorHandler::Dispatch));
}

// static
void SSLManager::NotifySSLInternalStateChanged(
    NavigationController* controller) {
  NotificationService::current()->Notify(
      content::NOTIFICATION_SSL_INTERNAL_STATE_CHANGED,
      Source<content::BrowserContext>(controller->browser_context()),
      NotificationService::NoDetails());
}

// static
std::string SSLManager::SerializeSecurityInfo(int cert_id,
                                              net::CertStatus cert_status,
                                              int security_bits,
                                              int ssl_connection_status) {
  Pickle pickle;
  pickle.WriteInt(cert_id);
  pickle.WriteUInt32(cert_status);
  pickle.WriteInt(security_bits);
  pickle.WriteInt(ssl_connection_status);
  return std::string(static_cast<const char*>(pickle.data()), pickle.size());
}

// static
bool SSLManager::DeserializeSecurityInfo(const std::string& state,
                                         int* cert_id,
                                         net::CertStatus* cert_status,
                                         int* security_bits,
                                         int* ssl_connection_status) {
  DCHECK(cert_id && cert_status && security_bits && ssl_connection_status);
  if (state.empty()) {
    // No SSL used.
    *cert_id = 0;
    // The following are not applicable and are set to the default values.
    *cert_status = 0;
    *security_bits = -1;
    *ssl_connection_status = 0;
    return false;
  }

  Pickle pickle(state.data(), static_cast<int>(state.size()));
  void * iter = NULL;
  return pickle.ReadInt(&iter, cert_id) &&
         pickle.ReadUInt32(&iter, cert_status) &&
         pickle.ReadInt(&iter, security_bits) &&
         pickle.ReadInt(&iter, ssl_connection_status);
}

SSLManager::SSLManager(NavigationController* controller)
    : backend_(controller),
      policy_(new SSLPolicy(&backend_)),
      controller_(controller) {
  DCHECK(controller_);

  // Subscribe to various notifications.
  registrar_.Add(this, content::NOTIFICATION_FAIL_PROVISIONAL_LOAD_WITH_ERROR,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, content::NOTIFICATION_RESOURCE_RESPONSE_STARTED,
                 Source<RenderViewHostDelegate>(controller_->tab_contents()));
  registrar_.Add(this, content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
                 Source<RenderViewHostDelegate>(controller_->tab_contents()));
  registrar_.Add(this, content::NOTIFICATION_LOAD_FROM_MEMORY_CACHE,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, content::NOTIFICATION_SSL_INTERNAL_STATE_CHANGED,
                 Source<content::BrowserContext>(
                     controller_->browser_context()));
}

SSLManager::~SSLManager() {
}

void SSLManager::DidCommitProvisionalLoad(
    const NotificationDetails& in_details) {
  content::LoadCommittedDetails* details =
      Details<content::LoadCommittedDetails>(in_details).ptr();

  NavigationEntry* entry = controller_->GetActiveEntry();

  if (details->is_main_frame) {
    if (entry) {
      // Decode the security details.
      int ssl_cert_id;
      net::CertStatus ssl_cert_status;
      int ssl_security_bits;
      int ssl_connection_status;
      DeserializeSecurityInfo(details->serialized_security_info,
                              &ssl_cert_id,
                              &ssl_cert_status,
                              &ssl_security_bits,
                              &ssl_connection_status);

      // We may not have an entry if this is a navigation to an initial blank
      // page. Reset the SSL information and add the new data we have.
      entry->ssl() = NavigationEntry::SSLStatus();
      entry->ssl().set_cert_id(ssl_cert_id);
      entry->ssl().set_cert_status(ssl_cert_status);
      entry->ssl().set_security_bits(ssl_security_bits);
      entry->ssl().set_connection_status(ssl_connection_status);
    }
  }

  UpdateEntry(entry);
}

void SSLManager::DidRunInsecureContent(const std::string& security_origin) {
  policy()->DidRunInsecureContent(controller_->GetActiveEntry(),
                                  security_origin);
}

bool SSLManager::ProcessedSSLErrorFromRequest() const {
  NavigationEntry* entry = controller_->GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return false;
  }

  return net::IsCertStatusError(entry->ssl().cert_status());
}

void SSLManager::Observe(int type,
                         const NotificationSource& source,
                         const NotificationDetails& details) {
  // Dispatch by type.
  switch (type) {
    case content::NOTIFICATION_FAIL_PROVISIONAL_LOAD_WITH_ERROR:
      // Do nothing.
      break;
    case content::NOTIFICATION_RESOURCE_RESPONSE_STARTED:
      DidStartResourceResponse(Details<ResourceRequestDetails>(details).ptr());
      break;
    case content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT:
      DidReceiveResourceRedirect(
          Details<ResourceRedirectDetails>(details).ptr());
      break;
    case content::NOTIFICATION_LOAD_FROM_MEMORY_CACHE:
      DidLoadFromMemoryCache(
          Details<LoadFromMemoryCacheDetails>(details).ptr());
      break;
    case content::NOTIFICATION_SSL_INTERNAL_STATE_CHANGED:
      DidChangeSSLInternalState();
      break;
    default:
      NOTREACHED() << "The SSLManager received an unexpected notification.";
  }
}

void SSLManager::DidLoadFromMemoryCache(LoadFromMemoryCacheDetails* details) {
  // Simulate loading this resource through the usual path.
  // Note that we specify SUB_RESOURCE as the resource type as WebCore only
  // caches sub-resources.
  // This resource must have been loaded with no filtering because filtered
  // resouces aren't cachable.
  scoped_refptr<SSLRequestInfo> info(new SSLRequestInfo(
      details->url(),
      ResourceType::SUB_RESOURCE,
      details->pid(),
      details->ssl_cert_id(),
      details->ssl_cert_status()));

  // Simulate loading this resource through the usual path.
  policy()->OnRequestStarted(info.get());
}

void SSLManager::DidStartResourceResponse(ResourceRequestDetails* details) {
  scoped_refptr<SSLRequestInfo> info(new SSLRequestInfo(
      details->url(),
      details->resource_type(),
      details->origin_child_id(),
      details->ssl_cert_id(),
      details->ssl_cert_status()));

  // Notify our policy that we started a resource request.  Ideally, the
  // policy should have the ability to cancel the request, but we can't do
  // that yet.
  policy()->OnRequestStarted(info.get());
}

void SSLManager::DidReceiveResourceRedirect(ResourceRedirectDetails* details) {
  // TODO(abarth): Make sure our redirect behavior is correct.  If we ever see a
  //               non-HTTPS resource in the redirect chain, we want to trigger
  //               insecure content, even if the redirect chain goes back to
  //               HTTPS.  This is because the network attacker can redirect the
  //               HTTP request to https://attacker.com/payload.js.
}

void SSLManager::DidChangeSSLInternalState() {
  UpdateEntry(controller_->GetActiveEntry());
}

void SSLManager::UpdateEntry(NavigationEntry* entry) {
  // We don't always have a navigation entry to update, for example in the
  // case of the Web Inspector.
  if (!entry)
    return;

  NavigationEntry::SSLStatus original_ssl_status = entry->ssl();  // Copy!

  policy()->UpdateEntry(entry, controller_->tab_contents());

  if (!entry->ssl().Equals(original_ssl_status)) {
    NotificationService::current()->Notify(
        content::NOTIFICATION_SSL_VISIBLE_STATE_CHANGED,
        Source<NavigationController>(controller_),
        NotificationService::NoDetails());
  }
}
