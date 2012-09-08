// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

namespace extensions {

using api::experimental_system_info_cpu::CpuInfo;

class MockCpuInfoProviderImpl : public CpuInfoProvider {
 public:
  MockCpuInfoProviderImpl() {}
  ~MockCpuInfoProviderImpl() {}

  virtual bool QueryInfo(CpuInfo* info) OVERRIDE {
    if (!info) return false;

    info->num_of_processors = 4;
    info->arch_name = "x86";
    info->model_name = "unknown";

    return true;
  }
};

class SystemInfoCpuApiTest: public ExtensionApiTest {
 public:
  SystemInfoCpuApiTest() {}
  ~SystemInfoCpuApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_UI));

    CpuInfoProvider* provider = new MockCpuInfoProviderImpl();
    // The provider is owned by the single CpuInfoProvider instance.
    CpuInfoProvider::InitializeForTesting(provider);
  }

 private:
  scoped_ptr<MessageLoop> message_loop_;
};

IN_PROC_BROWSER_TEST_F(SystemInfoCpuApiTest, Cpu) {
  ASSERT_TRUE(RunExtensionTest("systeminfo/cpu")) << message_;
}

}  // namespace extensions
