// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/ime_menu/ime_menu_tray.h"

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/accessibility_delegate.h"
#include "ash/common/system/chromeos/ime_menu/ime_list_view.h"
#include "ash/common/system/status_area_widget.h"
#include "ash/common/system/tray/ime_info.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/test/test_system_tray_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/status_area_widget_test_helper.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/mock_input_method_manager.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"

using base::UTF8ToUTF16;

namespace ash {

ImeMenuTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->ime_menu_tray();
}

class ImeMenuTrayTest : public test::AshTestBase {
 public:
  ImeMenuTrayTest() {}
  ~ImeMenuTrayTest() override {}

 protected:
  // Returns true if the IME menu tray is visible.
  bool IsVisible() { return GetTray()->visible(); }

  // Returns the label text of the tray.
  const base::string16& GetTrayText() { return GetTray()->label_->text(); }

  // Returns true if the background color of the tray is active.
  bool IsTrayBackgroundActive() { return GetTray()->is_active(); }

  // Returns true if the IME menu bubble has been shown.
  bool IsBubbleShown() { return GetTray()->IsImeMenuBubbleShown(); }

  // Returns true if the IME menu list has been updated with the right IME list.
  bool IsTrayImeListValid(const std::vector<IMEInfo>& expected_imes,
                          const IMEInfo& expected_current_ime) {
    const std::map<views::View*, std::string>& ime_map =
        ImeListViewTestApi(GetTray()->ime_list_view_).ime_map();
    if (ime_map.size() != expected_imes.size())
      return false;

    std::vector<std::string> expected_ime_ids;
    for (const auto& ime : expected_imes) {
      expected_ime_ids.push_back(ime.id);
    }
    for (const auto& ime : ime_map) {
      // Tests that all the IMEs on the view is in the list of selected IMEs.
      if (std::find(expected_ime_ids.begin(), expected_ime_ids.end(),
                    ime.second) == expected_ime_ids.end()) {
        return false;
      }

      // Tests that the checked IME is the current IME.
      ui::AXNodeData node_data;
      node_data.state = 0;
      ime.first->GetAccessibleNodeData(&node_data);
      if (node_data.HasStateFlag(ui::AX_STATE_CHECKED)) {
        if (ime.second != expected_current_ime.id)
          return false;
      }
    }
    return true;
  }

  // Focuses in the given type of input context.
  void FocusInInputContext(ui::TextInputType input_type) {
    ui::IMEEngineHandlerInterface::InputContext input_context(
        input_type, ui::TEXT_INPUT_MODE_DEFAULT, ui::TEXT_INPUT_FLAG_NONE);
    ui::IMEBridge::Get()->SetCurrentInputContext(input_context);
  }

 private:
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
  GetTray()->PerformAction(tap);
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  GetTray()->PerformAction(tap);
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(IsBubbleShown());

  // If disabling the IME menu feature when the menu tray is activated, the tray
  // element will be deactivated.
  GetTray()->PerformAction(tap);
  EXPECT_TRUE(IsTrayBackgroundActive());
  WmShell::Get()->system_tray_notifier()->NotifyRefreshIMEMenu(false);
  EXPECT_FALSE(IsVisible());
  EXPECT_FALSE(IsBubbleShown());
  EXPECT_FALSE(IsTrayBackgroundActive());
}

// Tests that IME menu list updates when changing the current IME. This should
// only happen by using shortcuts (Ctrl + Space / Ctrl + Shift + Space) to
// switch IMEs.
TEST_F(ImeMenuTrayTest, RefreshImeWithListViewCreated) {
  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);

  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  IMEInfo info1, info2, info3;
  info1.id = "ime1";
  info1.name = UTF8ToUTF16("English");
  info1.medium_name = UTF8ToUTF16("English");
  info1.short_name = UTF8ToUTF16("US");
  info1.third_party = false;
  info1.selected = true;

  info2.id = "ime2";
  info2.name = UTF8ToUTF16("English UK");
  info2.medium_name = UTF8ToUTF16("English UK");
  info2.short_name = UTF8ToUTF16("UK");
  info2.third_party = true;
  info2.selected = false;

  info3.id = "ime3";
  info3.name = UTF8ToUTF16("Pinyin");
  info3.medium_name = UTF8ToUTF16("Chinese Pinyin");
  info3.short_name = UTF8ToUTF16("拼");
  info3.third_party = false;
  info3.selected = false;

  std::vector<IMEInfo> ime_info_list{info1, info2, info3};

  GetSystemTrayDelegate()->SetAvailableIMEList(ime_info_list);
  GetSystemTrayDelegate()->SetCurrentIME(info1);
  WmShell::Get()->system_tray_notifier()->NotifyRefreshIME();
  EXPECT_EQ(UTF8ToUTF16("US"), GetTrayText());
  EXPECT_TRUE(IsTrayImeListValid(ime_info_list, info1));

  ime_info_list[0].selected = false;
  ime_info_list[2].selected = true;
  GetSystemTrayDelegate()->SetAvailableIMEList(ime_info_list);
  GetSystemTrayDelegate()->SetCurrentIME(info3);
  WmShell::Get()->system_tray_notifier()->NotifyRefreshIME();
  EXPECT_EQ(UTF8ToUTF16("拼"), GetTrayText());
  EXPECT_TRUE(IsTrayImeListValid(ime_info_list, info3));

  // Closes the menu before quitting.
  GetTray()->PerformAction(tap);
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(IsBubbleShown());
}

