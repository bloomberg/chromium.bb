// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/browser/url_request_context_factory.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/threading/worker_pool.h"
#include "chromecast/shell/browser/cast_http_user_agent_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/cert/cert_verifier.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream_factory.h"
#include "net/ocsp/nss_ocsp.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/next_proto.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace chromecast {
namespace shell {

namespace {

const char kCookieStoreFile[] = "Cookies";

}  // namespace

// Private classes to expose URLRequestContextGetter that call back to the
// URLRequestContextFactory to create the URLRequestContext on demand.
//
// The URLRequestContextFactory::URLRequestContextGetter class is used for both
// the system and media URLRequestCotnexts.
class URLRequestContextFactory::URLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  URLRequestContextGetter(URLRequestContextFactory* factory, bool is_media)
      : is_media_(is_media),
        factory_(factory) {
  }

  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    if (!request_context_) {
      if (is_media_) {
        request_context_.reset(factory_->CreateMediaRequestContext());
      } else {
        request_context_.reset(factory_->CreateSystemRequestContext());
#if defined(USE_NSS)
        // Set request context used by NSS for Crl requests.
        net::SetURLRequestContextForNSSHttpIO(request_context_.get());
#endif  // defined(USE_NSS)
      }
    }
    return request_context_.get();
  }

  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE {
    return content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::IO);
  }

 private:
  virtual ~URLRequestContextGetter() {}

  const bool is_media_;
  URLRequestContextFactory* const factory_;
  scoped_ptr<net::URLRequestContext> request_context_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextGetter);
};

// The URLRequestContextFactory::MainURLRequestContextGetter class is used for
// the main URLRequestContext.
class URLRequestContextFactory::MainURLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  MainURLRequestContextGetter(
      URLRequestContextFactory* factory,
      content::BrowserContext* browser_context,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors)
      : browser_context_(browser_context),
        factory_(factory),
        request_interceptors_(request_interceptors.Pass()) {
    std::swap(protocol_handlers_, *protocol_handlers);
  }

  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    if (!request_context_) {
      request_context_.reset(factory_->CreateMainRequestContext(
          browser_context_, &protocol_handlers_, request_interceptors_.Pass()));
      protocol_handlers_.clear();
    }
    return request_context_.get();
  }

  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE {
    return content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::IO);
  }

 private:
  virtual ~MainURLRequestContextGetter() {}

  content::BrowserContext* const browser_context_;
  URLRequestContextFactory* const factory_;
  content::ProtocolHandlerMap protocol_handlers_;
  content::URLRequestInterceptorScopedVector request_interceptors_;
  scoped_ptr<net::URLRequestContext> request_context_;

  DISALLOW_COPY_AND_ASSIGN(MainURLRequestContextGetter);
};

URLRequestContextFactory::URLRequestContextFactory()
    : system_dependencies_initialized_(false),
      main_dependencies_initialized_(false),
      media_dependencies_initialized_(false) {
}

URLRequestContextFactory::~URLRequestContextFactory() {
}

void URLRequestContextFactory::InitializeOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Cast http user agent settings must be initialized in UI thread
  // because it registers itself to pref notification observer which is not
  // thread safe.
  http_user_agent_settings_.reset(new CastHttpUserAgentSettings());

  // Proxy config service should be initialized in UI thread, since
  // ProxyConfigServiceDelegate on Android expects UI thread.
  proxy_config_service_.reset(net::ProxyService::CreateSystemProxyConfigService(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE)));
}

net::URLRequestContextGetter* URLRequestContextFactory::CreateMainGetter(
    content::BrowserContext* browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK(!main_getter_.get())
      << "Main URLRequestContextGetter already initialized";
  main_getter_ = new MainURLRequestContextGetter(this,
                                                 browser_context,
                                                 protocol_handlers,
                                                 request_interceptors.Pass());
  return main_getter_.get();
}

net::URLRequestContextGetter* URLRequestContextFactory::GetMainGetter() {
  CHECK(main_getter_.get());
  return main_getter_.get();
}

net::URLRequestContextGetter* URLRequestContextFactory::GetSystemGetter() {
  if (!system_getter_.get()) {
    system_getter_ = new URLRequestContextGetter(this, false);
  }
  return system_getter_.get();
}

