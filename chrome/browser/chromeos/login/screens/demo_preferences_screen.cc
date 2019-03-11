// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/demo_preferences_screen.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/screens/demo_preferences_screen_view.h"
#include "chrome/browser/chromeos/login/screens/welcome_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"

namespace chromeos {

namespace {

constexpr char kUserActionContinue[] = "continue-setup";
constexpr char kUserActionClose[] = "close-setup";

constexpr char kContextKeyLocale[] = "locale";
constexpr char kContextKeyInputMethod[] = "input-method";
constexpr char kContextKeyDemoModeCountry[] = "demo-mode-country";

WelcomeScreen* GetWelcomeScreen() {
  const WizardController* wizard_controller =
      WizardController::default_controller();
  DCHECK(wizard_controller);
  return WelcomeScreen::Get(wizard_controller->screen_manager());
}

// Sets locale and input method. If |locale| or |input_method| is empty then
// they will not be changed.
void SetApplicationLocaleAndInputMethod(const std::string& locale,
                                        const std::string& input_method) {
  WelcomeScreen* welcome_screen = GetWelcomeScreen();
  DCHECK(welcome_screen);
  welcome_screen->SetApplicationLocaleAndInputMethod(locale, input_method);
}

}  // namespace

DemoPreferencesScreen::DemoPreferencesScreen(
    DemoPreferencesScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES),
      input_manager_observer_(this),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);

  // TODO(agawronska): Add tests for locale and input changes.
  input_method::InputMethodManager* input_manager =
      input_method::InputMethodManager::Get();
  UpdateInputMethod(input_manager);
  input_manager_observer_.Add(input_manager);
}

DemoPreferencesScreen::~DemoPreferencesScreen() {
  input_method::InputMethodManager::Get()->RemoveObserver(this);

  if (view_)
    view_->Bind(nullptr);
}

void DemoPreferencesScreen::Show() {
  WelcomeScreen* welcome_screen = GetWelcomeScreen();
  if (welcome_screen) {
    initial_locale_ = welcome_screen->GetApplicationLocale();
    initial_input_method_ = welcome_screen->GetInputMethod();
  }

  if (view_)
    view_->Show();
}

void DemoPreferencesScreen::Hide() {
  initial_locale_.clear();
  initial_input_method_.clear();

  if (view_)
    view_->Hide();
}

void DemoPreferencesScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionContinue) {
    exit_callback_.Run(Result::COMPLETED);
  } else if (action_id == kUserActionClose) {
    // Restore initial locale and input method if the user pressed back button.
    SetApplicationLocaleAndInputMethod(initial_locale_, initial_input_method_);
    exit_callback_.Run(Result::CANCELED);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void DemoPreferencesScreen::OnContextKeyUpdated(
    const ::login::ScreenContext::KeyType& key) {
  if (key == kContextKeyLocale) {
    SetApplicationLocaleAndInputMethod(context_.GetString(key), std::string());
  } else if (key == kContextKeyInputMethod) {
    SetApplicationLocaleAndInputMethod(std::string(), context_.GetString(key));
  } else if (key == kContextKeyDemoModeCountry) {
    g_browser_process->local_state()->SetString(prefs::kDemoModeCountry,
                                                context_.GetString(key));
  } else {
    BaseScreen::OnContextKeyUpdated(key);
  }
}

void DemoPreferencesScreen::OnViewDestroyed(DemoPreferencesScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void DemoPreferencesScreen::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* profile,
    bool show_message) {
  UpdateInputMethod(manager);
}

void DemoPreferencesScreen::UpdateInputMethod(
    input_method::InputMethodManager* input_manager) {
  const input_method::InputMethodDescriptor input_method =
      input_manager->GetActiveIMEState()->GetCurrentInputMethod();
  GetContextEditor().SetString(kContextKeyInputMethod, input_method.id());
}

}  // namespace chromeos
