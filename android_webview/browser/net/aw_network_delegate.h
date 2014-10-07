// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_AW_NETWORK_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_NET_AW_NETWORK_DELEGATE_H_

#include "base/basictypes.h"
#include "net/base/network_delegate.h"

namespace data_reduction_proxy {
class DataReductionProxyAuthRequestHandler;
class DataReductionProxyParams;
}

namespace net {
class ProxyInfo;
class URLRequest;
}

namespace android_webview {

// WebView's implementation of the NetworkDelegate.
class AwNetworkDelegate : public net::NetworkDelegate {
 public:
  AwNetworkDelegate();
  virtual ~AwNetworkDelegate();

  // Sets the |DataReductionProxySettings| object to use. If not set, the
  // NetworkDelegate will not perform any operations related to the data
  // reduction proxy.
  void set_data_reduction_proxy_params(
      data_reduction_proxy::DataReductionProxyParams* params) {
    data_reduction_proxy_params_ = params;
  }

  void set_data_reduction_proxy_auth_request_handler(
      data_reduction_proxy::DataReductionProxyAuthRequestHandler* handler) {
    data_reduction_proxy_auth_request_handler_ = handler;
  }

 private:
  // NetworkDelegate implementation.
  virtual int OnBeforeURLRequest(net::URLRequest* request,
                                 const net::CompletionCallback& callback,
                                 GURL* new_url) override;
  virtual int OnBeforeSendHeaders(net::URLRequest* request,
                                  const net::CompletionCallback& callback,
                                  net::HttpRequestHeaders* headers) override;
  virtual void OnBeforeSendProxyHeaders(
      net::URLRequest* request,
      const net::ProxyInfo& proxy_info,
      net::HttpRequestHeaders* headers) override;
  virtual void OnSendHeaders(net::URLRequest* request,
                             const net::HttpRequestHeaders& headers) override;
  virtual int OnHeadersReceived(
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) override;
  virtual void OnBeforeRedirect(net::URLRequest* request,
                                const GURL& new_location) override;
  virtual void OnResponseStarted(net::URLRequest* request) override;
  virtual void OnRawBytesRead(const net::URLRequest& request,
                              int bytes_read) override;
  virtual void OnCompleted(net::URLRequest* request, bool started) override;
  virtual void OnURLRequestDestroyed(net::URLRequest* request) override;
  virtual void OnPACScriptError(int line_number,
                                const base::string16& error) override;
  virtual net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) override;
  virtual bool OnCanGetCookies(const net::URLRequest& request,
                               const net::CookieList& cookie_list) override;
  virtual bool OnCanSetCookie(const net::URLRequest& request,
                              const std::string& cookie_line,
                              net::CookieOptions* options) override;
  virtual bool OnCanAccessFile(const net::URLRequest& request,
                               const base::FilePath& path) const override;
  virtual bool OnCanThrottleRequest(
      const net::URLRequest& request) const override;
  virtual int OnBeforeSocketStreamConnect(
      net::SocketStream* stream,
      const net::CompletionCallback& callback) override;

  // Data reduction proxy parameters object. Must outlive this.
  data_reduction_proxy::DataReductionProxyParams* data_reduction_proxy_params_;
  data_reduction_proxy::DataReductionProxyAuthRequestHandler*
  data_reduction_proxy_auth_request_handler_;

  DISALLOW_COPY_AND_ASSIGN(AwNetworkDelegate);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_AW_NETWORK_DELEGATE_H_
