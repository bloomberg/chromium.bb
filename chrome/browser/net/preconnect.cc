// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/preconnect.h"

#include "base/histogram.h"
#include "base/logging.h"
#include "base/string_util.h"
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


Preconnect::~Preconnect() {
  if (!handle_.is_initialized())
    return;
  DCHECK(motivation_ == UrlInfo::LEARNED_REFERAL_MOTIVATED ||
         motivation_ == UrlInfo::OMNIBOX_MOTIVATED);
  if (motivation_ == UrlInfo::OMNIBOX_MOTIVATED)
    handle_.socket()->SetOmniboxSpeculation();
  else
    handle_.socket()->SetSubresourceSpeculation();
  handle_.Reset();
}

// static
void Preconnect::PreconnectOnUIThread(const GURL& url,
    UrlInfo::ResolutionMotivation motivation) {
  // Prewarm connection to Search URL.
  ChromeThread::PostTask(
      ChromeThread::IO,
      FROM_HERE,
      NewRunnableFunction(Preconnect::PreconnectOnIOThread, url, motivation));
  return;
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
void Preconnect::PreconnectOnIOThread(const GURL& url,
    UrlInfo::ResolutionMotivation motivation) {
  scoped_refptr<Preconnect> preconnect = new Preconnect(motivation);
  // TODO(jar): Should I use PostTask for LearnedSubresources to delay the
  // preconnection a tad?
  preconnect->Connect(url);
}

void Preconnect::Connect(const GURL& url) {
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
    if (!proxy_service->fetched_config().is_valid()) {
      HistogramPreconnectStatus(PROXY_UNINITIALIZED);
    } else {
      if (proxy_service->fetched_config().HasAutomaticSettings()) {
        HistogramPreconnectStatus(PROXY_PAC_RESOLVER);
        return;
      }
      if (!proxy_service->fetched_config().proxy_rules().empty()) {
        HistogramPreconnectStatus(PROXY_HAS_RULES);
        return;
      }
      HistogramPreconnectStatus(PROXY_NOT_USED);
    }
  }

  // We are now commited to doing the async preconnection call.
  UMA_HISTOGRAM_ENUMERATION("Net.PreconnectMotivation", motivation_,
                            UrlInfo::MAX_MOTIVATED);
  AddRef();  // Stay alive until socket is available.

  net::HttpTransactionFactory* factory = context->http_transaction_factory();
  net::HttpNetworkSession* session = factory->GetSession();

  scoped_refptr<net::TCPSocketParams> tcp_params =
      new net::TCPSocketParams(url.host(), url.EffectiveIntPort(), net::LOW,
                               GURL(), false);

  net::HostPortPair endpoint(url.host(), url.EffectiveIntPort());
  std::string group_name = endpoint.ToString();

  // It almost doesn't matter whether we use net::LOWEST or net::HIGHEST
  // priority here, as we won't make a request, and will surrender the created
  // socket to the pool as soon as we can.  However, we would like to mark the
  // speculative socket as such, and IF we use a net::LOWEST priority, and if
  // a navigation asked for a socket (after us) then it would get our socket,
  // and we'd get its later-arriving socket, which might make us record that
  // the speculation didn't help :-/.  By using net::HIGHEST, we ensure that
  // a socket is given to us if "we asked first" and this allows us to mark it
  // as speculative, and better detect stats (if it gets used).
  // TODO(jar): histogram to see how often we accidentally use a previously-
  // unused socket, when a previously used socket was available.
  net::RequestPriority priority = net::HIGHEST;

  int rv = net::OK;  // Result of Init call.
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
                                 0, false, false);

    const scoped_refptr<net::SSLClientSocketPool>& pool =
        session->ssl_socket_pool();

    rv = handle_.Init(group_name, ssl_params, priority, this, pool,
                 net::BoundNetLog());
  } else {
    const scoped_refptr<net::TCPClientSocketPool>& pool =
        session->tcp_socket_pool();
    rv = handle_.Init(group_name, tcp_params, priority, this, pool,
                 net::BoundNetLog());
  }

  if (rv != net::ERR_IO_PENDING)
    Release();  // There will be no callback.
}

void Preconnect::RunWithParams(const Tuple1<int>& params) {
  if (params.a < 0 && handle_.socket())
    handle_.socket()->Disconnect();
  Release();
}
}  // chrome_browser_net
