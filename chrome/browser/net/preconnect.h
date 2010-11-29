// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A Preconnect instance maintains state while a TCP/IP connection is made, and
// and then released into the pool of available connections for future use.

#ifndef CHROME_BROWSER_NET_PRECONNECT_H_
#define CHROME_BROWSER_NET_PRECONNECT_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/net/url_info.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/http/http_request_info.h"
#include "net/http/stream_factory.h"

namespace net {

class ProxyInfo;
struct SSLConfig;

}  // namespace net

namespace chrome_browser_net {

class Preconnect {
 public:
  // Try to preconnect.  Typically motivated by OMNIBOX to reach search service.
  // |count| may be used to request more than one connection be established in
  // parallel.
  static void PreconnectOnUIThread(const GURL& url,
                                   UrlInfo::ResolutionMotivation motivation,
                                   int count);

  // Try to preconnect.  Typically used by predictor when a subresource probably
  // needs a connection. |count| may be used to request more than one connection
  // be established in parallel.
  static void PreconnectOnIOThread(const GURL& url,
                                   UrlInfo::ResolutionMotivation motivation,
                                   int count);

 private:
  explicit Preconnect(UrlInfo::ResolutionMotivation motivation);
  virtual ~Preconnect();

  void OnPreconnectComplete(int error_code);

  // Request actual connection, via interface that tags request as needed for
  // preconnect only (so that they can be merged with connections needed for
  // navigations).
  void Connect(const GURL& url, int count);

  // Generally either LEARNED_REFERAL_MOTIVATED, OMNIBOX_MOTIVATED or
  // EARLY_LOAD_MOTIVATED to indicate why we were trying to do a preconnection.
  const UrlInfo::ResolutionMotivation motivation_;

  // HttpRequestInfo used for connecting.
  scoped_ptr<net::HttpRequestInfo> request_info_;

  // SSLConfig used for connecting.
  scoped_ptr<net::SSLConfig> ssl_config_;

  // ProxyInfo used for connecting.
  scoped_ptr<net::ProxyInfo> proxy_info_;

  // A net log to use for this preconnect.
  net::BoundNetLog net_log_;

  // Our preconnect.
  scoped_ptr<net::StreamRequest> stream_request_;

  net::CompletionCallbackImpl<Preconnect> io_callback_;

  DISALLOW_COPY_AND_ASSIGN(Preconnect);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_PRECONNECT_H_
