// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/mock_dbus_thread_manager_without_gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
class FakeSessionManagerClient;
}

namespace policy {

// Used to test Device policy changes in Chrome OS.
class DevicePolicyCrosBrowserTest : public InProcessBrowserTest {
 public:
  // Marks the device as enterprise-owned. Must be called to make device
  // policies apply Chrome-wide. If this is not called, device policies will
  // affect CrosSettings only.
  void MarkAsEnterpriseOwned();

 protected:
  DevicePolicyCrosBrowserTest();
  virtual ~DevicePolicyCrosBrowserTest();

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;

  // Writes the owner key to disk. To be called before installing a policy.
  void InstallOwnerKey();

  // Reinstalls |device_policy_| as the policy (to be used when it was
  // recently changed).
  void RefreshDevicePolicy();

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE;

  chromeos::MockDBusThreadManagerWithoutGMock* mock_dbus_thread_manager() {
    return mock_dbus_thread_manager_;
  }

  chromeos::FakeSessionManagerClient* session_manager_client() {
    return mock_dbus_thread_manager_->fake_session_manager_client();
  }

  DevicePolicyBuilder* device_policy() { return &device_policy_; }

 private:
  // Stores the device owner key and the install attributes.
  base::ScopedTempDir temp_dir_;

  // MockDBusThreadManagerWithoutGMock uses FakeSessionManagerClient.
  chromeos::MockDBusThreadManagerWithoutGMock* mock_dbus_thread_manager_;

  // Carries Chrome OS device policies for tests.
  DevicePolicyBuilder device_policy_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCrosBrowserTest);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_
