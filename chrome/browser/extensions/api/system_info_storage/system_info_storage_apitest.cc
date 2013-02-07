// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/timer.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/system_info_event_router.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"

namespace extensions {

using api::experimental_system_info_storage::ParseStorageUnitType;
using api::experimental_system_info_storage::StorageUnitInfo;

struct TestUnitInfo {
  std::string id;
  std::string type;
  double capacity;
  double available_capacity;
  // The change step of free space.
  int change_step;
};

struct TestUnitInfo testing_data[] = {
  {"0xbeaf", "unknown", 4098, 1000, 0},
  {"/home","harddisk", 4098, 1000, 10},
  {"/data", "harddisk", 10000, 1000, 4097}
};

const size_t kTestingIntervalMS = 10;

class TestStorageInfoProvider : public StorageInfoProvider {
 public:
  TestStorageInfoProvider() {}

 private:
  virtual ~TestStorageInfoProvider() {}

  virtual bool QueryInfo(StorageInfo* info) OVERRIDE {
    info->clear();

    for (size_t i = 0; i < arraysize(testing_data); i++) {
      linked_ptr<StorageUnitInfo> unit(new StorageUnitInfo());
      QueryUnitInfo(testing_data[i].id, unit.get());
      info->push_back(unit);
    }
    return true;
  }

  virtual bool QueryUnitInfo(const std::string& id,
                             StorageUnitInfo* info) OVERRIDE {
    for (size_t i = 0; i < arraysize(testing_data); i++) {
      if (testing_data[i].id == id) {
        info->id = testing_data[i].id;
        info->type = ParseStorageUnitType(testing_data[i].type);
        info->capacity = testing_data[i].capacity;
        info->available_capacity = testing_data[i].available_capacity;
        // Increase the available capacity with a fixed change step.
        testing_data[i].available_capacity += testing_data[i].change_step;
        return true;
      }
    }
    return false;
  }
};

class SystemInfoStorageApiTest: public ExtensionApiTest {
 public:
  SystemInfoStorageApiTest() {}
  virtual ~SystemInfoStorageApiTest() {}

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

IN_PROC_BROWSER_TEST_F(SystemInfoStorageApiTest, Storage) {
  TestStorageInfoProvider* provider = new TestStorageInfoProvider();
  provider->set_watching_interval(kTestingIntervalMS);
  StorageInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunPlatformAppTest("systeminfo/storage")) << message_;
}

} // namespace extensions
