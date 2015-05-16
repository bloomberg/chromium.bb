// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/instance_id_driver.h"

#include "components/gcm_driver/instance_id/instance_id.h"

namespace instance_id {

// static
bool InstanceIDDriver::IsInstanceIDEnabled() {
#if defined(OS_ANDROID)
  // Not implemented yet.
  return false;
#else
  return true;
#endif    // defined(OS_ANDROID)
}

InstanceIDDriver::InstanceIDDriver(gcm::GCMDriver* gcm_driver)
    : gcm_driver_(gcm_driver),
      instance_id_map_deleter_(&instance_id_map_) {
}

InstanceIDDriver::~InstanceIDDriver() {
}

InstanceID* InstanceIDDriver::GetInstanceID(const std::string& app_id) {
  auto iter = instance_id_map_.find(app_id);
  if (iter != instance_id_map_.end())
    return iter->second;

  InstanceID* instance_id = InstanceID::Create(app_id, gcm_driver_);
  instance_id_map_[app_id] = instance_id;
  return instance_id;
}

}  // namespace instance_id
