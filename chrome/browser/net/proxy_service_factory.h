// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROXY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NET_PROXY_SERVICE_FACTORY_H_
#pragma once

#include "base/basictypes.h"

class CommandLine;
class PrefProxyConfigTracker;

namespace net {
class NetLog;
class ProxyConfigService;
class ProxyService;
class URLRequestContext;
}  // namespace net

class ProxyServiceFactory {
 public:
  // Creates a ProxyConfigService that delivers the system preferences
  // (or the respective ChromeOS equivalent).
  static net::ProxyConfigService* CreateProxyConfigService(
      PrefProxyConfigTracker* proxy_config_tracker);

  // Create a proxy service according to the options on command line.
  static net::ProxyService* CreateProxyService(
      net::NetLog* net_log,
      net::URLRequestContext* context,
      net::ProxyConfigService* proxy_config_service,
      const CommandLine& command_line);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ProxyServiceFactory);
};

#endif  // CHROME_BROWSER_NET_PROXY_SERVICE_FACTORY_H_
