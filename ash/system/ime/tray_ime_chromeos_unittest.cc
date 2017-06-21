// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/tray_ime_chromeos.h"

#include <memory>
#include <vector>

#include "ash/accessibility_delegate.h"
#include "ash/accessibility_types.h"
#include "ash/ime/ime_controller.h"
#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "ash/system/ime_menu/ime_list_view.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {

class TrayIMETest : public test::AshTestBase {
 public:
  TrayIMETest() {}
  ~TrayIMETest() override {}

  views::View* default_view() const { return default_view_.get(); }

  views::View* detailed_view() const { return detailed_view_.get(); }

  // Mocks enabling the a11y virtual keyboard since the actual a11y manager
  // is not created in ash tests.
  void SetAccessibilityKeyboardEnabled(bool enabled);

  // Creates |count| simulated active IMEs.
  void SetActiveImeCount(int count);

  // Returns the view responsible for toggling virtual keyboard.
  views::View* GetToggleView() const;

  // Simulates IME being managed by policy.
  void SetImeManaged(bool managed);

  views::View* GetImeManagedIcon();

  void SetCurrentImeMenuItems(const std::vector<mojom::ImeMenuItem>& items);

  void SuppressKeyboard();
  void RestoreKeyboard();

  // test::AshTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  // Updates the ImeController with the latest IME test data.
  void RefreshImeController();

  std::unique_ptr<TrayIME> tray_;
  std::unique_ptr<views::View> default_view_;
  std::unique_ptr<views::View> detailed_view_;

  bool keyboard_suppressed_ = false;
  std::vector<ui::TouchscreenDevice> touchscreen_devices_to_restore_;
  std::vector<ui::InputDevice> keyboard_devices_to_restore_;

  mojom::ImeInfo current_ime_;
  std::vector<mojom::ImeInfo> available_imes_;
  std::vector<mojom::ImeMenuItem> menu_items_;

  DISALLOW_COPY_AND_ASSIGN(TrayIMETest);
};

void TrayIMETest::SetAccessibilityKeyboardEnabled(bool enabled) {
  Shell::Get()->accessibility_delegate()->SetVirtualKeyboardEnabled(enabled);
  keyboard::SetAccessibilityKeyboardEnabled(enabled);
  AccessibilityNotificationVisibility notification =
      enabled ? A11Y_NOTIFICATION_SHOW : A11Y_NOTIFICATION_NONE;
  Shell::Get()->system_tray_notifier()->NotifyAccessibilityModeChanged(
      notification);
}

void TrayIMETest::SetActiveImeCount(int count) {
  available_imes_.resize(count);
  for (int i = 0; i < count; ++i)
    available_imes_[i].id = base::IntToString(i);
  RefreshImeController();
}

views::View* TrayIMETest::GetToggleView() const {
  ImeListViewTestApi test_api(static_cast<ImeListView*>(detailed_view()));
  return test_api.GetToggleView();
}

void TrayIMETest::SetImeManaged(bool managed) {
  Shell::Get()->ime_controller()->SetImesManagedByPolicy(managed);
}

views::View* TrayIMETest::GetImeManagedIcon() {
  return tray_->GetControlledSettingIconForTesting();
}

void TrayIMETest::SetCurrentImeMenuItems(
    const std::vector<mojom::ImeMenuItem>& items) {
  menu_items_ = items;
  RefreshImeController();
}

void TrayIMETest::SuppressKeyboard() {
  DCHECK(!keyboard_suppressed_);
  keyboard_suppressed_ = true;

  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  touchscreen_devices_to_restore_ = device_manager->GetTouchscreenDevices();
  keyboard_devices_to_restore_ = device_manager->GetKeyboardDevices();

  ui::DeviceHotplugEventObserver* manager =
      ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  manager->OnTouchscreenDevicesUpdated(screens);

  std::vector<ui::InputDevice> keyboards;
  keyboards.push_back(ui::InputDevice(
      2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "keyboard"));
  manager->OnKeyboardDevicesUpdated(keyboards);
}

void TrayIMETest::RestoreKeyboard() {
  DCHECK(keyboard_suppressed_);
  ui::DeviceHotplugEventObserver* manager =
      ui::DeviceDataManager::GetInstance();
  manager->OnTouchscreenDevicesUpdated(touchscreen_devices_to_restore_);
  manager->OnKeyboardDevicesUpdated(keyboard_devices_to_restore_);
}

