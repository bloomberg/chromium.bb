// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_monitor_android.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MemoryMonitorAndroidTest : public testing::Test {
 public:
  MemoryMonitorAndroidTest() : monitor_(MemoryMonitorAndroid::Create()) {}

 protected:
  std::unique_ptr<MemoryMonitorAndroid> monitor_;
};

TEST_F(MemoryMonitorAndroidTest, GetMemoryInfo) {
  MemoryMonitorAndroid::MemoryInfo info;
  monitor_->GetMemoryInfo(&info);
  EXPECT_GT(info.avail_mem, 0);
  EXPECT_GT(info.threshold, 0);
  EXPECT_GT(info.total_mem, 0);
}

}  // namespace content
