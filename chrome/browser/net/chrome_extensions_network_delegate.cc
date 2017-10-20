// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_extensions_network_delegate.h"

#include <stdint.h>

#include "base/macros.h"
#include "extensions/features/features.h"
#include "net/base/net_errors.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/proxy/proxy_api.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/permissions/api_permission.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::ResourceRequestInfo;
using extensions::ExtensionWebRequestEventRouter;

namespace {

enum RequestStatus { REQUEST_STARTED, REQUEST_DONE };

// Notifies the extensions::ProcessManager that a request has started or stopped
// for a particular RenderFrame.
void NotifyEPMRequestStatus(RequestStatus status,
                            void* profile_id,
                            uint64_t request_id,
                            int process_id,
                            int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile_id))
    return;
  Profile* profile = reinterpret_cast<Profile*>(profile_id);

  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(profile);
  DCHECK(process_manager);

  // Will be NULL if the request was not issued on behalf of a renderer (e.g. a
  // system-level request).
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(process_id, render_frame_id);
  if (render_frame_host) {
    if (status == REQUEST_STARTED) {
      process_manager->OnNetworkRequestStarted(render_frame_host, request_id);
    } else if (status == REQUEST_DONE) {
      process_manager->OnNetworkRequestDone(render_frame_host, request_id);
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
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&NotifyEPMRequestStatus, status, profile_id,
                       request->identifier(), process_id, render_frame_id));
  }
}

