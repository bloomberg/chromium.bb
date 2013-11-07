// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/variations/experiment_labels_win.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_experiment_util.h"
#include "components/variations/variations_associated_data.h"

namespace chrome_variations {

namespace {

const wchar_t kVariationPrefix[] = L"CrVar";

// This method builds a single experiment label for a Chrome Variation,
// including a timestamp that is a year in the future from |current_time|. Since
// multiple headers can be transmitted, |count| is a number that is appended
// after the label key to differentiate the labels.
string16 CreateSingleExperimentLabel(int count, VariationID id,
                                     const base::Time& current_time) {
  // Build the parts separately so they can be validated.
  const string16 key = kVariationPrefix + base::IntToString16(count);
  DCHECK_LE(key.size(), 8U);
  const string16 value = base::IntToString16(id);
  DCHECK_LE(value.size(), 8U);
  string16 label(key);
  label += L'=';
  label += value;
  label += L'|';
  label += installer::BuildExperimentDateString(current_time);
  return label;
}

}  // namespace

string16 BuildGoogleUpdateExperimentLabel(
    const base::FieldTrial::ActiveGroups& active_groups) {
  string16 experiment_labels;
  int counter = 0;

  const base::Time current_time(base::Time::Now());

  // Find all currently active VariationIDs associated with Google Update.
  for (base::FieldTrial::ActiveGroups::const_iterator it =
       active_groups.begin(); it != active_groups.end(); ++it) {
    const VariationID id = GetGoogleVariationID(GOOGLE_UPDATE_SERVICE,
                                                it->trial_name, it->group_name);

    if (id == EMPTY_ID)
      continue;

    if (!experiment_labels.empty())
      experiment_labels += google_update::kExperimentLabelSep;
    experiment_labels += CreateSingleExperimentLabel(++counter, id,
                                                     current_time);
  }

  return experiment_labels;
}

string16 ExtractNonVariationLabels(const string16& labels) {
  string16 non_variation_labels;

  // First, split everything by the label separator.
  std::vector<string16> entries;
  base::SplitStringUsingSubstr(labels, google_update::kExperimentLabelSep,
                               &entries);

  // For each label, keep the ones that do not look like a Variations label.
  for (std::vector<string16>::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    if (it->empty() || StartsWith(*it, kVariationPrefix, false))
      continue;

    // Dump the whole thing, including the timestamp.
    if (!non_variation_labels.empty())
      non_variation_labels += google_update::kExperimentLabelSep;
    non_variation_labels += *it;
  }

  return non_variation_labels;
}

string16 CombineExperimentLabels(const string16& variation_labels,
                                 const string16& other_labels) {
  DCHECK(!StartsWith(variation_labels, google_update::kExperimentLabelSep,
                     false));
  DCHECK(!EndsWith(variation_labels, google_update::kExperimentLabelSep,
                   false));
  DCHECK(!StartsWith(other_labels, google_update::kExperimentLabelSep, false));
  DCHECK(!EndsWith(other_labels, google_update::kExperimentLabelSep, false));
  // Note that if either label is empty, a separator is not necessary.
  string16 combined_labels = other_labels;
  if (!other_labels.empty() && !variation_labels.empty())
    combined_labels += google_update::kExperimentLabelSep;
  combined_labels += variation_labels;
  return combined_labels;
}

}  // namespace chrome_variations
