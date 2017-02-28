// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_TEST_HELPER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestingProfile;

namespace chromeos {

class DBusThreadManagerSetter;

// A helper class for tests mocking out session_manager's device settings
// interface. The pattern is to initialize DeviceSettingsService with the helper
// for the SessionManagerClient pointer. The helper records calls made by
// DeviceSettingsService. The test can then verify state, after which it should
// call one of the Flush() variants that will resume processing.
class DeviceSettingsTestHelper : public MockSessionManagerClient {
 public:
  // Wraps a device settings service instance for testing.
  DeviceSettingsTestHelper();
  ~DeviceSettingsTestHelper() override;

  // Runs all pending store callbacks.
  void FlushStore();

  // Runs all pending retrieve callbacks.
  void FlushRetrieve();

  // Flushes all pending operations.
  void Flush();

  // Checks whether any asynchronous Store/Retrieve operations are pending.
  bool HasPendingOperations() const;

  bool store_result() {
    return device_policy_.store_result_;
  }
  void set_store_result(bool store_result) {
    device_policy_.store_result_ = store_result;
  }

  const std::string& policy_blob() {
    return device_policy_.policy_blob_;
  }
  void set_policy_blob(const std::string& policy_blob) {
    device_policy_.policy_blob_ = policy_blob;
  }

  const std::string& device_local_account_policy_blob(
      const std::string& id) const {
    const std::map<std::string, PolicyState>::const_iterator entry =
        device_local_account_policy_.find(id);
    return entry == device_local_account_policy_.end() ?
        base::EmptyString() : entry->second.policy_blob_;
  }

  void set_device_local_account_policy_blob(const std::string& id,
                                            const std::string& policy_blob) {
    device_local_account_policy_[id].policy_blob_ = policy_blob;
  }

  // SessionManagerClient:
  void RetrieveDevicePolicy(const RetrievePolicyCallback& callback) override;
  std::string BlockingRetrieveDevicePolicy() override;
  void RetrieveDeviceLocalAccountPolicy(
      const std::string& account_id,
      const RetrievePolicyCallback& callback) override;
  std::string BlockingRetrieveDeviceLocalAccountPolicy(
      const std::string& account_id) override;
  void StoreDevicePolicy(const std::string& policy_blob,
                         const StorePolicyCallback& callback) override;
  void StoreDeviceLocalAccountPolicy(
      const std::string& account_id,
      const std::string& policy_blob,
      const StorePolicyCallback& callback) override;

 private:
  struct PolicyState {
    bool store_result_;
    std::string policy_blob_;
    std::vector<StorePolicyCallback> store_callbacks_;
    std::vector<RetrievePolicyCallback> retrieve_callbacks_;

    PolicyState();
    PolicyState(const PolicyState& other);
    ~PolicyState();

    bool HasPendingOperations() const {
      return !store_callbacks_.empty() || !retrieve_callbacks_.empty();
    }
  };

  PolicyState device_policy_;
  std::map<std::string, PolicyState> device_local_account_policy_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsTestHelper);
};

// Wraps the singleton device settings and initializes it to the point where it
// reports OWNERSHIP_NONE for the ownership status.
class ScopedDeviceSettingsTestHelper : public DeviceSettingsTestHelper {
 public:
  ScopedDeviceSettingsTestHelper();
  ~ScopedDeviceSettingsTestHelper() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedDeviceSettingsTestHelper);
};

// A convenience test base class that initializes a DeviceSettingsService
// instance for testing and allows for straightforward updating of device
// settings. |device_settings_service_| starts out in uninitialized state, so
// startup code gets tested as well.
class DeviceSettingsTestBase : public testing::Test {
 protected:
  DeviceSettingsTestBase();
  ~DeviceSettingsTestBase() override;

  void SetUp() override;
  void TearDown() override;

  // Flushes any pending device settings operations.
  void FlushDeviceSettings();

  // Triggers an owner key and device settings reload on
  // |device_settings_service_| and flushes the resulting load operation.
  void ReloadDeviceSettings();

  void InitOwner(const AccountId& account_id, bool tpm_is_ready);

  content::TestBrowserThreadBundle thread_bundle_;

  policy::DevicePolicyBuilder device_policy_;

  DeviceSettingsTestHelper device_settings_test_helper_;
  // Note that FakeUserManager is used by ProfileHelper, which some of the
  // tested classes depend on implicitly.
  FakeChromeUserManager* user_manager_;
  ScopedUserManagerEnabler user_manager_enabler_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  // Local DeviceSettingsService instance for tests. Avoid using in combination
  // with the global instance (DeviceSettingsService::Get()).
  DeviceSettingsService device_settings_service_;
  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<DBusThreadManagerSetter> dbus_setter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsTestBase);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_TEST_HELPER_H_
