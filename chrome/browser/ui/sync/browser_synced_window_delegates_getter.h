// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATES_GETTER_H_
#define CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATES_GETTER_H_

#include <set>

#include "base/macros.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_driver/sessions/synced_window_delegates_getter.h"

class Profile;

namespace browser_sync {

class SyncedWindowDelegate;

// This class defines how to access SyncedWindowDelegates on desktop.
class BrowserSyncedWindowDelegatesGetter : public SyncedWindowDelegatesGetter {
 public:
  explicit BrowserSyncedWindowDelegatesGetter(Profile* profile);
  ~BrowserSyncedWindowDelegatesGetter() override;

  // SyncedWindowDelegatesGetter implementation
  std::set<const SyncedWindowDelegate*> GetSyncedWindowDelegates() override;
  const SyncedWindowDelegate* FindById(SessionID::id_type id) override;

 private:
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSyncedWindowDelegatesGetter);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_UI_SYNC_BROWSER_SYNCED_WINDOW_DELEGATES_GETTER_H_
