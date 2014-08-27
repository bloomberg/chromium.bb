// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {
class FakeSessionManagerClient;
}

namespace policy {

class DevicePolicyCrosTestHelper {
 public:
  DevicePolicyCrosTestHelper();
  ~DevicePolicyCrosTestHelper();

  // Marks the device as enterprise-owned. Must be called to make device
  // policies apply Chrome-wide. If this is not called, device policies will
  // affect CrosSettings only.
  void MarkAsEnterpriseOwned();

  // Writes the owner key to disk. To be called before installing a policy.
  void InstallOwnerKey();

  DevicePolicyBuilder* device_policy() { return &device_policy_; }

 private:
  void OverridePaths();

  // Carries Chrome OS device policies for tests.
  DevicePolicyBuilder device_policy_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCrosTestHelper);
};

// Used to test Device policy changes in Chrome OS.
class DevicePolicyCrosBrowserTest : public InProcessBrowserTest {
 protected:
  DevicePolicyCrosBrowserTest();
  virtual ~DevicePolicyCrosBrowserTest();

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE;

  // Marks the device as enterprise-owned. Must be called to make device
  // policies apply Chrome-wide. If this is not called, device policies will
  // affect CrosSettings only.
  void MarkAsEnterpriseOwned();

  // Writes the owner key to disk. To be called before installing a policy.
  void InstallOwnerKey();

  // Reinstalls |device_policy_| as the policy (to be used when it was
  // recently changed).
  void RefreshDevicePolicy();

  chromeos::DBusThreadManagerSetter* dbus_setter() {
    return dbus_setter_.get();
  }

  chromeos::FakeSessionManagerClient* session_manager_client() {
    return fake_session_manager_client_;
  }

  DevicePolicyBuilder* device_policy() { return test_helper_.device_policy(); }

 private:
  DevicePolicyCrosTestHelper test_helper_;

  // FakeDBusThreadManager uses FakeSessionManagerClient.
  scoped_ptr<chromeos::DBusThreadManagerSetter> dbus_setter_;
  chromeos::FakeSessionManagerClient* fake_session_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCrosBrowserTest);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_
