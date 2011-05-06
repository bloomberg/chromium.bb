// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_token_store.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(CloudPrintTokenStoreTest, Basic) {
  EXPECT_EQ(NULL, CloudPrintTokenStore::current());
  CloudPrintTokenStore* store = new CloudPrintTokenStore;
  EXPECT_EQ(store, CloudPrintTokenStore::current());
  CloudPrintTokenStore::current()->SetToken("myclientlogintoken", false);
  EXPECT_EQ(CloudPrintTokenStore::current()->token(), "myclientlogintoken");
  EXPECT_FALSE(CloudPrintTokenStore::current()->token_is_oauth());
  CloudPrintTokenStore::current()->SetToken("myoauth2token", true);
  EXPECT_EQ(CloudPrintTokenStore::current()->token(), "myoauth2token");
  EXPECT_TRUE(CloudPrintTokenStore::current()->token_is_oauth());
  delete store;
  EXPECT_EQ(NULL, CloudPrintTokenStore::current());
}

