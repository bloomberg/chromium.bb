// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_shared_client.h"

#include "content/public/browser/browser_thread.h"

#if defined(ENABLE_MDNS)
#include "chrome/browser/local_discovery/service_discovery_client_mdns.h"
#endif  // ENABLE_MDNS

namespace local_discovery {

using content::BrowserThread;

namespace {
ServiceDiscoverySharedClient* g_service_discovery_client = NULL;
}  // namespace

ServiceDiscoverySharedClient::ServiceDiscoverySharedClient() {
  DCHECK(!g_service_discovery_client);
  g_service_discovery_client = this;
}

ServiceDiscoverySharedClient::~ServiceDiscoverySharedClient() {
  DCHECK_EQ(g_service_discovery_client, this);
  g_service_discovery_client = NULL;
}

#if defined(ENABLE_MDNS)

scoped_refptr<ServiceDiscoverySharedClient>
    ServiceDiscoverySharedClient::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (g_service_discovery_client)
    return g_service_discovery_client;

  return new ServiceDiscoveryClientMdns();
}

#else  // ENABLE_MDNS

scoped_refptr<ServiceDiscoverySharedClient>
    ServiceDiscoverySharedClient::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  NOTIMPLEMENTED();
  return NULL;
}

#endif  // ENABLE_MDNS

}  // namespace local_discovery
