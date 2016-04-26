// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller.h"

#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/ash_interactive_ui_test_base.h"
#include "ash/test/test_screenshot_delegate.h"
#include "ash/test/test_volume_control_delegate.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/run_loop.h"
#include "chromeos/network/network_handler.h"
#include "ui/base/test/ui_controls.h"

namespace ash {
namespace test {

namespace {

// A network observer to watch for the toggle wifi events.
class TestNetworkObserver : public NetworkObserver {
 public:
  TestNetworkObserver() : wifi_enabled_status_(false) {}

  // ash::NetworkObserver:
  void RequestToggleWifi() override {
    wifi_enabled_status_ = !wifi_enabled_status_;
  }

  bool wifi_enabled_status() const { return wifi_enabled_status_; }

 private:
  bool wifi_enabled_status_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkObserver);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////

// This is intended to test few samples from each category of accelerators to
// make sure they work properly. The test is done as an interactive ui test
// using ui_controls::Send*() functions.
// This is to catch any future regressions (crbug.com/469235).
class AcceleratorInteractiveUITest : public AshInteractiveUITestBase,
                                     public ShellObserver {
 public:
  AcceleratorInteractiveUITest() : is_in_overview_mode_(false) {}

  void SetUp() override {
    AshInteractiveUITestBase::SetUp();

    Shell::GetInstance()->AddShellObserver(this);

    chromeos::NetworkHandler::Initialize();
  }

  void TearDown() override {
    chromeos::NetworkHandler::Shutdown();

    Shell::GetInstance()->RemoveShellObserver(this);

    AshInteractiveUITestBase::TearDown();
  }

  // Sends a key press event and waits synchronously until it's completely
  // processed.
  void SendKeyPressSync(ui::KeyboardCode key,
                        bool control,
                        bool shift,
                        bool alt) {
    base::RunLoop loop;
    ui_controls::SendKeyPressNotifyWhenDone(root_window(), key, control, shift,
                                            alt, false, loop.QuitClosure());
    loop.Run();
  }

  // ash::ShellObserver:
  void OnOverviewModeStarting() override { is_in_overview_mode_ = true; }
  void OnOverviewModeEnded() override { is_in_overview_mode_ = false; }

  Shell* shell() const { return Shell::GetInstance(); }
  aura::Window* root_window() const { return Shell::GetPrimaryRootWindow(); }

 protected:
  bool is_in_overview_mode_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratorInteractiveUITest);
};

////////////////////////////////////////////////////////////////////////////////

#if defined(OFFICIAL_BUILD)
#define MAYBE_NonRepeatableNeedingWindowActions \
  DISABLED_NonRepeatableNeedingWindowActions
#define MAYBE_ChromeOsAccelerators DISABLED_ChromeOsAccelerators
#define MAYBE_ToggleAppList DISABLED_ToggleAppList
#else
#define MAYBE_NonRepeatableNeedingWindowActions \
  NonRepeatableNeedingWindowActions
#define MAYBE_ChromeOsAccelerators ChromeOsAccelerators
#define MAYBE_ToggleAppList ToggleAppList
#endif  // defined(OFFICIAL_BUILD)

// Tests a sample of the non-repeatable accelerators that need windows to be
// enabled.
TEST_F(AcceleratorInteractiveUITest, MAYBE_NonRepeatableNeedingWindowActions) {
  // Create a bunch of windows to work with.
  aura::Window* window_1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100));
  aura::Window* window_2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100));
  window_1->Show();
  wm::ActivateWindow(window_1);
  window_2->Show();
  wm::ActivateWindow(window_2);

  // Test TOGGLE_OVERVIEW.
  EXPECT_FALSE(is_in_overview_mode_);
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP1, false, false, false);
  EXPECT_TRUE(is_in_overview_mode_);
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP1, false, false, false);
  EXPECT_FALSE(is_in_overview_mode_);

  // Test CYCLE_FORWARD_MRU and CYCLE_BACKWARD_MRU.
  wm::ActivateWindow(window_1);
  EXPECT_TRUE(wm::IsActiveWindow(window_1));
  EXPECT_FALSE(wm::IsActiveWindow(window_2));
  SendKeyPressSync(ui::VKEY_TAB, false, false, true);  // CYCLE_FORWARD_MRU.
  EXPECT_TRUE(wm::IsActiveWindow(window_2));
  EXPECT_FALSE(wm::IsActiveWindow(window_1));
  SendKeyPressSync(ui::VKEY_TAB, false, true, true);  // CYCLE_BACKWARD_MRU.
  EXPECT_TRUE(wm::IsActiveWindow(window_1));
  EXPECT_FALSE(wm::IsActiveWindow(window_2));

  // Test TOGGLE_FULLSCREEN.
  wm::WindowState* active_window_state = wm::GetActiveWindowState();
  EXPECT_FALSE(active_window_state->IsFullscreen());
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP2, false, false, false);
  EXPECT_TRUE(active_window_state->IsFullscreen());
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP2, false, false, false);
  EXPECT_FALSE(active_window_state->IsFullscreen());
}

