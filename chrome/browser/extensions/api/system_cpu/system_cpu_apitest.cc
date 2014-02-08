// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_cpu/cpu_info_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"

namespace extensions {

using api::system_cpu::CpuInfo;

class MockCpuInfoProviderImpl : public CpuInfoProvider {
 public:
  MockCpuInfoProviderImpl() {}

  virtual bool QueryInfo() OVERRIDE {
    info_.num_of_processors = 4;
    info_.arch_name = "x86";
    info_.model_name = "unknown";

    info_.features.clear();
    info_.features.push_back("mmx");
    info_.features.push_back("avx");

    info_.processors.clear();
    info_.processors.push_back(linked_ptr<api::system_cpu::ProcessorInfo>(
        new api::system_cpu::ProcessorInfo()));
    info_.processors[0]->usage.kernel = 1;
    info_.processors[0]->usage.user = 2;
    info_.processors[0]->usage.idle = 3;
    info_.processors[0]->usage.total = 6;
    return true;
  }

 private:
  virtual ~MockCpuInfoProviderImpl() {}
};

class SystemCpuApiTest: public ExtensionApiTest {
 public:
  SystemCpuApiTest() {}
  virtual ~SystemCpuApiTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(SystemCpuApiTest, Cpu) {
  CpuInfoProvider* provider = new MockCpuInfoProviderImpl();
  // The provider is owned by the single CpuInfoProvider instance.
  CpuInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunExtensionTest("system/cpu")) << message_;
}

}  // namespace extensions
