// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATES_GETTER_ANDROID_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATES_GETTER_ANDROID_H_

#include <set>

#include "base/macros.h"
#include "components/sync_driver/sessions/synced_window_delegates_getter.h"

namespace browser_sync {

class SyncedWindowDelegate;

// This class defines how to access SyncedWindowDelegates on Android.
class SyncedWindowDelegatesGetterAndroid : public SyncedWindowDelegatesGetter {
 public:
  SyncedWindowDelegatesGetterAndroid();
  ~SyncedWindowDelegatesGetterAndroid() override;

  // SyncedWindowDelegatesGetter implementation
  std::set<const SyncedWindowDelegate*> GetSyncedWindowDelegates() override;
  const SyncedWindowDelegate* FindById(SessionID::id_type id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedWindowDelegatesGetterAndroid);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_WINDOW_DELEGATES_GETTER_ANDROID_H_
