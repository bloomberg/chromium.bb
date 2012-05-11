// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_NET_SERVICE_URL_REQUEST_CONTEXT_H_
#define CHROME_SERVICE_NET_SERVICE_URL_REQUEST_CONTEXT_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/cookies/cookie_monster.h"
#include "net/disk_cache/disk_cache.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context_storage.h"

namespace base {
class MessageLoopProxy;
}

namespace net {
class ProxyConfigService;
}

// Subclass of net::URLRequestContext which can be used to store extra
// information for requests. This subclass is meant to be used in the service
// process where the profile is not available.
//
class ServiceURLRequestContext : public net::URLRequestContext {
 public:
  // This context takes ownership of |net_proxy_config_service|.
  explicit ServiceURLRequestContext(
      const std::string& user_agent,
      net::ProxyConfigService* net_proxy_config_service);

  virtual ~ServiceURLRequestContext();

  // Overridden from net::URLRequestContext:
  virtual const std::string& GetUserAgent(const GURL& url) const OVERRIDE;

 private:
  std::string user_agent_;
  net::URLRequestContextStorage storage_;
};

class ServiceURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy>
      GetIOMessageLoopProxy() const OVERRIDE;

  void set_user_agent(const std::string& ua) {
    user_agent_ = ua;
  }
  std::string user_agent() const {
    return user_agent_;
  }

 private:
  friend class ServiceProcess;
  ServiceURLRequestContextGetter();
  virtual ~ServiceURLRequestContextGetter();

  std::string user_agent_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_ptr<net::URLRequestContext> url_request_context_;
};

#endif  // CHROME_SERVICE_NET_SERVICE_URL_REQUEST_CONTEXT_H_
