// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"

#include "ash/magnifier/magnifier_constants.h"
#include "ash/shell.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_version_info.h"
#include "chromeos/chromeos_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/keyboard/keyboard_controller.h"

namespace {

const char kJsScreenPath[] = "cr.ui.Oobe";

// JS API callbacks names.
const char kJsApiEnableHighContrast[] = "enableHighContrast";
const char kJsApiEnableVirtualKeyboard[] = "enableVirtualKeyboard";
const char kJsApiEnableScreenMagnifier[] = "enableScreenMagnifier";
const char kJsApiEnableLargeCursor[] = "enableLargeCursor";
const char kJsApiEnableSpokenFeedback[] = "enableSpokenFeedback";
const char kJsApiScreenStateInitialize[] = "screenStateInitialize";
const char kJsApiSkipUpdateEnrollAfterEula[] = "skipUpdateEnrollAfterEula";
const char kJsApiScreenAssetsLoaded[] = "screenAssetsLoaded";
const char kJsApiHeaderBarVisible[] = "headerBarVisible";

}  // namespace

namespace chromeos {

// Note that show_oobe_ui_ defaults to false because WizardController assumes
// OOBE UI is not visible by default.
CoreOobeHandler::CoreOobeHandler(OobeUI* oobe_ui)
    : BaseScreenHandler(kJsScreenPath),
      oobe_ui_(oobe_ui),
      show_oobe_ui_(false),
      version_info_updater_(this),
      delegate_(NULL) {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  CHECK(accessibility_manager);
  accessibility_subscription_ = accessibility_manager->RegisterCallback(
      base::Bind(&CoreOobeHandler::OnAccessibilityStatusChanged,
                 base::Unretained(this)));
}

CoreOobeHandler::~CoreOobeHandler() {
}

void CoreOobeHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void CoreOobeHandler::DeclareLocalizedValues(LocalizedValuesBuilder* builder) {
  builder->Add("title", IDS_SHORT_PRODUCT_NAME);
  builder->Add("productName", IDS_SHORT_PRODUCT_NAME);
  builder->Add("learnMore", IDS_LEARN_MORE);

  // OOBE accessibility options menu strings shown on each screen.
  builder->Add("accessibilityLink", IDS_OOBE_ACCESSIBILITY_LINK);
  builder->Add("spokenFeedbackOption", IDS_OOBE_SPOKEN_FEEDBACK_OPTION);
  builder->Add("largeCursorOption", IDS_OOBE_LARGE_CURSOR_OPTION);
  builder->Add("highContrastOption", IDS_OOBE_HIGH_CONTRAST_MODE_OPTION);
  builder->Add("screenMagnifierOption", IDS_OOBE_SCREEN_MAGNIFIER_OPTION);
  builder->Add("virtualKeyboardOption", IDS_OOBE_VIRTUAL_KEYBOARD_OPTION);
  builder->Add("closeAccessibilityMenu", IDS_OOBE_CLOSE_ACCESSIBILITY_MENU);

  // Strings for the device requisition prompt.
  builder->Add("deviceRequisitionPromptCancel",
               IDS_ENTERPRISE_DEVICE_REQUISITION_PROMPT_CANCEL);
  builder->Add("deviceRequisitionPromptOk",
               IDS_ENTERPRISE_DEVICE_REQUISITION_PROMPT_OK);
  builder->Add("deviceRequisitionPromptText",
               IDS_ENTERPRISE_DEVICE_REQUISITION_PROMPT_TEXT);
  builder->Add("deviceRequisitionRemoraPromptCancel",
               IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL);
  builder->Add("deviceRequisitionRemoraPromptOk",
               IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL);
  builder->Add("deviceRequisitionRemoraPromptText",
               IDS_ENTERPRISE_DEVICE_REQUISITION_REMORA_PROMPT_TEXT);
  builder->Add("deviceRequisitionSharkPromptText",
               IDS_ENTERPRISE_DEVICE_REQUISITION_SHARK_PROMPT_TEXT);
}

void CoreOobeHandler::Initialize() {
  UpdateA11yState();
  UpdateOobeUIVisibility();
#if defined(OFFICIAL_BUILD)
  version_info_updater_.StartUpdate(true);
#else
  version_info_updater_.StartUpdate(false);
#endif
  UpdateDeviceRequisition();
  UpdateKeyboardState();
  UpdateClientAreaSize();
}

void CoreOobeHandler::RegisterMessages() {
  AddCallback(kJsApiScreenStateInitialize,
              &CoreOobeHandler::HandleInitialized);
  AddCallback(kJsApiSkipUpdateEnrollAfterEula,
              &CoreOobeHandler::HandleSkipUpdateEnrollAfterEula);
  AddCallback("updateCurrentScreen",
              &CoreOobeHandler::HandleUpdateCurrentScreen);
  AddCallback(kJsApiEnableHighContrast,
              &CoreOobeHandler::HandleEnableHighContrast);
  AddCallback(kJsApiEnableLargeCursor,
              &CoreOobeHandler::HandleEnableLargeCursor);
  AddCallback(kJsApiEnableVirtualKeyboard,
              &CoreOobeHandler::HandleEnableVirtualKeyboard);
  AddCallback(kJsApiEnableScreenMagnifier,
              &CoreOobeHandler::HandleEnableScreenMagnifier);
  AddCallback(kJsApiEnableSpokenFeedback,
              &CoreOobeHandler::HandleEnableSpokenFeedback);
  AddCallback("setDeviceRequisition",
              &CoreOobeHandler::HandleSetDeviceRequisition);
  AddCallback(kJsApiScreenAssetsLoaded,
              &CoreOobeHandler::HandleScreenAssetsLoaded);
  AddRawCallback("skipToLoginForTesting",
                 &CoreOobeHandler::HandleSkipToLoginForTesting);
  AddCallback("launchHelpApp",
              &CoreOobeHandler::HandleLaunchHelpApp);
  AddCallback("toggleResetScreen", &CoreOobeHandler::HandleToggleResetScreen);
  AddCallback(kJsApiHeaderBarVisible,
              &CoreOobeHandler::HandleHeaderBarVisible);
}

void CoreOobeHandler::ShowSignInError(
    int login_attempts,
    const std::string& error_text,
    const std::string& help_link_text,
    HelpAppLauncher::HelpTopic help_topic_id) {
  LOG(ERROR) << "CoreOobeHandler::ShowSignInError: error_text=" << error_text;
  CallJS("showSignInError", login_attempts, error_text,
         help_link_text, static_cast<int>(help_topic_id));
}

void CoreOobeHandler::ShowTpmError() {
  CallJS("showTpmError");
}

void CoreOobeHandler::ShowDeviceResetScreen() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector->IsEnterpriseManaged()) {
    // Don't recreate WizardController if it already exists.
    WizardController* wizard_controller =
        WizardController::default_controller();
    if (wizard_controller && !wizard_controller->login_screen_started()) {
      wizard_controller->AdvanceToScreen(WizardController::kResetScreenName);
    } else {
      scoped_ptr<base::DictionaryValue> params(new base::DictionaryValue());
      DCHECK(LoginDisplayHostImpl::default_host());
      if (LoginDisplayHostImpl::default_host()) {
        LoginDisplayHostImpl::default_host()->StartWizard(
            WizardController::kResetScreenName, params.Pass());
      }
    }
  }
}

