// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/ime_menu/ime_menu_tray.h"

#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_system_tray_delegate.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"

using base::UTF8ToUTF16;

namespace ash {

class ImeMenuTrayTest : public test::AshTestBase {
 public:
  ImeMenuTrayTest() {}
  ~ImeMenuTrayTest() override {}

  ImeMenuTray* ime_menu_tray() { return ime_menu_tray_.get(); }

  // test::AshTestBase:
  void SetUp() override {
    test::AshTestBase::SetUp();
    ime_menu_tray_.reset(new ImeMenuTray(GetPrimaryShelf()));
  }

  void TearDown() override {
    ime_menu_tray_.reset();
    test::AshTestBase::TearDown();
  }

 protected:
  // Returns true if the IME menu tray is visible.
  bool IsVisible() { return ime_menu_tray_->visible(); }

  // Returns the label text of the tray.
  const base::string16& GetTrayText() { return ime_menu_tray_->label_->text(); }

  // Returns true if the background color of the tray is active.
  bool IsTrayBackgroundActive() {
    return ime_menu_tray_->draw_background_as_active();
  }

 private:
  std::unique_ptr<ImeMenuTray> ime_menu_tray_;

  DISALLOW_COPY_AND_ASSIGN(ImeMenuTrayTest);
};

// Tests that visibility of IME menu tray should be consistent with the
// activation of the IME menu.
TEST_F(ImeMenuTrayTest, ImeMenuTrayVisibility) {
  ASSERT_FALSE(IsVisible());

  WmShell::Get()->system_tray_notifier()->NotifyRefreshIMEMenu(true);
  EXPECT_TRUE(IsVisible());

  WmShell::Get()->system_tray_notifier()->NotifyRefreshIMEMenu(false);
  EXPECT_FALSE(IsVisible());
}

// Tests that IME menu tray shows the right info of the current IME.
TEST_F(ImeMenuTrayTest, TrayLabelTest) {
  WmShell::Get()->system_tray_notifier()->NotifyRefreshIMEMenu(true);
  ASSERT_TRUE(IsVisible());

  // Changes the input method to "ime1".
  IMEInfo info1;
  info1.id = "ime1";
  info1.name = UTF8ToUTF16("English");
  info1.medium_name = UTF8ToUTF16("English");
  info1.short_name = UTF8ToUTF16("US");
  info1.third_party = false;
  info1.selected = true;
  GetSystemTrayDelegate()->SetCurrentIME(info1);
  WmShell::Get()->system_tray_notifier()->NotifyRefreshIME();
  EXPECT_EQ(UTF8ToUTF16("US"), GetTrayText());

  // Changes the input method to a third-party IME extension.
  IMEInfo info2;
  info2.id = "ime2";
  info2.name = UTF8ToUTF16("English UK");
  info2.medium_name = UTF8ToUTF16("English UK");
  info2.short_name = UTF8ToUTF16("UK");
  info2.third_party = true;
  info2.selected = true;
  GetSystemTrayDelegate()->SetCurrentIME(info2);
  WmShell::Get()->system_tray_notifier()->NotifyRefreshIME();
  EXPECT_EQ(UTF8ToUTF16("UK*"), GetTrayText());
}

// Tests that IME menu tray changes background color when tapped/clicked. And
// tests that the background color becomes 'inactive' when disabling the IME
// menu feature.
TEST_F(ImeMenuTrayTest, PerformAction) {
  WmShell::Get()->system_tray_notifier()->NotifyRefreshIMEMenu(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());

  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  ime_menu_tray()->PerformAction(tap);
  EXPECT_TRUE(IsTrayBackgroundActive());

  ime_menu_tray()->PerformAction(tap);
  EXPECT_FALSE(IsTrayBackgroundActive());

  // If disabling the IME menu feature when the menu tray is activated, the tray
  // element will be deactivated.
  ime_menu_tray()->PerformAction(tap);
  EXPECT_TRUE(IsTrayBackgroundActive());
  WmShell::Get()->system_tray_notifier()->NotifyRefreshIMEMenu(false);
  EXPECT_FALSE(IsVisible());
  EXPECT_FALSE(IsTrayBackgroundActive());
}

}  // namespace ash
