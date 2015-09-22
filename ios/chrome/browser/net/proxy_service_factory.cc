// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/net/proxy_service_factory.h"

#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "ios/web/public/web_thread.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_service.h"

namespace ios {

// static
scoped_ptr<net::ProxyConfigService>
ProxyServiceFactory::CreateProxyConfigService(PrefProxyConfigTracker* tracker) {
  scoped_ptr<net::ProxyConfigService> base_service(
      net::ProxyService::CreateSystemProxyConfigService(
          web::WebThread::GetTaskRunnerForThread(web::WebThread::IO),
          web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE)));
  return tracker->CreateTrackingProxyConfigService(base_service.Pass()).Pass();
}

// static
scoped_ptr<PrefProxyConfigTracker>
ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
    PrefService* browser_state_prefs,
    PrefService* local_state_prefs) {
  return make_scoped_ptr(new PrefProxyConfigTrackerImpl(
      browser_state_prefs,
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO)));
}

// static
scoped_ptr<PrefProxyConfigTracker>
ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
    PrefService* local_state_prefs) {
  return make_scoped_ptr(new PrefProxyConfigTrackerImpl(
      local_state_prefs,
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO)));
}

// static
scoped_ptr<net::ProxyService> ProxyServiceFactory::CreateProxyService(
    net::NetLog* net_log,
    net::URLRequestContext* context,
    net::NetworkDelegate* network_delegate,
    scoped_ptr<net::ProxyConfigService> proxy_config_service,
    bool quick_check_enabled) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  scoped_ptr<net::ProxyService> proxy_service(
      net::ProxyService::CreateUsingSystemProxyResolver(
          proxy_config_service.Pass(), 0, net_log));
  proxy_service->set_quick_check_enabled(quick_check_enabled);
  return proxy_service.Pass();
}

}  // namespace ios
