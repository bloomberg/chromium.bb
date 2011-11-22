// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// JS API callbacks names.
const char kJsApiScreenStateInitialize[] = "screenStateInitialize";
const char kJsApiToggleAccessibility[] = "toggleAccessibility";

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
  web_ui_->RegisterMessageCallback(kJsApiToggleAccessibility,
      base::Bind(&CoreOobeHandler::OnToggleAccessibility,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback(kJsApiScreenStateInitialize,
      base::Bind(&CoreOobeHandler::OnInitialized,
                 base::Unretained(this)));
}

void CoreOobeHandler::OnInitialized(const base::ListValue* args) {
  oobe_ui_->InitializeHandlers();
}

void CoreOobeHandler::OnToggleAccessibility(const base::ListValue* args) {
  accessibility::ToggleAccessibility(web_ui_);
}

void CoreOobeHandler::ShowOobeUI(bool show) {
  if (show == show_oobe_ui_)
    return;

  show_oobe_ui_ = show;

  if (page_is_ready())
    UpdateOobeUIVisibility();
}

void CoreOobeHandler::UpdateOobeUIVisibility() {
  base::FundamentalValue showValue(show_oobe_ui_);
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.showOobeUI", showValue);
}

void CoreOobeHandler::OnOSVersionLabelTextUpdated(
    const std::string& os_version_label_text) {
  UpdateLabel("version", os_version_label_text);
}

void CoreOobeHandler::OnBootTimesLabelTextUpdated(
    const std::string& boot_times_label_text) {
  UpdateLabel("boot-times", boot_times_label_text);
}

void CoreOobeHandler::UpdateLabel(const std::string& id,
                                  const std::string& text) {
  base::StringValue id_value(UTF8ToUTF16(id));
  base::StringValue text_value(UTF8ToUTF16(text));
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.setLabelText",
                                  id_value,
                                  text_value);
}

}  // namespace chromeos