net::URLRequestContextGetter* URLRequestContextFactory::GetMediaGetter() {
  if (!media_getter_.get()) {
    media_getter_ = new URLRequestContextGetter(this, true);
  }
  return media_getter_.get();
}

void URLRequestContextFactory::InitializeSystemContextDependencies() {
  if (system_dependencies_initialized_)
    return;

  host_resolver_ = net::HostResolver::CreateDefaultResolver(NULL);

  // TODO(lcwu): http://crbug.com/392352. For performance and security reasons,
  // a persistent (on-disk) HttpServerProperties and ChannelIDService might be
  // desirable in the future.
  channel_id_service_.reset(
      new net::ChannelIDService(new net::DefaultChannelIDStore(NULL),
                                base::WorkerPool::GetTaskRunner(true)));

  cert_verifier_.reset(net::CertVerifier::CreateDefault());

  ssl_config_service_ = new net::SSLConfigServiceDefaults;

  transport_security_state_.reset(new net::TransportSecurityState());
  http_auth_handler_factory_.reset(
      net::HttpAuthHandlerFactory::CreateDefault(host_resolver_.get()));

  http_server_properties_.reset(new net::HttpServerPropertiesImpl);

  proxy_service_.reset(net::ProxyService::CreateUsingSystemProxyResolver(
      proxy_config_service_.release(), 0, NULL));
  system_dependencies_initialized_ = true;
}

void URLRequestContextFactory::InitializeMainContextDependencies(
    net::HttpTransactionFactory* transaction_factory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  if (main_dependencies_initialized_)
    return;

  main_transaction_factory_.reset(transaction_factory);
  scoped_ptr<net::URLRequestJobFactoryImpl> job_factory(
      new net::URLRequestJobFactoryImpl());
  // Keep ProtocolHandlers added in sync with
  // CastContentBrowserClient::IsHandledURL().
  bool set_protocol = false;
  for (content::ProtocolHandlerMap::iterator it = protocol_handlers->begin();
       it != protocol_handlers->end();
       ++it) {
    set_protocol = job_factory->SetProtocolHandler(
        it->first, it->second.release());
    DCHECK(set_protocol);
  }
  set_protocol = job_factory->SetProtocolHandler(
      url::kDataScheme,
      new net::DataProtocolHandler);
  DCHECK(set_protocol);
#if defined(OS_ANDROID)
  set_protocol = job_factory->SetProtocolHandler(
      url::kFileScheme,
      new net::FileProtocolHandler(
          content::BrowserThread::GetBlockingPool()->
              GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)));
  DCHECK(set_protocol);
#endif  // defined(OS_ANDROID)

  // Set up interceptors in the reverse order.
  scoped_ptr<net::URLRequestJobFactory> top_job_factory =
      job_factory.PassAs<net::URLRequestJobFactory>();
  for (content::URLRequestInterceptorScopedVector::reverse_iterator i =
           request_interceptors.rbegin();
       i != request_interceptors.rend();
       ++i) {
    top_job_factory.reset(new net::URLRequestInterceptingJobFactory(
        top_job_factory.Pass(), make_scoped_ptr(*i)));
  }
  request_interceptors.weak_clear();

  main_job_factory_.reset(top_job_factory.release());

  main_dependencies_initialized_ = true;
}

void URLRequestContextFactory::InitializeMediaContextDependencies(
    net::HttpTransactionFactory* transaction_factory) {
  if (media_dependencies_initialized_)
    return;

  media_transaction_factory_.reset(transaction_factory);
  media_dependencies_initialized_ = true;
}

void URLRequestContextFactory::PopulateNetworkSessionParams(
    bool ignore_certificate_errors,
    net::HttpNetworkSession::Params* params) {
  params->host_resolver = host_resolver_.get();
  params->cert_verifier = cert_verifier_.get();
  params->channel_id_service = channel_id_service_.get();
  params->ssl_config_service = ssl_config_service_.get();
  params->transport_security_state = transport_security_state_.get();
  params->http_auth_handler_factory = http_auth_handler_factory_.get();
  params->http_server_properties = http_server_properties_->GetWeakPtr();
  params->ignore_certificate_errors = ignore_certificate_errors;
  params->proxy_service = proxy_service_.get();

  // TODO(lcwu): http://crbug.com/329681. Remove this once spdy is enabled
  // by default at the content level.
  params->next_protos = net::NextProtosSpdy31();
  params->use_alternate_protocols = true;
}

