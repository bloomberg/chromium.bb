// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_url_request_context_getter.h"

#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/net/aw_http_user_agent_settings.h"
#include "android_webview/browser/net/aw_network_delegate.h"
#include "android_webview/browser/net/aw_request_interceptor.h"
#include "android_webview/browser/net/aw_url_request_job_factory.h"
#include "android_webview/browser/net/init_native_callback.h"
#include "android_webview/common/aw_content_client.h"
#include "base/bind.h"
#include "base/command_line.h"
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

void ApplyCmdlineOverridesToURLRequestContextBuilder(
    net::URLRequestContextBuilder* builder) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kHostResolverRules)) {
    // If hostname remappings were specified on the command-line, layer these
    // rules on top of the real host resolver. This allows forwarding all
    // requests through a designated test server.
    scoped_ptr<net::MappedHostResolver> host_resolver(
        new net::MappedHostResolver(
            net::HostResolver::CreateDefaultResolver(NULL)));
    host_resolver->SetRulesFromString(
        command_line.GetSwitchValueASCII(switches::kHostResolverRules));
    builder->set_host_resolver(host_resolver.Pass());
  }
}

void ApplyCmdlineOverridesToNetworkSessionParams(
    net::HttpNetworkSession::Params* params) {
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
  params->use_alternative_services = true;

  ApplyCmdlineOverridesToNetworkSessionParams(params);
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
    const base::FilePath& cache_path, net::CookieStore* cookie_store,
    scoped_ptr<net::ProxyConfigService> config_service)
    : cache_path_(cache_path),
      net_log_(new net::NetLog()),
      proxy_config_service_(config_service.Pass()),
      cookie_store_(cookie_store),
      http_user_agent_settings_(new AwHttpUserAgentSettings()) {
  // CreateSystemProxyConfigService for Android must be called on main thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
      browser_context->GetDataReductionProxyIOData()
          ->CreateNetworkDelegate(
              aw_network_delegate.Pass(),
              false /* No UMA is produced to track bypasses. */)
          .Pass());
#if !defined(DISABLE_FTP_SUPPORT)
  builder.set_ftp_enabled(false);  // Android WebView does not support ftp yet.
#endif
  DCHECK(proxy_config_service_.get());
  // Android provides a local HTTP proxy that handles all the proxying.
  // Create the proxy without a resolver since we rely on this local HTTP proxy.
  // TODO(sgurun) is this behavior guaranteed through SDK?
  builder.set_proxy_service(net::ProxyService::CreateWithoutProxyResolver(
      proxy_config_service_.Pass(), net_log_.get()));
  builder.set_net_log(net_log_.get());
  builder.SetCookieAndChannelIdStores(cookie_store_, NULL);
  ApplyCmdlineOverridesToURLRequestContextBuilder(&builder);

  url_request_context_ = builder.Build().Pass();
  // TODO(mnaganov): Fix URLRequestContextBuilder to use proper threads.
  net::HttpNetworkSession::Params network_session_params;

  PopulateNetworkSessionParams(url_request_context_.get(),
                               &network_session_params);

  http_network_session_.reset(
      new net::HttpNetworkSession(network_session_params));
  main_http_factory_.reset(new net::HttpCache(
      http_network_session_.get(),
      make_scoped_ptr(new net::HttpCache::DefaultBackend(
          net::DISK_CACHE,
          net::CACHE_BACKEND_SIMPLE,
          cache_path_,
          20 * 1024 * 1024,  // 20M
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE))),
      true /* set_up_quic_server_info */));

  url_request_context_->set_http_transaction_factory(main_http_factory_.get());

  job_factory_ = CreateJobFactory(&protocol_handlers_,
                                  request_interceptors_.Pass());

  job_factory_.reset(new net::URLRequestInterceptingJobFactory(
      job_factory_.Pass(),
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

}  // namespace android_webview
