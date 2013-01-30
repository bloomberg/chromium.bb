// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_REGISTRY_SYNCER_WIN_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_REGISTRY_SYNCER_WIN_H_

#include <string>

#include "base/metrics/field_trial.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/common/metrics/variations/variation_ids.h"

namespace chrome_variations {

// This class manages synchronizing active VariationIDs with the Google Update
// experiment_labels value in the registry.
class VariationsRegistrySyncer {
 public:
  VariationsRegistrySyncer();
  ~VariationsRegistrySyncer();

  // Starts a timer that, when it expires, updates the registry with the current
  // Variations associated with Google Update. If the timer is already running,
  // calling this just resets the timer.
  void RequestRegistrySync();

  // Takes the list of active groups and builds the label for the ones that have
  // Google Update VariationID associated with them. This will return an empty
  // string if there are no such groups.
  // Note that |active_groups| is passed in separately rather than being fetched
  // here for testing.
  static string16 BuildGoogleUpdateExperimentLabel(
      base::FieldTrial::ActiveGroups& active_groups);

  // Creates a final combined experiment labels string with |variation_labels|
  // and |other_labels|, appropriately appending a separator based on their
  // contents. It is assumed that |variation_labels| and |other_labels| do not
  // have leading or trailing separators.
  static string16 CombineExperimentLabels(const string16& variation_labels,
                                          const string16& other_labels);

  // Takes the value of experiment_labels from the registry and returns a valid
  // experiment_labels string value containing only the labels that are not
  // associated with Chrome Variations. This is exposed as static for testing.
  static string16 ExtractNonVariationLabels(const string16& labels);

 private:
  // Perform the actual synchronization process with the registry.
  void SyncWithRegistry();

  // A timer used to delay the writes to the registry. This is done to optimize
  // the case where lazy-loaded features start their field trials some time
  // after initial batch of field trials are created, and also to avoid blocking
  // the UI thread. The timer effectively allows this class to batch together
  // update requests, to avoid reading and writing from the registry too much.
  base::OneShotTimer<VariationsRegistrySyncer> timer_;

  DISALLOW_COPY_AND_ASSIGN(VariationsRegistrySyncer);
};

}  // namespace chrome_variations

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_REGISTRY_SYNCER_WIN_H_
