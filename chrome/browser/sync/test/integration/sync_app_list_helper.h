// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_APP_LIST_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_APP_LIST_HELPER_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/sync/model/string_ordinal.h"

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

  // Returns true iff all existing profiles have the same app list entries.
  bool AllProfilesHaveSameAppList();

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

  // Helper function for debugging, used to log the app lists on test failures.
  void PrintAppList(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<SyncAppListHelper>;

  SyncAppListHelper();
  ~SyncAppListHelper();

  // Returns true iff |profile1| has the same app list as |profile2|
  // and the app list entries all have the same state.
  bool AppListMatch(Profile* profile1, Profile* profile2);

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