void TrayIMETest::SetUp() {
  test::AshTestBase::SetUp();
  tray_.reset(new TrayIME(GetPrimarySystemTray()));
  default_view_.reset(tray_->CreateDefaultView(LoginStatus::USER));
  detailed_view_.reset(tray_->CreateDetailedView(LoginStatus::USER));
}

void TrayIMETest::RefreshImeController() {
  std::vector<mojom::ImeInfoPtr> available_ime_ptrs;
  for (const auto& ime : available_imes_)
    available_ime_ptrs.push_back(ime.Clone());

  std::vector<mojom::ImeMenuItemPtr> menu_item_ptrs;
  for (const auto& item : menu_items_)
    menu_item_ptrs.push_back(item.Clone());

  Shell::Get()->ime_controller()->RefreshIme(current_ime_.id,
                                             std::move(available_ime_ptrs),
                                             std::move(menu_item_ptrs));
}

void TrayIMETest::TearDown() {
  if (keyboard_suppressed_)
    RestoreKeyboard();
  SetAccessibilityKeyboardEnabled(false);
  tray_.reset();
  default_view_.reset();
  detailed_view_.reset();
  test::AshTestBase::TearDown();
}

// Tests that if the keyboard is not suppressed the default view is hidden
// if less than 2 IMEs are present.
TEST_F(TrayIMETest, HiddenWithNoIMEs) {
  SetActiveImeCount(0);
  EXPECT_FALSE(default_view()->visible());
  SetActiveImeCount(1);
  EXPECT_FALSE(default_view()->visible());
  SetActiveImeCount(2);
  EXPECT_TRUE(default_view()->visible());
}

// Tests that if IMEs are managed, the default view is displayed even for a
// single IME.
TEST_F(TrayIMETest, ShownWithSingleIMEWhenManaged) {
  SetImeManaged(true);
  SetActiveImeCount(0);
  EXPECT_FALSE(default_view()->visible());
  SetActiveImeCount(1);
  EXPECT_TRUE(default_view()->visible());
  SetActiveImeCount(2);
  EXPECT_TRUE(default_view()->visible());
}

// Tests that if an IME has multiple properties the default view is
// displayed even for a single IME.
TEST_F(TrayIMETest, ShownWithSingleImeWithProperties) {
  SetActiveImeCount(1);
  EXPECT_FALSE(default_view()->visible());

  mojom::ImeMenuItem item1;
  mojom::ImeMenuItem item2;

  SetCurrentImeMenuItems({item1});
  EXPECT_FALSE(default_view()->visible());

  SetCurrentImeMenuItems({item1, item2});
  EXPECT_TRUE(default_view()->visible());
}

// Tests that when IMEs are managed an icon appears in the tray with an
// appropriate tooltip.
TEST_F(TrayIMETest, ImeManagedIcon) {
  SetActiveImeCount(1);
  EXPECT_FALSE(GetImeManagedIcon());

  SetImeManaged(true);
  views::View* icon = GetImeManagedIcon();
  ASSERT_TRUE(icon);
  base::string16 tooltip;
  icon->GetTooltipText(gfx::Point(), &tooltip);
  EXPECT_EQ(
      base::ASCIIToUTF16("Input methods are configured by your administrator."),
      tooltip);
}

// Tests that if no IMEs are present the default view is hidden when a11y is
// enabled.
TEST_F(TrayIMETest, HidesOnA11yEnabled) {
  // TODO: investigate failure in mash. http://crbug.com/695561.
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  SetActiveImeCount(0);
  SuppressKeyboard();
  EXPECT_TRUE(default_view()->visible());
  // Enable a11y keyboard.
  SetAccessibilityKeyboardEnabled(true);
  EXPECT_FALSE(default_view()->visible());
  // Disable the a11y keyboard.
  SetAccessibilityKeyboardEnabled(false);
  EXPECT_TRUE(default_view()->visible());
}

// Tests that clicking on the keyboard toggle causes the virtual keyboard
// to toggle between enabled and disabled.
TEST_F(TrayIMETest, PerformActionOnDetailedView) {
  // TODO: investigate failure in mash. http://crbug.com/695561.
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  SetActiveImeCount(0);
  SuppressKeyboard();
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
  views::View* toggle = GetToggleView();
  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  // Enable the keyboard.
  toggle->OnGestureEvent(&tap);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
  EXPECT_TRUE(default_view()->visible());

  // Clicking again should disable the keyboard.
  toggle = GetToggleView();
  tap = ui::GestureEvent(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  toggle->OnGestureEvent(&tap);
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
  EXPECT_TRUE(default_view()->visible());
}

}  // namespace ash
