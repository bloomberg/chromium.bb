// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/shutdown_client_proxy.h"

#include "ash/public/interfaces/shutdown.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ash {

ShutdownClientProxy::ShutdownClientProxy(service_manager::Connector* connector)
    : connector_(connector) {}

ShutdownClientProxy::~ShutdownClientProxy() {}

void ShutdownClientProxy::RequestShutdown() {
  mojom::ShutdownClientPtr client;
  connector_->ConnectToInterface("service:content_browser", &client);
  client->RequestShutdown();
}

}  // namespace ash
