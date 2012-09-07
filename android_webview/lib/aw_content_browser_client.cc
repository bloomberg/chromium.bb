// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/aw_content_browser_client.h"

#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"

namespace android_webview {

AwContentBrowserClient::AwContentBrowserClient()
    : ChromeContentBrowserClient() {
}

AwContentBrowserClient::~AwContentBrowserClient() {
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
