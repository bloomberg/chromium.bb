// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_CONTEXT_GETTER_H_
#define ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_CONTEXT_GETTER_H_

#include "content/public/browser/browser_thread_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {
class ResourceContext;
}

namespace net {
class ProxyConfigService;
class URLRequestContext;
}

namespace android_webview {

class AwBrowserContext;
class AwNetworkDelegate;

class AwURLRequestContextGetter : public net::URLRequestContextGetter,
                                  public content::BrowserThreadDelegate {
 public:
  AwURLRequestContextGetter(AwBrowserContext* browser_context);

  void InitializeOnNetworkThread();

  content::ResourceContext* GetResourceContext();

  // content::BrowserThreadDelegate implementation.
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE {}

  // net::URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE;

 private:
  virtual ~AwURLRequestContextGetter();

  AwBrowserContext* browser_context_;  // weak
  scoped_ptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_ptr<content::ResourceContext> resource_context_;

  DISALLOW_COPY_AND_ASSIGN(AwURLRequestContextGetter);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_CONTEXT_GETTER_H_