class ChromeExtensionsNetworkDelegateImpl
    : public ChromeExtensionsNetworkDelegate {
 public:
  explicit ChromeExtensionsNetworkDelegateImpl(
      extensions::EventRouterForwarder* event_router);
  ~ChromeExtensionsNetworkDelegateImpl() override;

 private:
  // ChromeExtensionsNetworkDelegate implementation.
  void ForwardProxyErrors(net::URLRequest* request, int net_error) override;
  void ForwardStartRequestStatus(net::URLRequest* request) override;
  void ForwardDoneRequestStatus(net::URLRequest* request) override;
  int OnBeforeURLRequest(net::URLRequest* request,
                         const net::CompletionCallback& callback,
                         GURL* new_url) override;
  int OnBeforeStartTransaction(net::URLRequest* request,
                               const net::CompletionCallback& callback,
                               net::HttpRequestHeaders* headers) override;
  void OnStartTransaction(net::URLRequest* request,
                          const net::HttpRequestHeaders& headers) override;
  int OnHeadersReceived(
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) override;
  void OnBeforeRedirect(net::URLRequest* request,
                        const GURL& new_location) override;
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnCompleted(net::URLRequest* request,
                   bool started,
                   int net_error) override;
  void OnURLRequestDestroyed(net::URLRequest* request) override;
  void OnPACScriptError(int line_number, const base::string16& error) override;
  net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) override;

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
    net::URLRequest* request,
    int net_error) {
  if (net_error != net::OK) {
    switch (net_error) {
      case net::ERR_PROXY_AUTH_UNSUPPORTED:
      case net::ERR_PROXY_CONNECTION_FAILED:
      case net::ERR_TUNNEL_CONNECTION_FAILED:
        extensions::ProxyEventRouter::GetInstance()->OnProxyError(
            event_router_.get(), profile_, net_error);
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
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  const GURL& url(request->url());

  // Block top-level navigations to blob: or filesystem: URLs with extension
  // origin from non-extension processes.  See https://crbug.com/645028.
  //
  // TODO(alexmos): This check is redundant with the one in
  // ExtensionNavigationThrottle::WillStartRequest, which was introduced in
  // M56. This check is reintroduced temporarily to tighten this blocking for
  // apps with a "webview" permission on M55/54 (see https://crbug.com/656752).
  // It will be removed after it's merged.  Unlike the check in
  // ExtensionNavigationThrottle, this check is incompatible with PlzNavigate
  // and is disabled for that mode.
  bool is_nested_url = url.SchemeIsFileSystem() || url.SchemeIsBlob();
  bool is_navigation =
      info && content::IsResourceTypeFrame(info->GetResourceType());
  if (is_nested_url && is_navigation && info->IsMainFrame()) {
    // Nested conditional so we don't always pay the GURL -> Origin conversion.
    url::Origin origin = url::Origin::Create(url);
    if (origin.scheme() == extensions::kExtensionScheme &&
        !extension_info_map_->process_map().Contains(info->GetChildID()) &&
        !content::IsBrowserSideNavigationEnabled()) {
      // Relax this restriction for apps that use <webview>.  See
      // https://crbug.com/652077.
      const extensions::Extension* extension =
          extension_info_map_->extensions().GetByID(origin.host());
      bool has_webview_permission =
          extension &&
          extension->permissions_data()->HasAPIPermission(
              extensions::APIPermission::kWebView);
      // Check whether the request is coming from a <webview> guest process via
      // ChildProcessSecurityPolicy.  A guest process should have already been
      // granted permission to request |origin| when its WebContents was
      // created. See https://crbug.com/656752.
      auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
      bool from_guest =
          policy->HasSpecificPermissionForOrigin(info->GetChildID(), origin);
      if (!has_webview_permission || !from_guest) {
        // TODO(alexmos): Temporary instrumentation to find any regressions for
        // this blocking.  Remove after verifying that this is not breaking any
        // legitimate use cases.
        char origin_copy[256];
        base::strlcpy(origin_copy, origin.Serialize().c_str(),
                      arraysize(origin_copy));
        base::debug::Alias(&origin_copy);
        base::debug::Alias(&from_guest);
        base::debug::DumpWithoutCrashing();
        return net::ERR_ABORTED;
      }
    }
  }

  return ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRequest(
      profile_, extension_info_map_.get(), request, callback, new_url);
}

int ChromeExtensionsNetworkDelegateImpl::OnBeforeStartTransaction(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnBeforeSendHeaders(
      profile_, extension_info_map_.get(), request, callback, headers);
}

void ChromeExtensionsNetworkDelegateImpl::OnStartTransaction(
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
      profile_, extension_info_map_.get(), request, callback,
      original_response_headers, override_response_headers,
      allowed_unsafe_redirect_url);
}

void ChromeExtensionsNetworkDelegateImpl::OnBeforeRedirect(
    net::URLRequest* request,
    const GURL& new_location) {
  ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRedirect(
      profile_, extension_info_map_.get(), request, new_location);
}

void ChromeExtensionsNetworkDelegateImpl::OnResponseStarted(
    net::URLRequest* request,
    int net_error) {
  ExtensionWebRequestEventRouter::GetInstance()->OnResponseStarted(
      profile_, extension_info_map_.get(), request, net_error);
  ForwardProxyErrors(request, net_error);
}

void ChromeExtensionsNetworkDelegateImpl::OnCompleted(net::URLRequest* request,
                                                      bool started,
                                                      int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (net_error != net::OK) {
    ExtensionWebRequestEventRouter::GetInstance()->OnErrorOccurred(
        profile_, extension_info_map_.get(), request, started, net_error);
    return;
  }

  bool is_redirect = request->response_headers() &&
                     net::HttpResponseHeaders::IsRedirectResponseCode(
                         request->response_headers()->response_code());
  if (!is_redirect) {
    ExtensionWebRequestEventRouter::GetInstance()->OnCompleted(
        profile_, extension_info_map_.get(), request, net_error);
  }
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

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// static
ChromeExtensionsNetworkDelegate* ChromeExtensionsNetworkDelegate::Create(
    extensions::EventRouterForwarder* event_router) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_info_map_ = extension_info_map;
#endif
}

void ChromeExtensionsNetworkDelegate::ForwardProxyErrors(
    net::URLRequest* request,
    int net_error) {}

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

int ChromeExtensionsNetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  return net::OK;
}

void ChromeExtensionsNetworkDelegate::OnStartTransaction(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {}

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
    net::URLRequest* request,
    int net_error) {}

void ChromeExtensionsNetworkDelegate::OnCompleted(net::URLRequest* request,
                                                  bool started,
                                                  int net_error) {}

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
