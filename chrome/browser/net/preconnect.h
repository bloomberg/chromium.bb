// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A Preconnect instance maintains state while a TCP/IP connection is made, and
// and then released into the pool of available connections for future use.

#ifndef CHROME_BROWSER_NET_PRECONNECT_H_
#define CHROME_BROWSER_NET_PRECONNECT_H_
#pragma once

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/net/url_info.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_request_info.h"
#include "net/http/stream_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/tcp_client_socket_pool.h"
#include "net/url_request/url_request_context.h"

namespace net {

class ProxyInfo;
struct SSLConfig;

}

namespace chrome_browser_net {

class Preconnect : public net::StreamFactory::StreamRequestDelegate {
 public:
  // Try to preconnect.  Typically motivated by OMNIBOX to reach search service.
  static void PreconnectOnUIThread(const GURL& url,
                                   UrlInfo::ResolutionMotivation motivation);

  // Try to preconnect.  Typically used by predictor when a subresource probably
  // needs a connection.
  static void PreconnectOnIOThread(const GURL& url,
                                   UrlInfo::ResolutionMotivation motivation);

  // StreamRequestDelegate interface
  virtual void OnStreamReady(net::HttpStream* stream);
  virtual void OnStreamFailed(int status);
  virtual void OnCertificateError(int status, const net::SSLInfo& ssl_info);
  virtual void OnNeedsProxyAuth(const net::HttpResponseInfo& proxy_response,
                                net::HttpAuthController* auth_controller);
  virtual void OnNeedsClientAuth(net::SSLCertRequestInfo* cert_info);

 private:
  friend class base::RefCountedThreadSafe<Preconnect>;

  explicit Preconnect(UrlInfo::ResolutionMotivation motivation)
      : motivation_(motivation) {
  }
  virtual ~Preconnect() {}

  // Request actual connection.
  void Connect(const GURL& url);

  // Generally either LEARNED_REFERAL_MOTIVATED or OMNIBOX_MOTIVATED to indicate
  // why we were trying to do a preconnection.
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
  scoped_refptr<net::StreamFactory::StreamRequestJob> stream_request_job_;

  DISALLOW_COPY_AND_ASSIGN(Preconnect);
};
}  // chrome_browser_net

#endif  // CHROME_BROWSER_NET_PRECONNECT_H_
