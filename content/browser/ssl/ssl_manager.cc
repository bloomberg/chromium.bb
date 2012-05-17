// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_manager.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "content/browser/load_from_memory_cache_details.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/public/browser/resource_request_details.h"
#include "content/browser/renderer_host/resource_request_info_impl.h"
#include "content/browser/ssl/ssl_cert_error_handler.h"
#include "content/browser/ssl/ssl_policy.h"
#include "content/browser/ssl/ssl_request_info.h"
#include "content/browser/web_contents/navigation_entry_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/ssl_status_serialization.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/ssl_status.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::NavigationEntryImpl;
using content::ResourceDispatcherHostImpl;
using content::ResourceRedirectDetails;
using content::ResourceRequestDetails;
using content::ResourceRequestInfoImpl;
using content::SSLStatus;
using content::WebContents;

// static
void SSLManager::OnSSLCertificateError(
    base::WeakPtr<SSLErrorHandler::Delegate> delegate,
    const content::GlobalRequestID& id,
    const ResourceType::Type resource_type,
    const GURL& url,
    int render_process_id,
    int render_view_id,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DCHECK(delegate);
  DVLOG(1) << "OnSSLCertificateError() cert_error: "
           << net::MapCertStatusToNetError(ssl_info.cert_status)
           << " id: " << id.child_id << "," << id.request_id
           << " resource_type: " << resource_type
           << " url: " << url.spec()
           << " render_process_id: " << render_process_id
           << " render_view_id: " << render_view_id
           << " cert_status: " << std::hex << ssl_info.cert_status;

  // A certificate error occurred.  Construct a SSLCertErrorHandler object and
  // hand it over to the UI thread for processing.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SSLCertErrorHandler::Dispatch,
                 new SSLCertErrorHandler(delegate,
                                         id,
                                         resource_type,
                                         url,
                                         render_process_id,
                                         render_view_id,
                                         ssl_info,
                                         fatal)));
}

// static
void SSLManager::NotifySSLInternalStateChanged(
    NavigationControllerImpl* controller) {
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_SSL_INTERNAL_STATE_CHANGED,
      content::Source<content::BrowserContext>(controller->GetBrowserContext()),
      content::NotificationService::NoDetails());
}

SSLManager::SSLManager(NavigationControllerImpl* controller)
    : backend_(controller),
      policy_(new SSLPolicy(&backend_)),
      controller_(controller) {
  DCHECK(controller_);

  // Subscribe to various notifications.
  registrar_.Add(
      this, content::NOTIFICATION_RESOURCE_RESPONSE_STARTED,
      content::Source<WebContents>(controller_->web_contents()));
  registrar_.Add(
      this, content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
      content::Source<WebContents>(controller_->web_contents()));
  registrar_.Add(
      this, content::NOTIFICATION_LOAD_FROM_MEMORY_CACHE,
      content::Source<NavigationController>(controller_));
  registrar_.Add(
      this, content::NOTIFICATION_SSL_INTERNAL_STATE_CHANGED,
      content::Source<content::BrowserContext>(
          controller_->GetBrowserContext()));
}

SSLManager::~SSLManager() {
}

void SSLManager::DidCommitProvisionalLoad(
    const content::NotificationDetails& in_details) {
  content::LoadCommittedDetails* details =
      content::Details<content::LoadCommittedDetails>(in_details).ptr();

  NavigationEntryImpl* entry =
      NavigationEntryImpl::FromNavigationEntry(controller_->GetActiveEntry());

  if (details->is_main_frame) {
    if (entry) {
      // Decode the security details.
      int ssl_cert_id;
      net::CertStatus ssl_cert_status;
      int ssl_security_bits;
      int ssl_connection_status;
      content::DeserializeSecurityInfo(details->serialized_security_info,
                                       &ssl_cert_id,
                                       &ssl_cert_status,
                                       &ssl_security_bits,
                                       &ssl_connection_status);

      // We may not have an entry if this is a navigation to an initial blank
      // page. Reset the SSL information and add the new data we have.
      entry->GetSSL() = SSLStatus();
      entry->GetSSL().cert_id = ssl_cert_id;
      entry->GetSSL().cert_status = ssl_cert_status;
      entry->GetSSL().security_bits = ssl_security_bits;
      entry->GetSSL().connection_status = ssl_connection_status;
    }
  }

  UpdateEntry(entry);
}

void SSLManager::DidRunInsecureContent(const std::string& security_origin) {
  policy()->DidRunInsecureContent(
      NavigationEntryImpl::FromNavigationEntry(controller_->GetActiveEntry()),
      security_origin);
}

void SSLManager::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  // Dispatch by type.
  switch (type) {
    case content::NOTIFICATION_RESOURCE_RESPONSE_STARTED:
      DidStartResourceResponse(
          content::Details<ResourceRequestDetails>(details).ptr());
      break;
    case content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT:
      DidReceiveResourceRedirect(
          content::Details<ResourceRedirectDetails>(details).ptr());
      break;
    case content::NOTIFICATION_LOAD_FROM_MEMORY_CACHE:
      DidLoadFromMemoryCache(
          content::Details<LoadFromMemoryCacheDetails>(details).ptr());
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
      details->url,
      details->resource_type,
      details->origin_child_id,
      details->ssl_cert_id,
      details->ssl_cert_status));

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
  UpdateEntry(
      NavigationEntryImpl::FromNavigationEntry(controller_->GetActiveEntry()));
}

void SSLManager::UpdateEntry(NavigationEntryImpl* entry) {
  // We don't always have a navigation entry to update, for example in the
  // case of the Web Inspector.
  if (!entry)
    return;

  SSLStatus original_ssl_status = entry->GetSSL();  // Copy!

  policy()->UpdateEntry(entry, controller_->web_contents());

  if (!entry->GetSSL().Equals(original_ssl_status)) {
    content::NotificationService::current()->Notify(
        content::NOTIFICATION_SSL_VISIBLE_STATE_CHANGED,
        content::Source<NavigationController>(controller_),
        content::NotificationService::NoDetails());
  }
}
