// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_EXTENSIONS_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_EXTENSIONS_SYNC_TEST_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/scoped_temp_dir.h"
#include "base/ref_counted.h"
#include "chrome/test/live_sync/live_extensions_sync_test_base.h"

class Extension;
class Profile;

class LiveExtensionsSyncTest : public LiveExtensionsSyncTestBase {
 public:
  explicit LiveExtensionsSyncTest(TestType test_type);
  virtual ~LiveExtensionsSyncTest();

 protected:
  // Returns true iff the given profile has the same extensions as the
  // verifier.
  bool HasSameExtensionsAsVerifier(int profile) WARN_UNUSED_RESULT;

  // Returns true iff all existing profiles have the same extensions
  // as the verifier.
  bool AllProfilesHaveSameExtensionsAsVerifier() WARN_UNUSED_RESULT;

 private:
  // Returns true iff the given two profiles have the same set of
  // enabled, disabled, and pending extensions.
  static bool HasSameExtensionsHelper(
      Profile* profile1, Profile* profile2) WARN_UNUSED_RESULT;

  DISALLOW_COPY_AND_ASSIGN(LiveExtensionsSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_EXTENSIONS_SYNC_TEST_H_
