// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_delegate_chromeos.h"

#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/login/login_state.h"
#include "media/audio/audio_manager.h"
#include "media/audio/sounds/sounds_manager.h"

namespace chromeos {

class SystemTrayDelegateChromeOSTest : public ash::test::AshTestBase {
 public:
  SystemTrayDelegateChromeOSTest()
      : user_manager_enabler_(new chromeos::FakeChromeUserManager) {}
  ~SystemTrayDelegateChromeOSTest() override {
    // The system clock must be destroyed before |settings_helper_|.
    g_browser_process->platform_part()->DestroySystemClock();
  }

  void SetUp() override {
    input_method::Initialize();
    ash::test::AshTestBase::SetUp();
    audio_manager_ = media::AudioManager::CreateForTesting(
        base::ThreadTaskRunnerHandle::Get());
    media::SoundsManager::Create();
    LoginState::Initialize();
    AccessibilityManager::Initialize();
  }

  void TearDown() override {
    AccessibilityManager::Shutdown();
    LoginState::Shutdown();
    media::SoundsManager::Shutdown();
    ash::test::AshTestBase::TearDown();
    input_method::Shutdown();
  }

 private:
  media::ScopedAudioManagerPtr audio_manager_;
  ScopedUserManagerEnabler user_manager_enabler_;
  ScopedCrosSettingsTestHelper settings_helper_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateChromeOSTest);
};

// This test is times out flakily: http://crbug.com/639938
// Test that checking bluetooth status early on does not cause a crash.
TEST_F(SystemTrayDelegateChromeOSTest, DISABLED_BluetoothStatus) {
  SystemTrayDelegateChromeOS delegate;
  EXPECT_FALSE(delegate.GetBluetoothAvailable());
  EXPECT_FALSE(delegate.GetBluetoothEnabled());
  EXPECT_FALSE(delegate.IsBluetoothDiscovering());
}

}  // namespace chromeos
