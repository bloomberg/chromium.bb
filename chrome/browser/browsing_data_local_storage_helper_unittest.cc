// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_local_storage_helper.h"

#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef TestingBrowserProcessTest CannedBrowsingDataLocalStorageTest;

TEST_F(CannedBrowsingDataLocalStorageTest, Empty) {
  TestingProfile profile;

  const GURL origin("http://host1:1/");

  scoped_refptr<CannedBrowsingDataLocalStorageHelper> helper(
      new CannedBrowsingDataLocalStorageHelper(&profile));

  ASSERT_TRUE(helper->empty());
  helper->AddLocalStorage(origin);
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

}  // namespace
