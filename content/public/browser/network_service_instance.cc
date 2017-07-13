// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/network_service_instance.h"

#include "base/feature_list.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

mojom::NetworkService* GetNetworkService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(base::FeatureList::IsEnabled(features::kNetworkService));

  static mojom::NetworkServicePtr* g_network_service =
      new mojom::NetworkServicePtr;
  if (!g_network_service->is_bound()) {
    ServiceManagerConnection::GetForProcess()->GetConnector()->BindInterface(
        mojom::kNetworkServiceName, g_network_service);
  }
  return g_network_service->get();
}

}  // namespace content
