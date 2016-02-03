// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_url_request_context_getter.h"

#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/net/aw_http_user_agent_settings.h"
#include "android_webview/browser/net/aw_network_delegate.h"
#include "android_webview/browser/net/aw_request_interceptor.h"
#include "android_webview/browser/net/aw_url_request_job_factory.h"
#include "android_webview/browser/net/init_native_callback.h"
#include "android_webview/browser/net/token_binding_manager.h"
#include "android_webview/common/aw_content_client.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/worker_pool.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/base/cache_type.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/extras/sqlite/sqlite_channel_id_store.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_cache.h"
#include "net/http/http_stream_factory.h"
#include "net/log/net_log.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/next_proto.h"
#include "net/ssl/channel_id_service.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"

using content::BrowserThread;

namespace android_webview {


namespace {

const base::FilePath::CharType kChannelIDFilename[] = "Origin Bound Certs";

void ApplyCmdlineOverridesToHostResolver(
    net::MappedHostResolver* host_resolver) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kHostResolverRules)) {
    // If hostname remappings were specified on the command-line, layer these
    // rules on top of the real host resolver. This allows forwarding all
    // requests through a designated test server.
    host_resolver->SetRulesFromString(
        command_line.GetSwitchValueASCII(switches::kHostResolverRules));
  }
}

void ApplyCmdlineOverridesToNetworkSessionParams(
    net::URLRequestContextBuilder::HttpNetworkSessionParams* params) {
  int value;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kTestingFixedHttpPort)) {
    base::StringToInt(command_line.GetSwitchValueASCII(
        switches::kTestingFixedHttpPort), &value);
    params->testing_fixed_http_port = value;
  }
  if (command_line.HasSwitch(switches::kTestingFixedHttpsPort)) {
    base::StringToInt(command_line.GetSwitchValueASCII(
        switches::kTestingFixedHttpsPort), &value);
    params->testing_fixed_https_port = value;
  }
  if (command_line.HasSwitch(switches::kIgnoreCertificateErrors)) {
    params->ignore_certificate_errors = true;
  }
}

scoped_ptr<net::URLRequestJobFactory> CreateJobFactory(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  scoped_ptr<AwURLRequestJobFactory> aw_job_factory(new AwURLRequestJobFactory);
  // Note that the registered schemes must also be specified in
  // AwContentBrowserClient::IsHandledURL.
  bool set_protocol = aw_job_factory->SetProtocolHandler(
      url::kFileScheme,
      make_scoped_ptr(new net::FileProtocolHandler(
          content::BrowserThread::GetBlockingPool()
              ->GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN))));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      url::kDataScheme, make_scoped_ptr(new net::DataProtocolHandler()));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      url::kBlobScheme,
      make_scoped_ptr((*protocol_handlers)[url::kBlobScheme].release()));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      url::kFileSystemScheme,
      make_scoped_ptr((*protocol_handlers)[url::kFileSystemScheme].release()));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      content::kChromeUIScheme,
      make_scoped_ptr(
          (*protocol_handlers)[content::kChromeUIScheme].release()));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      content::kChromeDevToolsScheme,
      make_scoped_ptr(
          (*protocol_handlers)[content::kChromeDevToolsScheme].release()));
  DCHECK(set_protocol);
  protocol_handlers->clear();

  // Note that even though the content:// scheme handler is created here,
  // it cannot be used by child processes until access to it is granted via
  // ChildProcessSecurityPolicy::GrantScheme(). This is done in
  // AwContentBrowserClient.
  request_interceptors.push_back(
      CreateAndroidContentRequestInterceptor().release());
  request_interceptors.push_back(
      CreateAndroidAssetFileRequestInterceptor().release());
  // The AwRequestInterceptor must come after the content and asset file job
  // factories. This for WebViewClassic compatibility where it was not
  // possible to intercept resource loads to resolvable content:// and
  // file:// URIs.
  // This logical dependency is also the reason why the Content
  // URLRequestInterceptor has to be added as an interceptor rather than as a
  // ProtocolHandler.
  request_interceptors.push_back(new AwRequestInterceptor());

  // The chain of responsibility will execute the handlers in reverse to the
  // order in which the elements of the chain are created.
  scoped_ptr<net::URLRequestJobFactory> job_factory(std::move(aw_job_factory));
  for (content::URLRequestInterceptorScopedVector::reverse_iterator i =
           request_interceptors.rbegin();
       i != request_interceptors.rend();
       ++i) {
    job_factory.reset(new net::URLRequestInterceptingJobFactory(
        std::move(job_factory), make_scoped_ptr(*i)));
  }
  request_interceptors.weak_clear();

  return job_factory;
}

}  // namespace

AwURLRequestContextGetter::AwURLRequestContextGetter(
    const base::FilePath& cache_path,
    net::CookieStore* cookie_store,
    scoped_ptr<net::ProxyConfigService> config_service,
    PrefService* user_pref_service)
    : cache_path_(cache_path),
      net_log_(new net::NetLog()),
      proxy_config_service_(std::move(config_service)),
      cookie_store_(cookie_store),
      http_user_agent_settings_(new AwHttpUserAgentSettings()) {
  // CreateSystemProxyConfigService for Android must be called on main thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_refptr<base::SingleThreadTaskRunner> io_thread_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);

  auth_server_whitelist_.Init(
      prefs::kAuthServerWhitelist, user_pref_service,
      base::Bind(&AwURLRequestContextGetter::UpdateServerWhitelist,
                 base::Unretained(this)));
  auth_server_whitelist_.MoveToThread(io_thread_proxy);

  auth_android_negotiate_account_type_.Init(
      prefs::kAuthAndroidNegotiateAccountType, user_pref_service,
      base::Bind(
          &AwURLRequestContextGetter::UpdateAndroidAuthNegotiateAccountType,
          base::Unretained(this)));
  auth_android_negotiate_account_type_.MoveToThread(io_thread_proxy);
}

