// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/in_memory_store.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement_tracker {

namespace {
void NoOpCallback(bool success) {}

class InMemoryStoreTest : public ::testing::Test {};
}  // namespace

TEST_F(InMemoryStoreTest, LoadShouldMakeReady) {
  InMemoryStore store;
  EXPECT_FALSE(store.IsReady());
  store.Load(base::Bind(&NoOpCallback));
  EXPECT_TRUE(store.IsReady());
}

}  // namespace feature_engagement_tracker
