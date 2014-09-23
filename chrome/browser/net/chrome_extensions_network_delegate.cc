// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_extensions_network_delegate.h"

#include "net/base/net_errors.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/proxy/proxy_api.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/process_manager.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::ResourceRequestInfo;

namespace {

enum RequestStatus { REQUEST_STARTED, REQUEST_DONE };

// Notifies the extensions::ProcessManager that a request has started or stopped
// for a particular RenderFrame.
void NotifyEPMRequestStatus(RequestStatus status,
                            void* profile_id,
                            int process_id,
                            int render_frame_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  extensions::ProcessManager* process_manager =
      extensions::ExtensionSystem::Get(profile)->process_manager();
  // This may be NULL in unit tests.
  if (!process_manager)
    return;

  // Will be NULL if the request was not issued on behalf of a renderer (e.g. a
  // system-level request).
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(process_id, render_frame_id);
  if (render_frame_host) {
    if (status == REQUEST_STARTED) {
      process_manager->OnNetworkRequestStarted(render_frame_host);
    } else if (status == REQUEST_DONE) {
      process_manager->OnNetworkRequestDone(render_frame_host);
    } else {
      NOTREACHED();
    }
  }
}

void ForwardRequestStatus(
    RequestStatus status, net::URLRequest* request, void* profile_id) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (!info)
    return;

  if (status == REQUEST_STARTED && request->url_chain().size() > 1) {
    // It's a redirect, this request has already been counted.
    return;
  }

  int process_id, render_frame_id;
  if (info->GetAssociatedRenderFrame(&process_id, &render_frame_id)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&NotifyEPMRequestStatus,
                   status, profile_id, process_id, render_frame_id));
  }
}

class ChromeExtensionsNetworkDelegateImpl
    : public ChromeExtensionsNetworkDelegate {
 public:
  explicit ChromeExtensionsNetworkDelegateImpl(
      extensions::EventRouterForwarder* event_router);
  virtual ~ChromeExtensionsNetworkDelegateImpl();

 private:
  // ChromeExtensionsNetworkDelegate implementation.
  virtual void ForwardProxyErrors(net::URLRequest* request) OVERRIDE;
  virtual void ForwardStartRequestStatus(net::URLRequest* request) OVERRIDE;
  virtual void ForwardDoneRequestStatus(net::URLRequest* request) OVERRIDE;
  virtual int OnBeforeURLRequest(net::URLRequest* request,
                                 const net::CompletionCallback& callback,
                                 GURL* new_url) OVERRIDE;
  virtual int OnBeforeSendHeaders(net::URLRequest* request,
                                  const net::CompletionCallback& callback,
                                  net::HttpRequestHeaders* headers) OVERRIDE;
  virtual void OnSendHeaders(net::URLRequest* request,
                             const net::HttpRequestHeaders& headers) OVERRIDE;
  virtual int OnHeadersReceived(
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) OVERRIDE;
  virtual void OnBeforeRedirect(net::URLRequest* request,
                                const GURL& new_location) OVERRIDE;
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnCompleted(net::URLRequest* request, bool started) OVERRIDE;
  virtual void OnURLRequestDestroyed(net::URLRequest* request) OVERRIDE;
  virtual void OnPACScriptError(int line_number,
                                const base::string16& error) OVERRIDE;
  virtual net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) OVERRIDE;

  scoped_refptr<extensions::EventRouterForwarder> event_router_;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsNetworkDelegateImpl);
};

ChromeExtensionsNetworkDelegateImpl::ChromeExtensionsNetworkDelegateImpl(
    extensions::EventRouterForwarder* event_router) {
  DCHECK(event_router);
  event_router_ = event_router;
}

ChromeExtensionsNetworkDelegateImpl::~ChromeExtensionsNetworkDelegateImpl() {}

void ChromeExtensionsNetworkDelegateImpl::ForwardProxyErrors(
    net::URLRequest* request) {
  if (request->status().status() == net::URLRequestStatus::FAILED) {
    switch (request->status().error()) {
      case net::ERR_PROXY_AUTH_UNSUPPORTED:
      case net::ERR_PROXY_CONNECTION_FAILED:
      case net::ERR_TUNNEL_CONNECTION_FAILED:
        extensions::ProxyEventRouter::GetInstance()->OnProxyError(
            event_router_.get(), profile_, request->status().error());
    }
  }
}

void ChromeExtensionsNetworkDelegateImpl::ForwardStartRequestStatus(
    net::URLRequest* request) {
  ForwardRequestStatus(REQUEST_STARTED, request, profile_);
}

void ChromeExtensionsNetworkDelegateImpl::ForwardDoneRequestStatus(
    net::URLRequest* request) {
  ForwardRequestStatus(REQUEST_DONE, request, profile_);
}

int ChromeExtensionsNetworkDelegateImpl::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRequest(
      profile_, extension_info_map_.get(), request, callback, new_url);
}

int ChromeExtensionsNetworkDelegateImpl::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnBeforeSendHeaders(
      profile_, extension_info_map_.get(), request, callback, headers);
}

