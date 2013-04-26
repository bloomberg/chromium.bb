// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/policy/proto/chromeos/chrome_device_policy.pb.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
class MockDBusThreadManager;
}

namespace policy {

// Used to test Device policy changes in Chrome OS.
class DevicePolicyCrosBrowserTest :
    public chromeos::CrosInProcessBrowserTest {
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

  chromeos::MockDBusThreadManager* mock_dbus_thread_manager() {
    return mock_dbus_thread_manager_;
  }

  chromeos::FakeSessionManagerClient* session_manager_client() {
    return &session_manager_client_;
  }

  DevicePolicyBuilder* device_policy() { return &device_policy_; }

 private:
  // Set additional expectations on |mock_dbus_thread_manager_| to suppress
  // gmock warnings.
  void SetMockDBusThreadManagerExpectations();

  chromeos::MockDBusThreadManager* mock_dbus_thread_manager_;

  // Mock out policy loads/stores from/to the device.
  chromeos::FakeSessionManagerClient session_manager_client_;

  // Carries Chrome OS device policies for tests.
  DevicePolicyBuilder device_policy_;

  // Stores the device owner key.
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCrosBrowserTest);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_