// Tests that quits Chrome with IME menu openned will not crash.
TEST_F(ImeMenuTrayTest, QuitChromeWithMenuOpen) {
  WmShell::Get()->system_tray_notifier()->NotifyRefreshIMEMenu(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());

  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());
}

// Tests using 'Alt+Shift+K' to open the menu.
TEST_F(ImeMenuTrayTest, TestAccelerator) {
  WmShell::Get()->system_tray_notifier()->NotifyRefreshIMEMenu(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());

  WmShell::Get()->accelerator_controller()->PerformActionIfEnabled(
      SHOW_IME_MENU_BUBBLE);
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);
  EXPECT_FALSE(IsTrayBackgroundActive());
  EXPECT_FALSE(IsBubbleShown());
}

TEST_F(ImeMenuTrayTest, ShowEmojiKeyset) {
  WmShell::Get()->system_tray_notifier()->NotifyRefreshIMEMenu(true);
  ASSERT_TRUE(IsVisible());
  ASSERT_FALSE(IsTrayBackgroundActive());

  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);
  EXPECT_TRUE(IsTrayBackgroundActive());
  EXPECT_TRUE(IsBubbleShown());

  AccessibilityDelegate* accessibility_delegate =
      WmShell::Get()->accessibility_delegate();

  accessibility_delegate->SetVirtualKeyboardEnabled(true);
  EXPECT_TRUE(accessibility_delegate->IsVirtualKeyboardEnabled());

  GetTray()->ShowKeyboardWithKeyset("emoji");
  // The menu should be hidden.
  EXPECT_FALSE(IsBubbleShown());
  // The virtual keyboard should be enabled.
  EXPECT_TRUE(accessibility_delegate->IsVirtualKeyboardEnabled());

  // Hides the keyboard.
  GetTray()->OnKeyboardHidden();
  // The keyboard should still be enabled.
  EXPECT_TRUE(accessibility_delegate->IsVirtualKeyboardEnabled());
}

TEST_F(ImeMenuTrayTest, ForceToShowEmojiKeyset) {
  AccessibilityDelegate* accessibility_delegate =
      WmShell::Get()->accessibility_delegate();
  accessibility_delegate->SetVirtualKeyboardEnabled(false);
  ASSERT_FALSE(accessibility_delegate->IsVirtualKeyboardEnabled());

  GetTray()->ShowKeyboardWithKeyset("emoji");
  // The virtual keyboard should be enabled.
  EXPECT_TRUE(accessibility_delegate->IsVirtualKeyboardEnabled());

  // Hides the keyboard.
  GetTray()->OnKeyboardHidden();
  // The keyboard should still be disabled.
  EXPECT_FALSE(accessibility_delegate->IsVirtualKeyboardEnabled());
}

TEST_F(ImeMenuTrayTest, ShowEmojiHandwritingVoiceButtons) {
  FocusInInputContext(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_FALSE(GetTray()->ShouldShowEmojiHandwritingVoiceButtons());

  chromeos::input_method::InputMethodManager* input_method_manager =
      chromeos::input_method::InputMethodManager::Get();
  EXPECT_FALSE(input_method_manager);
  chromeos::input_method::InputMethodManager::Initialize(
      new chromeos::input_method::MockInputMethodManager);
  input_method_manager = chromeos::input_method::InputMethodManager::Get();
  EXPECT_TRUE(input_method_manager &&
              input_method_manager->IsEmojiHandwritingVoiceOnImeMenuEnabled());
  EXPECT_TRUE(GetTray()->ShouldShowEmojiHandwritingVoiceButtons());

  FocusInInputContext(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_FALSE(GetTray()->ShouldShowEmojiHandwritingVoiceButtons());
}

}  // namespace ash