// Tests a sample of ChromeOS specific accelerators.
TEST_F(AcceleratorInteractiveUITest, MAYBE_ChromeOsAccelerators) {
  // Test TAKE_SCREENSHOT and TAKE_PARTIAL_SCREENSHOT.
  TestScreenshotDelegate* screenshot_delegate = GetScreenshotDelegate();
  screenshot_delegate->set_can_take_screenshot(true);
  EXPECT_EQ(0, screenshot_delegate->handle_take_screenshot_count());
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP1, true, false, false);
  EXPECT_EQ(1, screenshot_delegate->handle_take_screenshot_count());
  SendKeyPressSync(ui::VKEY_PRINT, false, false, false);
  EXPECT_EQ(2, screenshot_delegate->handle_take_screenshot_count());
  SendKeyPressSync(ui::VKEY_MEDIA_LAUNCH_APP1, true, true, false);
  EXPECT_EQ(2, screenshot_delegate->handle_take_screenshot_count());
  // Press ESC to go out of the partial screenshot mode.
  SendKeyPressSync(ui::VKEY_ESCAPE, false, false, false);

  // Test VOLUME_MUTE, VOLUME_DOWN, and VOLUME_UP.
  TestVolumeControlDelegate* volume_delegate = new TestVolumeControlDelegate;
  shell()->system_tray_delegate()->SetVolumeControlDelegate(
      std::unique_ptr<VolumeControlDelegate>(volume_delegate));
  // VOLUME_MUTE.
  EXPECT_EQ(0, volume_delegate->handle_volume_mute_count());
  SendKeyPressSync(ui::VKEY_VOLUME_MUTE, false, false, false);
  EXPECT_EQ(1, volume_delegate->handle_volume_mute_count());
  // VOLUME_DOWN.
  EXPECT_EQ(0, volume_delegate->handle_volume_down_count());
  SendKeyPressSync(ui::VKEY_VOLUME_DOWN, false, false, false);
  EXPECT_EQ(1, volume_delegate->handle_volume_down_count());
  // VOLUME_UP.
  EXPECT_EQ(0, volume_delegate->handle_volume_up_count());
  SendKeyPressSync(ui::VKEY_VOLUME_UP, false, false, false);
  EXPECT_EQ(1, volume_delegate->handle_volume_up_count());

  // Test TOGGLE_WIFI.
  TestNetworkObserver network_observer;
  shell()->system_tray_notifier()->AddNetworkObserver(&network_observer);

  EXPECT_FALSE(network_observer.wifi_enabled_status());
  SendKeyPressSync(ui::VKEY_WLAN, false, false, false);
  EXPECT_TRUE(network_observer.wifi_enabled_status());
  SendKeyPressSync(ui::VKEY_WLAN, false, false, false);
  EXPECT_FALSE(network_observer.wifi_enabled_status());

  shell()->system_tray_notifier()->RemoveNetworkObserver(&network_observer);
}

// Tests the app list accelerator.
TEST_F(AcceleratorInteractiveUITest, MAYBE_ToggleAppList) {
  EXPECT_FALSE(shell()->GetAppListTargetVisibility());
  SendKeyPressSync(ui::VKEY_LWIN, false, false, false);
  EXPECT_TRUE(shell()->GetAppListTargetVisibility());
  SendKeyPressSync(ui::VKEY_LWIN, false, false, false);
  EXPECT_FALSE(shell()->GetAppListTargetVisibility());
}

}  // namespace test
}  // namespace ash
