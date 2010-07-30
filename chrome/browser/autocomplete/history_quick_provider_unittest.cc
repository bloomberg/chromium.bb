// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_quick_provider.h"

#include "base/scoped_ptr.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class HistoryQuickProviderTest : public testing::Test,
                                 public ACProviderListener {
 public:
  // ACProviderListener
  virtual void OnProviderUpdate(bool updated_matches);

 protected:
  void SetUp() {
    profile_.reset(new TestingProfile());
    provider_ = new HistoryQuickProvider(this, profile_.get());
  }
  void TearDown() {
    provider_ = NULL;
  }

  scoped_refptr<HistoryQuickProvider> provider_;
  scoped_ptr<TestingProfile> profile_;
};

void HistoryQuickProviderTest::OnProviderUpdate(bool updated_matches) {
}

TEST_F(HistoryQuickProviderTest, Construction) {
  EXPECT_TRUE(provider_.get());
}
