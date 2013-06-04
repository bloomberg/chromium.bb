// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CpuInfoProvider unit tests

#include "base/message_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using api::system_info_cpu::CpuInfo;

struct TestCpuInfo {
  std::string arch_name;
  std::string model_name;
  int num_of_processors;
};

const struct TestCpuInfo kTestingCpuInfoData = {
  "x86", "Intel Core", 2
};

class TestCpuInfoProvider : public CpuInfoProvider {
 public:
  TestCpuInfoProvider();
  virtual bool QueryInfo(CpuInfo* info) OVERRIDE;

 private:
  virtual ~TestCpuInfoProvider();
};

TestCpuInfoProvider::TestCpuInfoProvider() {}

TestCpuInfoProvider::~TestCpuInfoProvider() {}

bool TestCpuInfoProvider::QueryInfo(CpuInfo* info) {
  if (info == NULL)
    return false;
  info->arch_name = kTestingCpuInfoData.arch_name;
  info->model_name = kTestingCpuInfoData.model_name;
  info->num_of_processors = kTestingCpuInfoData.num_of_processors;
  return true;
}

class CpuInfoProviderTest : public testing::Test {
 public:
  CpuInfoProviderTest();

 protected:
  scoped_refptr<TestCpuInfoProvider> cpu_info_provider_;
};

CpuInfoProviderTest::CpuInfoProviderTest() {}

TEST_F(CpuInfoProviderTest, QueryCpuInfo) {
  cpu_info_provider_ = new TestCpuInfoProvider();
  scoped_ptr<CpuInfo> cpu_info(new CpuInfo());
  EXPECT_TRUE(cpu_info_provider_->QueryInfo(cpu_info.get()));
  EXPECT_EQ(kTestingCpuInfoData.arch_name, cpu_info->arch_name);
  EXPECT_EQ(kTestingCpuInfoData.model_name, cpu_info->model_name);
  EXPECT_EQ(kTestingCpuInfoData.num_of_processors, cpu_info->num_of_processors);
}

}
