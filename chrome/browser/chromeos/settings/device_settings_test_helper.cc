// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"

#include "base/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/mock_owner_key_util.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

DeviceSettingsTestHelper::DeviceSettingsTestHelper()
    : store_result_(true) {}

DeviceSettingsTestHelper::~DeviceSettingsTestHelper() {}

void DeviceSettingsTestHelper::FlushLoops() {
  // DeviceSettingsService may trigger operations that hop back and forth
  // between the message loop and the blocking pool. 2 iterations are currently
  // sufficient (key loading, signing).
  for (int i = 0; i < 2; ++i) {
    MessageLoop::current()->RunAllPending();
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
  }
  MessageLoop::current()->RunAllPending();
}

void DeviceSettingsTestHelper::FlushStore() {
  std::vector<StorePolicyCallback> callbacks;
  callbacks.swap(store_callbacks_);
  for (std::vector<StorePolicyCallback>::iterator cb(callbacks.begin());
       cb != callbacks.end(); ++cb) {
    cb->Run(store_result_);
  }
}

void DeviceSettingsTestHelper::FlushRetrieve() {
  std::vector<RetrievePolicyCallback> callbacks;
  callbacks.swap(retrieve_callbacks_);
  for (std::vector<RetrievePolicyCallback>::iterator cb(callbacks.begin());
       cb != callbacks.end(); ++cb) {
    cb->Run(policy_blob_);
  }
}

void DeviceSettingsTestHelper::Flush() {
  do {
    FlushLoops();
    FlushStore();
    FlushLoops();
    FlushRetrieve();
    FlushLoops();
  } while (!store_callbacks_.empty() || !retrieve_callbacks_.empty());
}

void DeviceSettingsTestHelper::AddObserver(Observer* observer) {}

void DeviceSettingsTestHelper::RemoveObserver(Observer* observer) {}

bool DeviceSettingsTestHelper::HasObserver(Observer* observer) {
  return false;
}

void DeviceSettingsTestHelper::EmitLoginPromptReady() {}

void DeviceSettingsTestHelper::EmitLoginPromptVisible() {}

void DeviceSettingsTestHelper::RestartJob(int pid,
                                          const std::string& command_line) {}

void DeviceSettingsTestHelper::RestartEntd() {}

void DeviceSettingsTestHelper::StartSession(const std::string& user_email) {}

void DeviceSettingsTestHelper::StopSession() {}

void DeviceSettingsTestHelper::RequestLockScreen() {}

void DeviceSettingsTestHelper::RequestUnlockScreen() {}

bool DeviceSettingsTestHelper::GetIsScreenLocked() {
  return false;
}

void DeviceSettingsTestHelper::RetrieveDevicePolicy(
    const RetrievePolicyCallback& callback) {
  retrieve_callbacks_.push_back(callback);
}

void DeviceSettingsTestHelper::RetrieveUserPolicy(
    const RetrievePolicyCallback& callback) {
}

void DeviceSettingsTestHelper::StoreDevicePolicy(
    const std::string& policy_blob,
    const StorePolicyCallback& callback) {
  policy_blob_ = policy_blob;
  store_callbacks_.push_back(callback);
}

void DeviceSettingsTestHelper::StoreUserPolicy(
    const std::string& policy_blob,
    const StorePolicyCallback& callback) {}

ScopedDeviceSettingsTestHelper::ScopedDeviceSettingsTestHelper() {
  DeviceSettingsService::Get()->Initialize(this, new MockOwnerKeyUtil());
  DeviceSettingsService::Get()->Load();
  Flush();
}

ScopedDeviceSettingsTestHelper::~ScopedDeviceSettingsTestHelper() {
  Flush();
  DeviceSettingsService::Get()->Shutdown();
}

}  // namespace chromeos