void CoreOobeHandler::ShowSignInUI(const std::string& email) {
  CallJS("showSigninUI", email);
}

void CoreOobeHandler::ResetSignInUI(bool force_online) {
  CallJS("resetSigninUI", force_online);
}

void CoreOobeHandler::ClearUserPodPassword() {
  CallJS("clearUserPodPassword");
}

void CoreOobeHandler::RefocusCurrentPod() {
  CallJS("refocusCurrentPod");
}

void CoreOobeHandler::ShowPasswordChangedScreen(bool show_password_error) {
  CallJS("showPasswordChangedScreen", show_password_error);
}

void CoreOobeHandler::SetUsageStats(bool checked) {
  CallJS("setUsageStats", checked);
}

void CoreOobeHandler::SetOemEulaUrl(const std::string& oem_eula_url) {
  CallJS("setOemEulaUrl", oem_eula_url);
}

void CoreOobeHandler::SetTpmPassword(const std::string& tpm_password) {
  CallJS("setTpmPassword", tpm_password);
}

void CoreOobeHandler::ClearErrors() {
  CallJS("clearErrors");
}

void CoreOobeHandler::ReloadContent(const base::DictionaryValue& dictionary) {
  CallJS("reloadContent", dictionary);
}

void CoreOobeHandler::ShowControlBar(bool show) {
  CallJS("showControlBar", show);
}

void CoreOobeHandler::SetKeyboardState(bool shown, const gfx::Rect& bounds) {
  CallJS("setKeyboardState", shown, bounds.width(), bounds.height());
}

void CoreOobeHandler::SetClientAreaSize(int width, int height) {
  CallJS("setClientAreaSize", width, height);
}

void CoreOobeHandler::HandleInitialized() {
  oobe_ui_->InitializeHandlers();
}

void CoreOobeHandler::HandleSkipUpdateEnrollAfterEula() {
  WizardController* controller = WizardController::default_controller();
  DCHECK(controller);
  if (controller)
    controller->SkipUpdateEnrollAfterEula();
}

void CoreOobeHandler::HandleUpdateCurrentScreen(const std::string& screen) {
  if (delegate_)
    delegate_->OnCurrentScreenChanged(screen);
}

void CoreOobeHandler::HandleEnableHighContrast(bool enabled) {
  AccessibilityManager::Get()->EnableHighContrast(enabled);
}

void CoreOobeHandler::HandleEnableLargeCursor(bool enabled) {
  AccessibilityManager::Get()->EnableLargeCursor(enabled);
}

void CoreOobeHandler::HandleEnableVirtualKeyboard(bool enabled) {
  AccessibilityManager::Get()->EnableVirtualKeyboard(enabled);
}

void CoreOobeHandler::HandleEnableScreenMagnifier(bool enabled) {
  // TODO(nkostylev): Add support for partial screen magnifier.
  DCHECK(MagnificationManager::Get());
  MagnificationManager::Get()->SetMagnifierEnabled(enabled);
}

