// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_APP_LIST_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_APP_LIST_HELPER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "sync/api/string_ordinal.h"

class Profile;
class SyncTest;

class SyncAppListHelper {
 public:
  // Singleton implementation.
  static SyncAppListHelper* GetInstance();

  // Initializes the profiles in |test| and registers them with
  // internal data structures.
  void SetupIfNecessary(SyncTest* test);

  // Returns true iff all existing profiles have the same app list entries
  // as the verifier.
  bool AllProfilesHaveSameAppListAsVerifier();

  // Moves an app in |profile|.
  void MoveApp(Profile* profile, size_t from, size_t to);

  // Copies ordinals for item matching |id| from |profile1| to test_->verifier.
  void CopyOrdinalsToVerifier(Profile* profile1, const std::string& id);

  // Helper function for debugging, used to log the app lists on test failures.
  void PrintAppList(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<SyncAppListHelper>;

  SyncAppListHelper();
  ~SyncAppListHelper();

  // Returns true iff |profile| has the same app list as |test_|->verifier()
  // and the app list entries all have the same state.
  bool AppListMatchesVerifier(Profile* profile);

  SyncTest* test_;
  bool setup_completed_;

  DISALLOW_COPY_AND_ASSIGN(SyncAppListHelper);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_APP_LIST_HELPER_H_
