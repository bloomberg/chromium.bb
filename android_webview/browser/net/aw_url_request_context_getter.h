// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_CONTEXT_GETTER_H_
#define ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_CONTEXT_GETTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_thread_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "net/http/http_network_session.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {
class HttpTransactionFactory;
class ProxyConfigService;
class URLRequestContext;
class URLRequestJobFactory;
}

namespace android_webview {

class AwBrowserContext;
class AwNetworkDelegate;

class AwURLRequestContextGetter : public net::URLRequestContextGetter,
                                  public content::BrowserThreadDelegate {
 public:
  explicit AwURLRequestContextGetter(AwBrowserContext* browser_context);

  void InitializeOnNetworkThread();

  // content::BrowserThreadDelegate implementation.
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE {}

  // net::URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE;

 private:
  friend class AwBrowserContext;

  virtual ~AwURLRequestContextGetter();

  // Prior to GetURLRequestContext() being called, SetProtocolHandlers() is
  // called to hand over the ProtocolHandlers that GetURLRequestContext() will
  // later install into |job_factory_|.  This ordering is enforced by having
  // AwBrowserContext::CreateRequestContext() call SetProtocolHandlers().
  // SetProtocolHandlers() is necessary because the ProtocolHandlers are created
  // on the UI thread while |job_factory_| must be created on the IO thread.
  void SetProtocolHandlers(content::ProtocolHandlerMap* protocol_handlers);

  void PopulateNetworkSessionParams(net::HttpNetworkSession::Params* params);

  AwBrowserContext* browser_context_;  // weak
  scoped_ptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_ptr<net::URLRequestJobFactory> job_factory_;
  scoped_ptr<net::HttpTransactionFactory> main_http_factory_;

  // ProtocolHandlers are stored here between SetProtocolHandlers() and the
  // first GetURLRequestContext() call.
  content::ProtocolHandlerMap protocol_handlers_;

  DISALLOW_COPY_AND_ASSIGN(AwURLRequestContextGetter);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_CONTEXT_GETTER_H_
