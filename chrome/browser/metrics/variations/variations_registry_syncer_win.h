// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_REGISTRY_SYNCER_WIN_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_REGISTRY_SYNCER_WIN_H_

#include "base/timer/timer.h"

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
