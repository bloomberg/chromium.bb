// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_cookie_access_policy.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"

using base::AutoLock;
using content::BrowserThread;
using net::StaticCookiePolicy;

namespace android_webview {

namespace {
base::LazyInstance<AwCookieAccessPolicy>::Leaky g_lazy_instance;
}  // namespace

AwCookieAccessPolicy::~AwCookieAccessPolicy() {
}

AwCookieAccessPolicy::AwCookieAccessPolicy()
    : allow_access_(true),
      allow_third_party_access_(true) {
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

bool AwCookieAccessPolicy::GetThirdPartyAllowAccess() {
  AutoLock lock(lock_);
  return allow_third_party_access_;
}

void AwCookieAccessPolicy::SetThirdPartyAllowAccess(bool allow) {
  AutoLock lock(lock_);
  allow_third_party_access_ = allow;
}

bool AwCookieAccessPolicy::OnCanGetCookies(const net::URLRequest& request,
                                           const net::CookieList& cookie_list) {
  return AllowGet(request.url(), request.first_party_for_cookies());
}

bool AwCookieAccessPolicy::OnCanSetCookie(const net::URLRequest& request,
                                          const std::string& cookie_line,
                                          net::CookieOptions* options) {
  return AllowSet(request.url(), request.first_party_for_cookies());
}

bool AwCookieAccessPolicy::AllowGetCookie(const GURL& url,
                                          const GURL& first_party,
                                          const net::CookieList& cookie_list,
                                          content::ResourceContext* context,
                                          int render_process_id,
                                          int render_frame_id) {
  return AllowGet(url, first_party);
}

bool AwCookieAccessPolicy::AllowSetCookie(const GURL& url,
                                          const GURL& first_party,
                                          const std::string& cookie_line,
                                          content::ResourceContext* context,
                                          int render_process_id,
                                          int render_frame_id,
                                          net::CookieOptions* options) {
  return AllowSet(url, first_party);
}

StaticCookiePolicy::Type AwCookieAccessPolicy::GetPolicy() {
  if (!GetGlobalAllowAccess()) {
    return StaticCookiePolicy::BLOCK_ALL_COOKIES;
  } else if (!GetThirdPartyAllowAccess()) {
    return StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES;
  }
  return StaticCookiePolicy::ALLOW_ALL_COOKIES;
}

bool AwCookieAccessPolicy::AllowSet(const GURL& url, const GURL& first_party) {
  return StaticCookiePolicy(GetPolicy()).CanSetCookie(url, first_party)
      == net::OK;
}

bool AwCookieAccessPolicy::AllowGet(const GURL& url, const GURL& first_party) {
  return StaticCookiePolicy(GetPolicy()).CanGetCookies(url, first_party)
      == net::OK;
}

}  // namespace android_webview
