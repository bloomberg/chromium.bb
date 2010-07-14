// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/preconnect.h"

#include "base/histogram.h"
#include "base/logging.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"

namespace chrome_browser_net {

// static
bool Preconnect::preconnect_despite_proxy_ = false;

// We will deliberately leak this singular instance, which is used only for
// callbacks.
// static
Preconnect* Preconnect::callback_instance_;

// static
bool Preconnect::PreconnectOnUIThread(const GURL& url) {
  // Try to do connection warming for this search provider.
  URLRequestContextGetter* getter = Profile::GetDefaultRequestContext();
  if (!getter)
    return false;
  // Prewarm connection to Search URL.
  ChromeThread::PostTask(
      ChromeThread::IO,
      FROM_HERE,
      NewRunnableFunction(Preconnect::PreconnectOnIOThread, url));
  return true;
}

enum ProxyStatus {
  PROXY_STATUS_IGNORED,
  PROXY_UNINITIALIZED,
  PROXY_NOT_USED,
  PROXY_PAC_RESOLVER,
  PROXY_HAS_RULES,
  PROXY_MAX,
};

static void HistogramPreconnectStatus(ProxyStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Net.PreconnectProxyStatus", status, PROXY_MAX);
}

// static
void Preconnect::PreconnectOnIOThread(const GURL& url) {
  // TODO(jar): This does not handle proxies currently.
  URLRequestContextGetter* getter = Profile::GetDefaultRequestContext();
  if (!getter)
    return;
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    LOG(DFATAL) << "This must be run only on the IO thread.";
    return;
  }
  URLRequestContext* context = getter->GetURLRequestContext();

  if (preconnect_despite_proxy_) {
    HistogramPreconnectStatus(PROXY_STATUS_IGNORED);
  } else {
    // Currently we avoid all preconnects if there is a proxy configuration.
    net::ProxyService* proxy_service = context->proxy_service();
    if (!proxy_service->config_has_been_initialized()) {
      HistogramPreconnectStatus(PROXY_UNINITIALIZED);
    } else {
      if (proxy_service->config().MayRequirePACResolver()) {
        HistogramPreconnectStatus(PROXY_PAC_RESOLVER);
        return;
      }
      if (!proxy_service->config().proxy_rules().empty()) {
        HistogramPreconnectStatus(PROXY_HAS_RULES);
        return;
      }
      HistogramPreconnectStatus(PROXY_NOT_USED);
    }
  }

  net::HttpTransactionFactory* factory = context->http_transaction_factory();
  net::HttpNetworkSession* session = factory->GetSession();

  net::ClientSocketHandle handle;
  if (!callback_instance_)
    callback_instance_ = new Preconnect;

  scoped_refptr<net::TCPSocketParams> tcp_params =
      new net::TCPSocketParams(url.host(), url.EffectiveIntPort(), net::LOW,
                               GURL(), false);

  net::HostPortPair endpoint(url.host(), url.EffectiveIntPort());
  std::string group_name = endpoint.ToString();

  if (url.SchemeIs("https")) {
    group_name = StringPrintf("ssl/%s", group_name.c_str());

    net::SSLConfig ssl_config;
    session->ssl_config_service()->GetSSLConfig(&ssl_config);
    // All preconnects should be for main pages.
    ssl_config.verify_ev_cert = true;

    scoped_refptr<net::SSLSocketParams> ssl_params =
        new net::SSLSocketParams(tcp_params, NULL, NULL,
                                 net::ProxyServer::SCHEME_DIRECT,
                                 url.HostNoBrackets(), ssl_config,
                                 0, false);

    const scoped_refptr<net::SSLClientSocketPool>& pool =
        session->ssl_socket_pool();

    handle.Init(group_name, ssl_params, net::LOWEST, callback_instance_, pool,
                net::BoundNetLog());
    handle.Reset();
    return;
  }

  const scoped_refptr<net::TCPClientSocketPool>& pool =
      session->tcp_socket_pool();
  handle.Init(group_name, tcp_params, net::LOWEST, callback_instance_, pool,
              net::BoundNetLog());
  handle.Reset();
}

void Preconnect::RunWithParams(const Tuple1<int>& params) {
  // This will rarely be called, as we reset the connection just after creating.
  NOTREACHED();
}
}  // chrome_browser_net
