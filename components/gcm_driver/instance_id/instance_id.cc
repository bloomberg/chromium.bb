// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/instance_id.h"

namespace instance_id {

InstanceID::InstanceID(const std::string& app_id)
    : app_id_(app_id) {
}

InstanceID::~InstanceID() {
}

void InstanceID::SetTokenRefreshCallback(const TokenRefreshCallback& callback) {
  token_refresh_callback_ = callback;
}

void InstanceID::NotifyTokenRefresh(bool update_id) {
  if (!token_refresh_callback_.is_null())
    token_refresh_callback_.Run(app_id_, update_id);
}

}  // namespace instance_id
