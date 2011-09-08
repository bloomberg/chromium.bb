// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/keyboard_switch_menu.h"

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/button/menu_button.h"
#include "views/widget/widget.h"

namespace chromeos {

KeyboardSwitchMenu::KeyboardSwitchMenu()
    : InputMethodMenu(NULL /* pref_service */,
                      StatusAreaHost::kViewsLoginMode,
                      true /* for_out_of_box_experience_dialog */) {
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodMenu::InputMethodMenuHost implementation.
void KeyboardSwitchMenu::UpdateUI(const std::string& input_method_id,
                                  const std::wstring& name,
                                  const std::wstring& tooltip,
                                  size_t num_active_input_methods) {
  // Update all view hierarchies so that the new input method name is shown in
  // the menu button.
  views::Widget::NotifyLocaleChanged();
}

bool KeyboardSwitchMenu::ShouldSupportConfigUI() {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// views::ViewMenuDelegate implementation.
void KeyboardSwitchMenu::RunMenu(views::View* source, const gfx::Point& pt) {
  gfx::Point new_pt(pt);
  views::MenuButton* button = static_cast<views::MenuButton*>(source);
  // Keyboard switch menu is aligned on left by default.  MenuButton passes
  // in pt the lower left corner for RTL and the lower right corner for
  // non-RTL (with menu_offset applied).
  int reverse_offset = button->width() + button->menu_offset().x() * 2;
  if (base::i18n::IsRTL()) {
    new_pt.set_x(pt.x() + reverse_offset);
  } else {
    new_pt.set_x(pt.x() - reverse_offset);
  }

  set_menu_alignment(views::MenuItemView::TOPLEFT);
  InputMethodMenu::RunMenu(source, new_pt);
}

string16 KeyboardSwitchMenu::GetCurrentKeyboardName() const {
  const int count = GetItemCount();
  for (int i = 0; i < count; ++i) {
    if (IsItemCheckedAt(i))
      return GetLabelAt(i);
  }
  VLOG(1) << "The input method menu is not ready yet. Show the display "
          << "name of the current input method";
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::GetInstance();
  return UTF8ToUTF16(input_method::GetInputMethodDisplayNameFromId(
      manager->current_input_method().id()));
}

}  // namespace chromeos
