// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/stats_options_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"

using base::UserMetricsAction;

namespace chromeos {
namespace options {

StatsOptionsHandler::StatsOptionsHandler() {
}

// OptionsPageUIHandler implementation.
void StatsOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
}

// WebUIMessageHandler implementation.
void StatsOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("metricsReportingCheckboxAction",
      base::Bind(&StatsOptionsHandler::HandleMetricsReportingCheckbox,
                 base::Unretained(this)));
}

void StatsOptionsHandler::HandleMetricsReportingCheckbox(
    const base::ListValue* args) {
#if defined(GOOGLE_CHROME_BUILD)
  const std::string checked_str = base::UTF16ToUTF8(ExtractStringValue(args));
  const bool enabled = (checked_str == "true");
  content::RecordAction(
      enabled ?
      UserMetricsAction("Options_MetricsReportingCheckbox_Enable") :
      UserMetricsAction("Options_MetricsReportingCheckbox_Disable"));
#endif
}

}  // namespace options
}  // namespace chromeos
