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
#include "net/http/http_cache.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/protocol_intercept_job_factory.h"
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
  builder.set_accept_language(net::HttpUtil::GenerateAcceptLanguageHeader(
      content::GetContentClient()->browser()->GetAcceptLangs(
          browser_context_)));

  builder.set_accept_charset(
      net::HttpUtil::GenerateAcceptCharsetHeader("utf-8"));

  url_request_context_.reset(builder.Build());

  scoped_ptr<AwURLRequestJobFactory> job_factory(new AwURLRequestJobFactory);
  bool set_protocol = job_factory->SetProtocolHandler(
      chrome::kFileScheme, new net::FileProtocolHandler());
  DCHECK(set_protocol);
  set_protocol = job_factory->SetProtocolHandler(
      chrome::kDataScheme, new net::DataProtocolHandler());
  DCHECK(set_protocol);

  // TODO(mnaganov): Fix URLRequestContextBuilder to use proper threads.
  net::HttpNetworkSession::Params network_session_params;
  PopulateNetworkSessionParams(&network_session_params);
  net::HttpCache* main_cache = new net::HttpCache(
      network_session_params,
      new net::HttpCache::DefaultBackend(
          net::DISK_CACHE,
          browser_context_->GetPath().Append(FILE_PATH_LITERAL("Cache")),
          10 * 1024 * 1024,  // 10M
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE)));
  main_http_factory_.reset(main_cache);
  url_request_context_->set_http_transaction_factory(main_cache);

  job_factory_ = CreateAndroidJobFactoryAndCookieMonster(
      url_request_context_.get(), job_factory.Pass());
  job_factory_.reset(new net::ProtocolInterceptJobFactory(
      job_factory_.Pass(),
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>(
          new AwRequestInterceptor())));
  url_request_context_->set_job_factory(job_factory_.get());
}

void AwURLRequestContextGetter::PopulateNetworkSessionParams(
    net::HttpNetworkSession::Params* params) {
  net::URLRequestContext* context = url_request_context_.get();
  params->host_resolver = context->host_resolver();
  params->cert_verifier = context->cert_verifier();
  params->server_bound_cert_service = context->server_bound_cert_service();
  params->transport_security_state = context->transport_security_state();
  params->proxy_service = context->proxy_service();
  params->ssl_config_service = context->ssl_config_service();
  params->http_auth_handler_factory = context->http_auth_handler_factory();
  params->network_delegate = context->network_delegate();
  params->http_server_properties = context->http_server_properties();
  params->net_log = context->net_log();
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
