// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemoryInfoProvider unit tests.

#include "chrome/browser/extensions/api/system_info_memory/memory_info_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using api::experimental_system_info_memory::MemoryInfo;

struct TestMemoryInfo {
  double capacity;
  double available_capacity;
};

const struct TestMemoryInfo kTestingMemoryInfoData = { 4096.0, 1024.0 };

class TestMemoryInfoProvider : public MemoryInfoProvider {
 public:
  virtual bool QueryInfo(MemoryInfo* info) OVERRIDE;
 private:
  virtual ~TestMemoryInfoProvider();
};

TestMemoryInfoProvider::~TestMemoryInfoProvider() {}

bool TestMemoryInfoProvider::QueryInfo(MemoryInfo* info) {
  if (info == NULL)
    return false;
  info->capacity = kTestingMemoryInfoData.capacity;
  info->available_capacity = kTestingMemoryInfoData.available_capacity;
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
  scoped_ptr<MemoryInfo> memory_info(new MemoryInfo());
  EXPECT_TRUE(memory_info_provider_->QueryInfo(memory_info.get()));
  EXPECT_DOUBLE_EQ(kTestingMemoryInfoData.capacity, memory_info->capacity);
  EXPECT_DOUBLE_EQ(kTestingMemoryInfoData.available_capacity,
                   memory_info->available_capacity);
}

}  // namespace extensions
