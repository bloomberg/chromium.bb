// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "googleurl/src/gurl.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/socket/tcp_server_socket.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace {

class SyncUrlFetcher : public net::URLFetcherDelegate {
 public:
  SyncUrlFetcher(const GURL& url,
                 URLRequestContextGetter* getter,
                 std::string* response)
      : url_(url), getter_(getter), response_(response), event_(false, false) {}

  virtual ~SyncUrlFetcher() {}

  bool Fetch() {
    getter_->GetNetworkTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&SyncUrlFetcher::FetchOnIOThread, base::Unretained(this)));
    event_.Wait();
    return success_;
  }

  void FetchOnIOThread() {
    fetcher_.reset(net::URLFetcher::Create(url_, net::URLFetcher::GET, this));
    fetcher_->SetRequestContext(getter_);
    fetcher_->Start();
  }

  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    success_ = (source->GetResponseCode() == 200);
    if (success_)
      success_ = source->GetResponseAsString(response_);
    fetcher_.reset();  // Destroy the fetcher on IO thread.
    event_.Signal();
  }

 private:
  GURL url_;
  URLRequestContextGetter* getter_;
  std::string* response_;
  base::WaitableEvent event_;
  scoped_ptr<net::URLFetcher> fetcher_;
  bool success_;
};

}  // namespace

bool FetchUrl(const GURL& url,
              URLRequestContextGetter* getter,
              std::string* response) {
  return SyncUrlFetcher(url, getter, response).Fetch();
}

bool FindOpenPort(int* port) {
  char parts[] = {127, 0, 0, 1};
  net::IPAddressNumber address(parts, parts + arraysize(parts));
  net::NetLog::Source source;
  for (int i = 0; i < 10; ++i) {
    net::TCPServerSocket sock(NULL, source);
    // Use port 0, so that the OS will assign an available ephemeral port.
    if (sock.Listen(net::IPEndPoint(address, 0), 1) != net::OK)
      continue;
    net::IPEndPoint end_point;
    if (sock.GetLocalAddress(&end_point) != net::OK)
      continue;
    *port = end_point.port();
    return true;
  }
  return false;
}
