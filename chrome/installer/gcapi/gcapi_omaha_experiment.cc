// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi/gcapi_omaha_experiment.h"

#include "base/basictypes.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/installer/gcapi/gcapi.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_experiment_util.h"
#include "chrome/installer/util/google_update_settings.h"

namespace {

// Returns the number of weeks since 2/3/2003.
int GetCurrentRlzWeek(const base::Time& current_time) {
  base::Time::Exploded february_third_2003_exploded =
      {2003, 2, 1, 3, 0, 0, 0, 0};
  base::Time f = base::Time::FromUTCExploded(february_third_2003_exploded);
  base::TimeDelta delta = current_time - f;
  return delta.InDays() / 7;
}

bool SetExperimentLabel(const wchar_t* brand_code,
                        const base::string16& label,
                        int shell_mode) {
  if (!brand_code) {
    return false;
  }

  const bool system_level = shell_mode == GCAPI_INVOKED_UAC_ELEVATION;

  base::string16 original_labels;
  if (!GoogleUpdateSettings::ReadExperimentLabels(system_level,
                                                  &original_labels)) {
    return false;
  }

  // Split the original labels by the label separator.
  std::vector<base::string16> entries;
  base::SplitString(original_labels, google_update::kExperimentLabelSeparator,
                    &entries);

  // Keep all labels, but the one we want to add/replace.
  base::string16 new_labels;
  for (std::vector<base::string16>::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    if (!it->empty() && !StartsWith(*it, label + L"=", true)) {
      new_labels += *it;
      new_labels += google_update::kExperimentLabelSeparator;
    }
  }

  new_labels.append(
      gcapi_internals::GetGCAPIExperimentLabel(brand_code, label));

  return GoogleUpdateSettings::SetExperimentLabels(system_level,
                                                   new_labels);
}

}  // namespace

namespace gcapi_internals {

const wchar_t kReactivationLabel[] = L"reacbrand";
const wchar_t kRelaunchLabel[] = L"relaunchbrand";

base::string16 GetGCAPIExperimentLabel(const wchar_t* brand_code,
                                       const base::string16& label) {
  // Keeps a fixed time state for this GCAPI instance; this makes tests reliable
  // when crossing time boundaries on the system clock and doesn't otherwise
  // affect results of this short lived binary.
  static time_t instance_time_value = 0;
  if (instance_time_value == 0)
    instance_time_value = base::Time::Now().ToTimeT();

  base::Time instance_time = base::Time::FromTimeT(instance_time_value);

  base::string16 gcapi_experiment_label;
  base::SStringPrintf(&gcapi_experiment_label,
                      L"%ls=%ls_%d|%ls",
                      label.c_str(),
                      brand_code,
                      GetCurrentRlzWeek(instance_time),
                      installer::BuildExperimentDateString(
                          instance_time).c_str());
  return gcapi_experiment_label;
}

}  // namespace gcapi_internals

bool SetReactivationExperimentLabels(const wchar_t* brand_code,
                                     int shell_mode) {
  return SetExperimentLabel(brand_code, gcapi_internals::kReactivationLabel,
                            shell_mode);
}

bool SetRelaunchExperimentLabels(const wchar_t* brand_code, int shell_mode) {
  return SetExperimentLabel(brand_code, gcapi_internals::kRelaunchLabel,
                            shell_mode);
}
