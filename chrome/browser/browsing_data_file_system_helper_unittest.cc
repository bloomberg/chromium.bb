// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_file_system_helper.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_profile.h"

namespace {

typedef TestingBrowserProcessTest CannedBrowsingDataFileSystemHelperTest;

TEST_F(CannedBrowsingDataFileSystemHelperTest, Empty) {
  TestingProfile profile;

  const GURL origin("http://host1:1/");

  scoped_refptr<CannedBrowsingDataFileSystemHelper> helper(
      new CannedBrowsingDataFileSystemHelper(&profile));

  ASSERT_TRUE(helper->empty());
  helper->AddFileSystem(origin, fileapi::kFileSystemTypeTemporary, 0);
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

}  // namespace
