// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/browser/api/system_memory/memory_info_provider.h"

namespace extensions {

using core_api::system_memory::MemoryInfo;

class MockMemoryInfoProviderImpl : public MemoryInfoProvider {
 public:
  MockMemoryInfoProviderImpl() {}

  virtual bool QueryInfo() OVERRIDE {
    info_.capacity = 4096;
    info_.available_capacity = 1024;
    return true;
  }
 private:
  virtual ~MockMemoryInfoProviderImpl() {}
};

class SystemMemoryApiTest: public ExtensionApiTest {
 public:
  SystemMemoryApiTest() {}
  virtual ~SystemMemoryApiTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    message_loop_.reset(new base::MessageLoopForUI);
  }

 private:
  scoped_ptr<base::MessageLoop> message_loop_;
};

IN_PROC_BROWSER_TEST_F(SystemMemoryApiTest, Memory) {
  scoped_refptr<MemoryInfoProvider> provider = new MockMemoryInfoProviderImpl();
  // The provider is owned by the single MemoryInfoProvider instance.
  MemoryInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunExtensionTest("system/memory")) << message_;
}

}  // namespace extensions
