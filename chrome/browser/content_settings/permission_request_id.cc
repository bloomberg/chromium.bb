// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/permission_request_id.h"

#include "base/strings/stringprintf.h"


PermissionRequestID::PermissionRequestID(int render_process_id,
                                         int render_view_id,
                                         int bridge_id,
                                         int group_id)
    : render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      bridge_id_(bridge_id),
      group_id_(group_id) {
}

PermissionRequestID::~PermissionRequestID() {
}

bool PermissionRequestID::Equals(const PermissionRequestID& other) const {
  return IsForSameTabAs(other) && (bridge_id_ == other.bridge_id_);
}

bool PermissionRequestID::IsForSameTabAs(
    const PermissionRequestID& other) const {
  return (render_process_id_ == other.render_process_id_) &&
      (render_view_id_ == other.render_view_id_);
}

std::string PermissionRequestID::ToString() const {
  return base::StringPrintf("%d,%d,%d,%d",
                            render_process_id_,
                            render_view_id_,
                            bridge_id_,
                            group_id_);
}
