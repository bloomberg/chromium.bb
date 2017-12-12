// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_H_
#define COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_H_

#include "base/macros.h"

class PrefRegistrySimple;
class PrefService;

namespace variations {

// The primary class that encapsulates state for managing the safe seed.
class SafeSeedManager {
 public:
  // Creates a SafeSeedManager instance, and updates safe mode prefs for
  // bookkeeping.
  SafeSeedManager(bool did_previous_session_exit_cleanly,
                  PrefService* local_state);
  ~SafeSeedManager();

  // Register safe mode prefs in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Records that a fetch has started: pessimistically increments the
  // corresponding failure streak for safe mode.
  void RecordFetchStarted();

  // Records a successful fetch: resets the failure streaks for safe mode.
  void RecordSuccessfulFetch();

 private:
  // The pref service used to store persist the variations seed. Weak reference;
  // must outlive |this| instance.
  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(SafeSeedManager);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_SAFE_SEED_MANAGER_H_
