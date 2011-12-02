// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/stats_options_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/user_metrics.h"

namespace chromeos {

StatsOptionsHandler::StatsOptionsHandler() {
}

// OptionsPageUIHandler implementation.
void StatsOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
}

void StatsOptionsHandler::Initialize() {
}

// WebUIMessageHandler implementation.
void StatsOptionsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("metricsReportingCheckboxAction",
      base::Bind(&StatsOptionsHandler::HandleMetricsReportingCheckbox,
                 base::Unretained(this)));
}

void StatsOptionsHandler::HandleMetricsReportingCheckbox(
    const ListValue* args) {
#if defined(GOOGLE_CHROME_BUILD)
  const std::string checked_str = UTF16ToUTF8(ExtractStringValue(args));
  const bool enabled = (checked_str == "true");
  UserMetrics::RecordAction(
      enabled ?
      UserMetricsAction("Options_MetricsReportingCheckbox_Enable") :
      UserMetricsAction("Options_MetricsReportingCheckbox_Disable"));
#endif
}

}  // namespace chromeos
