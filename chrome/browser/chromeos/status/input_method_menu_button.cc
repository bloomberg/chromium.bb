// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/input_method_menu_button.h"

#include <string>

#include "app/resource_bundle.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"

namespace {

// Returns PrefService object associated with |host|. Returns NULL if we are NOT
// within a browser.
PrefService* GetPrefService(chromeos::StatusAreaHost* host) {
  if (host->GetProfile()) {
    return host->GetProfile()->GetPrefs();
  }
  return NULL;
}

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// InputMethodMenuButton

InputMethodMenuButton::InputMethodMenuButton(StatusAreaHost* host)
    : StatusAreaButton(this),
      InputMethodMenu(GetPrefService(host),
                      host->IsBrowserMode(),
                      host->IsScreenLockerMode(),
                      false /* is_out_of_box_experience_mode */),
      host_(host) {
  set_border(NULL);
  set_use_menu_button_paint(true);
  SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(1));
  SetEnabledColor(0xB3FFFFFF);  // White with 70% Alpha
  SetDisabledColor(0x00FFFFFF);  // White with 00% Alpha (invisible)
  SetShowMultipleIconStates(false);
  set_alignment(TextButton::ALIGN_CENTER);

  // Draw the default indicator "US". The default indicator "US" is used when
  // |pref_service| is not available (for example, unit tests) or |pref_service|
  // is available, but Chrome preferences are not available (for example,
  // initial OS boot).
  InputMethodMenuButton::UpdateUI(L"US", L"");
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
  const InputMethodDescriptor& input_method =
      CrosLibrary::Get()->GetInputMethodLibrary()->current_input_method();
  UpdateUIFromInputMethod(input_method);
  Layout();
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodMenu::InputMethodMenuHost implementation:

void InputMethodMenuButton::UpdateUI(
    const std::wstring& name, const std::wstring& tooltip) {
  // Hide the button only if there is only one input method, and the input
  // method is a XKB keyboard layout. We don't hide the button for other
  // types of input methods as these might have intra input method modes,
  // like Hiragana and Katakana modes in Japanese input methods.
  scoped_ptr<InputMethodDescriptors> active_input_methods(
      CrosLibrary::Get()->GetInputMethodLibrary()->GetActiveInputMethods());
  if (active_input_methods->size() == 1 &&
      input_method::IsKeyboardLayout(active_input_methods->at(0).id) &&
      host_->IsBrowserMode()) {
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