void ChromeExtensionsNetworkDelegateImpl::OnSendHeaders(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
  ExtensionWebRequestEventRouter::GetInstance()->OnSendHeaders(
      profile_, extension_info_map_.get(), request, headers);
}

int ChromeExtensionsNetworkDelegateImpl::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnHeadersReceived(
      profile_,
      extension_info_map_.get(),
      request,
      callback,
      original_response_headers,
      override_response_headers,
      allowed_unsafe_redirect_url);
}

void ChromeExtensionsNetworkDelegateImpl::OnBeforeRedirect(
    net::URLRequest* request,
    const GURL& new_location) {
  ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRedirect(
      profile_, extension_info_map_.get(), request, new_location);
}


void ChromeExtensionsNetworkDelegateImpl::OnResponseStarted(
    net::URLRequest* request) {
  ExtensionWebRequestEventRouter::GetInstance()->OnResponseStarted(
      profile_, extension_info_map_.get(), request);
  ForwardProxyErrors(request);
}

void ChromeExtensionsNetworkDelegateImpl::OnCompleted(
    net::URLRequest* request,
    bool started) {
  if (request->status().status() == net::URLRequestStatus::SUCCESS) {
    bool is_redirect = request->response_headers() &&
        net::HttpResponseHeaders::IsRedirectResponseCode(
            request->response_headers()->response_code());
    if (!is_redirect) {
      ExtensionWebRequestEventRouter::GetInstance()->OnCompleted(
          profile_, extension_info_map_.get(), request);
    }
    return;
  }

  if (request->status().status() == net::URLRequestStatus::FAILED ||
      request->status().status() == net::URLRequestStatus::CANCELED) {
    ExtensionWebRequestEventRouter::GetInstance()->OnErrorOccurred(
        profile_, extension_info_map_.get(), request, started);
    return;
  }

  NOTREACHED();
}

void ChromeExtensionsNetworkDelegateImpl::OnURLRequestDestroyed(
    net::URLRequest* request) {
  ExtensionWebRequestEventRouter::GetInstance()->OnURLRequestDestroyed(
      profile_, request);
}

void ChromeExtensionsNetworkDelegateImpl::OnPACScriptError(
    int line_number,
    const base::string16& error) {
  extensions::ProxyEventRouter::GetInstance()->OnPACScriptError(
      event_router_.get(), profile_, line_number, error);
}

net::NetworkDelegate::AuthRequiredResponse
ChromeExtensionsNetworkDelegateImpl::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    net::AuthCredentials* credentials) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnAuthRequired(
      profile_, extension_info_map_.get(), request, auth_info, callback,
      credentials);
}

}  // namespace

#endif  // defined(ENABLE_EXTENSIONS)

// static
ChromeExtensionsNetworkDelegate* ChromeExtensionsNetworkDelegate::Create(
    extensions::EventRouterForwarder* event_router) {
#if defined(ENABLE_EXTENSIONS)
  return new ChromeExtensionsNetworkDelegateImpl(event_router);
#else
  return new ChromeExtensionsNetworkDelegate();
#endif
}

ChromeExtensionsNetworkDelegate::ChromeExtensionsNetworkDelegate()
    : profile_(NULL) {
}

ChromeExtensionsNetworkDelegate::~ChromeExtensionsNetworkDelegate() {}

void ChromeExtensionsNetworkDelegate::set_extension_info_map(
    extensions::InfoMap* extension_info_map) {
#if defined(ENABLE_EXTENSIONS)
  extension_info_map_ = extension_info_map;
#endif
}

void ChromeExtensionsNetworkDelegate::ForwardProxyErrors(
    net::URLRequest* request) {
}

void ChromeExtensionsNetworkDelegate::ForwardStartRequestStatus(
    net::URLRequest* request) {
}

void ChromeExtensionsNetworkDelegate::ForwardDoneRequestStatus(
    net::URLRequest* request) {
}

int ChromeExtensionsNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  return net::OK;
}

int ChromeExtensionsNetworkDelegate::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  return net::OK;
}

void ChromeExtensionsNetworkDelegate::OnSendHeaders(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
}

int ChromeExtensionsNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  return net::OK;
}

void ChromeExtensionsNetworkDelegate::OnBeforeRedirect(
    net::URLRequest* request,
    const GURL& new_location) {
}


void ChromeExtensionsNetworkDelegate::OnResponseStarted(
    net::URLRequest* request) {
}

void ChromeExtensionsNetworkDelegate::OnCompleted(
    net::URLRequest* request,
    bool started) {
}

void ChromeExtensionsNetworkDelegate::OnURLRequestDestroyed(
    net::URLRequest* request) {
}

void ChromeExtensionsNetworkDelegate::OnPACScriptError(
    int line_number,
    const base::string16& error) {
}

net::NetworkDelegate::AuthRequiredResponse
ChromeExtensionsNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    net::AuthCredentials* credentials) {
  return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
}
