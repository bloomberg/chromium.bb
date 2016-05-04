// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/geolocation_dispatcher.h"

#include "content/public/common/geoposition.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebGeolocationPermissionRequest.h"
#include "third_party/WebKit/public/web/WebGeolocationPermissionRequestManager.h"
#include "third_party/WebKit/public/web/WebGeolocationClient.h"
#include "third_party/WebKit/public/web/WebGeolocationPosition.h"
#include "third_party/WebKit/public/web/WebGeolocationError.h"

using blink::WebGeolocationController;
using blink::WebGeolocationError;
using blink::WebGeolocationPermissionRequest;
using blink::WebGeolocationPermissionRequestManager;
using blink::WebGeolocationPosition;

namespace content {

GeolocationDispatcher::GeolocationDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      pending_permissions_(new WebGeolocationPermissionRequestManager()),
      enable_high_accuracy_(false) {
}

GeolocationDispatcher::~GeolocationDispatcher() {}

void GeolocationDispatcher::startUpdating() {
  if (!geolocation_service_) {
    render_frame()->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&geolocation_service_));
  }
  if (enable_high_accuracy_)
    geolocation_service_->SetHighAccuracy(true);
  QueryNextPosition();
}

void GeolocationDispatcher::stopUpdating() {
  geolocation_service_.reset();
}

void GeolocationDispatcher::setEnableHighAccuracy(bool enable_high_accuracy) {
  // GeolocationController calls setEnableHighAccuracy(true) before
  // startUpdating in response to the first high-accuracy Geolocation
  // subscription. When the last high-accuracy Geolocation unsubscribes
  // it calls setEnableHighAccuracy(false) after stopUpdating.
  bool has_changed = enable_high_accuracy_ != enable_high_accuracy;
  enable_high_accuracy_ = enable_high_accuracy;
  // We have a different accuracy requirement. Request browser to update.
  if (geolocation_service_ && has_changed)
    geolocation_service_->SetHighAccuracy(enable_high_accuracy_);
}

void GeolocationDispatcher::setController(
    WebGeolocationController* controller) {
  controller_.reset(controller);
}

bool GeolocationDispatcher::lastPosition(WebGeolocationPosition&) {
  // The latest position is stored in the browser, not the renderer, so we
  // would have to fetch it synchronously to give a good value here.  The
  // WebCore::GeolocationController already caches the last position it
  // receives, so there is not much benefit to more position caching here.
  return false;
}

// TODO(jknotten): Change the messages to use a security origin, so no
// conversion is necessary.
void GeolocationDispatcher::requestPermission(
    const WebGeolocationPermissionRequest& permissionRequest) {
  if (!permission_service_.get()) {
    render_frame()->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&permission_service_));
  }

  int permission_request_id = pending_permissions_->add(permissionRequest);

  permission_service_->RequestPermission(
      blink::mojom::PermissionName::GEOLOCATION,
      permissionRequest.getSecurityOrigin().toString().utf8(),
      base::Bind(&GeolocationDispatcher::OnPermissionSet,
                 base::Unretained(this), permission_request_id));
}

void GeolocationDispatcher::cancelPermissionRequest(
    const blink::WebGeolocationPermissionRequest& permissionRequest) {
  int permission_request_id;
  pending_permissions_->remove(permissionRequest, permission_request_id);
}

// Permission for using geolocation has been set.
void GeolocationDispatcher::OnPermissionSet(
    int permission_request_id,
    blink::mojom::PermissionStatus status) {
  WebGeolocationPermissionRequest permissionRequest;
  if (!pending_permissions_->remove(permission_request_id, permissionRequest))
    return;

  permissionRequest.setIsAllowed(status ==
                                 blink::mojom::PermissionStatus::GRANTED);
}

void GeolocationDispatcher::QueryNextPosition() {
  DCHECK(geolocation_service_);
  geolocation_service_->QueryNextPosition(
      base::Bind(&GeolocationDispatcher::OnPositionUpdate,
                 base::Unretained(this)));
}

void GeolocationDispatcher::OnPositionUpdate(
    blink::mojom::GeopositionPtr geoposition) {
  QueryNextPosition();

  if (geoposition->valid) {
    controller_->positionChanged(WebGeolocationPosition(
        geoposition->timestamp,
        geoposition->latitude,
        geoposition->longitude,
        geoposition->accuracy,
        // Lowest point on land is at approximately -400 meters.
            geoposition->altitude > -10000.,
        geoposition->altitude,
        geoposition->altitude_accuracy >= 0.,
        geoposition->altitude_accuracy,
        geoposition->heading >= 0. && geoposition->heading <= 360.,
        geoposition->heading,
        geoposition->speed >= 0.,
        geoposition->speed));
  } else {
    WebGeolocationError::Error code;
    switch (geoposition->error_code) {
      case blink::mojom::Geoposition::ErrorCode::PERMISSION_DENIED:
        code = WebGeolocationError::ErrorPermissionDenied;
        break;
      case blink::mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE:
        code = WebGeolocationError::ErrorPositionUnavailable;
        break;
      default:
        NOTREACHED() << geoposition->error_code;
        return;
    }
    controller_->errorOccurred(WebGeolocationError(
        code, blink::WebString::fromUTF8(geoposition->error_message)));
  }
}

}  // namespace content
