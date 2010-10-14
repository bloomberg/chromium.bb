// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/preconnect.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_stream.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"

namespace chrome_browser_net {

// static
void Preconnect::PreconnectOnUIThread(const GURL& url,
    UrlInfo::ResolutionMotivation motivation) {
  // Prewarm connection to Search URL.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableFunction(Preconnect::PreconnectOnIOThread, url, motivation));
  return;
}

// static
void Preconnect::PreconnectOnIOThread(const GURL& url,
    UrlInfo::ResolutionMotivation motivation) {
  Preconnect* preconnect = new Preconnect(motivation);
  // TODO(jar): Should I use PostTask for LearnedSubresources to delay the
  // preconnection a tad?
  preconnect->Connect(url);
}

Preconnect::Preconnect(UrlInfo::ResolutionMotivation motivation)
    : motivation_(motivation) {}
Preconnect::~Preconnect() {}

void Preconnect::Connect(const GURL& url) {
  URLRequestContextGetter* getter = Profile::GetDefaultRequestContext();
  if (!getter)
    return;
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    LOG(DFATAL) << "This must be run only on the IO thread.";
    return;
  }

  // We are now commited to doing the async preconnection call.
  UMA_HISTOGRAM_ENUMERATION("Net.PreconnectMotivation", motivation_,
                            UrlInfo::MAX_MOTIVATED);

  URLRequestContext* context = getter->GetURLRequestContext();
  net::HttpTransactionFactory* factory = context->http_transaction_factory();
  net::HttpNetworkSession* session = factory->GetSession();

  request_info_.reset(new net::HttpRequestInfo());
  request_info_->url = url;
  request_info_->method = "GET";
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
  request_info_->priority = net::HIGHEST;

  // Translate the motivation from UrlRequest motivations to HttpRequest
  // motivations.
  switch (motivation_) {
    case UrlInfo::OMNIBOX_MOTIVATED:
      request_info_->motivation = net::HttpRequestInfo::OMNIBOX_MOTIVATED;
      break;
    case UrlInfo::LEARNED_REFERAL_MOTIVATED:
      request_info_->motivation = net::HttpRequestInfo::PRECONNECT_MOTIVATED;
      break;
    case UrlInfo::EARLY_LOAD_MOTIVATED:
      break;
    default:
      // Other motivations should never happen here.
      NOTREACHED();
      break;
  }

  // Setup the SSL Configuration.
  ssl_config_.reset(new net::SSLConfig());
  session->ssl_config_service()->GetSSLConfig(ssl_config_.get());
  if (session->http_stream_factory()->next_protos())
    ssl_config_->next_protos = *session->http_stream_factory()->next_protos();

  // All preconnects should be for main pages.
  ssl_config_->verify_ev_cert = true;

  proxy_info_.reset(new net::ProxyInfo());
  net::StreamFactory* stream_factory = session->http_stream_factory();
  stream_request_.reset(
      stream_factory->RequestStream(request_info_.get(), ssl_config_.get(),
                                    proxy_info_.get(), session, this,
                                    net_log_));
}

void Preconnect::OnStreamReady(net::HttpStream* stream) {
  delete stream;
  delete this;
}

void Preconnect::OnStreamFailed(int status) {
  delete this;
}

void Preconnect::OnCertificateError(int status, const net::SSLInfo& ssl_info) {
  delete this;
}

void Preconnect::OnNeedsProxyAuth(const net::HttpResponseInfo& proxy_response,
                                  net::HttpAuthController* auth_controller) {
  delete this;
}

void Preconnect::OnNeedsClientAuth(net::SSLCertRequestInfo* cert_info) {
  delete this;
}

}  // chrome_browser_net
