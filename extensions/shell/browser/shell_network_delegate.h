// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_NETNETWORK_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_NETNETWORK_DELEGATE_H_

#include "extensions/browser/info_map.h"
#include "net/base/network_delegate.h"

namespace extensions {

class InfoMap;

class ShellNetworkDelegate : public net::NetworkDelegate {
 public:
  ShellNetworkDelegate(void* browser_context, InfoMap* extension_info_map);
  virtual ~ShellNetworkDelegate();

  static void SetAcceptAllCookies(bool accept);

 private:
  // NetworkDelegate implementation.
  virtual int OnBeforeURLRequest(net::URLRequest* request,
                                 const net::CompletionCallback& callback,
                                 GURL* new_url) override;
  virtual int OnBeforeSendHeaders(net::URLRequest* request,
                                  const net::CompletionCallback& callback,
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
  virtual void OnCompleted(net::URLRequest* request, bool started) override;
  virtual void OnURLRequestDestroyed(net::URLRequest* request) override;
  virtual void OnPACScriptError(int line_number,
                                const base::string16& error) override;
  virtual net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) override;

  void* browser_context_;
  scoped_refptr<extensions::InfoMap> extension_info_map_;

  DISALLOW_COPY_AND_ASSIGN(ShellNetworkDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_NETNETWORK_DELEGATE_H_
