// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/logging.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/extension_event_router_forwarder.h"
#include "chrome/browser/extensions/extension_proxy_api.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"

namespace {

// If the |request| failed due to problems with a proxy, forward the error to
// the proxy extension API.
void ForwardProxyErrors(net::URLRequest* request,
                        ExtensionEventRouterForwarder* event_router,
                        ProfileId profile_id) {
  if (request->status().status() == net::URLRequestStatus::FAILED) {
    switch (request->status().os_error()) {
      case net::ERR_PROXY_AUTH_UNSUPPORTED:
      case net::ERR_PROXY_CONNECTION_FAILED:
      case net::ERR_TUNNEL_CONNECTION_FAILED:
        ExtensionProxyEventRouter::GetInstance()->OnProxyError(
            event_router, profile_id, request->status().os_error());
    }
  }
}

}  // namespace

ChromeNetworkDelegate::ChromeNetworkDelegate(
    ExtensionEventRouterForwarder* event_router,
    ProfileId profile_id,
    BooleanPrefMember* enable_referrers,
    ProtocolHandlerRegistry* protocol_handler_registry)
    : event_router_(event_router),
      profile_id_(profile_id),
      enable_referrers_(enable_referrers),
      protocol_handler_registry_(protocol_handler_registry) {
  DCHECK(event_router);
  DCHECK(enable_referrers);
}

ChromeNetworkDelegate::~ChromeNetworkDelegate() {}

// static
void ChromeNetworkDelegate::InitializeReferrersEnabled(
    BooleanPrefMember* enable_referrers,
    PrefService* pref_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  enable_referrers->Init(prefs::kEnableReferrers, pref_service, NULL);
  enable_referrers->MoveToThread(BrowserThread::IO);
}

int ChromeNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    net::CompletionCallback* callback,
    GURL* new_url) {
  if (!enable_referrers_->GetValue())
    request->set_referrer(std::string());
  return ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRequest(
      profile_id_, event_router_.get(), request, callback, new_url);
}

int ChromeNetworkDelegate::OnBeforeSendHeaders(
    uint64 request_id,
    net::CompletionCallback* callback,
    net::HttpRequestHeaders* headers) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnBeforeSendHeaders(
      profile_id_, event_router_.get(), request_id, callback, headers);
}

void ChromeNetworkDelegate::OnResponseStarted(net::URLRequest* request) {
  ForwardProxyErrors(request, event_router_.get(), profile_id_);
}

void ChromeNetworkDelegate::OnReadCompleted(net::URLRequest* request,
                                            int bytes_read) {
  ForwardProxyErrors(request, event_router_.get(), profile_id_);
}

void ChromeNetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {
  ExtensionWebRequestEventRouter::GetInstance()->OnURLRequestDestroyed(
      profile_id_, request);
}

net::URLRequestJob* ChromeNetworkDelegate::OnMaybeCreateURLRequestJob(
      net::URLRequest* request) {
  if (!protocol_handler_registry_)
    return NULL;
  return protocol_handler_registry_->MaybeCreateJob(request);
}
