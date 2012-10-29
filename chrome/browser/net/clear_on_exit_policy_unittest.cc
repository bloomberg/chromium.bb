// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/net/clear_on_exit_policy.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/mock_special_storage_policy.h"


TEST(ClearOnExitPolicyTest, HasClearOnExitOrigins) {
  scoped_refptr<quota::MockSpecialStoragePolicy> storage_policy =
      new quota::MockSpecialStoragePolicy;
  scoped_refptr<ClearOnExitPolicy> policy =
      new ClearOnExitPolicy(storage_policy.get());

  EXPECT_FALSE(policy->HasClearOnExitOrigins());

  storage_policy->AddSessionOnly(GURL("http://test.com/"));
  EXPECT_TRUE(policy->HasClearOnExitOrigins());
}

TEST(ClearOnExitPolicyTest, ShouldClearOriginOnExit) {
  scoped_refptr<quota::MockSpecialStoragePolicy> storage_policy =
      new quota::MockSpecialStoragePolicy;
  storage_policy->AddSessionOnly(GURL("http://session.com/"));
  storage_policy->AddSessionOnly(GURL("https://secure.com/"));

  scoped_refptr<ClearOnExitPolicy> policy =
      new ClearOnExitPolicy(storage_policy.get());

  EXPECT_TRUE(policy->ShouldClearOriginOnExit("session.com", false));
  EXPECT_FALSE(policy->ShouldClearOriginOnExit("session.com", true));

  EXPECT_FALSE(policy->ShouldClearOriginOnExit("secure.com", false));
  EXPECT_TRUE(policy->ShouldClearOriginOnExit("secure.com", true));

  EXPECT_FALSE(policy->ShouldClearOriginOnExit("other.com", false));
  EXPECT_FALSE(policy->ShouldClearOriginOnExit("other.com", true));
}
