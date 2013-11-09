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

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
#define MAYBE_Cpu DISABLED_Cpu
#else
#define MAYBE_Cpu Cpu
#endif

IN_PROC_BROWSER_TEST_F(SystemCpuApiTest, MAYBE_Cpu) {
  CpuInfoProvider* provider = new MockCpuInfoProviderImpl();
  // The provider is owned by the single CpuInfoProvider instance.
  CpuInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunExtensionTest("system/cpu")) << message_;
}

}  // namespace extensions
