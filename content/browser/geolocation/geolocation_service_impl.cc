// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_service_impl.h"

#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "device/geolocation/geolocation_context.h"
#include "third_party/WebKit/common/feature_policy/feature_policy_feature.h"

namespace content {

GeolocationServiceImplContext::GeolocationServiceImplContext(
    PermissionManager* permission_manager)
    : permission_manager_(permission_manager),
      request_id_(PermissionManager::kNoPendingOperation) {}

GeolocationServiceImplContext::~GeolocationServiceImplContext() {
  if (request_id_ == PermissionManager::kNoPendingOperation)
    return;

  CancelPermissionRequest();
}

void GeolocationServiceImplContext::RequestPermission(
    RenderFrameHost* render_frame_host,
    bool user_gesture,
    const base::Callback<void(blink::mojom::PermissionStatus)>& callback) {
  request_id_ = permission_manager_->RequestPermission(
      PermissionType::GEOLOCATION, render_frame_host,
      render_frame_host->GetLastCommittedOrigin().GetURL(), user_gesture,
      // NOTE: The permission request is canceled in the destructor, so it is
      // safe to pass |this| as Unretained.
      base::Bind(&GeolocationServiceImplContext::HandlePermissionStatus,
                 base::Unretained(this), std::move(callback)));
}

void GeolocationServiceImplContext::CancelPermissionRequest() {
  if (request_id_ == PermissionManager::kNoPendingOperation) {
    mojo::ReportBadMessage(
        "GeolocationService client may only create one Geolocation at a "
        "time.");
    return;
  }

  permission_manager_->CancelPermissionRequest(request_id_);
  request_id_ = PermissionManager::kNoPendingOperation;
}

void GeolocationServiceImplContext::HandlePermissionStatus(
    const base::Callback<void(blink::mojom::PermissionStatus)>& callback,
    blink::mojom::PermissionStatus permission_status) {
  request_id_ = PermissionManager::kNoPendingOperation;
  callback.Run(permission_status);
}

GeolocationServiceImpl::GeolocationServiceImpl(
    device::GeolocationContext* geolocation_context,
    PermissionManager* permission_manager,
    RenderFrameHost* render_frame_host)
    : geolocation_context_(geolocation_context),
      permission_manager_(permission_manager),
      render_frame_host_(render_frame_host) {
  DCHECK(geolocation_context);
  DCHECK(permission_manager);
  DCHECK(render_frame_host);
}

GeolocationServiceImpl::~GeolocationServiceImpl() {}

void GeolocationServiceImpl::Bind(
    blink::mojom::GeolocationServiceRequest request) {
  binding_set_.AddBinding(
      this, std::move(request),
      std::make_unique<GeolocationServiceImplContext>(permission_manager_));
}

void GeolocationServiceImpl::CreateGeolocation(
    mojo::InterfaceRequest<device::mojom::Geolocation> request,
    bool user_gesture) {
  if (base::FeatureList::IsEnabled(features::kFeaturePolicy) &&
      base::FeatureList::IsEnabled(features::kUseFeaturePolicyForPermissions) &&
      !render_frame_host_->IsFeatureEnabled(
          blink::WebFeaturePolicyFeature::kGeolocation)) {
    return;
  }

  binding_set_.dispatch_context()->RequestPermission(
      render_frame_host_, user_gesture,
      // NOTE: The request is canceled by the destructor of the
      // dispatch_context, so it is safe to bind |this| as Unretained.
      base::Bind(&GeolocationServiceImpl::CreateGeolocationWithPermissionStatus,
                 base::Unretained(this), base::Passed(&request)));
}

void GeolocationServiceImpl::CreateGeolocationWithPermissionStatus(
    device::mojom::GeolocationRequest request,
    blink::mojom::PermissionStatus permission_status) {
  if (permission_status != blink::mojom::PermissionStatus::GRANTED)
    return;

  geolocation_context_->Bind(std::move(request));
}

}  // namespace content
