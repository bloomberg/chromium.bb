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

namespace app_list {
class AppListItem;
}

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

  // Moves an app in |profile| to |folder_id|.
  void MoveAppToFolder(Profile* profile,
                       size_t index,
                       const std::string& folder_id);

  // Moves an app in |profile| from |folder_id| to the top level list of apps.
  void MoveAppFromFolder(Profile* profile,
                         size_t index_in_folder,
                         const std::string& folder_id);

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

  // Helper function for debugging, logs info for an item, including the
  // contents of any folder items.
  void PrintItem(Profile* profile,
                 app_list::AppListItem* item,
                 const std::string& label);

  SyncTest* test_;
  bool setup_completed_;

  DISALLOW_COPY_AND_ASSIGN(SyncAppListHelper);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_APP_LIST_HELPER_H_
