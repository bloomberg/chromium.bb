// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/instance_id_android.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

namespace instance_id {

// static
scoped_ptr<InstanceID> InstanceID::Create(const std::string& app_id,
                                          gcm::GCMDriver* gcm_driver) {
  return make_scoped_ptr(new InstanceIDAndroid(app_id));
}

InstanceIDAndroid::InstanceIDAndroid(const std::string& app_id)
    : InstanceID(app_id) {
}

InstanceIDAndroid::~InstanceIDAndroid() {
}

void InstanceIDAndroid::GetID(const GetIDCallback& callback) {
  NOTIMPLEMENTED();
}

void InstanceIDAndroid::GetCreationTime(
    const GetCreationTimeCallback& callback) {
  NOTIMPLEMENTED();
}

void InstanceIDAndroid::GetToken(
    const std::string& audience,
    const std::string& scope,
    const std::map<std::string, std::string>& options,
    const GetTokenCallback& callback) {
  NOTIMPLEMENTED();
}

void InstanceIDAndroid::DeleteToken(const std::string& audience,
                                    const std::string& scope,
                                    const DeleteTokenCallback& callback) {
  NOTIMPLEMENTED();
}

void InstanceIDAndroid::DeleteID(const DeleteIDCallback& callback) {
  NOTIMPLEMENTED();
}

}  // namespace instance_id
