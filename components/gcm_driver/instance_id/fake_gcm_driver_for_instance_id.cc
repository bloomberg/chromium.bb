// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace instance_id {

FakeGCMDriverForInstanceID::FakeGCMDriverForInstanceID() {
}

FakeGCMDriverForInstanceID::~FakeGCMDriverForInstanceID() {
}

gcm::InstanceIDStore* FakeGCMDriverForInstanceID::GetInstanceIDStore() {
  return this;
}

void FakeGCMDriverForInstanceID::AddInstanceIDData(
    const std::string& app_id,
    const std::string& instance_id_data) {
  instance_id_data_[app_id] = instance_id_data;
}

void FakeGCMDriverForInstanceID::RemoveInstanceIDData(
    const std::string& app_id) {
  instance_id_data_.erase(app_id);
}

void FakeGCMDriverForInstanceID::GetInstanceIDData(
    const std::string& app_id,
    const gcm::InstanceIDStore::GetInstanceIDDataCallback& callback) {
  std::string data;
  auto iter = instance_id_data_.find(app_id);
  if (iter != instance_id_data_.end())
    data = iter->second;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, data));
}

}  // namespace instance_id
