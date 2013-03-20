// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/browser/storage_monitor/test_storage_monitor.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"

using chrome::StorageMonitor;
using chrome::test::TestStorageMonitor;
using extensions::api::experimental_system_info_storage::ParseStorageUnitType;
using extensions::api::experimental_system_info_storage::StorageUnitInfo;
using extensions::StorageInfoProvider;
using extensions::StorageInfo;

struct TestUnitInfo {
  std::string id;
  std::string type;
  double capacity;
  double available_capacity;
  // The change step of free space.
  int change_step;
};

struct TestUnitInfo kTestingData[] = {
  {"0xbeaf", "unknown", 4098, 1000, 0},
  {"/home","harddisk", 4098, 1000, 10},
  {"/data", "harddisk", 10000, 1000, 4097}
};

struct TestUnitInfo kRemovableStorageData[] = {
  {"/media/usb1", "removable", 4098, 1000, 1}
};

const char kRemovableStorageDeviceName[] = "deviceName";
const char kRemovableStorageDeviceId[] = "path://0xbeaf1";
const base::FilePath::CharType kRemovableStorageLocation[] =
    FILE_PATH_LITERAL("/media/usb1");
const size_t kTestingIntervalMS = 10;

class TestStorageInfoProvider : public StorageInfoProvider {
 public:
  TestStorageInfoProvider(struct TestUnitInfo testing_data[], size_t n)
      : testing_data_(testing_data, testing_data + n) {
    SetWatchingIntervalForTesting(kTestingIntervalMS);
  }

 private:
  virtual ~TestStorageInfoProvider() {}

  virtual bool QueryInfo(StorageInfo* info) OVERRIDE {
    info->clear();

    for (size_t i = 0; i < testing_data_.size(); i++) {
      linked_ptr<StorageUnitInfo> unit(new StorageUnitInfo());
      QueryUnitInfo(testing_data_[i].id, unit.get());
      info->push_back(unit);
    }
    return true;
  }

  virtual bool QueryUnitInfo(const std::string& id,
                             StorageUnitInfo* info) OVERRIDE {
    for (size_t i = 0; i < testing_data_.size(); i++) {
      if (testing_data_[i].id == id) {
        info->id = testing_data_[i].id;
        info->type = ParseStorageUnitType(testing_data_[i].type);
        info->capacity = testing_data_[i].capacity;
        info->available_capacity = testing_data_[i].available_capacity;
        // Increase the available capacity with a fixed change step.
        testing_data_[i].available_capacity += testing_data_[i].change_step;
        return true;
      }
    }
    return false;
  }

 private:
  std::vector<struct TestUnitInfo> testing_data_;
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

  void ProcessAttach(const std::string& device_id,
                     const string16& name,
                     const base::FilePath::StringType& location) {
    chrome::StorageInfo info(device_id, name, location);
    StorageMonitor::GetInstance()->receiver()->ProcessAttach(info);
  }

  void ProcessDetach(const std::string& id) {
    StorageMonitor::GetInstance()->receiver()->ProcessDetach(id);
  }

 private:
  scoped_ptr<MessageLoop> message_loop_;
};

IN_PROC_BROWSER_TEST_F(SystemInfoStorageApiTest, Storage) {
  TestStorageInfoProvider* provider =
      new TestStorageInfoProvider(kTestingData, arraysize(kTestingData));
  StorageInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunPlatformAppTest("systeminfo/storage")) << message_;
}

IN_PROC_BROWSER_TEST_F(SystemInfoStorageApiTest, StorageAttachment) {
  scoped_ptr<TestStorageMonitor> monitor(
      TestStorageMonitor::CreateForBrowserTests());

  TestStorageInfoProvider* provider =
      new TestStorageInfoProvider(kRemovableStorageData,
                                  arraysize(kRemovableStorageData));
  StorageInfoProvider::InitializeForTesting(provider);

  ResultCatcher catcher;
  ExtensionTestMessageListener attach_listener("attach", false);
  ExtensionTestMessageListener detach_listener("detach", false);

  EXPECT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("systeminfo/storage_attachment")));

  // Simulate triggering onAttached event.
  ASSERT_TRUE(attach_listener.WaitUntilSatisfied());
  ProcessAttach(kRemovableStorageDeviceId,
                ASCIIToUTF16(kRemovableStorageDeviceName),
                kRemovableStorageLocation);
  // Simulate triggering onDetached event.
  ASSERT_TRUE(detach_listener.WaitUntilSatisfied());
  ProcessDetach(kRemovableStorageDeviceId);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
