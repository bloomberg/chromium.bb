// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/instance_id_driver.h"

#include "base/metrics/field_trial.h"
#include "components/gcm_driver/instance_id/instance_id.h"

namespace instance_id {

namespace {
#if !defined(OS_ANDROID)
const char kInstanceIDFieldTrialName[] = "InstanceID";
const char kInstanceIDFieldTrialEnabledGroupName[] = "Enabled";
#endif    // !defined(OS_ANDROID)
}  // namespace

// static
bool InstanceIDDriver::IsInstanceIDEnabled() {
#if defined(OS_ANDROID)
  // Not implemented yet.
  return false;
#else
  std::string group_name =
      base::FieldTrialList::FindFullName(kInstanceIDFieldTrialName);
  return group_name == kInstanceIDFieldTrialEnabledGroupName;
#endif    // defined(OS_ANDROID)
}

InstanceIDDriver::InstanceIDDriver(gcm::GCMDriver* gcm_driver)
    : gcm_driver_(gcm_driver) {
}

InstanceIDDriver::~InstanceIDDriver() {
}

InstanceID* InstanceIDDriver::GetInstanceID(const std::string& app_id) {
  auto iter = instance_id_map_.find(app_id);
  if (iter != instance_id_map_.end())
    return iter->second;

  scoped_ptr<InstanceID> instance_id = InstanceID::Create(app_id, gcm_driver_);
  InstanceID* instance_id_ptr = instance_id.get();
  instance_id_map_.insert(app_id, instance_id.Pass());
  return instance_id_ptr;
}

void InstanceIDDriver::RemoveInstanceID(const std::string& app_id) {
  instance_id_map_.erase(app_id);
}

bool InstanceIDDriver::ExistsInstanceID(const std::string& app_id) const {
  return instance_id_map_.find(app_id) != instance_id_map_.end();
}

}  // namespace instance_id
