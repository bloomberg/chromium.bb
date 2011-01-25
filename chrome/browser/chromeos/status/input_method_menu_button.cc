// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/input_method_menu_button.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Returns PrefService object associated with |host|. Returns NULL if we are NOT
// within a browser.
PrefService* GetPrefService(chromeos::StatusAreaHost* host) {
  if (host->GetProfile()) {
    return host->GetProfile()->GetPrefs();
  }
  return NULL;
}

#if defined(CROS_FONTS_USING_BCI)
const int kFontSizeDelta = 0;
#else
const int kFontSizeDelta = 1;
#endif

// A class which implements interfaces of chromeos::InputMethodMenu. This class
// is just for avoiding multiple inheritance.
class MenuImpl : public chromeos::InputMethodMenu {
 public:
  MenuImpl(chromeos::InputMethodMenuButton* button,
           PrefService* pref_service,
           chromeos::StatusAreaHost::ScreenMode screen_mode)
      : InputMethodMenu(pref_service, screen_mode, false), button_(button) {}

 private:
  // InputMethodMenu implementation.
  virtual void UpdateUI(const std::string& input_method_id,
                        const std::wstring& name,
                        const std::wstring& tooltip,
                        size_t num_active_input_methods) {
    button_->UpdateUI(input_method_id, name, tooltip, num_active_input_methods);
  }
  virtual bool ShouldSupportConfigUI() {
    return button_->ShouldSupportConfigUI();
  }
  virtual void OpenConfigUI() {
    button_->OpenConfigUI();
  }
  // The UI (views button) to which this class delegates all requests.
  chromeos::InputMethodMenuButton* button_;

  DISALLOW_COPY_AND_ASSIGN(MenuImpl);
};

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// InputMethodMenuButton

InputMethodMenuButton::InputMethodMenuButton(StatusAreaHost* host)
    : StatusAreaButton(this),
      menu_(new MenuImpl(this, GetPrefService(host), host->GetScreenMode())),
      host_(host) {
  set_border(NULL);
  set_use_menu_button_paint(true);
  SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(kFontSizeDelta));
  SetEnabledColor(0xB3FFFFFF);  // White with 70% Alpha
  SetDisabledColor(0x00FFFFFF);  // White with 00% Alpha (invisible)
  SetShowMultipleIconStates(false);
  set_alignment(TextButton::ALIGN_CENTER);

  chromeos::KeyboardLibrary* keyboard_library =
      chromeos::CrosLibrary::Get()->GetKeyboardLibrary();
  const std::string hardware_keyboard_id =  // e.g. "xkb:us::eng"
      keyboard_library->GetHardwareKeyboardLayoutName();

  // Draw the default indicator "US". The default indicator "US" is used when
  // |pref_service| is not available (for example, unit tests) or |pref_service|
  // is available, but Chrome preferences are not available (for example,
  // initial OS boot).
  UpdateUI(hardware_keyboard_id, L"US", L"", 1);
}

////////////////////////////////////////////////////////////////////////////////
// views::View implementation:

gfx::Size InputMethodMenuButton::GetPreferredSize() {
  // If not enabled, then hide this button.
  if (!IsEnabled()) {
    return gfx::Size(0, 0);
  }
  return StatusAreaButton::GetPreferredSize();
}

void InputMethodMenuButton::OnLocaleChanged() {
  input_method::OnLocaleChanged();

  chromeos::InputMethodLibrary* input_method_library =
      chromeos::CrosLibrary::Get()->GetInputMethodLibrary();
  const InputMethodDescriptor& input_method =
      input_method_library->current_input_method();
  const std::wstring name = InputMethodMenu::GetTextForIndicator(input_method);
  const std::wstring tooltip = InputMethodMenu::GetTextForMenu(input_method);

  // In general, we should not call an input method API in the input method
  // button classes (status/input_menu_button*.cc) for performance reasons (see
  // http://crosbug.com/8284). However, since OnLocaleChanged is called only in
  // OOBE/Login screen which does not have two or more Chrome windows, it's okay
  // to call GetNumActiveInputMethods here.
  const size_t num_active_input_methods =
      input_method_library->GetNumActiveInputMethods();
  UpdateUI(input_method.id, name, tooltip, num_active_input_methods);

  Layout();
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// views::ViewMenuDelegate implementation:

void InputMethodMenuButton::RunMenu(views::View* unused_source,
                                    const gfx::Point& pt) {
  menu_->RunMenu(unused_source, pt);
}

void InputMethodMenuButton::UpdateUI(const std::string& input_method_id,
                                     const std::wstring& name,
                                     const std::wstring& tooltip,
                                     size_t num_active_input_methods) {
  // Hide the button only if there is only one input method, and the input
  // method is a XKB keyboard layout. We don't hide the button for other
  // types of input methods as these might have intra input method modes,
  // like Hiragana and Katakana modes in Japanese input methods.
  if (num_active_input_methods == 1 &&
      input_method::IsKeyboardLayout(input_method_id) &&
      host_->GetScreenMode() == StatusAreaHost::kBrowserMode) {
    // As the disabled color is set to invisible, disabling makes the
    // button disappear.
    SetEnabled(false);
    SetTooltipText(L"");  // remove tooltip
  } else {
    SetEnabled(true);
    SetTooltipText(tooltip);
  }
  SetText(name);
  SchedulePaint();
}

void InputMethodMenuButton::OpenConfigUI() {
  host_->OpenButtonOptions(this);  // ask browser to open the DOMUI page.
}

bool InputMethodMenuButton::ShouldSupportConfigUI() {
  return host_->ShouldOpenButtonOptions(this);
}

}  // namespace chromeos
