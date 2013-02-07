// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"

namespace extensions {

using api::experimental_system_info_cpu::CpuInfo;

class MockCpuInfoProviderImpl : public CpuInfoProvider {
 public:
  MockCpuInfoProviderImpl() : num_of_processors_(4) {
    // Set sampling interval to 200ms for testing.
    sampling_interval_ = 200;
  }

  virtual bool QueryInfo(CpuInfo* info) OVERRIDE {
    if (!info) return false;

    info->num_of_processors = num_of_processors_;
    info->arch_name = "x86";
    info->model_name = "unknown";

    return true;
  }

 private:
  virtual ~MockCpuInfoProviderImpl() {}

  virtual bool QueryCpuTimePerProcessor(std::vector<CpuTime>* times) OVERRIDE {
    DCHECK(times);

    times->clear();
    static int user_step = 3;
    static int kernel_step = 2;
    static int idle_step = 1;
    static int count = 0;
    for (int i = 0; i < num_of_processors_; i++) {
      CpuTime time;
      time.user += user_step * count;
      time.kernel += kernel_step * count;
      time.idle += idle_step * count;
      times->push_back(time);

      count++;
    }
    return true;
  }

  int num_of_processors_;
};

class SystemInfoCpuApiTest: public ExtensionApiTest {
 public:
  SystemInfoCpuApiTest() {}
  virtual ~SystemInfoCpuApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_UI));
  }

 private:
  scoped_ptr<MessageLoop> message_loop_;
};

IN_PROC_BROWSER_TEST_F(SystemInfoCpuApiTest, Cpu) {
  CpuInfoProvider* provider = new MockCpuInfoProviderImpl();
  // The provider is owned by the single CpuInfoProvider instance.
  CpuInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunExtensionTest("systeminfo/cpu")) << message_;
}

}  // namespace extensions
