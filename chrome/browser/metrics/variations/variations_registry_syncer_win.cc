// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_registry_syncer_win.h"

#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"

namespace {

// Delay before performing a registry sync, in seconds.
const int kRegistrySyncDelaySeconds = 5;

const wchar_t kVariationPrefix[] = L"CrVar";
const wchar_t kExperimentLabelSep[] = L";";

// This method builds a single experiment label for a Chrome Variation,
// including a timestamp that is a year in the future from now. Since multiple
// headers can be transmitted, |count| is a number that is appended after the
// label key to differentiate the labels.
string16 CreateSingleExperimentLabel(int count,
                                     chrome_variations::VariationID id) {
  // Build the parts separately so they can be validated.
  const string16 key = kVariationPrefix + base::IntToString16(count);
  DCHECK_LE(key.size(), 8U);
  const string16 value = base::IntToString16(id);
  DCHECK_LE(value.size(), 8U);
  string16 label(key);
  label += L"=";
  label += value;
  label += L"|";
  label += GoogleUpdateSettings::BuildExperimentDateString();
  return label;
}

}  // namespace

namespace chrome_variations {

VariationsRegistrySyncer::VariationsRegistrySyncer() {
}

VariationsRegistrySyncer::~VariationsRegistrySyncer() {
}

void VariationsRegistrySyncer::RequestRegistrySync() {
  if (timer_.IsRunning()) {
    timer_.Reset();
    return;
  }

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(kRegistrySyncDelaySeconds),
               this, &VariationsRegistrySyncer::SyncWithRegistry);
}

// static
string16 VariationsRegistrySyncer::BuildGoogleUpdateExperimentLabel(
    base::FieldTrial::ActiveGroups& active_groups) {
  string16 experiment_labels;
  int counter = 0;

  // Find all currently active VariationIDs associated with Google Update.
  for (base::FieldTrial::ActiveGroups::const_iterator it =
       active_groups.begin(); it != active_groups.end(); ++it) {
    const chrome_variations::VariationID id =
        chrome_variations::GetGoogleVariationID(
            chrome_variations::GOOGLE_UPDATE_SERVICE,
            it->trial_name, it->group_name);

    if (id == chrome_variations::EMPTY_ID)
      continue;

    if (!experiment_labels.empty())
      experiment_labels += kExperimentLabelSep;
    experiment_labels += CreateSingleExperimentLabel(++counter, id);
  }

  return experiment_labels;
}

// static
string16 VariationsRegistrySyncer::CombineExperimentLabels(
    const string16& variation_labels,
    const string16& other_labels) {
  DCHECK(!StartsWith(variation_labels, kExperimentLabelSep, false));
  DCHECK(!EndsWith(variation_labels, kExperimentLabelSep, false));
  DCHECK(!StartsWith(other_labels, kExperimentLabelSep, false));
  DCHECK(!EndsWith(other_labels, kExperimentLabelSep, false));
  // Note that if either label is empty, a separator is not necessary.
  string16 combined_labels = other_labels;
  if (!other_labels.empty() && !variation_labels.empty())
    combined_labels += kExperimentLabelSep;
  combined_labels += variation_labels;
  return combined_labels;
}

// static
string16 VariationsRegistrySyncer::ExtractNonVariationLabels(
    const string16& labels) {
  string16 non_variation_labels;

  // First, split everything by the label separator.
  std::vector<string16> entries;
  base::SplitStringUsingSubstr(labels, kExperimentLabelSep, &entries);

  // For each label, keep the ones that do not look like a Variations label.
  for (std::vector<string16>::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    if (it->empty() || StartsWith(*it, kVariationPrefix, false))
      continue;

    // Dump the whole thing, including the timestamp.
    if (!non_variation_labels.empty())
      non_variation_labels += kExperimentLabelSep;
    non_variation_labels += *it;
  }

  return non_variation_labels;
}

void VariationsRegistrySyncer::SyncWithRegistry() {
  // Note that all registry operations are done here on the UI thread as there
  // are no threading restrictions on them.
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Failed to get chrome exe path";
    return;
  }
  const bool is_system_install =
      !InstallUtil::IsPerUserInstall(chrome_exe.value().c_str());

  // Read the current bits from the registry.
  string16 registry_labels;
  bool success = GoogleUpdateSettings::ReadExperimentLabels(is_system_install,
                                                            &registry_labels);
  if (!success) {
    DVLOG(1) << "Error reading Variation labels from the registry.";
    return;
  }

  // Since the non-Variations contents of experiment_labels should not be,
  // clobbered, separate them from the Variations contents.
  const string16 other_labels = ExtractNonVariationLabels(registry_labels);

  // Compute the new Variations part of the label.
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  const string16 variation_labels =
      BuildGoogleUpdateExperimentLabel(active_groups);

  // Append the old non-Variations labels with the new Variations labels and
  // write it back to the registry.
  const string16 combined_labels =
      CombineExperimentLabels(variation_labels, other_labels);

  if (!GoogleUpdateSettings::SetExperimentLabels(is_system_install,
                                                 combined_labels)) {
    DVLOG(1) << "Error writing Variation labels to the registry: "
             << combined_labels;
  }
}

}  // namespace chrome_variations