net::URLRequestContext* URLRequestContextFactory::CreateSystemRequestContext() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  InitializeSystemContextDependencies();
  net::HttpNetworkSession::Params system_params;
  PopulateNetworkSessionParams(false, &system_params);
  system_transaction_factory_.reset(new net::HttpNetworkLayer(
      new net::HttpNetworkSession(system_params)));
  system_job_factory_.reset(new net::URLRequestJobFactoryImpl());

  net::URLRequestContext* system_context = new net::URLRequestContext();
  system_context->set_host_resolver(host_resolver_.get());
  system_context->set_channel_id_service(channel_id_service_.get());
  system_context->set_cert_verifier(cert_verifier_.get());
  system_context->set_proxy_service(proxy_service_.get());
  system_context->set_ssl_config_service(ssl_config_service_.get());
  system_context->set_transport_security_state(
      transport_security_state_.get());
  system_context->set_http_auth_handler_factory(
      http_auth_handler_factory_.get());
  system_context->set_http_server_properties(
      http_server_properties_->GetWeakPtr());
  system_context->set_http_transaction_factory(
      system_transaction_factory_.get());
  system_context->set_http_user_agent_settings(
      http_user_agent_settings_.get());
  system_context->set_job_factory(system_job_factory_.get());
  system_context->set_cookie_store(
      content::CreateCookieStore(content::CookieStoreConfig()));
  return system_context;
}

net::URLRequestContext* URLRequestContextFactory::CreateMediaRequestContext() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(main_getter_.get())
      << "Getting MediaRequestContext before MainRequestContext";
  net::URLRequestContext* main_context = main_getter_->GetURLRequestContext();

  // Set non caching backend.
  net::HttpNetworkSession* main_session =
      main_transaction_factory_->GetSession();
  InitializeMediaContextDependencies(
      new net::HttpNetworkLayer(main_session));

  net::URLRequestContext* media_context = new net::URLRequestContext();
  media_context->CopyFrom(main_context);
  media_context->set_http_transaction_factory(
      media_transaction_factory_.get());
  return media_context;
}

net::URLRequestContext* URLRequestContextFactory::CreateMainRequestContext(
    content::BrowserContext* browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  InitializeSystemContextDependencies();

  net::HttpCache::BackendFactory* main_backend =
      net::HttpCache::DefaultBackend::InMemory(16 * 1024 * 1024);

  bool ignore_certificate_errors = false;
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kIgnoreCertificateErrors)) {
    ignore_certificate_errors = true;
  }
  net::HttpNetworkSession::Params network_session_params;
  PopulateNetworkSessionParams(ignore_certificate_errors,
                               &network_session_params);
  InitializeMainContextDependencies(
      new net::HttpCache(network_session_params, main_backend),
      protocol_handlers,
      request_interceptors.Pass());

  content::CookieStoreConfig cookie_config(
      browser_context->GetPath().Append(kCookieStoreFile),
      content::CookieStoreConfig::PERSISTANT_SESSION_COOKIES,
      NULL, NULL);
  cookie_config.background_task_runner =
      scoped_refptr<base::SequencedTaskRunner>();
  scoped_refptr<net::CookieStore> cookie_store =
      content::CreateCookieStore(cookie_config);

  net::URLRequestContext* main_context = new net::URLRequestContext();
  main_context->set_host_resolver(host_resolver_.get());
  main_context->set_channel_id_service(channel_id_service_.get());
  main_context->set_cert_verifier(cert_verifier_.get());
  main_context->set_proxy_service(proxy_service_.get());
  main_context->set_ssl_config_service(ssl_config_service_.get());
  main_context->set_transport_security_state(transport_security_state_.get());
  main_context->set_http_auth_handler_factory(
      http_auth_handler_factory_.get());
  main_context->set_http_server_properties(
      http_server_properties_->GetWeakPtr());
  main_context->set_cookie_store(cookie_store.get());
  main_context->set_http_user_agent_settings(
      http_user_agent_settings_.get());

  main_context->set_http_transaction_factory(
      main_transaction_factory_.get());
  main_context->set_job_factory(main_job_factory_.get());
  return main_context;
}

}  // namespace shell
}  // namespace chromecast
