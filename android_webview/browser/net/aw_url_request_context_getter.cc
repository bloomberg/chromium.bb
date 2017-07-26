// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_url_request_context_getter.h"

#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/net/aw_cookie_store_wrapper.h"
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
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
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
#include "net/http/http_network_session.h"
#include "net/http/http_stream_factory.h"
#include "net/log/net_log.h"
#include "net/net_features.h"
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

#if DCHECK_IS_ON()
bool g_created_url_request_context_builder = false;
#endif
// On apps targeting API level O or later, check cleartext is enforced.
bool g_check_cleartext_permitted = false;


const base::FilePath::CharType kChannelIDFilename[] = "Origin Bound Certs";
const char kProxyServerSwitch[] = "proxy-server";

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

std::unique_ptr<net::URLRequestJobFactory> CreateJobFactory(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  std::unique_ptr<AwURLRequestJobFactory> aw_job_factory(
      new AwURLRequestJobFactory);
  // Note that the registered schemes must also be specified in
  // AwContentBrowserClient::IsHandledURL.
  bool set_protocol = aw_job_factory->SetProtocolHandler(
      url::kFileScheme,
      base::MakeUnique<net::FileProtocolHandler>(
          base::CreateTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::BACKGROUND,
               base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      url::kDataScheme, base::MakeUnique<net::DataProtocolHandler>());
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      url::kBlobScheme,
      base::WrapUnique((*protocol_handlers)[url::kBlobScheme].release()));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      url::kFileSystemScheme,
      base::WrapUnique((*protocol_handlers)[url::kFileSystemScheme].release()));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      content::kChromeUIScheme,
      base::WrapUnique(
          (*protocol_handlers)[content::kChromeUIScheme].release()));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      content::kChromeDevToolsScheme,
      base::WrapUnique(
          (*protocol_handlers)[content::kChromeDevToolsScheme].release()));
  DCHECK(set_protocol);
  protocol_handlers->clear();

  // Note that even though the content:// scheme handler is created here,
  // it cannot be used by child processes until access to it is granted via
  // ChildProcessSecurityPolicy::GrantScheme(). This is done in
  // AwContentBrowserClient.
  request_interceptors.push_back(CreateAndroidContentRequestInterceptor());
  request_interceptors.push_back(CreateAndroidAssetFileRequestInterceptor());
  // The AwRequestInterceptor must come after the content and asset file job
  // factories. This for WebViewClassic compatibility where it was not
  // possible to intercept resource loads to resolvable content:// and
  // file:// URIs.
  // This logical dependency is also the reason why the Content
  // URLRequestInterceptor has to be added as an interceptor rather than as a
  // ProtocolHandler.
  request_interceptors.push_back(base::MakeUnique<AwRequestInterceptor>());

  // The chain of responsibility will execute the handlers in reverse to the
  // order in which the elements of the chain are created.
  std::unique_ptr<net::URLRequestJobFactory> job_factory(
      std::move(aw_job_factory));
  for (auto i = request_interceptors.rbegin(); i != request_interceptors.rend();
       ++i) {
    job_factory.reset(new net::URLRequestInterceptingJobFactory(
        std::move(job_factory), std::move(*i)));
  }
  request_interceptors.clear();

  return job_factory;
}

}  // namespace

AwURLRequestContextGetter::AwURLRequestContextGetter(
    const base::FilePath& cache_path,
    std::unique_ptr<net::ProxyConfigService> config_service,
    PrefService* user_pref_service)
    : cache_path_(cache_path),
      net_log_(new net::NetLog()),
      proxy_config_service_(std::move(config_service)),
      http_user_agent_settings_(new AwHttpUserAgentSettings()) {
  // CreateSystemProxyConfigService for Android must be called on main thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_refptr<base::SingleThreadTaskRunner> io_thread_proxy =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);

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

  AwBrowserContext* browser_context = AwBrowserContext::GetDefault();
  DCHECK(browser_context);

  builder.set_network_delegate(base::MakeUnique<AwNetworkDelegate>());
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  builder.set_ftp_enabled(false);  // Android WebView does not support ftp yet.
#endif
  DCHECK(proxy_config_service_.get());
  std::unique_ptr<net::ChannelIDService> channel_id_service;
  if (TokenBindingManager::GetInstance()->is_enabled()) {
    base::FilePath channel_id_path =
        browser_context->GetPath().Append(kChannelIDFilename);
    scoped_refptr<net::SQLiteChannelIDStore> channel_id_db;
    channel_id_db = new net::SQLiteChannelIDStore(
        channel_id_path,
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BACKGROUND}));

    channel_id_service.reset(new net::ChannelIDService(
        new net::DefaultChannelIDStore(channel_id_db.get())));
  }

  // Android provides a local HTTP proxy that handles all the proxying.
  // Create the proxy without a resolver since we rely on this local HTTP proxy.
  // TODO(sgurun) is this behavior guaranteed through SDK?

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kProxyServerSwitch)) {
    std::string proxy = command_line.GetSwitchValueASCII(kProxyServerSwitch);
    builder.set_proxy_service(net::ProxyService::CreateFixed(proxy));
  } else {
    builder.set_proxy_service(net::ProxyService::CreateWithoutProxyResolver(
        std::move(proxy_config_service_), net_log_.get()));
  }
  builder.set_net_log(net_log_.get());
  builder.SetCookieAndChannelIdStores(base::MakeUnique<AwCookieStoreWrapper>(),
                                      std::move(channel_id_service));

  net::URLRequestContextBuilder::HttpCacheParams cache_params;
  cache_params.type =
      net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE;
  cache_params.max_size = 20 * 1024 * 1024;  // 20M
  cache_params.path = cache_path_;
  builder.EnableHttpCache(cache_params);

  net::HttpNetworkSession::Params network_session_params;
  ApplyCmdlineOverridesToNetworkSessionParams(&network_session_params);
  builder.set_http_network_session_params(network_session_params);
  builder.SetSpdyAndQuicEnabled(true, false);

  std::unique_ptr<net::MappedHostResolver> host_resolver(
      new net::MappedHostResolver(
          net::HostResolver::CreateDefaultResolver(nullptr)));
  ApplyCmdlineOverridesToHostResolver(host_resolver.get());
  builder.SetHttpAuthHandlerFactory(
      CreateAuthHandlerFactory(host_resolver.get()));
  builder.set_host_resolver(std::move(host_resolver));

  url_request_context_ = builder.Build();
#if DCHECK_IS_ON()
  g_created_url_request_context_builder = true;
#endif
  url_request_context_->set_check_cleartext_permitted(
    g_check_cleartext_permitted);

  job_factory_ =
      CreateJobFactory(&protocol_handlers_, std::move(request_interceptors_));
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
  return BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);
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

// static
void AwURLRequestContextGetter::set_check_cleartext_permitted(bool permitted) {
#if DCHECK_IS_ON()
    DCHECK(!g_created_url_request_context_builder);
#endif
    g_check_cleartext_permitted = permitted;
}

std::unique_ptr<net::HttpAuthHandlerFactory>
AwURLRequestContextGetter::CreateAuthHandlerFactory(
    net::HostResolver* resolver) {
  DCHECK(resolver);

  // In Chrome this is configurable via the AuthSchemes policy. For WebView
  // there is no interest to have it available so far.
  std::vector<std::string> supported_schemes = {"basic", "digest", "ntlm",
                                                "negotiate"};
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
