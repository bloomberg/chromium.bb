// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/geolocation_dispatcher.h"

#include "content/common/geolocation_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationPermissionRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationPermissionRequestManager.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationPosition.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

using WebKit::WebGeolocationController;
using WebKit::WebGeolocationError;
using WebKit::WebGeolocationPermissionRequest;
using WebKit::WebGeolocationPermissionRequestManager;
using WebKit::WebGeolocationPosition;

GeolocationDispatcher::GeolocationDispatcher(RenderViewImpl* render_view)
    : content::RenderViewObserver(render_view),
      pending_permissions_(new WebGeolocationPermissionRequestManager()),
      enable_high_accuracy_(false),
      updating_(false) {
}

GeolocationDispatcher::~GeolocationDispatcher() {}

bool GeolocationDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GeolocationDispatcher, message)
    IPC_MESSAGE_HANDLER(GeolocationMsg_PermissionSet, OnPermissionSet)
    IPC_MESSAGE_HANDLER(GeolocationMsg_PositionUpdated, OnPositionUpdated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GeolocationDispatcher::geolocationDestroyed() {
  controller_.reset();
  DCHECK(!updating_);
}

void GeolocationDispatcher::startUpdating() {
  GURL url;
  Send(new GeolocationHostMsg_StartUpdating(
      routing_id(), url, enable_high_accuracy_));
  updating_ = true;
}

void GeolocationDispatcher::stopUpdating() {
  Send(new GeolocationHostMsg_StopUpdating(routing_id()));
  updating_ = false;
}

void GeolocationDispatcher::setEnableHighAccuracy(bool enable_high_accuracy) {
  // GeolocationController calls setEnableHighAccuracy(true) before
  // startUpdating in response to the first high-accuracy Geolocation
  // subscription. When the last high-accuracy Geolocation unsubscribes
  // it calls setEnableHighAccuracy(false) after stopUpdating.
  bool has_changed = enable_high_accuracy_ != enable_high_accuracy;
  enable_high_accuracy_ = enable_high_accuracy;
  // We have a different accuracy requirement. Request browser to update.
  if (updating_ && has_changed)
    startUpdating();
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
  int bridge_id = pending_permissions_->add(permissionRequest);
  string16 origin = permissionRequest.securityOrigin().toString();
  Send(new GeolocationHostMsg_RequestPermission(
      routing_id(), bridge_id, GURL(origin)));
}

// TODO(jknotten): Change the messages to use a security origin, so no
// conversion is necessary.
void GeolocationDispatcher::cancelPermissionRequest(
    const WebGeolocationPermissionRequest& permissionRequest) {
  int bridge_id;
  if (!pending_permissions_->remove(permissionRequest, bridge_id))
    return;
  string16 origin = permissionRequest.securityOrigin().toString();
  Send(new GeolocationHostMsg_CancelPermissionRequest(
      routing_id(), bridge_id, GURL(origin)));
}

// Permission for using geolocation has been set.
void GeolocationDispatcher::OnPermissionSet(int bridge_id, bool is_allowed) {
  WebGeolocationPermissionRequest permissionRequest;
  if (!pending_permissions_->remove(bridge_id, permissionRequest))
    return;
  permissionRequest.setIsAllowed(is_allowed);
}

// We have an updated geolocation position or error code.
void GeolocationDispatcher::OnPositionUpdated(const Geoposition& geoposition) {
  // It is possible for the browser process to have queued an update message
  // before receiving the stop updating message.
  if (!updating_)
    return;

  DCHECK(geoposition.IsInitialized());
  if (geoposition.IsValidFix()) {
    controller_->positionChanged(
        WebGeolocationPosition(
            geoposition.timestamp.ToDoubleT(),
            geoposition.latitude, geoposition.longitude,
            geoposition.accuracy,
            geoposition.is_valid_altitude(), geoposition.altitude,
            geoposition.is_valid_altitude_accuracy(),
            geoposition.altitude_accuracy,
            geoposition.is_valid_heading(), geoposition.heading,
            geoposition.is_valid_speed(), geoposition.speed));
  } else {
    WebGeolocationError::Error code;
    switch (geoposition.error_code) {
      case Geoposition::ERROR_CODE_PERMISSION_DENIED:
        code = WebGeolocationError::ErrorPermissionDenied;
        break;
      case Geoposition::ERROR_CODE_POSITION_UNAVAILABLE:
        code = WebGeolocationError::ErrorPositionUnavailable;
        break;
      default:
        DCHECK(false);
        NOTREACHED() << geoposition.error_code;
        return;
    }
    controller_->errorOccurred(
        WebGeolocationError(
            code, WebKit::WebString::fromUTF8(geoposition.error_message)));
  }
}
