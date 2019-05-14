// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_TEST_HELPER_H_

#include "chrome/browser/chromeos/login/users/mock_user_manager.h"

class TestingProfile;

namespace plugin_vm {

// A helper class for enabling Plugin VM in unit tests.
class PluginVmTestHelper {
 public:
  explicit PluginVmTestHelper(TestingProfile* testing_profile);
  ~PluginVmTestHelper();

  void SetPolicyRequirementsToAllowPluginVm();
  void SetUserRequirementsToAllowPluginVm();
  void AllowPluginVm();

 private:
  TestingProfile* testing_profile_;
  chromeos::MockUserManager user_manager_;

  DISALLOW_COPY_AND_ASSIGN(PluginVmTestHelper);
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_TEST_HELPER_H_
