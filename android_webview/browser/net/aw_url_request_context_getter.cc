// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_url_request_context_getter.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_request_interceptor.h"
#include "android_webview/browser/net/aw_network_delegate.h"
#include "android_webview/browser/net/aw_url_request_job_factory.h"
#include "android_webview/browser/net/register_android_protocols.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/transport_security_state.h"
#include "net/base/transport_security_state.h"
#include "net/cookies/cookie_monster.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"

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

// Inspired by BasicURLRequestContext.
// TODO(boliu): Remove this class and use URLRequestContextBuilder when it
// has all the necessary features for Android WebView.
class AwURLRequestContext : public net::URLRequestContext {
 public:
  AwURLRequestContext();
  virtual ~AwURLRequestContext();

  virtual const std::string& GetUserAgent(const GURL& url) const OVERRIDE;

  net::URLRequestContextStorage* storage();

 private:
  net::URLRequestContextStorage storage_;

  DISALLOW_COPY_AND_ASSIGN(AwURLRequestContext);
};

AwURLRequestContext::AwURLRequestContext()
    : ALLOW_THIS_IN_INITIALIZER_LIST(storage_(this)) {
}

AwURLRequestContext::~AwURLRequestContext() {
}

const std::string& AwURLRequestContext::GetUserAgent(const GURL& url) const {
  return content::GetUserAgent(url);
}

net::URLRequestContextStorage* AwURLRequestContext::storage() {
  return &storage_;
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

  url_request_context_.reset(new AwURLRequestContext);

  url_request_context_->set_accept_language(
      net::HttpUtil::GenerateAcceptLanguageHeader(
          content::GetContentClient()->browser()->GetAcceptLangs(
              browser_context_)));

  // TODO(boliu): Values from chrome/app/resources/locale_settings_en-GB.xtb
  url_request_context_->set_accept_charset(
      net::HttpUtil::GenerateAcceptCharsetHeader("ISO-8859-1"));

  net::URLRequestContextStorage* storage =
      static_cast<AwURLRequestContext*>(url_request_context_.get())->storage();

  storage->set_network_delegate(new AwNetworkDelegate());

  storage->set_host_resolver(net::HostResolver::CreateDefaultResolver(
      content::GetContentClient()->browser()->GetNetLog()));

  // Note we do not call set_ftp_transaction_factory here since we do not
  // support ftp yet in Android WebView.

  storage->set_proxy_service(
      net::ProxyService::CreateUsingSystemProxyResolver(
          proxy_config_service_.release(),
          0,  // Use default number of threads.
          url_request_context_->net_log()));

  storage->set_ssl_config_service(new net::SSLConfigServiceDefaults);
  storage->set_http_auth_handler_factory(
      net::HttpAuthHandlerRegistryFactory::CreateDefault(
          url_request_context_->host_resolver()));
  storage->set_cookie_store(new net::CookieMonster(NULL, NULL));
  storage->set_transport_security_state(new net::TransportSecurityState());
  storage->set_http_server_properties(new net::HttpServerPropertiesImpl);
  storage->set_cert_verifier(net::CertVerifier::CreateDefault());

  net::HttpNetworkSession::Params network_session_params;
  network_session_params.host_resolver =
      url_request_context_->host_resolver();
  network_session_params.cert_verifier =
      url_request_context_->cert_verifier();
  network_session_params.transport_security_state =
      url_request_context_->transport_security_state();
  network_session_params.proxy_service =
      url_request_context_->proxy_service();
  network_session_params.ssl_config_service =
      url_request_context_->ssl_config_service();
  network_session_params.http_auth_handler_factory =
      url_request_context_->http_auth_handler_factory();
  network_session_params.network_delegate =
      url_request_context_->network_delegate();
  network_session_params.http_server_properties =
      url_request_context_->http_server_properties();
  network_session_params.net_log = url_request_context_->net_log();
  network_session_params.server_bound_cert_service =
      url_request_context_->server_bound_cert_service();
  network_session_params.ignore_certificate_errors = false;
  network_session_params.http_pipelining_enabled = false;
  // TODO(boliu): Other settings are set by commandline switches in chrome/
  // and seem to be only used in testing. Check if we need to set them here.

  scoped_ptr<net::HttpCache::BackendFactory> http_cache_backend(
        new net::HttpCache::DefaultBackend(
            net::DISK_CACHE,
            browser_context_->GetPath().Append(FILE_PATH_LITERAL("Cache")),
            10*1024*1024,  // Max cache size 10M
            BrowserThread::GetMessageLoopProxyForThread(
                BrowserThread::CACHE)));

  storage->set_http_transaction_factory(new net::HttpCache(
      network_session_params, http_cache_backend.release()));

  job_factory_.reset(new AwURLRequestJobFactory);
  bool set_protocol = job_factory_->SetProtocolHandler(
      chrome::kFileScheme, new net::FileProtocolHandler());
  DCHECK(set_protocol);
  set_protocol = job_factory_->SetProtocolHandler(
      chrome::kDataScheme, new net::DataProtocolHandler());
  DCHECK(set_protocol);
  RegisterAndroidProtocolsOnIOThread(job_factory_.get());
  job_factory_->AddInterceptor(new AwRequestInterceptor());
  storage->set_job_factory(job_factory_.get());
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
