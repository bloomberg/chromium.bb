// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_extensions_sync_test.h"

#include "base/basictypes.h"
#include "chrome/common/extensions/extension.h"

class TwoClientLiveExtensionsSyncTest : public LiveExtensionsSyncTest {
 public:
  TwoClientLiveExtensionsSyncTest()
      : LiveExtensionsSyncTest(TWO_CLIENT) {}

  virtual ~TwoClientLiveExtensionsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientLiveExtensionsSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientLiveExtensionsSyncTest,
                       StartWithNoExtensions) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveExtensionsSyncTest,
                       StartWithSameExtensions) {
  ASSERT_TRUE(SetupClients());

  const int kNumExtensions = 5;
  for (int i = 0; i < kNumExtensions; ++i) {
    InstallExtension(GetProfile(0), GetExtension(i));
    InstallExtension(GetProfile(1), GetExtension(i));
    InstallExtension(verifier(), GetExtension(i));
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveExtensionsSyncTest,
                       StartWithDifferentExtensions) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonExtensions = 5;
  for (int j = 0; j < kNumCommonExtensions; ++i, ++j) {
    InstallExtension(GetProfile(0), GetExtension(i));
    InstallExtension(GetProfile(1), GetExtension(i));
    InstallExtension(verifier(), GetExtension(i));
  }

  const int kNumProfile0Extensions = 10;
  for (int j = 0; j < kNumProfile0Extensions; ++i, ++j) {
    InstallExtension(GetProfile(0), GetExtension(i));
    InstallExtension(verifier(), GetExtension(i));
  }

  const int kNumProfile1Extensions = 10;
  for (int j = 0; j < kNumProfile1Extensions; ++i, ++j) {
    InstallExtension(GetProfile(1), GetExtension(i));
    InstallExtension(verifier(), GetExtension(i));
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  InstallAllPendingExtensions(GetProfile(0));
  InstallAllPendingExtensions(GetProfile(1));

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveExtensionsSyncTest,
                       InstallDifferentExtensions) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonExtensions = 5;
  for (int j = 0; j < kNumCommonExtensions; ++i, ++j) {
    InstallExtension(GetProfile(0), GetExtension(i));
    InstallExtension(GetProfile(1), GetExtension(i));
    InstallExtension(verifier(), GetExtension(i));
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  const int kNumProfile0Extensions = 10;
  for (int j = 0; j < kNumProfile0Extensions; ++i, ++j) {
    InstallExtension(GetProfile(0), GetExtension(i));
    InstallExtension(verifier(), GetExtension(i));
  }

  const int kNumProfile1Extensions = 10;
  for (int j = 0; j < kNumProfile1Extensions; ++i, ++j) {
    InstallExtension(GetProfile(1), GetExtension(i));
    InstallExtension(verifier(), GetExtension(i));
  }

  ASSERT_TRUE(AwaitQuiescence());

  InstallAllPendingExtensions(GetProfile(0));
  InstallAllPendingExtensions(GetProfile(1));

  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

// TODO(akalin): Add tests exercising:
//   - Extensions enabled/disabled state sync behavior
//   - Extension uninstallation
//   - Offline installation/uninstallation behavior
