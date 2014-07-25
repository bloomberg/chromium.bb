// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/variations/experiment_labels.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/installer/util/google_update_experiment_util.h"
#include "components/variations/variations_associated_data.h"

namespace chrome_variations {

namespace {

const char kVariationPrefix[] = "CrVar";

// This method builds a single experiment label for a Chrome Variation,
// including a timestamp that is a year in the future from |current_time|. Since
// multiple headers can be transmitted, |count| is a number that is appended
// after the label key to differentiate the labels.
base::string16 CreateSingleExperimentLabel(int count,
                                           variations::VariationID id,
                                           const base::Time& current_time) {
  // Build the parts separately so they can be validated.
  const base::string16 key =
      base::ASCIIToUTF16(kVariationPrefix) + base::IntToString16(count);
  DCHECK_LE(key.size(), 8U);
  const base::string16 value = base::IntToString16(id);
  DCHECK_LE(value.size(), 8U);
  base::string16 label(key);
  label += base::ASCIIToUTF16("=");
  label += value;
  label += base::ASCIIToUTF16("|");
  label += installer::BuildExperimentDateString(current_time);
  return label;
}

}  // namespace

base::string16 BuildGoogleUpdateExperimentLabel(
    const base::FieldTrial::ActiveGroups& active_groups) {
  base::string16 experiment_labels;
  int counter = 0;

  const base::Time current_time(base::Time::Now());

  // Find all currently active VariationIDs associated with Google Update.
  for (base::FieldTrial::ActiveGroups::const_iterator it =
       active_groups.begin(); it != active_groups.end(); ++it) {
    const variations::VariationID id =
        variations::GetGoogleVariationID(variations::GOOGLE_UPDATE_SERVICE,
                                         it->trial_name, it->group_name);

    if (id == variations::EMPTY_ID)
      continue;

    if (!experiment_labels.empty())
      experiment_labels += google_update::kExperimentLabelSeparator;
    experiment_labels += CreateSingleExperimentLabel(++counter, id,
                                                     current_time);
  }

  return experiment_labels;
}

base::string16 ExtractNonVariationLabels(const base::string16& labels) {
  // First, split everything by the label separator.
  std::vector<base::string16> entries;
  base::SplitString(labels, google_update::kExperimentLabelSeparator, &entries);

  // For each label, keep the ones that do not look like a Variations label.
  base::string16 non_variation_labels;
  for (std::vector<base::string16>::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    if (it->empty() ||
        StartsWith(*it, base::ASCIIToUTF16(kVariationPrefix), false)) {
      continue;
    }

    // Dump the whole thing, including the timestamp.
    if (!non_variation_labels.empty())
      non_variation_labels += google_update::kExperimentLabelSeparator;
    non_variation_labels += *it;
  }

  return non_variation_labels;
}

base::string16 CombineExperimentLabels(const base::string16& variation_labels,
                                       const base::string16& other_labels) {
  const base::string16 separator(1, google_update::kExperimentLabelSeparator);
  DCHECK(!StartsWith(variation_labels, separator, false));
  DCHECK(!EndsWith(variation_labels, separator, false));
  DCHECK(!StartsWith(other_labels, separator, false));
  DCHECK(!EndsWith(other_labels, separator, false));
  // Note that if either label is empty, a separator is not necessary.
  base::string16 combined_labels = other_labels;
  if (!other_labels.empty() && !variation_labels.empty())
    combined_labels += google_update::kExperimentLabelSeparator;
  combined_labels += variation_labels;
  return combined_labels;
}

}  // namespace chrome_variations
