// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/preconnect.h"

#include "base/logging.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"

namespace chrome_browser_net {

// We will deliberately leak this singular instance, which is used only for
// callbacks.
// static
Preconnect* Preconnect::callback_instance_;

// static
bool Preconnect::PreconnectOnUIThread(const net::HostPortPair& hostport) {
  // Try to do connection warming for this search provider.
  URLRequestContextGetter* getter = Profile::GetDefaultRequestContext();
  if (!getter)
    return false;
  // Prewarm connection to Search URL.
  ChromeThread::PostTask(
      ChromeThread::IO,
      FROM_HERE,
      NewRunnableFunction(Preconnect::PreconnectOnIOThread, hostport));
  return true;
}

// static
void Preconnect::PreconnectOnIOThread(const net::HostPortPair& hostport) {
  URLRequestContextGetter* getter = Profile::GetDefaultRequestContext();
  if (!getter)
    return;
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    LOG(DFATAL) << "This must be run only on the IO thread.";
    return;
  }
  URLRequestContext* context = getter->GetURLRequestContext();
  net::HttpTransactionFactory* factory = context->http_transaction_factory();
  net::HttpNetworkSession* session = factory->GetSession();
  scoped_refptr<net::TCPClientSocketPool> pool = session->tcp_socket_pool();

  net::TCPSocketParams params(hostport.host, hostport.port, net::LOW,
                              GURL(), false);

  net::ClientSocketHandle handle;
  if (!callback_instance_)
    callback_instance_ = new Preconnect;

  // TODO(jar): This does not handle proxies currently.
  handle.Init(hostport.ToString() , params, net::LOWEST,
              callback_instance_, pool, net::BoundNetLog());
  handle.Reset();
}

void Preconnect::RunWithParams(const Tuple1<int>& params) {
  // This will rarely be called, as we reset the connection just after creating.
  NOTREACHED();
}
}  // chrome_browser_net
