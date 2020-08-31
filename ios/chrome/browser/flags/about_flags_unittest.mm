// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/flags/about_flags.h"

#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/flags_test_helpers.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using AboutFlagsTest = PlatformTest;

// Makes sure that every flag has an owner and an expiry entry in
// flag-metadata.json.
TEST_F(AboutFlagsTest, EveryFlagHasMetadata) {
  size_t count;
  const flags_ui::FeatureEntry* entries = testing::GetFeatureEntries(&count);
  flags_ui::testing::EnsureEveryFlagHasMetadata(entries, count);
}

// Ensures that all flags marked as never expiring in flag-metadata.json is
// listed in flag-never-expire-list.json.
TEST_F(AboutFlagsTest, OnlyPermittedFlagsNeverExpire) {
  flags_ui::testing::EnsureOnlyPermittedFlagsNeverExpire();
}

// Ensures that every flag has an owner.
TEST_F(AboutFlagsTest, EveryFlagHasNonEmptyOwners) {
  flags_ui::testing::EnsureEveryFlagHasNonEmptyOwners();
}

// Ensures that owners conform to rules in flag-metadata.json.
TEST_F(AboutFlagsTest, OwnersLookValid) {
  flags_ui::testing::EnsureOwnersLookValid();
}

// Ensures that flags are listed in alphabetical order in flag-metadata.json and
// flag-never-expire-list.json.
TEST_F(AboutFlagsTest, FlagsListedInAlphabeticalOrder) {
  flags_ui::testing::EnsureFlagsAreListedInAlphabeticalOrder();
}
