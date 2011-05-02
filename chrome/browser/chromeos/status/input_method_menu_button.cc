// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/input_method_menu_button.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "views/window/window.h"

namespace {

// Returns PrefService object associated with |host|. Returns NULL if we are NOT
// within a browser.
PrefService* GetPrefService(chromeos::StatusAreaHost* host) {
  if (host->GetProfile()) {
    return host->GetProfile()->GetPrefs();
  }
  return NULL;
}

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
    : StatusAreaButton(host, this),
      menu_(new MenuImpl(this, GetPrefService(host), host->GetScreenMode())) {
  UpdateUIFromCurrentInputMethod();
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
  UpdateUIFromCurrentInputMethod();
  Layout();
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// views::ViewMenuDelegate implementation:

void InputMethodMenuButton::RunMenu(views::View* unused_source,
                                    const gfx::Point& pt) {
  menu_->RunMenu(unused_source, pt);
}

bool InputMethodMenuButton::WindowIsActive() {
  Browser* active_browser = BrowserList::GetLastActive();
  if (!active_browser) {
    // Can't get an active browser. Just return true, which is safer.
    return true;
  }
  BrowserWindow* active_window = active_browser->window();
  const views::Window* current_window = GetWindow();
  if (!active_window || !current_window) {
    // Can't get an active or current window. Just return true as well.
    return true;
  }
  return active_window->GetNativeHandle() == current_window->GetNativeWindow();
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

  if (WindowIsActive()) {
    // We don't call these functions if the |current_window| is not active since
    // the calls are relatively expensive (crosbug.com/9206). Please note that
    // PrepareMenu() is necessary for fixing crosbug.com/7522 when the window
    // is active.
    menu_->PrepareMenu();
    SchedulePaint();
  }

  // TODO(yusukes): For a window which isn't on top, probably it's better to
  // update the texts when the window gets activated because SetTooltipText()
  // and SetText() are also expensive.
}

void InputMethodMenuButton::OpenConfigUI() {
  host_->OpenButtonOptions(this);  // ask browser to open the WebUI page.
}

bool InputMethodMenuButton::ShouldSupportConfigUI() {
  return host_->ShouldOpenButtonOptions(this);
}

void InputMethodMenuButton::UpdateUIFromCurrentInputMethod() {
  chromeos::InputMethodLibrary* input_method_library =
      chromeos::CrosLibrary::Get()->GetInputMethodLibrary();
  const InputMethodDescriptor& input_method =
      input_method_library->current_input_method();
  const std::wstring name = InputMethodMenu::GetTextForIndicator(input_method);
  const std::wstring tooltip = InputMethodMenu::GetTextForMenu(input_method);
  const size_t num_active_input_methods =
      input_method_library->GetNumActiveInputMethods();
  UpdateUI(input_method.id, name, tooltip, num_active_input_methods);
}

}  // namespace chromeos
