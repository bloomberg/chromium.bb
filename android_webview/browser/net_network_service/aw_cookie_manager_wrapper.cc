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
  // (http://crbug.com/902641).
  cookie_manager_->GetCookieList(url, cookie_options, std::move(callback));
}

}  // namespace android_webview
