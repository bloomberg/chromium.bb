// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/cfbundle_blocker.h"

#import <Foundation/Foundation.h>

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {
namespace common {
namespace mac {
namespace {

struct IsBundleAllowedTestcase {
  NSString* bundle_id;
  NSString* version;
  bool allowed;
};

TEST(CFBundleBlockerTest, IsBundleAllowed) {
  const IsBundleAllowedTestcase kTestcases[] = {
    // Block things without a bundle ID.
    { nil, nil, false },

    // Block bundle IDs that aren't in the whitelist.
    { @"org.chromium.Chromium.evil", nil, false },

    // The AllowedBundle structure for Google Authetnicator BT doesn't
    // require a version, so this should work equally well with any version
    // including no version at all.
    { @"com.google.osax.Google_Authenticator_BT", nil, true },
    { @"com.google.osax.Google_Authenticator_BT", @"0.5.0.0", true },

    // Typos should be blocked.
    { @"com.google.osax.Google_Authenticator_B", nil, false },
    { @"com.google.osax.Google_Authenticator_BQ", nil, false },
    { @"com.google.osax.Google_Authenticator_BTQ", nil, false },
    { @"com.google.osax", nil, false },
    { @"com.google", nil, false },
    { @"com", nil, false },
    { @"", nil, false },

    // MySpeed requires a version, so make sure that versions below don't work
    // and versions above do.
    { @"com.enounce.MySpeed.osax", nil, false },
    { @"com.enounce.MySpeed.osax", @"", false },
    { @"com.enounce.MySpeed.osax", @"1200", false },
    { @"com.enounce.MySpeed.osax", @"1201", true },
    { @"com.enounce.MySpeed.osax", @"1202", true },

    // DefaultFolderX is whitelisted as com.stclairsoft.DefaultFolderX. Make
    // sure that "child" IDs such as com.stclairsoft.DefaultFolderX.osax work.
    // It uses a dotted versioning scheme, so test the version comparator out.
    { @"com.stclairsoft.DefaultFolderX.osax", nil, false },
    { @"com.stclairsoft.DefaultFolderX.osax", @"", false },
    { @"com.stclairsoft.DefaultFolderX.osax", @"3.5.4", false },
    { @"com.stclairsoft.DefaultFolderX.osax", @"4.3.4", false },
    { @"com.stclairsoft.DefaultFolderX.osax", @"4.4.2", false },
    { @"com.stclairsoft.DefaultFolderX.osax", @"4.4.3", true },
    { @"com.stclairsoft.DefaultFolderX.osax", @"4.4.4", true },
    { @"com.stclairsoft.DefaultFolderX.osax", @"4.4.10", true },
    { @"com.stclairsoft.DefaultFolderX.osax", @"4.5", true },
    { @"com.stclairsoft.DefaultFolderX.osax", @"4.5.2", true },
    { @"com.stclairsoft.DefaultFolderX.osax", @"4.10", true },
    { @"com.stclairsoft.DefaultFolderX.osax", @"4.10.2", true },
    { @"com.stclairsoft.DefaultFolderX.osax", @"5", true },
    { @"com.stclairsoft.DefaultFolderX.osax", @"5.3", true },
    { @"com.stclairsoft.DefaultFolderX.osax", @"5.3.2", true },

    // Other "child" IDs that might want to load.
    { @"com.stclairsoft.DefaultFolderX.CarbonPatcher", @"4.4.3", true },
    { @"com.stclairsoft.DefaultFolderX.CocoaPatcher", @"4.4.3", true },
  };

  for (size_t index = 0; index < arraysize(kTestcases); ++index) {
    const IsBundleAllowedTestcase& testcase = kTestcases[index];
    NSString* bundle_id = testcase.bundle_id;
    NSString* version = testcase.version;
    NSString* version_print = version ? version : @"(nil)";
    EXPECT_EQ(testcase.allowed, IsBundleAllowed(bundle_id, version))
        << "index " << index
        << ", bundle_id " << [bundle_id UTF8String]
        << ", version " << [version_print UTF8String];
  }
}

}  // namespace
}  // namespace mac
}  // namespace common
}  // namespace chrome
