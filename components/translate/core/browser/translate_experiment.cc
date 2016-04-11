// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_experiment.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "components/translate/core/browser/translate_download_manager.h"

using base::CommandLine;

namespace translate {

namespace {

const char kTranslateExperimentTrial[] = "TranslateUiLangTrial";
const char kDisableTranslateExperiment[] = "disable-translate-experiment";
const char kEnableTranslateExperiment[] = "enable-translate-experiment";
const char kForceTranslateLanguage[] = "force-translate-language";

}  // namespace

// static
void TranslateExperiment::OverrideUiLanguage(const std::string& country,
                                             std::string* ui_lang) {
  DCHECK(ui_lang);
  if (!InExperiment())
    return;

  // Forced target language takes precedence.
  if (CommandLine::ForCurrentProcess()->HasSwitch(kForceTranslateLanguage)) {
    *ui_lang = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        kForceTranslateLanguage);
    return;
  }

  // Otherwise, base the target language on the current country.
  if (country == "my")
    *ui_lang = "ms";
  else if (country == "id")
    *ui_lang = "id";
  else if (country == "th")
    *ui_lang = "th";
}

// static
bool TranslateExperiment::ShouldOverrideBlocking(const std::string& ui_language,
                                                 const std::string& language) {
  return InExperiment() && (language == ui_language);
}

// static
bool TranslateExperiment::InExperiment() {
  // Note: It's important to query the field trial state first, to ensure that
  // UMA reports the correct group.
  const std::string group_name =
      base::FieldTrialList::FindFullName(kTranslateExperimentTrial);
  if (CommandLine::ForCurrentProcess()->HasSwitch(kDisableTranslateExperiment))
    return false;
  // TODO(groby): Figure out if we can get rid of the enable branch completely.
  // Or, if we allow enable, we have to gate this on country. I'd rather not,
  // since this complicates the experiment.
  if (CommandLine::ForCurrentProcess()->HasSwitch(kEnableTranslateExperiment))
    return true;
  if (CommandLine::ForCurrentProcess()->HasSwitch(kForceTranslateLanguage))
    return true;
  // Use StartsWith() for more flexibility (e.g. multiple Enabled groups).
  return base::StartsWith(group_name, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace translate
