// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/live_sync/live_preferences_sync_test.h"

IN_PROC_BROWSER_TEST_F(MultipleClientLivePreferencesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  for (int i = 0; i < num_clients(); ++i) {
    ListPrefUpdate update(GetPrefs(i), prefs::kURLsToRestoreOnStartup);
    ListValue* client_urls = update.Get();
    client_urls->Append(Value::CreateStringValue(base::StringPrintf(
        "http://www.google.com/%d", i)));
  }
  for (int i = 0; i < num_clients(); ++i) {
    GetClient(i)->AwaitGroupSyncCycleCompletion(clients());
  }
  for (int i = 1; i < num_clients(); ++i) {
    ASSERT_TRUE(GetPrefs(0)->GetList(prefs::kURLsToRestoreOnStartup)->
        Equals(GetPrefs(i)->GetList(prefs::kURLsToRestoreOnStartup)));
  }
}
