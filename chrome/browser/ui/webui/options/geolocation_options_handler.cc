// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/geolocation_options_handler.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_ui.h"

namespace options {

GeolocationOptionsHandler::GeolocationOptionsHandler() {}

GeolocationOptionsHandler::~GeolocationOptionsHandler() {}

void GeolocationOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
}

void GeolocationOptionsHandler::InitializePage() {
  DCHECK(web_ui());

  const char kEnablePrefix[] = "Enable";
  const char kFieldTrialName[] = "GoogleNow";
  std::string enable_prefix(kEnablePrefix);
  std::string field_trial_result =
      base::FieldTrialList::FindFullName(kFieldTrialName);
  if (field_trial_result.compare(
          0,
          enable_prefix.length(),
          enable_prefix) == 0) {
    web_ui()->CallJavascriptFunction(
        "GeolocationOptions.showGeolocationOption");
  }
}

}  // namespace options

