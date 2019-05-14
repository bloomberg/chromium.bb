// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_vm_plugin_dispatcher_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

class PluginVmManagerTest : public testing::Test {
 public:
  PluginVmManagerTest()
      : test_helper_(&testing_profile_), plugin_vm_manager_(&testing_profile_) {
    chromeos::DBusThreadManager::Initialize();
  }

  ~PluginVmManagerTest() override { chromeos::DBusThreadManager::Shutdown(); }

 protected:
  chromeos::FakeVmPluginDispatcherClient& VmPluginDispatcherClient() {
    return *static_cast<chromeos::FakeVmPluginDispatcherClient*>(
        chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient());
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile testing_profile_;
  PluginVmTestHelper test_helper_;
  PluginVmManager plugin_vm_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginVmManagerTest);
};

TEST_F(PluginVmManagerTest, LaunchPluginVmRequiresPluginVmAllowed) {
  EXPECT_FALSE(IsPluginVmAllowedForProfile(&testing_profile_));
  plugin_vm_manager_.LaunchPluginVm();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_FALSE(VmPluginDispatcherClient().show_vm_called());
}

TEST_F(PluginVmManagerTest, LaunchPluginVmStartAndShow) {
  test_helper_.AllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(&testing_profile_));

  // The PluginVmManager calls StartVm when the VM is not yet running.
  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
  list_vms_response.add_vm_info()->set_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);
  VmPluginDispatcherClient().set_list_vms_response(list_vms_response);

  plugin_vm_manager_.LaunchPluginVm();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_TRUE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_TRUE(VmPluginDispatcherClient().show_vm_called());
}

TEST_F(PluginVmManagerTest, LaunchPluginVmShow) {
  test_helper_.AllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(&testing_profile_));

  // The PluginVmManager skips calling StartVm when the VM is already running.
  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
  list_vms_response.add_vm_info()->set_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  VmPluginDispatcherClient().set_list_vms_response(list_vms_response);

  plugin_vm_manager_.LaunchPluginVm();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_TRUE(VmPluginDispatcherClient().show_vm_called());
}

}  // namespace plugin_vm
