// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/api/system_info_memory/memory_info_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"

namespace extensions {

using api::experimental_system_info_memory::MemoryInfo;

class MockMemoryInfoProviderImpl : public MemoryInfoProvider {
 public:
  MockMemoryInfoProviderImpl() {}

  virtual bool QueryInfo(MemoryInfo* info) OVERRIDE {
    info->capacity = 4096;
    info->available_capacity = 1024;
    return true;
  }
 private:
  virtual ~MockMemoryInfoProviderImpl() {}
};

class SystemInfoMemoryApiTest: public ExtensionApiTest {
 public:
  SystemInfoMemoryApiTest() {}
  virtual ~SystemInfoMemoryApiTest() {}

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

IN_PROC_BROWSER_TEST_F(SystemInfoMemoryApiTest, Memory) {
  scoped_refptr<MemoryInfoProvider> provider = new MockMemoryInfoProviderImpl();
  // The provider is owned by the single MemoryInfoProvider instance.
  MemoryInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunExtensionTest("systeminfo/memory")) << message_;
}

}  // namespace extensions
