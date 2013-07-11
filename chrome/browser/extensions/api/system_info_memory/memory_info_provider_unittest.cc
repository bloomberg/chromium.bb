// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemoryInfoProvider unit tests.

#include "chrome/browser/extensions/api/system_info_memory/memory_info_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using api::system_info_memory::MemoryInfo;

struct TestMemoryInfo {
  double capacity;
  double available_capacity;
};

const struct TestMemoryInfo kTestingMemoryInfoData = { 4096.0, 1024.0 };

class TestMemoryInfoProvider : public MemoryInfoProvider {
 public:
  virtual bool QueryInfo() OVERRIDE;

 private:
  virtual ~TestMemoryInfoProvider();
};

TestMemoryInfoProvider::~TestMemoryInfoProvider() {}

bool TestMemoryInfoProvider::QueryInfo() {
  info_.capacity = kTestingMemoryInfoData.capacity;
  info_.available_capacity = kTestingMemoryInfoData.available_capacity;
  return true;
}

class MemoryInfoProviderTest : public testing::Test {
 public:
  MemoryInfoProviderTest();

 protected:
  scoped_refptr<TestMemoryInfoProvider> memory_info_provider_;
};

MemoryInfoProviderTest::MemoryInfoProviderTest()
    : memory_info_provider_(new TestMemoryInfoProvider()) {
}

TEST_F(MemoryInfoProviderTest, QueryMemoryInfo) {
  EXPECT_TRUE(memory_info_provider_->QueryInfo());
  EXPECT_DOUBLE_EQ(kTestingMemoryInfoData.capacity,
                   memory_info_provider_->memory_info().capacity);
  EXPECT_DOUBLE_EQ(kTestingMemoryInfoData.available_capacity,
                   memory_info_provider_->memory_info().available_capacity);
}

}  // namespace extensions
