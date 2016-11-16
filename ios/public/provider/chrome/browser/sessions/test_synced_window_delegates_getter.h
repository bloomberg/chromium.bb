// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SESSIONS_TEST_SYNCED_WINDOW_DELEGATES_GETTER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SESSIONS_TEST_SYNCED_WINDOW_DELEGATES_GETTER_H_

#include "base/macros.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"

class TestSyncedWindowDelegatesGetter
    : public sync_sessions::SyncedWindowDelegatesGetter {
 public:
  TestSyncedWindowDelegatesGetter() {}
  ~TestSyncedWindowDelegatesGetter() override {}

  // SyncedWindowDelegatesGetter.
  std::set<const sync_sessions::SyncedWindowDelegate*>
  GetSyncedWindowDelegates() override;
  const sync_sessions::SyncedWindowDelegate* FindById(
      SessionID::id_type id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSyncedWindowDelegatesGetter);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SESSIONS_TEST_SYNCED_WINDOW_DELEGATES_GETTER_H_
