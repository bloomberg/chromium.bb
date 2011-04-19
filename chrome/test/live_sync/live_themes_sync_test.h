// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_THEMES_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_THEMES_SYNC_TEST_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/test/live_sync/live_sync_extension_helper.h"
#include "chrome/test/live_sync/live_sync_test.h"

class Profile;

class LiveThemesSyncTest : public LiveSyncTest {
 public:
  explicit LiveThemesSyncTest(TestType test_type);
  virtual ~LiveThemesSyncTest();

 protected:
  // Like LiveSyncTest::SetupClients(), but also sets up
  // |extension_helper_|.
  virtual bool SetupClients() OVERRIDE WARN_UNUSED_RESULT;

  // Gets the unique ID of the custom theme with the given index.
  std::string GetCustomTheme(int index) const WARN_UNUSED_RESULT;

  // Gets the ID of |profile|'s theme.
  std::string GetThemeID(Profile* profile) const WARN_UNUSED_RESULT;

  // Returns true iff |profile| is using a custom theme.
  bool UsingCustomTheme(Profile* profile) const WARN_UNUSED_RESULT;

  // Returns true iff |profile| is using the default theme.
  bool UsingDefaultTheme(Profile* profile) const WARN_UNUSED_RESULT;

  // Returns true iff |profile| is using the native theme.
  bool UsingNativeTheme(Profile* profile) const WARN_UNUSED_RESULT;

  // Returns true iff a theme with the given ID is pending install in
  // |profile|.
  bool ThemeIsPendingInstall(
      Profile* profile, const std::string& id) const WARN_UNUSED_RESULT;

  // Returns true iff |profile|'s current theme is the given
  // custom theme or if the given theme is pending install.
  bool HasOrWillHaveCustomTheme(
      Profile* profile, const std::string& id) const WARN_UNUSED_RESULT;

  // Sets |profile| to use the custom theme with the given index.
  void UseCustomTheme(Profile* profile, int index);

  // Sets |profile| to use the default theme.
  void UseDefaultTheme(Profile* profile);

  // Sets |profile| to use the native theme.
  void UseNativeTheme(Profile* profile);

 private:
  LiveSyncExtensionHelper extension_helper_;

  DISALLOW_COPY_AND_ASSIGN(LiveThemesSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_THEMES_SYNC_TEST_H_
