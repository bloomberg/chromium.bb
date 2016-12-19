// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_MODEL_SYNCED_WINDOW_DELEGATE_GETTER_H_
#define IOS_CHROME_BROWSER_TABS_TAB_MODEL_SYNCED_WINDOW_DELEGATE_GETTER_H_

#include <set>

#include "base/macros.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"

namespace browser_sync {
class SyncedWindowDelegate;
}

namespace ios {
class ChromeBrowserState;
}

class TabModelSyncedWindowDelegatesGetter
    : public sync_sessions::SyncedWindowDelegatesGetter {
 public:
  // TODO(crbug.com/548612): |browser_state| may be unnecessary as iOS does not
  // supports multi-profile starting with M47. Should it be removed?
  explicit TabModelSyncedWindowDelegatesGetter(
      ios::ChromeBrowserState* browser_state);
  ~TabModelSyncedWindowDelegatesGetter() override;

  // sync_sessions::SyncedWindowDelegatesGetter:
  std::set<const sync_sessions::SyncedWindowDelegate*>
  GetSyncedWindowDelegates() override;
  const sync_sessions::SyncedWindowDelegate* FindById(
      SessionID::id_type id) override;

 private:
  const ios::ChromeBrowserState* const browser_state_;

  DISALLOW_COPY_AND_ASSIGN(TabModelSyncedWindowDelegatesGetter);
};

#endif  // IOS_CHROME_BROWSER_TABS_TAB_MODEL_SYNCED_WINDOW_DELEGATE_GETTER_H_
