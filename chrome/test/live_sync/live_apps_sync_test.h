// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_APPS_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_APPS_SYNC_TEST_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/test/live_sync/live_sync_extension_helper.h"
#include "chrome/test/live_sync/live_sync_test.h"

class Profile;

class LiveAppsSyncTest : public LiveSyncTest {
 public:
  explicit LiveAppsSyncTest(TestType test_type);
  virtual ~LiveAppsSyncTest();

 protected:
  // Like LiveSyncTest::SetupClients(), but also sets up
  // |extension_helper_|.
  virtual bool SetupClients() OVERRIDE WARN_UNUSED_RESULT;

  // Returns true iff the profile with index |index| has the same apps as the
  // verifier.
  bool HasSameAppsAsVerifier(int index) WARN_UNUSED_RESULT;

  // Returns true iff all existing profiles have the same apps as the
  // verifier.
  bool AllProfilesHaveSameAppsAsVerifier() WARN_UNUSED_RESULT;

  // Installs the app for the given index to |profile|.
  void InstallApp(Profile* profile, int index);

  // Uninstalls the app for the given index from |profile|. Assumes that it was
  // previously installed.
  void UninstallApp(Profile* profile, int index);

  // Installs all pending synced apps for |profile|.
  void InstallAppsPendingForSync(Profile* profile);

 private:
  LiveSyncExtensionHelper extension_helper_;

  DISALLOW_COPY_AND_ASSIGN(LiveAppsSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_APPS_SYNC_TEST_H_
