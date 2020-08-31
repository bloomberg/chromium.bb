// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/service_connector.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "chromecast/common/mojom/constants.mojom.h"
#include "chromecast/common/mojom/multiroom.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"

namespace chromecast {

namespace {

ServiceConnector* g_instance = nullptr;

}  // namespace

const ServiceConnectorClientId kBrowserProcessClientId =
    ServiceConnectorClientId::FromUnsafeValue(1);

const ServiceConnectorClientId kMediaServiceClientId =
    ServiceConnectorClientId::FromUnsafeValue(2);

ServiceConnector::ServiceConnector() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!g_instance);
  g_instance = this;
}

ServiceConnector::~ServiceConnector() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
mojo::PendingRemote<mojom::ServiceConnector> ServiceConnector::MakeRemote(
    ServiceConnectorClientId client_id) {
  DCHECK(g_instance);
  mojo::PendingRemote<mojom::ServiceConnector> remote;
  BindReceiver(client_id, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

// static
void ServiceConnector::BindReceiver(
    ServiceConnectorClientId client_id,
    mojo::PendingReceiver<mojom::ServiceConnector> receiver) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    base::PostTask(FROM_HERE, content::BrowserThread::UI,
                   base::BindOnce(&ServiceConnector::BindReceiver, client_id,
                                  std::move(receiver)));
    return;
  }

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(g_instance);
  g_instance->receivers_.Add(g_instance, std::move(receiver), client_id);
}

void ServiceConnector::Connect(const std::string& service_name,
                               mojo::GenericPendingReceiver receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const ServiceConnectorClientId client_id = receivers_.current_context();

  // If the client is browser code, forward indscriminately through the Service
  // Manager. The browser generally has access unfettered to everything.
  if (client_id == kBrowserProcessClientId) {
    auto interface_name = *receiver.interface_name();
    content::GetSystemConnector()->BindInterface(
        service_manager::ServiceFilter::ByName(service_name),
        interface_name, receiver.PassPipe());
    return;
  }

  // In the public implementation of this class, the only other client we
  // support is the Media Service, binding to the MultiroomManager interface in
  // the Chromecast service.
  if (client_id != kMediaServiceClientId ||
      service_name != mojom::kChromecastServiceName) {
    LOG(ERROR) << "Client " << client_id.GetUnsafeValue()
               << " attempted to bind " << *receiver.interface_name()
               << " in inaccessible service " << service_name;
    return;
  }

  if (auto r = receiver.As<mojom::MultiroomManager>()) {
    content::GetSystemConnector()->BindInterface(service_name, std::move(r));
    return;
  }

  LOG(ERROR) << "Media Service attempted to bind inaccessible interface "
             << *receiver.interface_name() << " in \"chromecast\" service.";
}

}  // namespace chromecast
