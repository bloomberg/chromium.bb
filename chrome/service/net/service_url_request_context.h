// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_NET_SERVICE_URL_REQUEST_CONTEXT_H_
#define CHROME_SERVICE_NET_SERVICE_URL_REQUEST_CONTEXT_H_
#pragma once

#include <string>

#include "chrome/common/net/url_request_context_getter.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_policy.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/disk_cache/disk_cache.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"

namespace base {
class MessageLoopProxy;
}

// Subclass of net::URLRequestContext which can be used to store extra
// information for requests. This subclass is meant to be used in the service
// process where the profile is not available.
//
class ServiceURLRequestContext : public net::URLRequestContext {
 public:
  explicit ServiceURLRequestContext(const std::string& user_agent);

  // Overridden from net::URLRequestContext:
  virtual const std::string& GetUserAgent(const GURL& url) const;

 protected:
  virtual ~ServiceURLRequestContext();

 private:
  std::string user_agent_;
};

class ServiceURLRequestContextGetter : public URLRequestContextGetter {
 public:
  ServiceURLRequestContextGetter();

  virtual net::URLRequestContext* GetURLRequestContext();
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const;

  void set_user_agent(const std::string& ua) {
    user_agent_ = ua;
  }
  std::string user_agent() const {
    return user_agent_;
  }
 private:
  virtual ~ServiceURLRequestContextGetter();

  std::string user_agent_;
  scoped_refptr<net::URLRequestContext> url_request_context_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
};

#endif  // CHROME_SERVICE_NET_SERVICE_URL_REQUEST_CONTEXT_H_
