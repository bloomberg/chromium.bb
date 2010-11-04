// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_THEMES_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_THEMES_SYNC_TEST_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/scoped_temp_dir.h"
#include "base/ref_counted.h"
#include "chrome/test/live_sync/live_sync_test.h"

class Extension;
class Profile;

class LiveThemesSyncTest : public LiveSyncTest {
 public:
  explicit LiveThemesSyncTest(TestType test_type);
  virtual ~LiveThemesSyncTest();

  // Like LiveSyncTest::SetupClients(), but also initializes
  // extensions for each profile and also creates n themes (n =
  // num_clients()).
  virtual bool SetupClients() WARN_UNUSED_RESULT;

 protected:
  // Get the index'th theme created by SetupClients().
  scoped_refptr<Extension> GetTheme(int index) WARN_UNUSED_RESULT;

  // Set the theme of the given profile to a custom theme from the
  // given theme extension.
  static void SetTheme(Profile* profile, scoped_refptr<Extension> theme);

  // Gets the custom theme of the given profile, or NULL if the given
  // profile doesn't have one.
  const Extension* GetCustomTheme(Profile* profile) WARN_UNUSED_RESULT;

  // Returns true iff the given profile is using the default theme.
  bool UsingDefaultTheme(Profile* profile) WARN_UNUSED_RESULT;

  // Returns true iff the given profile is using the native theme.  On
  // platforms where the native theme is just the default theme, this
  // is equivalent to UsingDefaultTheme().
  bool UsingNativeTheme(Profile* profile) WARN_UNUSED_RESULT;

  // Returns true iff the given extension is pending install for the
  // given profile.
  bool ExtensionIsPendingInstall(
      Profile* profile, const Extension* extension) WARN_UNUSED_RESULT;

  // Returns true iff the given profile's current theme is the given
  // custom theme or if the given theme is pending install.
  bool HasOrWillHaveCustomTheme(Profile* profile,
                                const Extension* theme) WARN_UNUSED_RESULT;

 private:
  ScopedTempDir theme_dir_;
  std::vector<scoped_refptr<Extension> > themes_;

  DISALLOW_COPY_AND_ASSIGN(LiveThemesSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_THEMES_SYNC_TEST_H_
