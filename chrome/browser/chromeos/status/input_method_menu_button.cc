// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/input_method_menu_button.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/views/widget/widget.h"

namespace {

PrefService* GetPrefService() {
  Profile* profile = ProfileManager::GetDefaultProfile();
  if (profile)
    return profile->GetPrefs();
  return NULL;
}

// A class which implements interfaces of chromeos::InputMethodMenu. This class
// is just for avoiding multiple inheritance.
class MenuImpl : public chromeos::InputMethodMenu {
 public:
  MenuImpl(chromeos::InputMethodMenuButton* button,
           PrefService* pref_service,
           chromeos::StatusAreaViewChromeos::ScreenMode screen_mode)
      : InputMethodMenu(pref_service, screen_mode, false), button_(button) {}

 private:
  // InputMethodMenu implementation.
  virtual void UpdateUI(const std::string& input_method_id,
                        const string16& name,
                        const string16& tooltip,
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

InputMethodMenuButton::InputMethodMenuButton(
    StatusAreaButton::Delegate* delegate,
    StatusAreaViewChromeos::ScreenMode screen_mode)
    : StatusAreaButton(delegate, this),
      menu_(new MenuImpl(this, GetPrefService(), screen_mode)),
      screen_mode_(screen_mode) {
  set_id(VIEW_ID_STATUS_BUTTON_INPUT_METHOD);
  UpdateUIFromCurrentInputMethod();
}

InputMethodMenuButton::~InputMethodMenuButton() {}

////////////////////////////////////////////////////////////////////////////////
// views::View implementation:

void InputMethodMenuButton::OnLocaleChanged() {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::GetInstance();
  manager->GetInputMethodUtil()->OnLocaleChanged();
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
  const views::Widget* current_window = GetWidget();
  if (!active_window || !current_window) {
    // Can't get an active or current window. Just return true as well.
    return true;
  }
  return active_window->GetNativeHandle() == current_window->GetNativeWindow();
}

void InputMethodMenuButton::UpdateUI(const std::string& input_method_id,
                                     const string16& name,
                                     const string16& tooltip,
                                     size_t num_active_input_methods) {
  // Hide the button only if there is only one input method, and the input
  // method is a XKB keyboard layout. We don't hide the button for other
  // types of input methods as these might have intra input method modes,
  // like Hiragana and Katakana modes in Japanese input methods.
  const bool hide_button =
      num_active_input_methods == 1 &&
      input_method::InputMethodUtil::IsKeyboardLayout(input_method_id) &&
      screen_mode_ == StatusAreaViewChromeos::BROWSER_MODE;
  SetVisible(!hide_button);
  SetText(name);
  SetTooltipText(tooltip);
  SetAccessibleName(tooltip);

  if (WindowIsActive()) {
    // We don't call these functions if the |current_window| is not active since
    // the calls are relatively expensive (crosbug.com/9206). Please note that
    // PrepareMenuModel() is necessary for fixing crosbug.com/7522 when the
    // window is active.
    menu_->PrepareMenuModel();
    SchedulePaint();
  }

  // TODO(yusukes): For a window which isn't on top, probably it's better to
  // update the texts when the window gets activated because SetTooltipText()
  // and SetText() are also expensive.
}

void InputMethodMenuButton::OpenConfigUI() {
  // Ask browser to open the WebUI page.
  delegate()->ExecuteStatusAreaCommand(
      this, StatusAreaButton::Delegate::SHOW_LANGUAGE_OPTIONS);
}

bool InputMethodMenuButton::ShouldSupportConfigUI() {
  return delegate()->ShouldExecuteStatusAreaCommand(
      this, StatusAreaButton::Delegate::SHOW_LANGUAGE_OPTIONS);
}

void InputMethodMenuButton::UpdateUIFromCurrentInputMethod() {
  input_method::InputMethodManager* input_method_manager =
      input_method::InputMethodManager::GetInstance();
  const input_method::InputMethodDescriptor& input_method =
      input_method_manager->current_input_method();
  const string16 name = InputMethodMenu::GetTextForIndicator(input_method);
  const string16 tooltip = InputMethodMenu::GetTextForMenu(input_method);
  const size_t num_active_input_methods =
      input_method_manager->GetNumActiveInputMethods();
  UpdateUI(input_method.id(), name, tooltip, num_active_input_methods);
}

}  // namespace chromeos
