// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_cookie_access_policy.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_thread.h"

using base::AutoLock;
using content::BrowserThread;

namespace android_webview {

namespace {
base::LazyInstance<AwCookieAccessPolicy>::Leaky g_lazy_instance;
}  // namespace

AwCookieAccessPolicy::~AwCookieAccessPolicy() {
}

AwCookieAccessPolicy::AwCookieAccessPolicy()
    : allow_access_(false) {
}

AwCookieAccessPolicy* AwCookieAccessPolicy::GetInstance() {
  return g_lazy_instance.Pointer();
}

bool AwCookieAccessPolicy::GetGlobalAllowAccess() {
  AutoLock lock(lock_);
  return allow_access_;
}

void AwCookieAccessPolicy::SetGlobalAllowAccess(bool allow) {
  AutoLock lock(lock_);
  allow_access_ = allow;
}

bool AwCookieAccessPolicy::OnCanGetCookies(const net::URLRequest& request,
                                           const net::CookieList& cookie_list) {
  return GetGlobalAllowAccess();
}

bool AwCookieAccessPolicy::OnCanSetCookie(const net::URLRequest& request,
                                          const std::string& cookie_line,
                                          net::CookieOptions* options) {
  return GetGlobalAllowAccess();
}

bool AwCookieAccessPolicy::AllowGetCookie(const GURL& url,
                                          const GURL& first_party,
                                          const net::CookieList& cookie_list,
                                          content::ResourceContext* context,
                                          int render_process_id,
                                          int render_view_id) {
  return GetGlobalAllowAccess();
}

bool AwCookieAccessPolicy::AllowSetCookie(const GURL& url,
                                          const GURL& first_party,
                                          const std::string& cookie_line,
                                          content::ResourceContext* context,
                                          int render_process_id,
                                          int render_view_id,
                                          net::CookieOptions* options) {
  return GetGlobalAllowAccess();
}

}  // namespace android_webview
