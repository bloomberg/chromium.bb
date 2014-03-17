// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROXY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NET_PROXY_SERVICE_FACTORY_H_

#include "base/basictypes.h"

class PrefProxyConfigTracker;
class PrefService;

namespace base {
class CommandLine;
}

namespace net {
class NetLog;
class NetworkDelegate;
class ProxyConfigService;
class ProxyService;
class URLRequestContext;
}

class ProxyServiceFactory {
 public:
  // Creates a ProxyConfigService that delivers the system preferences
  // (or the respective ChromeOS equivalent).
  static net::ProxyConfigService* CreateProxyConfigService(
      PrefProxyConfigTracker* tracker);

  // Creates a PrefProxyConfigTracker that tracks preferences of a
  // profile. On ChromeOS it additionaly tracks local state for shared proxy
  // settings. This tracker should be used if the profile's preferences should
  // be respected. On ChromeOS's signin screen this is for example not the case.
  static PrefProxyConfigTracker* CreatePrefProxyConfigTrackerOfProfile(
      PrefService* profile_prefs,
      PrefService* local_state_prefs);

  // Creates a PrefProxyConfigTracker that tracks local state only. This tracker
  // should be used for the system request context and the signin screen
  // (ChromeOS only).
  static PrefProxyConfigTracker* CreatePrefProxyConfigTrackerOfLocalState(
      PrefService* local_state_prefs);

  // Create a proxy service according to the options on command line.
  static net::ProxyService* CreateProxyService(
      net::NetLog* net_log,
      net::URLRequestContext* context,
      net::NetworkDelegate* network_delegate,
      net::ProxyConfigService* proxy_config_service,
      const base::CommandLine& command_line,
      bool quick_check_enabled);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ProxyServiceFactory);
};

#endif  // CHROME_BROWSER_NET_PROXY_SERVICE_FACTORY_H_
