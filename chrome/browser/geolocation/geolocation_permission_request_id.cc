// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_request_id.h"

#include "base/stringprintf.h"


GeolocationPermissionRequestID::GeolocationPermissionRequestID(
    int render_process_id,
    int render_view_id,
    int bridge_id)
    : render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      bridge_id_(bridge_id) {
}

GeolocationPermissionRequestID::~GeolocationPermissionRequestID() {
}

bool GeolocationPermissionRequestID::Equals(
    const GeolocationPermissionRequestID& other) const {
  return IsForSameTabAs(other) && (bridge_id_ == other.bridge_id_);
}

bool GeolocationPermissionRequestID::IsForSameTabAs(
    const GeolocationPermissionRequestID& other) const {
  return (render_process_id_ == other.render_process_id_) &&
      (render_view_id_ == other.render_view_id_);
}

std::string GeolocationPermissionRequestID::ToString() const {
  return base::StringPrintf("%d,%d,%d", render_process_id_, render_view_id_,
                            bridge_id_);
}