void CoreOobeHandler::HandleEnableSpokenFeedback() {
  // Checkbox is initialized on page init and updates when spoken feedback
  // setting is changed so just toggle spoken feedback here.
  AccessibilityManager::Get()->ToggleSpokenFeedback(
      ash::A11Y_NOTIFICATION_NONE);
}

void CoreOobeHandler::HandleSetDeviceRequisition(
    const std::string& requisition) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  std::string initial_requisition =
      connector->GetDeviceCloudPolicyManager()->GetDeviceRequisition();
  connector->GetDeviceCloudPolicyManager()->SetDeviceRequisition(requisition);
  // Exit Chrome to force the restart as soon as a new requisition is set.
  if (initial_requisition !=
          connector->GetDeviceCloudPolicyManager()->GetDeviceRequisition()) {
    chrome::AttemptRestart();
  }
}

void CoreOobeHandler::HandleScreenAssetsLoaded(
    const std::string& screen_async_load_id) {
  oobe_ui_->OnScreenAssetsLoaded(screen_async_load_id);
}

void CoreOobeHandler::HandleSkipToLoginForTesting(
    const base::ListValue* args) {
  LoginScreenContext context(args);
  if (WizardController::default_controller())
      WizardController::default_controller()->SkipToLoginForTesting(context);
}

void CoreOobeHandler::HandleToggleResetScreen() { ShowDeviceResetScreen(); }

void CoreOobeHandler::ShowOobeUI(bool show) {
  if (show == show_oobe_ui_)
    return;

  show_oobe_ui_ = show;

  if (page_is_ready())
    UpdateOobeUIVisibility();
}

void CoreOobeHandler::UpdateA11yState() {
  DCHECK(MagnificationManager::Get());
  base::DictionaryValue a11y_info;
  a11y_info.SetBoolean("highContrastEnabled",
                       AccessibilityManager::Get()->IsHighContrastEnabled());
  a11y_info.SetBoolean("largeCursorEnabled",
                       AccessibilityManager::Get()->IsLargeCursorEnabled());
  a11y_info.SetBoolean("spokenFeedbackEnabled",
                       AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  a11y_info.SetBoolean("screenMagnifierEnabled",
                       MagnificationManager::Get()->IsMagnifierEnabled());
  a11y_info.SetBoolean("virtualKeyboardEnabled",
                       AccessibilityManager::Get()->IsVirtualKeyboardEnabled());
  CallJS("refreshA11yInfo", a11y_info);
}

void CoreOobeHandler::UpdateOobeUIVisibility() {
  // Don't show version label on the stable channel by default.
  bool should_show_version = true;
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    should_show_version = false;
  }
  CallJS("showVersion", should_show_version);
  CallJS("showOobeUI", show_oobe_ui_);
  if (system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation())
    CallJS("enableKeyboardFlow", true);
}

void CoreOobeHandler::OnOSVersionLabelTextUpdated(
    const std::string& os_version_label_text) {
  UpdateLabel("version", os_version_label_text);
}

void CoreOobeHandler::OnEnterpriseInfoUpdated(
    const std::string& message_text) {
  CallJS("setEnterpriseInfo", message_text);
}

void CoreOobeHandler::UpdateLabel(const std::string& id,
                                  const std::string& text) {
  CallJS("setLabelText", id, text);
}

void CoreOobeHandler::UpdateDeviceRequisition() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  CallJS("updateDeviceRequisition",
         connector->GetDeviceCloudPolicyManager()->GetDeviceRequisition());
}

void CoreOobeHandler::UpdateKeyboardState() {
  if (!login::LoginScrollIntoViewEnabled())
    return;

  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller) {
    gfx::Rect bounds = keyboard_controller->current_keyboard_bounds();
    SetKeyboardState(!bounds.IsEmpty(), bounds);
  }
}

void CoreOobeHandler::UpdateClientAreaSize() {
  const gfx::Size& size = ash::Shell::GetScreen()->GetPrimaryDisplay().size();
  SetClientAreaSize(size.width(), size.height());
}

void CoreOobeHandler::OnAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  if (details.notification_type == ACCESSIBILITY_MANAGER_SHUTDOWN)
    accessibility_subscription_.reset();
  else
    UpdateA11yState();
}

void CoreOobeHandler::HandleLaunchHelpApp(double help_topic_id) {
  if (!help_app_)
    help_app_ = new HelpAppLauncher(GetNativeWindow());
  help_app_->ShowHelpTopic(
      static_cast<HelpAppLauncher::HelpTopic>(help_topic_id));
}

void CoreOobeHandler::HandleHeaderBarVisible() {
  LoginDisplayHost* login_display_host = LoginDisplayHostImpl::default_host();
  if (login_display_host)
    login_display_host->SetStatusAreaVisible(true);
}

void CoreOobeHandler::InitDemoModeDetection() {
  demo_mode_detector_.InitDetection();
}

void CoreOobeHandler::StopDemoModeDetection() {
  demo_mode_detector_.StopDetection();
}

}  // namespace chromeos
