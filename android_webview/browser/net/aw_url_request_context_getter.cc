// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_url_request_context_getter.h"

#include <vector>

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_request_interceptor.h"
#include "android_webview/browser/net/aw_network_delegate.h"
#include "android_webview/browser/net/aw_url_request_job_factory.h"
#include "android_webview/browser/net/init_native_callback.h"
#include "android_webview/common/aw_switches.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
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
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/protocol_intercept_job_factory.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context.h"

using content::BrowserThread;

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
}

void PopulateNetworkSessionParams(
    net::URLRequestContext* context,
    net::HttpNetworkSession::Params* params) {
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
  ApplyCmdlineOverridesToNetworkSessionParams(params);
}

scoped_ptr<net::URLRequestJobFactory> CreateJobFactory(
    content::ProtocolHandlerMap* protocol_handlers) {
  scoped_ptr<AwURLRequestJobFactory> aw_job_factory(new AwURLRequestJobFactory);
  bool set_protocol = aw_job_factory->SetProtocolHandler(
      content::kFileScheme,
      new net::FileProtocolHandler(
          content::BrowserThread::GetBlockingPool()->
              GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      content::kDataScheme, new net::DataProtocolHandler());
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      content::kBlobScheme,
      (*protocol_handlers)[content::kBlobScheme].release());
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      content::kFileSystemScheme,
      (*protocol_handlers)[content::kFileSystemScheme].release());
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

  // Create a chain of URLRequestJobFactories. The handlers will be invoked
  // in the order in which they appear in the protocol_handlers vector.
  typedef std::vector<net::URLRequestJobFactory::ProtocolHandler*>
      ProtocolHandlerVector;
  ProtocolHandlerVector protocol_interceptors;

  // Note that even though the content:// scheme handler is created here,
  // it cannot be used by child processes until access to it is granted via
  // ChildProcessSecurityPolicy::GrantScheme(). This is done in
  // AwContentBrowserClient.
  protocol_interceptors.push_back(
      CreateAndroidContentProtocolHandler().release());
  protocol_interceptors.push_back(
      CreateAndroidAssetFileProtocolHandler().release());
  // The AwRequestInterceptor must come after the content and asset file job
  // factories. This for WebViewClassic compatibility where it was not
  // possible to intercept resource loads to resolvable content:// and
  // file:// URIs.
  // This logical dependency is also the reason why the Content
  // ProtocolHandler has to be added as a ProtocolInterceptJobFactory rather
  // than via SetProtocolHandler.
  protocol_interceptors.push_back(new AwRequestInterceptor());

  // The chain of responsibility will execute the handlers in reverse to the
  // order in which the elements of the chain are created.
  scoped_ptr<net::URLRequestJobFactory> job_factory(aw_job_factory.Pass());
  for (ProtocolHandlerVector::reverse_iterator
           i = protocol_interceptors.rbegin();
       i != protocol_interceptors.rend();
       ++i) {
    job_factory.reset(new net::ProtocolInterceptJobFactory(
        job_factory.Pass(), make_scoped_ptr(*i)));
  }

  return job_factory.Pass();
}

}  // namespace

AwURLRequestContextGetter::AwURLRequestContextGetter(
    const base::FilePath& partition_path, net::CookieStore* cookie_store)
    : partition_path_(partition_path),
      cookie_store_(cookie_store),
      proxy_config_service_(net::ProxyService::CreateSystemProxyConfigService(
          GetNetworkTaskRunner(),
          NULL /* Ignored on Android */)) {
  // CreateSystemProxyConfigService for Android must be called on main thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

AwURLRequestContextGetter::~AwURLRequestContextGetter() {
}

void AwURLRequestContextGetter::InitializeURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!url_request_context_);

  net::URLRequestContextBuilder builder;
  builder.set_user_agent(content::GetUserAgent(GURL()));
  builder.set_network_delegate(new AwNetworkDelegate());
#if !defined(DISABLE_FTP_SUPPORT)
  builder.set_ftp_enabled(false);  // Android WebView does not support ftp yet.
#endif
  builder.set_proxy_config_service(proxy_config_service_.release());
  builder.set_accept_language(net::HttpUtil::GenerateAcceptLanguageHeader(
      AwContentBrowserClient::GetAcceptLangsImpl()));
  ApplyCmdlineOverridesToURLRequestContextBuilder(&builder);

  url_request_context_.reset(builder.Build());
  // TODO(mnaganov): Fix URLRequestContextBuilder to use proper threads.
  net::HttpNetworkSession::Params network_session_params;

  net::BackendType cache_type = net::CACHE_BACKEND_SIMPLE;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableSimpleCache)) {
    cache_type = net::CACHE_BACKEND_BLOCKFILE;
  }
  PopulateNetworkSessionParams(url_request_context_.get(),
                               &network_session_params);

  net::HttpCache* main_cache = new net::HttpCache(
      network_session_params,
      new net::HttpCache::DefaultBackend(
          net::DISK_CACHE,
          cache_type,
          partition_path_.Append(FILE_PATH_LITERAL("Cache")),
          20 * 1024 * 1024,  // 20M
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE)));
  main_http_factory_.reset(main_cache);
  url_request_context_->set_http_transaction_factory(main_cache);
  url_request_context_->set_cookie_store(cookie_store_);

  job_factory_ = CreateJobFactory(&protocol_handlers_);
  url_request_context_->set_job_factory(job_factory_.get());

  // TODO(sgurun) remove once crbug.com/329681 is fixed. Should be
  // called only once.
  net::HttpStreamFactory::EnableNpnSpdy31();
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

void AwURLRequestContextGetter::SetProtocolHandlers(
    content::ProtocolHandlerMap* protocol_handlers) {
  std::swap(protocol_handlers_, *protocol_handlers);
}

}  // namespace android_webview
