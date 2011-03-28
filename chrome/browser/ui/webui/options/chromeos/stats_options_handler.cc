// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/stats_options_handler.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/metrics_cros_settings_provider.h"
#include "chrome/browser/metrics/user_metrics.h"

namespace chromeos {

StatsOptionsHandler::StatsOptionsHandler()
    : CrosOptionsPageUIHandler(new MetricsCrosSettingsProvider()) {
}

// OptionsPageUIHandler implementation.
void StatsOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
}
void StatsOptionsHandler::Initialize() {
  SetupMetricsReportingCheckbox(false);
}

// WebUIMessageHandler implementation.
void StatsOptionsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      "metricsReportingCheckboxAction",
      NewCallback(this, &StatsOptionsHandler::HandleMetricsReportingCheckbox));
}

MetricsCrosSettingsProvider* StatsOptionsHandler::provider() const {
  return static_cast<MetricsCrosSettingsProvider*>(settings_provider_.get());
}

void StatsOptionsHandler::HandleMetricsReportingCheckbox(
    const ListValue* args) {
#if defined(GOOGLE_CHROME_BUILD)
  const std::string checked_str = UTF16ToUTF8(ExtractStringValue(args));
  const bool enabled = (checked_str == "true");
  UserMetricsRecordAction(
      enabled ?
      UserMetricsAction("Options_MetricsReportingCheckbox_Enable") :
      UserMetricsAction("Options_MetricsReportingCheckbox_Disable"));
  const bool is_enabled = MetricsCrosSettingsProvider::GetMetricsStatus();
  SetupMetricsReportingCheckbox(enabled == is_enabled);
#endif
}

void StatsOptionsHandler::SetupMetricsReportingCheckbox(bool user_changed) {
#if defined(GOOGLE_CHROME_BUILD)
  FundamentalValue checked(MetricsCrosSettingsProvider::GetMetricsStatus());
  FundamentalValue disabled(!UserManager::Get()->current_user_is_owner());
  FundamentalValue user_has_changed(user_changed);
  web_ui_->CallJavascriptFunction(
      "options.AdvancedOptions.SetMetricsReportingCheckboxState", checked,
      disabled, user_has_changed);
#endif
}

}  // namespace chromeos
