// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_registry_syncer_win.h"

#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "chrome/common/variations/experiment_labels.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"

namespace {

// Delay before performing a registry sync, in seconds.
const int kRegistrySyncDelaySeconds = 5;

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
  base::string16 registry_labels;
  bool success = GoogleUpdateSettings::ReadExperimentLabels(is_system_install,
                                                            &registry_labels);
  if (!success) {
    DVLOG(1) << "Error reading Variation labels from the registry.";
    return;
  }

  // Since the non-Variations contents of experiment_labels should not be,
  // clobbered, separate them from the Variations contents.
  const base::string16 other_labels =
      ExtractNonVariationLabels(registry_labels);

  // Compute the new Variations part of the label.
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  const base::string16 variation_labels =
      BuildGoogleUpdateExperimentLabel(active_groups);

  // Append the old non-Variations labels with the new Variations labels and
  // write it back to the registry.
  const base::string16 combined_labels =
      CombineExperimentLabels(variation_labels, other_labels);

  if (!GoogleUpdateSettings::SetExperimentLabels(is_system_install,
                                                 combined_labels)) {
    DVLOG(1) << "Error writing Variation labels to the registry: "
             << combined_labels;
  }
}

}  // namespace chrome_variations
