// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/keyboard_switch_menu.h"

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "grit/generated_resources.h"
#include "views/widget/widget_gtk.h"

namespace chromeos {

KeyboardSwitchMenu::KeyboardSwitchMenu()
    : InputMethodMenu(NULL /* pref_service */,
                      false /* is_browser_mode */,
                      false /* is_screen_locker */),
      delta_x_(0),
      delta_y_(0) {
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodMenu::InputMethodMenuHost implementation.
void KeyboardSwitchMenu::UpdateUI(
    const std::wstring& name, const std::wstring& tooltip) {
  // Update all view hierarchies so that the new input method name is shown in
  // the menu button.
  views::Widget::NotifyLocaleChanged();
}

////////////////////////////////////////////////////////////////////////////////
// views::ViewMenuDelegate implementation.
void KeyboardSwitchMenu::RunMenu(views::View* source, const gfx::Point& point) {
  gfx::Point offset_point(point);
  offset_point.Offset(delta_x_, delta_y_);
  InputMethodMenu::RunMenu(source, offset_point);
}

std::wstring KeyboardSwitchMenu::GetCurrentKeyboardName() const {
  const int count = GetItemCount();
  for (int i = 0; i < count; ++i) {
    if (IsItemCheckedAt(i)) {
      return UTF16ToWide(GetLabelAt(i));
    }
  }
  LOG(INFO) << "The input method menu is not ready yet. "
            << "Show a language name that matches the hardware keyboard layout";
  KeyboardLibrary *library = CrosLibrary::Get()->GetKeyboardLibrary();
  const std::string keyboard_layout_id =
      library->GetHardwareKeyboardLayoutName();
  const std::string language_code =
      input_method::GetLanguageCodeFromInputMethodId(keyboard_layout_id);
  return input_method::GetLanguageDisplayNameFromCode(language_code);
}

}  // namespace chromeos
