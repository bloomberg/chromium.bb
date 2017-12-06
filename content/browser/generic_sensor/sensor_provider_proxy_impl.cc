// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/generic_sensor/sensor_provider_proxy_impl.h"

#include <utility>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

SensorProviderProxyImpl::SensorProviderProxyImpl(
    PermissionManager* permission_manager,
    RenderFrameHost* render_frame_host)
    : permission_manager_(permission_manager),
      render_frame_host_(render_frame_host) {
  DCHECK(permission_manager);
  DCHECK(render_frame_host);
}

SensorProviderProxyImpl::~SensorProviderProxyImpl() = default;

void SensorProviderProxyImpl::Bind(
    device::mojom::SensorProviderRequest request) {
  binding_set_.AddBinding(this, std::move(request));
}

void SensorProviderProxyImpl::GetSensor(
    device::mojom::SensorType type,
    GetSensorCallback callback) {
  ServiceManagerConnection* connection =
      ServiceManagerConnection::GetForProcess();

  if (!connection || !CheckPermission(type)) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (!sensor_provider_) {
    connection->GetConnector()->BindInterface(
        device::mojom::kServiceName, mojo::MakeRequest(&sensor_provider_));
    sensor_provider_.set_connection_error_handler(base::BindOnce(
        &SensorProviderProxyImpl::OnConnectionError, base::Unretained(this)));
  }
  sensor_provider_->GetSensor(type, std::move(callback));
}

bool SensorProviderProxyImpl::CheckPermission(
    device::mojom::SensorType type) const {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  if (!web_contents)
    return false;

  const GURL& embedding_origin = web_contents->GetLastCommittedURL();
  const GURL& requesting_origin = render_frame_host_->GetLastCommittedURL();

  blink::mojom::PermissionStatus permission_status =
      permission_manager_->GetPermissionStatus(
          PermissionType::SENSORS, requesting_origin, embedding_origin);
  return permission_status == blink::mojom::PermissionStatus::GRANTED;
}

void SensorProviderProxyImpl::OnConnectionError() {
  // Close all the upstream bindings to notify them of this failure as the
  // GetSensorCallbacks will never be called.
  binding_set_.CloseAllBindings();
  sensor_provider_.reset();
}

}  // namespace content
