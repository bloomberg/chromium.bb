// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

namespace extensions {

using api::experimental_system_info_storage::StorageUnitInfo;

class MockStorageInfoProvider : public StorageInfoProvider {
 public:
  MockStorageInfoProvider() {}
  virtual ~MockStorageInfoProvider() {}

  virtual bool QueryInfo(StorageInfo* info) OVERRIDE {
    info->clear();

    linked_ptr<StorageUnitInfo> unit(new StorageUnitInfo());
    unit->id = "0xbeaf";
    unit->type = systeminfo::kStorageTypeUnknown;
    unit->capacity = 4098;
    unit->available_capacity = 1024;

    info->push_back(unit);
    return true;
  }

  virtual bool QueryUnitInfo(const std::string& id,
                             StorageUnitInfo* info) OVERRIDE {
    return false;
  }
};

class SystemInfoStorageApiTest: public ExtensionApiTest {
 public:
  SystemInfoStorageApiTest() {}
  ~SystemInfoStorageApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_UI));
  }

 private:
  scoped_ptr<MessageLoop> message_loop_;
};

IN_PROC_BROWSER_TEST_F(SystemInfoStorageApiTest, Storage) {
  StorageInfoProvider* provider = new MockStorageInfoProvider();
  StorageInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunExtensionTest("systeminfo/storage")) << message_;
}

} // namespace extensions
