// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"

#include "ash/magnifier/magnifier_constants.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_version_info.h"
#include "chromeos/chromeos_constants.h"
#include "content/public/browser/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

const char kJsScreenPath[] = "cr.ui.Oobe";

// JS API callbacks names.
const char kJsApiEnableHighContrast[] = "enableHighContrast";
const char kJsApiEnableScreenMagnifier[] = "enableScreenMagnifier";
const char kJsApiEnableLargeCursor[] = "enableLargeCursor";
const char kJsApiEnableSpokenFeedback[] = "enableSpokenFeedback";
const char kJsApiScreenStateInitialize[] = "screenStateInitialize";
const char kJsApiSkipUpdateEnrollAfterEula[] = "skipUpdateEnrollAfterEula";

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
  registrar_.Add(
      this,
      chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_LARGE_CURSOR,
      content::NotificationService::AllSources());
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

  // Strings for the device requisition prompt.
  builder->Add("deviceRequisitionPromptCancel",
               IDS_ENTERPRISE_DEVICE_REQUISITION_PROMPT_CANCEL);
  builder->Add("deviceRequisitionPromptOk",
               IDS_ENTERPRISE_DEVICE_REQUISITION_PROMPT_OK);
  // TODO(xiyuan): Restore generic device requisition prompt text.
  builder->Add("deviceRequisitionPromptText",
               IDS_ENTERPRISE_DEVICE_REQUISITION_PROMPT_TEXT);
  builder->Add("deviceRequisitionRemoraPromptText",
               IDS_ENTERPRISE_DEVICE_REQUISITION_PROMPT_TEXT);
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
  AddCallback(kJsApiEnableScreenMagnifier,
              &CoreOobeHandler::HandleEnableScreenMagnifier);
  AddCallback(kJsApiEnableSpokenFeedback,
              &CoreOobeHandler::HandleEnableSpokenFeedback);
  AddCallback("setDeviceRequisition",
              &CoreOobeHandler::HandleSetDeviceRequisition);
  AddCallback("skipToLoginForTesting",
              &CoreOobeHandler::HandleSkipToLoginForTesting);
}

void CoreOobeHandler::ShowSignInError(
    int login_attempts,
    const std::string& error_text,
    const std::string& help_link_text,
    HelpAppLauncher::HelpTopic help_topic_id) {
  CallJS("showSignInError", login_attempts, error_text,
         help_link_text, static_cast<int>(help_topic_id));
}

void CoreOobeHandler::ShowTpmError() {
  CallJS("showTpmError");
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

void CoreOobeHandler::OnLoginSuccess(const std::string& username) {
  CallJS("onLoginSuccess", username);
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
  g_browser_process->browser_policy_connector()->GetDeviceCloudPolicyManager()->
      SetDeviceRequisition(requisition);
  // Exit Chrome to force the restart as soon as a new requisition is set.
  chrome::ExitCleanly();
}

void CoreOobeHandler::HandleSkipToLoginForTesting() {
  if (WizardController::default_controller())
      WizardController::default_controller()->SkipToLoginForTesting();
}

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
  if (system::keyboard_settings::ForceKeyboardDrivenUINavigation())
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
  CallJS("updateDeviceRequisition",
         g_browser_process->browser_policy_connector()->
             GetDeviceCloudPolicyManager()->GetDeviceRequisition());
}

void CoreOobeHandler::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  if (type ==
          chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE ||
      type == chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_LARGE_CURSOR ||
      type == chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER ||
      type == chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK) {
    UpdateA11yState();
  } else {
    NOTREACHED() << "Unexpected notification " << type;
  }
}

}  // namespace chromeos
