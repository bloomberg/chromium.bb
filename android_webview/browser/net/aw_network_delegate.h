// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_AW_NETWORK_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_NET_AW_NETWORK_DELEGATE_H_

#include "base/macros.h"
#include "net/base/network_delegate_impl.h"

namespace net {
class URLRequest;
}

namespace android_webview {

// WebView's implementation of the NetworkDelegate.
class AwNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  AwNetworkDelegate();
  ~AwNetworkDelegate() override;

 private:
  // NetworkDelegate implementation.
  int OnBeforeStartTransaction(net::URLRequest* request,
                               net::CompletionOnceCallback callback,
                               net::HttpRequestHeaders* headers) override;
  void OnStartTransaction(net::URLRequest* request,
                          const net::HttpRequestHeaders& headers) override;
  int OnHeadersReceived(
      net::URLRequest* request,
      net::CompletionOnceCallback callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) override;
  bool OnCanGetCookies(const net::URLRequest& request,
                       const net::CookieList& cookie_list) override;
  bool OnCanSetCookie(const net::URLRequest& request,
                      const net::CanonicalCookie& cookie,
                      net::CookieOptions* options) override;
  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& original_path,
                       const base::FilePath& absolute_path) const override;

  DISALLOW_COPY_AND_ASSIGN(AwNetworkDelegate);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_AW_NETWORK_DELEGATE_H_
