// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_url_request_context_getter.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/task_scheduler/post_task.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/devtools_network_transaction_factory.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_context_options.h"
#include "headless/lib/browser/headless_network_delegate.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace headless {

HeadlessURLRequestContextGetter::HeadlessURLRequestContextGetter(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    content::ProtocolHandlerMap* protocol_handlers,
    ProtocolHandlerMap context_protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors,
    HeadlessBrowserContextOptions* options,
    net::NetLog* net_log,
    HeadlessBrowserContextImpl* headless_browser_context)
    : io_task_runner_(std::move(io_task_runner)),
      accept_language_(options->accept_language()),
      user_agent_(options->user_agent()),
      host_resolver_rules_(options->host_resolver_rules()),
      proxy_config_(options->proxy_config()),
      request_interceptors_(std::move(request_interceptors)),
      net_log_(net_log),
      headless_browser_context_(headless_browser_context) {
  // Must first be created on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::swap(protocol_handlers_, *protocol_handlers);

  for (auto& pair : context_protocol_handlers) {
    protocol_handlers_[pair.first] =
        linked_ptr<net::URLRequestJobFactory::ProtocolHandler>(
            pair.second.release());
  }
  context_protocol_handlers.clear();

  // We must create the proxy config service on the UI loop on Linux because it
  // must synchronously run on the glib message loop. This will be passed to
  // the URLRequestContextStorage on the IO thread in GetURLRequestContext().
  if (!proxy_config_) {
    proxy_config_service_ =
        net::ProxyService::CreateSystemProxyConfigService(io_task_runner_);
  }
  base::AutoLock lock(lock_);
  headless_browser_context_->AddObserver(this);
}

HeadlessURLRequestContextGetter::~HeadlessURLRequestContextGetter() {
  base::AutoLock lock(lock_);
  if (headless_browser_context_)
    headless_browser_context_->RemoveObserver(this);
}

net::URLRequestContext*
HeadlessURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!url_request_context_) {
    net::URLRequestContextBuilder builder;

    // Don't store cookies in incognito mode or if no user-data-dir was
    // specified
    // TODO: Enable this always once saving/restoring sessions is implemented
    // (https://crbug.com/617931)
    if (headless_browser_context_ &&
        !headless_browser_context_->IsOffTheRecord() &&
        !headless_browser_context_->options()->user_data_dir().empty()) {
      content::CookieStoreConfig cookie_config(
          headless_browser_context_->GetPath().Append(
              FILE_PATH_LITERAL("Cookies")),
          content::CookieStoreConfig::PERSISTANT_SESSION_COOKIES, NULL);
      std::unique_ptr<net::CookieStore> cookie_store =
          CreateCookieStore(cookie_config);
      std::unique_ptr<net::ChannelIDService> channel_id_service =
          base::MakeUnique<net::ChannelIDService>(
              new net::DefaultChannelIDStore(nullptr));

      cookie_store->SetChannelIDServiceID(channel_id_service->GetUniqueID());
      builder.SetCookieAndChannelIdStores(std::move(cookie_store),
                                          std::move(channel_id_service));
    }

    builder.set_accept_language(
        net::HttpUtil::GenerateAcceptLanguageHeader(accept_language_));
    builder.set_user_agent(user_agent_);
    // TODO(skyostil): Make these configurable.
    builder.set_data_enabled(true);
    builder.set_file_enabled(true);
    if (proxy_config_) {
      builder.set_proxy_service(net::ProxyService::CreateFixed(*proxy_config_));
    } else {
      builder.set_proxy_config_service(std::move(proxy_config_service_));
    }

    {
      base::AutoLock lock(lock_);
      builder.set_network_delegate(
          base::MakeUnique<HeadlessNetworkDelegate>(headless_browser_context_));
    }

    if (!host_resolver_rules_.empty()) {
      std::unique_ptr<net::HostResolver> host_resolver(
          net::HostResolver::CreateDefaultResolver(net_log_));
      std::unique_ptr<net::MappedHostResolver> mapped_host_resolver(
          new net::MappedHostResolver(std::move(host_resolver)));
      mapped_host_resolver->SetRulesFromString(host_resolver_rules_);
      builder.set_host_resolver(std::move(mapped_host_resolver));
    }

    // Extra headers are required for network emulation and are removed in
    // DevToolsNetworkTransaction. If a protocol handler is set for http or
    // https, then it is likely that the HttpTransactionFactoryCallback will
    // not be called and DevToolsNetworkTransaction would not remove the header.
    // In that case, the headers should be removed in HeadlessNetworkDelegate.
    bool has_http_handler = false;
    for (auto& pair : protocol_handlers_) {
      builder.SetProtocolHandler(pair.first,
                                 base::WrapUnique(pair.second.release()));
      if (pair.first == url::kHttpScheme || pair.first == url::kHttpsScheme)
        has_http_handler = true;
    }
    protocol_handlers_.clear();
    builder.SetInterceptors(std::move(request_interceptors_));

    if (!has_http_handler && headless_browser_context_) {
      headless_browser_context_->SetRemoveHeaders(false);
      builder.SetCreateHttpTransactionFactoryCallback(
          base::BindOnce(&content::CreateDevToolsNetworkTransactionFactory));
    }

    url_request_context_ = builder.Build();
    url_request_context_->set_net_log(net_log_);
  }

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
HeadlessURLRequestContextGetter::GetNetworkTaskRunner() const {
  return io_task_runner_;
}

net::HostResolver* HeadlessURLRequestContextGetter::host_resolver() const {
  return url_request_context_->host_resolver();
}

void HeadlessURLRequestContextGetter::OnHeadlessBrowserContextDestruct() {
  base::AutoLock lock(lock_);
  headless_browser_context_ = nullptr;
}

}  // namespace headless
