// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_url_request_context_getter.h"

#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_request_interceptor.h"
#include "android_webview/browser/net/aw_network_delegate.h"
#include "android_webview/browser/net/aw_url_request_job_factory.h"
#include "android_webview/browser/net/init_native_callback.h"
#include "android_webview/common/aw_content_client.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/worker_pool.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_config_service.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/base/cache_type.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_cache.h"
#include "net/http/http_stream_factory.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/next_proto.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"

using content::BrowserThread;
using data_reduction_proxy::DataReductionProxySettings;

namespace android_webview {


namespace {

void ApplyCmdlineOverridesToURLRequestContextBuilder(
    net::URLRequestContextBuilder* builder) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kHostResolverRules)) {
    // If hostname remappings were specified on the command-line, layer these
    // rules on top of the real host resolver. This allows forwarding all
    // requests through a designated test server.
    scoped_ptr<net::MappedHostResolver> host_resolver(
        new net::MappedHostResolver(
            net::HostResolver::CreateDefaultResolver(NULL)));
    host_resolver->SetRulesFromString(
        command_line.GetSwitchValueASCII(switches::kHostResolverRules));
    builder->set_host_resolver(host_resolver.release());
  }
}

void ApplyCmdlineOverridesToNetworkSessionParams(
    net::HttpNetworkSession::Params* params) {
  int value;
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
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

void PopulateNetworkSessionParams(
    net::URLRequestContext* context,
    net::HttpNetworkSession::Params* params) {
  params->host_resolver = context->host_resolver();
  params->cert_verifier = context->cert_verifier();
  params->channel_id_service = context->channel_id_service();
  params->transport_security_state = context->transport_security_state();
  params->proxy_service = context->proxy_service();
  params->ssl_config_service = context->ssl_config_service();
  params->http_auth_handler_factory = context->http_auth_handler_factory();
  params->network_delegate = context->network_delegate();
  params->http_server_properties = context->http_server_properties();
  params->net_log = context->net_log();
  // TODO(sgurun) remove once crbug.com/329681 is fixed.
  params->next_protos = net::NextProtosSpdy31();
  params->use_alternate_protocols = true;

  ApplyCmdlineOverridesToNetworkSessionParams(params);
}

scoped_ptr<net::URLRequestJobFactory> CreateJobFactory(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  scoped_ptr<AwURLRequestJobFactory> aw_job_factory(new AwURLRequestJobFactory);
  bool set_protocol = aw_job_factory->SetProtocolHandler(
      url::kFileScheme,
      new net::FileProtocolHandler(
          content::BrowserThread::GetBlockingPool()->
              GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      url::kDataScheme, new net::DataProtocolHandler());
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      url::kBlobScheme,
      (*protocol_handlers)[url::kBlobScheme].release());
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      url::kFileSystemScheme,
      (*protocol_handlers)[url::kFileSystemScheme].release());
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      content::kChromeUIScheme,
      (*protocol_handlers)[content::kChromeUIScheme].release());
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      content::kChromeDevToolsScheme,
      (*protocol_handlers)[content::kChromeDevToolsScheme].release());
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
  scoped_ptr<net::URLRequestJobFactory> job_factory(aw_job_factory.Pass());
  for (content::URLRequestInterceptorScopedVector::reverse_iterator i =
           request_interceptors.rbegin();
       i != request_interceptors.rend();
       ++i) {
    job_factory.reset(new net::URLRequestInterceptingJobFactory(
        job_factory.Pass(), make_scoped_ptr(*i)));
  }
  request_interceptors.weak_clear();

  return job_factory.Pass();
}

}  // namespace

AwURLRequestContextGetter::AwURLRequestContextGetter(
    const base::FilePath& partition_path, net::CookieStore* cookie_store,
    scoped_ptr<data_reduction_proxy::DataReductionProxyConfigService>
        config_service)
    : partition_path_(partition_path),
      cookie_store_(cookie_store) {
  data_reduction_proxy_config_service_ = config_service.Pass();
  // CreateSystemProxyConfigService for Android must be called on main thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

AwURLRequestContextGetter::~AwURLRequestContextGetter() {
}

void AwURLRequestContextGetter::InitializeURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!url_request_context_);

  net::URLRequestContextBuilder builder;
  builder.set_user_agent(GetUserAgent());
  AwNetworkDelegate* aw_network_delegate = new AwNetworkDelegate();
  builder.set_network_delegate(aw_network_delegate);
#if !defined(DISABLE_FTP_SUPPORT)
  builder.set_ftp_enabled(false);  // Android WebView does not support ftp yet.
#endif
  if (data_reduction_proxy_config_service_.get()) {
    builder.set_proxy_config_service(
        data_reduction_proxy_config_service_.release());
  } else {
    builder.set_proxy_config_service(
        net::ProxyService::CreateSystemProxyConfigService(
            GetNetworkTaskRunner(), NULL /* Ignored on Android */ ));
  }
  builder.set_accept_language(net::HttpUtil::GenerateAcceptLanguageHeader(
      AwContentBrowserClient::GetAcceptLangsImpl()));
  ApplyCmdlineOverridesToURLRequestContextBuilder(&builder);

  url_request_context_.reset(builder.Build());
  channel_id_service_.reset(
      new net::ChannelIDService(
          new net::DefaultChannelIDStore(NULL),
          base::WorkerPool::GetTaskRunner(true)));
  url_request_context_->set_channel_id_service(channel_id_service_.get());
  // TODO(mnaganov): Fix URLRequestContextBuilder to use proper threads.
  net::HttpNetworkSession::Params network_session_params;

  PopulateNetworkSessionParams(url_request_context_.get(),
                               &network_session_params);

  net::HttpCache* main_cache = new net::HttpCache(
      network_session_params,
      new net::HttpCache::DefaultBackend(
          net::DISK_CACHE,
          net::CACHE_BACKEND_SIMPLE,
          partition_path_.Append(FILE_PATH_LITERAL("Cache")),
          20 * 1024 * 1024,  // 20M
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE)));

#if defined(SPDY_PROXY_AUTH_ORIGIN)
  AwBrowserContext* browser_context = AwBrowserContext::GetDefault();
  DCHECK(browser_context);
  DataReductionProxySettings* data_reduction_proxy_settings =
      browser_context->GetDataReductionProxySettings();
  DCHECK(data_reduction_proxy_settings);
  data_reduction_proxy_auth_request_handler_.reset(
      new data_reduction_proxy::DataReductionProxyAuthRequestHandler(
          data_reduction_proxy::kClientAndroidWebview,
          data_reduction_proxy::kAndroidWebViewProtocolVersion,
          data_reduction_proxy_settings->params(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));

  aw_network_delegate->set_data_reduction_proxy_params(
      data_reduction_proxy_settings->params());
  aw_network_delegate->set_data_reduction_proxy_auth_request_handler(
      data_reduction_proxy_auth_request_handler_.get());
#endif

  main_http_factory_.reset(main_cache);
  url_request_context_->set_http_transaction_factory(main_cache);
  url_request_context_->set_cookie_store(cookie_store_);

  job_factory_ = CreateJobFactory(&protocol_handlers_,
                                  request_interceptors_.Pass());
  url_request_context_->set_job_factory(job_factory_.get());
}

net::URLRequestContext* AwURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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

data_reduction_proxy::DataReductionProxyAuthRequestHandler*
AwURLRequestContextGetter::GetDataReductionProxyAuthRequestHandler() const {
  return data_reduction_proxy_auth_request_handler_.get();
}

}  // namespace android_webview
