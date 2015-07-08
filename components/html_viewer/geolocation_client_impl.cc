// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/geolocation_client_impl.h"

#include "base/logging.h"
#include "third_party/WebKit/public/web/WebGeolocationController.h"

namespace html_viewer {

GeolocationClientImpl::GeolocationClientImpl() {
}

GeolocationClientImpl::~GeolocationClientImpl() {
}

void GeolocationClientImpl::startUpdating() {
  NOTIMPLEMENTED();
}

void GeolocationClientImpl::stopUpdating() {
  NOTIMPLEMENTED();
}

void GeolocationClientImpl::setEnableHighAccuracy(bool) {
  NOTIMPLEMENTED();
}

bool GeolocationClientImpl::lastPosition(blink::WebGeolocationPosition&) {
  NOTIMPLEMENTED();
  return false;
}

void GeolocationClientImpl::requestPermission(
    const blink::WebGeolocationPermissionRequest&) {
  NOTIMPLEMENTED();
}

void GeolocationClientImpl::cancelPermissionRequest(
    const blink::WebGeolocationPermissionRequest&) {
  NOTIMPLEMENTED();
}

void GeolocationClientImpl::setController(
    blink::WebGeolocationController* controller) {
  // Ownership of the WebGeolocationController is transferred to the client.
  controller_.reset(controller);
}

}  // namespace html_viewer
