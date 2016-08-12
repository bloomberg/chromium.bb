// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_manager.h"

#include <set>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/browser/ssl/ssl_policy.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/ssl_status_serialization.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/common/ssl_status.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

const char kSSLManagerKeyName[] = "content_ssl_manager";

class SSLManagerSet : public base::SupportsUserData::Data {
 public:
  SSLManagerSet() {
  }

  std::set<SSLManager*>& get() { return set_; }

 private:
  std::set<SSLManager*> set_;

  DISALLOW_COPY_AND_ASSIGN(SSLManagerSet);
};

void HandleSSLErrorOnUI(
    const base::Callback<WebContents*(void)>& web_contents_getter,
    const base::WeakPtr<SSLErrorHandler::Delegate>& delegate,
    const ResourceType resource_type,
    const GURL& url,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  content::WebContents* web_contents = web_contents_getter.Run();
  std::unique_ptr<SSLErrorHandler> handler(new SSLErrorHandler(
      web_contents, delegate, resource_type, url, ssl_info, fatal));

  if (!web_contents) {
    // Requests can fail to dispatch because they don't have a WebContents. See
    // https://crbug.com/86537. In this case we have to make a decision in this
    // function, so we ignore revocation check failures.
    if (net::IsCertStatusMinorError(ssl_info.cert_status)) {
      handler->ContinueRequest();
    } else {
      handler->CancelRequest();
    }
    return;
  }

  SSLManager* manager =
      static_cast<NavigationControllerImpl*>(&web_contents->GetController())
          ->ssl_manager();
  manager->policy()->OnCertError(std::move(handler));
}

}  // namespace

// static
void SSLManager::OnSSLCertificateError(
    const base::WeakPtr<SSLErrorHandler::Delegate>& delegate,
    const ResourceType resource_type,
    const GURL& url,
    const base::Callback<WebContents*(void)>& web_contents_getter,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DCHECK(delegate.get());
  DVLOG(1) << "OnSSLCertificateError() cert_error: "
           << net::MapCertStatusToNetError(ssl_info.cert_status)
           << " resource_type: " << resource_type
           << " url: " << url.spec()
           << " cert_status: " << std::hex << ssl_info.cert_status;

  // A certificate error occurred. Construct a SSLErrorHandler object
  // on the UI thread for processing.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&HandleSSLErrorOnUI, web_contents_getter, delegate,
                 resource_type, url, ssl_info, fatal));
}

// static
void SSLManager::OnSSLCertificateSubresourceError(
    const base::WeakPtr<SSLErrorHandler::Delegate>& delegate,
    const GURL& url,
    int render_process_id,
    int render_frame_id,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  OnSSLCertificateError(delegate, RESOURCE_TYPE_SUB_RESOURCE, url,
                        base::Bind(&WebContentsImpl::FromRenderFrameHostID,
                                   render_process_id, render_frame_id),
                        ssl_info, fatal);
}

// static
void SSLManager::NotifySSLInternalStateChanged(BrowserContext* context) {
  SSLManagerSet* managers = static_cast<SSLManagerSet*>(
      context->GetUserData(kSSLManagerKeyName));

  for (std::set<SSLManager*>::iterator i = managers->get().begin();
       i != managers->get().end(); ++i) {
    (*i)->UpdateEntry((*i)->controller()->GetLastCommittedEntry());
  }
}

SSLManager::SSLManager(NavigationControllerImpl* controller)
    : backend_(controller),
      policy_(new SSLPolicy(&backend_)),
      controller_(controller) {
  DCHECK(controller_);

  SSLManagerSet* managers = static_cast<SSLManagerSet*>(
      controller_->GetBrowserContext()->GetUserData(kSSLManagerKeyName));
  if (!managers) {
    managers = new SSLManagerSet;
    controller_->GetBrowserContext()->SetUserData(kSSLManagerKeyName, managers);
  }
  managers->get().insert(this);
}

SSLManager::~SSLManager() {
  SSLManagerSet* managers = static_cast<SSLManagerSet*>(
      controller_->GetBrowserContext()->GetUserData(kSSLManagerKeyName));
  managers->get().erase(this);
}

void SSLManager::DidCommitProvisionalLoad(const LoadCommittedDetails& details) {
  NavigationEntryImpl* entry = controller_->GetLastCommittedEntry();

  if (details.is_main_frame) {
    if (entry) {
      // We may not have an entry if this is a navigation to an initial blank
      // page. Add the new data we have.
      entry->GetSSL() = details.ssl_status;
    }
  }

  policy()->UpdateEntry(entry, controller_->delegate()->GetWebContents());
  // Always notify the WebContents that the SSL state changed when a
  // load is committed, in case the active navigation entry has changed.
  NotifyDidChangeVisibleSSLState();
}

void SSLManager::DidRunInsecureContent(const GURL& security_origin) {
  NavigationEntryImpl* navigation_entry = controller_->GetLastCommittedEntry();
  policy()->DidRunInsecureContent(navigation_entry, security_origin);
  UpdateEntry(navigation_entry);
}

void SSLManager::DidRunContentWithCertErrors(const GURL& security_origin) {
  NavigationEntryImpl* navigation_entry = controller_->GetLastCommittedEntry();
  policy()->DidRunContentWithCertErrors(navigation_entry, security_origin);
  UpdateEntry(navigation_entry);
}

void SSLManager::DidStartResourceResponse(
    const ResourceRequestDetails& details) {
  // Notify our policy that we started a resource request.  Ideally, the
  // policy should have the ability to cancel the request, but we can't do
  // that yet.
  policy()->OnRequestStarted(details.url, details.ssl_cert_id,
                             details.ssl_cert_status);
}

void SSLManager::DidReceiveResourceRedirect(
    const ResourceRedirectDetails& details) {
  // TODO(abarth): Make sure our redirect behavior is correct.  If we ever see a
  //               non-HTTPS resource in the redirect chain, we want to trigger
  //               insecure content, even if the redirect chain goes back to
  //               HTTPS.  This is because the network attacker can redirect the
  //               HTTP request to https://attacker.com/payload.js.
}

void SSLManager::UpdateEntry(NavigationEntryImpl* entry) {
  // We don't always have a navigation entry to update, for example in the
  // case of the Web Inspector.
  if (!entry)
    return;

  SSLStatus original_ssl_status = entry->GetSSL();  // Copy!

  policy()->UpdateEntry(entry, controller_->delegate()->GetWebContents());

  if (!entry->GetSSL().Equals(original_ssl_status))
    NotifyDidChangeVisibleSSLState();
}

void SSLManager::NotifyDidChangeVisibleSSLState() {
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(controller_->delegate()->GetWebContents());
  contents->DidChangeVisibleSSLState();
}

}  // namespace content
