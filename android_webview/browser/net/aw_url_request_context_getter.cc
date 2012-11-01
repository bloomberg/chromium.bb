// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_url_request_context_getter.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_request_interceptor.h"
#include "android_webview/browser/net/aw_network_delegate.h"
#include "android_webview/browser/net/aw_url_request_job_factory.h"
#include "android_webview/browser/net/init_native_callback.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context.h"

using content::BrowserThread;

namespace android_webview {

namespace {

class AwResourceContext : public content::ResourceContext {
 public:
  AwResourceContext(net::URLRequestContext* getter);
  virtual ~AwResourceContext();
  virtual net::HostResolver* GetHostResolver() OVERRIDE;
  virtual net::URLRequestContext* GetRequestContext() OVERRIDE;

 private:
  net::URLRequestContext* context_;  // weak

  DISALLOW_COPY_AND_ASSIGN(AwResourceContext);
};

AwResourceContext::AwResourceContext(net::URLRequestContext* context)
    : context_(context) {
}

AwResourceContext::~AwResourceContext() {
}

net::HostResolver* AwResourceContext::GetHostResolver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return context_->host_resolver();
}

net::URLRequestContext* AwResourceContext::GetRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return context_;
}

}  // namespace

AwURLRequestContextGetter::AwURLRequestContextGetter(
    AwBrowserContext* browser_context)
    : browser_context_(browser_context),
      proxy_config_service_(net::ProxyService::CreateSystemProxyConfigService(
          GetNetworkTaskRunner(),
          NULL /* Ignored on Android */)) {
  // CreateSystemProxyConfigService for Android must be called on main thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // All network stack initialization is done on the synchronous Init call when
  // the IO thread is created.
  BrowserThread::SetDelegate(BrowserThread::IO, this);
}

AwURLRequestContextGetter::~AwURLRequestContextGetter() {
  BrowserThread::SetDelegate(BrowserThread::IO, NULL);
}

void AwURLRequestContextGetter::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::URLRequestContextBuilder builder;
  builder.set_user_agent(content::GetUserAgent(GURL()));
  builder.set_network_delegate(new AwNetworkDelegate());
  builder.set_ftp_enabled(false);  // Android WebView does not support ftp yet.
  builder.set_proxy_config_service(proxy_config_service_.release());

  net::URLRequestContextBuilder::HttpCacheParams cache_params;
  cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::DISK;
  cache_params.max_size = 10 * 1024 * 1024;  // 10M
  cache_params.path =
      browser_context_->GetPath().Append(FILE_PATH_LITERAL("Cache")),
  builder.EnableHttpCache(cache_params);

  url_request_context_.reset(builder.Build());

  url_request_context_->set_accept_language(
      net::HttpUtil::GenerateAcceptLanguageHeader(
          content::GetContentClient()->browser()->GetAcceptLangs(
              browser_context_)));

  // TODO(boliu): Values from chrome/app/resources/locale_settings_en-GB.xtb
  url_request_context_->set_accept_charset(
      net::HttpUtil::GenerateAcceptCharsetHeader("ISO-8859-1"));

  job_factory_.reset(new AwURLRequestJobFactory);
  bool set_protocol = job_factory_->SetProtocolHandler(
      chrome::kFileScheme, new net::FileProtocolHandler());
  DCHECK(set_protocol);
  set_protocol = job_factory_->SetProtocolHandler(
      chrome::kDataScheme, new net::DataProtocolHandler());
  DCHECK(set_protocol);
  job_factory_->AddInterceptor(new AwRequestInterceptor());
  url_request_context_->set_job_factory(job_factory_.get());

  OnNetworkStackInitialized(url_request_context_.get(),
                            job_factory_.get());
}

content::ResourceContext* AwURLRequestContextGetter::GetResourceContext() {
  DCHECK(url_request_context_);
  if (!resource_context_)
    resource_context_.reset(new AwResourceContext(url_request_context_.get()));
  return resource_context_.get();
}

net::URLRequestContext* AwURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
AwURLRequestContextGetter::GetNetworkTaskRunner() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

}  // namespace android_webview
