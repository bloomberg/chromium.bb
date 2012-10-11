// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/aw_content_browser_client.h"

#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"
#include "android_webview/common/url_constants.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"

namespace android_webview {

AwContentBrowserClient::AwContentBrowserClient()
    : ChromeContentBrowserClient() {
}

AwContentBrowserClient::~AwContentBrowserClient() {
}

void AwContentBrowserClient::RenderProcessHostCreated(
    content::RenderProcessHost* host) {
  ChromeContentBrowserClient::RenderProcessHostCreated(host);

  // If WebView becomes multi-process capable, this may be insecure.
  // More benefit can be derived from the ChildProcessSecurotyPolicy by
  // deferring the GrantScheme calls until we know that a given child process
  // really does need that priviledge. Check here to ensure we rethink this
  // when the time comes.
  CHECK(content::RenderProcessHost::run_renderer_in_process());

  // Grant content: and file: scheme to the whole process, since we impose
  // per-view access checks.
  content::ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
      host->GetID(), android_webview::kContentScheme);
  content::ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
      host->GetID(), chrome::kFileScheme);
}

void AwContentBrowserClient::ResourceDispatcherHostCreated() {
  ChromeContentBrowserClient::ResourceDispatcherHostCreated();
  AwResourceDispatcherHostDelegate::ResourceDispatcherHostCreated();
}

bool AwContentBrowserClient::AllowGetCookie(const GURL& url,
                                            const GURL& first_party,
                                            const net::CookieList& cookie_list,
                                            content::ResourceContext* context,
                                            int render_process_id,
                                            int render_view_id) {
  // Not base-calling into ChromeContentBrowserClient as we are not dependent
  // on chrome/ for any cookie policy decisions.
  return AwCookieAccessPolicy::GetInstance()->AllowGetCookie(url,
                                                             first_party,
                                                             cookie_list,
                                                             context,
                                                             render_process_id,
                                                             render_view_id);
}

bool AwContentBrowserClient::AllowSetCookie(const GURL& url,
                                            const GURL& first_party,
                                            const std::string& cookie_line,
                                            content::ResourceContext* context,
                                            int render_process_id,
                                            int render_view_id,
                                            net::CookieOptions* options) {
  // Not base-calling into ChromeContentBrowserClient as we are not dependent
  // on chrome/ for any cookie policy decisions.
  return AwCookieAccessPolicy::GetInstance()->AllowSetCookie(url,
                                                             first_party,
                                                             cookie_line,
                                                             context,
                                                             render_process_id,
                                                             render_view_id,
                                                             options);
}

}  // namespace android_webview
