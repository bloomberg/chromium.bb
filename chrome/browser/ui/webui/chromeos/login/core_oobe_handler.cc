// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"

#include "ash/magnifier/magnifier_constants.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_version_info.h"
#include "chromeos/chromeos_constants.h"
#include "content/public/browser/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

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
    : oobe_ui_(oobe_ui),
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
  builder->Add("deviceRequisitionPromptText",
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
  CallJS("cr.ui.Oobe.refreshA11yInfo", a11y_info);
}

void CoreOobeHandler::UpdateOobeUIVisibility() {
  // Don't show version label on the stable channel by default.
  bool should_show_version = true;
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    should_show_version = false;
  }
  CallJS("cr.ui.Oobe.showVersion", should_show_version);
  CallJS("cr.ui.Oobe.showOobeUI", show_oobe_ui_);

  bool enable_keyboard_flow = false;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  provider->GetMachineFlag(chromeos::system::kOemKeyboardDrivenOobeKey,
                           &enable_keyboard_flow);
  if (enable_keyboard_flow)
    CallJS("cr.ui.Oobe.enableKeyboardFlow", enable_keyboard_flow);
}

void CoreOobeHandler::OnOSVersionLabelTextUpdated(
    const std::string& os_version_label_text) {
  UpdateLabel("version", os_version_label_text);
}

void CoreOobeHandler::OnBootTimesLabelTextUpdated(
    const std::string& boot_times_label_text) {
  UpdateLabel("boot-times", boot_times_label_text);
}

void CoreOobeHandler::OnEnterpriseInfoUpdated(
    const std::string& message_text) {
  CallJS("cr.ui.Oobe.setEnterpriseInfo", message_text);
}

void CoreOobeHandler::UpdateLabel(const std::string& id,
                                  const std::string& text) {
  CallJS("cr.ui.Oobe.setLabelText", id, text);
}

void CoreOobeHandler::UpdateDeviceRequisition() {
  CallJS("cr.ui.Oobe.updateDeviceRequisition",
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
