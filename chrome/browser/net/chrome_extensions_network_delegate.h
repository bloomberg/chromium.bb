// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_EXTENSIONS_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_NET_CHROME_EXTENSIONS_NETWORK_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/base/network_delegate.h"

namespace extensions {
class EventRouterForwarder;
class InfoMap;
}

// ChromeExtensionsNetworkDelegate is the extensions-only portion of
// ChromeNetworkDelegate. When extensions are disabled, do nothing.
class ChromeExtensionsNetworkDelegate : public net::NetworkDelegate {
 public:
  static ChromeExtensionsNetworkDelegate* Create(
      extensions::EventRouterForwarder* event_router);

  virtual ~ChromeExtensionsNetworkDelegate();

  // Not inlined because we assign a scoped_refptr, which requires us to include
  // the header file.
  void set_extension_info_map(extensions::InfoMap* extension_info_map);

  // If |profile| is NULL or not set, events will be broadcast to all profiles,
  // otherwise they will only be sent to the specified profile.
  void set_profile(void* profile) {
    profile_ = profile;
  }

  // If the |request| failed due to problems with a proxy, forward the error to
  // the proxy extension API.
  virtual void ForwardProxyErrors(net::URLRequest* request);

  // Notifies the extensions::ProcessManager for the associated RenderFrame, if
  // any, that a request has started or stopped.
  virtual void ForwardStartRequestStatus(net::URLRequest* request);
  virtual void ForwardDoneRequestStatus(net::URLRequest* request);

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

 protected:
  ChromeExtensionsNetworkDelegate();

  void* profile_;

#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::InfoMap> extension_info_map_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsNetworkDelegate);
};

#endif  // CHROME_BROWSER_NET_CHROME_EXTENSIONS_NETWORK_DELEGATE_H_
