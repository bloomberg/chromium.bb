// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/variations_registry_syncer_win.h"

#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "components/variations/experiment_labels.h"
#include "content/public/browser/browser_thread.h"

namespace chrome_variations {

namespace {

// Delay before performing a registry sync, in seconds.
const int kRegistrySyncDelaySeconds = 5;

// Performs the actual synchronization process with the registry, which should
// be done on a blocking pool thread.
void SyncWithRegistryOnBlockingPool() {
  base::ThreadRestrictions::AssertIOAllowed();

  // Note that all registry operations are done here on the UI thread as there
  // are no threading restrictions on them.
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Failed to get chrome exe path";
    return;
  }
  const bool is_system_install = !InstallUtil::IsPerUserInstall(chrome_exe);

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
      variations::ExtractNonVariationLabels(registry_labels);

  // Compute the new Variations part of the label.
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  const base::string16 variation_labels =
      variations::BuildGoogleUpdateExperimentLabel(active_groups);

  // Append the old non-Variations labels with the new Variations labels and
  // write it back to the registry.
  const base::string16 combined_labels =
      variations::CombineExperimentLabels(variation_labels, other_labels);

  if (!GoogleUpdateSettings::SetExperimentLabels(is_system_install,
                                                 combined_labels)) {
    DVLOG(1) << "Error writing Variation labels to the registry: "
             << combined_labels;
  }
}

}  // namespace

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
               this,
               &VariationsRegistrySyncer::StartRegistrySync);
}

void VariationsRegistrySyncer::StartRegistrySync() {
  // Do the work on a blocking pool thread, as chrome://profiler has shown
  // that it can cause jank if done on the UI thrread.
  content::BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE, base::Bind(&SyncWithRegistryOnBlockingPool));
}

}  // namespace chrome_variations
