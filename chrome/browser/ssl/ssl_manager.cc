// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_manager.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/load_from_memory_cache_details.h"
#include "chrome/browser/net/url_request_tracking.h"
#include "chrome/browser/ssl/ssl_cert_error_handler.h"
#include "chrome/browser/ssl/ssl_policy.h"
#include "chrome/browser/ssl/ssl_request_info.h"
#include "chrome/common/notification_service.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/resource_request_details.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/provisional_load_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "net/base/cert_status_flags.h"
#include "ui/base/l10n/l10n_util.h"

// static
void SSLManager::OnSSLCertificateError(ResourceDispatcherHost* rdh,
                                       net::URLRequest* request,
                                       int cert_error,
                                       net::X509Certificate* cert) {
  DVLOG(1) << "OnSSLCertificateError() cert_error: " << cert_error
           << " url: " << request->url().spec();

  ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  DCHECK(info);

  // A certificate error occurred.  Construct a SSLCertErrorHandler object and
  // hand it over to the UI thread for processing.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(new SSLCertErrorHandler(rdh,
                                                request,
                                                info->resource_type(),
                                                cert_error,
                                                cert),
                        &SSLCertErrorHandler::Dispatch));
}

// static
void SSLManager::NotifySSLInternalStateChanged() {
  NotificationService::current()->Notify(
      NotificationType::SSL_INTERNAL_STATE_CHANGED,
      NotificationService::AllSources(),
      NotificationService::NoDetails());
}

// static
std::string SSLManager::SerializeSecurityInfo(int cert_id,
                                              int cert_status,
                                              int security_bits,
                                              int ssl_connection_status) {
  Pickle pickle;
  pickle.WriteInt(cert_id);
  pickle.WriteInt(cert_status);
  pickle.WriteInt(security_bits);
  pickle.WriteInt(ssl_connection_status);
  return std::string(static_cast<const char*>(pickle.data()), pickle.size());
}

// static
bool SSLManager::DeserializeSecurityInfo(const std::string& state,
                                         int* cert_id,
                                         int* cert_status,
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
         pickle.ReadInt(&iter, cert_status) &&
         pickle.ReadInt(&iter, security_bits) &&
         pickle.ReadInt(&iter, ssl_connection_status);
}

// static
string16 SSLManager::GetEVCertName(const net::X509Certificate& cert) {
  // EV are required to have an organization name and country.
  if (cert.subject().organization_names.empty() ||
      cert.subject().country_name.empty()) {
    NOTREACHED();
    return string16();
  }

  return l10n_util::GetStringFUTF16(
      IDS_SECURE_CONNECTION_EV,
      UTF8ToUTF16(cert.subject().organization_names[0]),
      UTF8ToUTF16(cert.subject().country_name));
}

SSLManager::SSLManager(NavigationController* controller)
    : backend_(controller),
      policy_(new SSLPolicy(&backend_)),
      controller_(controller) {
  DCHECK(controller_);

  // Subscribe to various notifications.
  registrar_.Add(this, NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, NotificationType::RESOURCE_RESPONSE_STARTED,
                 Source<RenderViewHostDelegate>(controller_->tab_contents()));
  registrar_.Add(this, NotificationType::RESOURCE_RECEIVED_REDIRECT,
                 Source<RenderViewHostDelegate>(controller_->tab_contents()));
  registrar_.Add(this, NotificationType::LOAD_FROM_MEMORY_CACHE,
                 Source<NavigationController>(controller_));
  registrar_.Add(this, NotificationType::SSL_INTERNAL_STATE_CHANGED,
                 NotificationService::AllSources());
}

SSLManager::~SSLManager() {
}

void SSLManager::DidCommitProvisionalLoad(
    const NotificationDetails& in_details) {
  NavigationController::LoadCommittedDetails* details =
      Details<NavigationController::LoadCommittedDetails>(in_details).ptr();

  NavigationEntry* entry = controller_->GetActiveEntry();

  if (details->is_main_frame) {
    if (entry) {
      // Decode the security details.
      int ssl_cert_id, ssl_cert_status, ssl_security_bits,
          ssl_connection_status;
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

void SSLManager::Observe(NotificationType type,
                         const NotificationSource& source,
                         const NotificationDetails& details) {
  // Dispatch by type.
  switch (type.value) {
    case NotificationType::FAIL_PROVISIONAL_LOAD_WITH_ERROR:
      // Do nothing.
      break;
    case NotificationType::RESOURCE_RESPONSE_STARTED:
      DidStartResourceResponse(Details<ResourceRequestDetails>(details).ptr());
      break;
    case NotificationType::RESOURCE_RECEIVED_REDIRECT:
      DidReceiveResourceRedirect(
          Details<ResourceRedirectDetails>(details).ptr());
      break;
    case NotificationType::LOAD_FROM_MEMORY_CACHE:
      DidLoadFromMemoryCache(
          Details<LoadFromMemoryCacheDetails>(details).ptr());
      break;
    case NotificationType::SSL_INTERNAL_STATE_CHANGED:
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
        NotificationType::SSL_VISIBLE_STATE_CHANGED,
        Source<NavigationController>(controller_),
        NotificationService::NoDetails());
  }
}
