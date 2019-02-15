// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_NETWORK_SERVICE_AW_COOKIE_MANAGER_WRAPPER_H_
#define ANDROID_WEBVIEW_BROWSER_NET_NETWORK_SERVICE_AW_COOKIE_MANAGER_WRAPPER_H_

#include "base/callback.h"
#include "net/cookies/cookie_store.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace android_webview {

using GetCookieListCallback =
    base::OnceCallback<void(const std::vector<net::CanonicalCookie>&)>;

// AwCookieManagerWrapper is a thin wrapper around
// network::mojom::CookieManager. This lives on the CookieStore TaskRunner. This
// class's main responsibility is to support the CookieManager APIs before it
// receives a CookieManager from the NetworkContext, at which point it will
// switch to that CookieManager.
class AwCookieManagerWrapper {
 public:
  AwCookieManagerWrapper();
  ~AwCookieManagerWrapper();

  // Called when content layer starts up, to pass in a NetworkContextPtr for us
  // to use for Cookies APIs.
  void SetMojoCookieManager(
      network::mojom::CookieManagerPtrInfo cookie_manager_info);

  // Thin wrappers around network::mojom::CookieManager APIs.
  // TODO(ntfschr): implement the other APIs we need (http://crbug.com/902641).
  void GetCookieList(const GURL& url,
                     const net::CookieOptions& cookie_options,
                     GetCookieListCallback callback);

 private:
  // A CookieManagerPtr which is cloned from the NetworkContext's
  // CookieManagerPtr (but, lives on this thread).
  network::mojom::CookieManagerPtr cookie_manager_;

  DISALLOW_COPY_AND_ASSIGN(AwCookieManagerWrapper);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_NETWORK_SERVICE_AW_COOKIE_MANAGER_WRAPPER_H_