AwURLRequestContextGetter::~AwURLRequestContextGetter() {
}

void AwURLRequestContextGetter::InitializeURLRequestContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!url_request_context_);

  net::URLRequestContextBuilder builder;
  scoped_ptr<AwNetworkDelegate> aw_network_delegate(new AwNetworkDelegate());

  AwBrowserContext* browser_context = AwBrowserContext::GetDefault();
  DCHECK(browser_context);

  builder.set_network_delegate(
      browser_context->GetDataReductionProxyIOData()->CreateNetworkDelegate(
          std::move(aw_network_delegate),
          false /* No UMA is produced to track bypasses. */));
#if !defined(DISABLE_FTP_SUPPORT)
  builder.set_ftp_enabled(false);  // Android WebView does not support ftp yet.
#endif
  DCHECK(proxy_config_service_.get());
  scoped_ptr<net::ChannelIDService> channel_id_service;
  if (TokenBindingManager::GetInstance()->is_enabled()) {
    base::FilePath channel_id_path =
        browser_context->GetPath().Append(kChannelIDFilename);
    scoped_refptr<net::SQLiteChannelIDStore> channel_id_db;
    channel_id_db = new net::SQLiteChannelIDStore(
        channel_id_path,
        BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
            BrowserThread::GetBlockingPool()->GetSequenceToken()));

    channel_id_service.reset(new net::ChannelIDService(
        new net::DefaultChannelIDStore(channel_id_db.get()),
        base::WorkerPool::GetTaskRunner(true)));
  }

  // Android provides a local HTTP proxy that handles all the proxying.
  // Create the proxy without a resolver since we rely on this local HTTP proxy.
  // TODO(sgurun) is this behavior guaranteed through SDK?
  builder.set_proxy_service(net::ProxyService::CreateWithoutProxyResolver(
      std::move(proxy_config_service_), net_log_.get()));
  builder.set_net_log(net_log_.get());
  builder.SetCookieAndChannelIdStores(cookie_store_,
                                      std::move(channel_id_service));

  net::URLRequestContextBuilder::HttpCacheParams cache_params;
  cache_params.type =
      net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE;
  cache_params.max_size = 20 * 1024 * 1024;  // 20M
  cache_params.path = cache_path_;
  builder.EnableHttpCache(cache_params);
  builder.SetFileTaskRunner(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE));

  net::URLRequestContextBuilder::HttpNetworkSessionParams
      network_session_params;
  ApplyCmdlineOverridesToNetworkSessionParams(&network_session_params);
  builder.set_http_network_session_params(network_session_params);
  builder.SetSpdyAndQuicEnabled(true, false);

  scoped_ptr<net::MappedHostResolver> host_resolver(new net::MappedHostResolver(
      net::HostResolver::CreateDefaultResolver(nullptr)));
  ApplyCmdlineOverridesToHostResolver(host_resolver.get());
  builder.SetHttpAuthHandlerFactory(
      CreateAuthHandlerFactory(host_resolver.get()));
  builder.set_host_resolver(std::move(host_resolver));

  url_request_context_ = builder.Build();

  job_factory_ =
      CreateJobFactory(&protocol_handlers_, std::move(request_interceptors_));
  job_factory_.reset(new net::URLRequestInterceptingJobFactory(
      std::move(job_factory_),
      browser_context->GetDataReductionProxyIOData()->CreateInterceptor()));
  url_request_context_->set_job_factory(job_factory_.get());
  url_request_context_->set_http_user_agent_settings(
      http_user_agent_settings_.get());
}

net::URLRequestContext* AwURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!url_request_context_)
    InitializeURLRequestContext();

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
AwURLRequestContextGetter::GetNetworkTaskRunner() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

void AwURLRequestContextGetter::SetHandlersAndInterceptors(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  std::swap(protocol_handlers_, *protocol_handlers);
  request_interceptors_.swap(request_interceptors);
}

net::NetLog* AwURLRequestContextGetter::GetNetLog() {
  return net_log_.get();
}

void AwURLRequestContextGetter::SetKeyOnIO(const std::string& key) {
  DCHECK(AwBrowserContext::GetDefault()->GetDataReductionProxyIOData());
  AwBrowserContext::GetDefault()->GetDataReductionProxyIOData()->
      request_options()->SetKeyOnIO(key);
}

scoped_ptr<net::HttpAuthHandlerFactory>
AwURLRequestContextGetter::CreateAuthHandlerFactory(
    net::HostResolver* resolver) {
  DCHECK(resolver);

  // In Chrome this is configurable via the AuthSchemes policy. For WebView
  // there is no interest to have it available so far.
  std::vector<std::string> supported_schemes = {"basic", "digest", "negotiate"};
  http_auth_preferences_.reset(new net::HttpAuthPreferences(supported_schemes));

  UpdateServerWhitelist();
  UpdateAndroidAuthNegotiateAccountType();

  return net::HttpAuthHandlerRegistryFactory::Create(
      http_auth_preferences_.get(), resolver);
}

void AwURLRequestContextGetter::UpdateServerWhitelist() {
  http_auth_preferences_->set_server_whitelist(
      auth_server_whitelist_.GetValue());
}

void AwURLRequestContextGetter::UpdateAndroidAuthNegotiateAccountType() {
  http_auth_preferences_->set_auth_android_negotiate_account_type(
      auth_android_negotiate_account_type_.GetValue());
}

}  // namespace android_webview
