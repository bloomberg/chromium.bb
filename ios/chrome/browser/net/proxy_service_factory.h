// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_PROXY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_NET_PROXY_SERVICE_FACTORY_H_

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

class PrefProxyConfigTracker;
class PrefService;

namespace net {
class NetLog;
class NetworkDelegate;
class ProxyConfigService;
class ProxyService;
class URLRequestContext;
}

namespace ios {

class ProxyServiceFactory {
 public:
  // Creates a ProxyConfigService that delivers the system preferences.
  static scoped_ptr<net::ProxyConfigService> CreateProxyConfigService(
      PrefProxyConfigTracker* tracker);

  // Creates a PrefProxyConfigTracker that tracks browser state preferences.
  static scoped_ptr<PrefProxyConfigTracker>
  CreatePrefProxyConfigTrackerOfProfile(PrefService* browser_state_prefs,
                                        PrefService* local_state_prefs);

  // Creates a PrefProxyConfigTracker that tracks local state only. This tracker
  // should be used for the system request context.
  static scoped_ptr<PrefProxyConfigTracker>
  CreatePrefProxyConfigTrackerOfLocalState(PrefService* local_state_prefs);

  // Create a proxy service.
  static scoped_ptr<net::ProxyService> CreateProxyService(
      net::NetLog* net_log,
      net::URLRequestContext* context,
      net::NetworkDelegate* network_delegate,
      scoped_ptr<net::ProxyConfigService> proxy_config_service,
      bool quick_check_enabled);

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyServiceFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_NET_PROXY_SERVICE_FACTORY_H_
