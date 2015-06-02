// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "components/gcm_driver/gcm_client.h"

namespace instance_id {

FakeGCMDriverForInstanceID::FakeGCMDriverForInstanceID() {
}

FakeGCMDriverForInstanceID::~FakeGCMDriverForInstanceID() {
}

gcm::InstanceIDHandler* FakeGCMDriverForInstanceID::GetInstanceIDHandler() {
  return this;
}

void FakeGCMDriverForInstanceID::AddInstanceIDData(
    const std::string& app_id,
    const std::string& instance_id,
    const std::string& extra_data) {
  instance_id_data_[app_id] = std::make_pair(instance_id, extra_data);
}

void FakeGCMDriverForInstanceID::RemoveInstanceIDData(
    const std::string& app_id) {
  instance_id_data_.erase(app_id);
}

void FakeGCMDriverForInstanceID::GetInstanceIDData(
    const std::string& app_id,
    const GetInstanceIDDataCallback& callback) {
  auto iter = instance_id_data_.find(app_id);
  std::string instance_id;
  std::string extra_data;
  if (iter != instance_id_data_.end()) {
    instance_id = iter->second.first;
    extra_data = iter->second.second;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, instance_id, extra_data));
}

void FakeGCMDriverForInstanceID::GetToken(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope,
    const std::map<std::string, std::string>& options,
    const GetTokenCallback& callback) {
  std::string token;
  std::string key = app_id + authorized_entity + scope;
  auto iter = tokens_.find(key);
  if (iter != tokens_.end()) {
    token = iter->second;
  } else {
    token = base::Uint64ToString(base::RandUint64());
    tokens_[key] = token;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, token, gcm::GCMClient::SUCCESS));
}

void FakeGCMDriverForInstanceID::DeleteToken(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope,
    const DeleteTokenCallback& callback) {
  std::string key = app_id + authorized_entity + scope;
  tokens_.erase(key);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, gcm::GCMClient::SUCCESS));
}

}  // namespace instance_id
