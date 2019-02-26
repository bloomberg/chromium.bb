// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net_network_service/aw_cookie_manager_wrapper.h"

#include "base/feature_list.h"
#include "services/network/public/cpp/features.h"

namespace android_webview {

AwCookieManagerWrapper::AwCookieManagerWrapper() {
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
}

AwCookieManagerWrapper::~AwCookieManagerWrapper() {}

void AwCookieManagerWrapper::SetMojoCookieManager(
    network::mojom::CookieManagerPtrInfo cookie_manager_info) {
  DCHECK(!cookie_manager_.is_bound());
  cookie_manager_.Bind(std::move(cookie_manager_info));
}

void AwCookieManagerWrapper::GetCookieList(
    const GURL& url,
    const net::CookieOptions& cookie_options,
    GetCookieListCallback callback) {
  // TODO(ntfschr): handle the case where content layer isn't initialized yet
  // (http://crbug.com/933461).
  cookie_manager_->GetCookieList(url, cookie_options, std::move(callback));
}

void AwCookieManagerWrapper::SetCanonicalCookie(
    const net::CanonicalCookie& cc,
    std::string source_scheme,
    bool modify_http_only,
    SetCanonicalCookieCallback callback) {
  // TODO(ntfschr): handle the case where content layer isn't initialized yet
  // (http://crbug.com/933461).
  cookie_manager_->SetCanonicalCookie(cc, std::move(source_scheme),
                                      modify_http_only, std::move(callback));
}

void AwCookieManagerWrapper::DeleteCookies(
    network::mojom::CookieDeletionFilterPtr filter,
    DeleteCookiesCallback callback) {
  // TODO(ntfschr): handle the case where content layer isn't initialized yet
  // (http://crbug.com/933461).
  cookie_manager_->DeleteCookies(std::move(filter), std::move(callback));
}

void AwCookieManagerWrapper::GetAllCookies(GetCookieListCallback callback) {
  // TODO(ntfschr): handle the case where content layer isn't initialized yet
  // (http://crbug.com/933461).
  cookie_manager_->GetAllCookies(std::move(callback));
}

void AwCookieManagerWrapper::FlushCookieStore(
    FlushCookieStoreCallback callback) {
  // TODO(ntfschr): handle the case where content layer isn't initialized yet
  // (http://crbug.com/933461).
  cookie_manager_->FlushCookieStore(std::move(callback));
}

}  // namespace android_webview
