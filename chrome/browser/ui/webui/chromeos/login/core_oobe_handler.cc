// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// JS API callbacks names.
const char kJsApiScreenStateInitialize[] = "screenStateInitialize";
const char kJsApiSkipUpdateEnrollAfterEula[] = "skipUpdateEnrollAfterEula";

}  // namespace

namespace chromeos {

// Note that show_oobe_ui_ defaults to false because WizardController assumes
// OOBE UI is not visible by default.
CoreOobeHandler::CoreOobeHandler(OobeUI* oobe_ui)
    : oobe_ui_(oobe_ui),
      show_oobe_ui_(false),
      version_info_updater_(this) {
}

CoreOobeHandler::~CoreOobeHandler() {
}

void CoreOobeHandler::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString(
      "title", l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
  localized_strings->SetString(
      "productName", l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
  localized_strings->SetString(
      "learnMore", l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  localized_strings->SetString(
      "reportingHint",
      l10n_util::GetStringUTF16(IDS_LOGIN_MANAGED_REPORTING_HINT));
}

void CoreOobeHandler::Initialize() {
  UpdateOobeUIVisibility();
#if defined(OFFICIAL_BUILD)
  version_info_updater_.StartUpdate(true);
#else
  version_info_updater_.StartUpdate(false);
#endif
}

void CoreOobeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(kJsApiScreenStateInitialize,
      base::Bind(&CoreOobeHandler::HandleInitialized,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiSkipUpdateEnrollAfterEula,
      base::Bind(&CoreOobeHandler::HandleSkipUpdateEnrollAfterEula,
                 base::Unretained(this)));
}

void CoreOobeHandler::HandleInitialized(const base::ListValue* args) {
  oobe_ui_->InitializeHandlers();
}

void CoreOobeHandler::HandleSkipUpdateEnrollAfterEula(
    const base::ListValue* args) {
  WizardController* controller = WizardController::default_controller();
  DCHECK(controller);
  if (controller)
    controller->SkipUpdateEnrollAfterEula();
}

void CoreOobeHandler::ShowOobeUI(bool show) {
  if (show == show_oobe_ui_)
    return;

  show_oobe_ui_ = show;

  if (page_is_ready())
    UpdateOobeUIVisibility();
}

void CoreOobeHandler::UpdateOobeUIVisibility() {
  // Don't show version label on the stable channel by default.
  bool should_show_version = true;
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    should_show_version = false;
  }
  base::FundamentalValue show_version(should_show_version);
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.showVersion", show_version);
  base::FundamentalValue show_value(show_oobe_ui_);
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.showOobeUI", show_value);
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
    const std::string& message_text,
    bool reporting_hint) {
  base::StringValue message_text_vaue(UTF8ToUTF16(message_text));
  base::FundamentalValue show_help_link(reporting_hint);
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.setEnterpriseInfo",
                                   message_text_vaue,
                                   show_help_link);
}

void CoreOobeHandler::UpdateLabel(const std::string& id,
                                  const std::string& text) {
  base::StringValue id_value(UTF8ToUTF16(id));
  base::StringValue text_value(UTF8ToUTF16(text));
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.setLabelText",
                                   id_value,
                                   text_value);
}

}  // namespace chromeos
