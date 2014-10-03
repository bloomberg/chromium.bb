// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_NETNETWORK_DELEGATE_H_
#define EXTENSIONS_BROWSER_EXTENSION_NETNETWORK_DELEGATE_H_

#include "extensions/browser/info_map.h"
#include "net/base/network_delegate.h"

namespace extensions {

class InfoMap;

class ExtensionNetworkDelegate : public net::NetworkDelegate {
 public:
  explicit ExtensionNetworkDelegate(
      void* browser_context, InfoMap* extension_info_map);
  virtual ~ExtensionNetworkDelegate();

  static void SetAcceptAllCookies(bool accept);

 private:
  // NetworkDelegate implementation.
  virtual int OnBeforeURLRequest(net::URLRequest* request,
                                 const net::CompletionCallback& callback,
                                 GURL* new_url) OVERRIDE;
  virtual int OnBeforeSendHeaders(net::URLRequest* request,
                                  const net::CompletionCallback& callback,
                                  net::HttpRequestHeaders* headers) OVERRIDE;
  virtual void OnSendHeaders(net::URLRequest* request,
                             const net::HttpRequestHeaders& headers) OVERRIDE;
  virtual int OnHeadersReceived(
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) OVERRIDE;
  virtual void OnBeforeRedirect(net::URLRequest* request,
                                const GURL& new_location) OVERRIDE;
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnCompleted(net::URLRequest* request, bool started) OVERRIDE;
  virtual void OnURLRequestDestroyed(net::URLRequest* request) OVERRIDE;
  virtual void OnPACScriptError(int line_number,
                                const base::string16& error) OVERRIDE;
  virtual net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) OVERRIDE;

  void* browser_context_;
  scoped_refptr<extensions::InfoMap> extension_info_map_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionNetworkDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_NETNETWORK_DELEGATE_H_
