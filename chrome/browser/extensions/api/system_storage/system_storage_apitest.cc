// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/system_storage/storage_info_provider.h"
#include "chrome/browser/extensions/api/system_storage/test_storage_info_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/switches.h"

namespace {

using chrome::StorageMonitor;
using extensions::api::system_storage::ParseStorageUnitType;
using extensions::api::system_storage::StorageUnitInfo;
using extensions::StorageInfoProvider;
using extensions::StorageUnitInfoList;
using extensions::systeminfo::kStorageTypeFixed;
using extensions::systeminfo::kStorageTypeRemovable;
using extensions::systeminfo::kStorageTypeUnknown;
using extensions::TestStorageUnitInfo;
using extensions::TestStorageInfoProvider;

const struct TestStorageUnitInfo kTestingData[] = {
  {"dcim:device:0004", "transient:0004", "0xbeaf", kStorageTypeUnknown,
    4098, 1000, 0},
  {"path:device:002", "transient:002", "/home", kStorageTypeFixed,
    4098, 1000, 10},
  {"path:device:003", "transient:003", "/data", kStorageTypeFixed,
    10000, 1000, 4097}
};

const struct TestStorageUnitInfo kRemovableStorageData[] = {
  {"dcim:device:0004", "transient:0004", "/media/usb1",
    kStorageTypeRemovable, 4098, 1000, 1}
};

}  // namespace

class SystemStorageApiTest : public ExtensionApiTest {
 public:
  SystemStorageApiTest() {}
  virtual ~SystemStorageApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        extensions::switches::kEnableExperimentalExtensionApis);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    message_loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_UI));
  }

  void AttachRemovableStorage(const std::string& device_id) {
    for (size_t i = 0; i < arraysize(kRemovableStorageData); ++i) {
      if (kRemovableStorageData[i].device_id != device_id)
        continue;

      StorageMonitor::GetInstance()->receiver()->ProcessAttach(
          TestStorageInfoProvider::BuildStorageInfo(kRemovableStorageData[i]));
    }
  }

  void DetachRemovableStorage(const std::string& id) {
    StorageMonitor::GetInstance()->receiver()->ProcessDetach(id);
  }

 private:
  scoped_ptr<base::MessageLoop> message_loop_;
};

IN_PROC_BROWSER_TEST_F(SystemStorageApiTest, Storage) {
  TestStorageInfoProvider* provider =
      new TestStorageInfoProvider(kTestingData, arraysize(kTestingData));
  StorageInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunPlatformAppTest("system/storage")) << message_;
}

IN_PROC_BROWSER_TEST_F(SystemStorageApiTest, StorageAttachment) {
  TestStorageInfoProvider* provider =
      new TestStorageInfoProvider(kRemovableStorageData,
                                  arraysize(kRemovableStorageData));
  StorageInfoProvider::InitializeForTesting(provider);

  ResultCatcher catcher;
  ExtensionTestMessageListener attach_listener("attach", false);
  ExtensionTestMessageListener detach_listener("detach", false);

  EXPECT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("system/storage_attachment")));

  // Simulate triggering onAttached event.
  ASSERT_TRUE(attach_listener.WaitUntilSatisfied());
  AttachRemovableStorage(kRemovableStorageData[0].device_id);
  // Simulate triggering onDetached event.
  ASSERT_TRUE(detach_listener.WaitUntilSatisfied());
  DetachRemovableStorage(kRemovableStorageData[0].device_id);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
