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

using api::experimental_system_info_storage::StorageUnitInfo;

const int kDefaultIntervalMs = 200;
const int kAvailableCapacityBytes = 10000;
const int kChangeDelta = 10;

class MockStorageInfoProvider : public StorageInfoProvider {
 public:
  MockStorageInfoProvider() : is_watching_(false) {
  }

  virtual bool QueryInfo(StorageInfo* info) OVERRIDE {
    info->clear();

    linked_ptr<StorageUnitInfo> unit(new StorageUnitInfo());
    unit->id = "0xbeaf";
    unit->type =
        api::experimental_system_info_storage::STORAGE_UNIT_TYPE_UNKNOWN;
    unit->capacity = 4098;
    unit->available_capacity = 1024;

    info->push_back(unit);
    return true;
  }

  virtual bool QueryUnitInfo(const std::string& id,
                             StorageUnitInfo* info) OVERRIDE {
    return false;
  }

  bool Start() {
    if (is_watching_) return false;

    // Start the timer to emulate storage.onChanged event.
    timer_.Start(FROM_HERE,
                  base::TimeDelta::FromMilliseconds(kDefaultIntervalMs),
                  this, &MockStorageInfoProvider::OnTimeoutEvent);

    is_watching_ = true;
    return true;
  }

  bool Stop() {
    if (!is_watching_) return false;
    is_watching_ = false;
    timer_.Stop();
    return true;
  }

 private:
  virtual ~MockStorageInfoProvider() {
    Stop();
  }

  void OnTimeoutEvent() {
    static int count;
    SystemInfoEventRouter::GetInstance()->
      OnStorageAvailableCapacityChanged("/dev/sda1",
          kAvailableCapacityBytes - count * kChangeDelta);
    count++;
  }

  // Use a repeating timer to emulate storage free space change event.
  base::RepeatingTimer<MockStorageInfoProvider> timer_;
  bool is_watching_;
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
  ResultCatcher catcher;
  scoped_refptr<MockStorageInfoProvider> provider =
      new MockStorageInfoProvider();
  StorageInfoProvider::InitializeForTesting(provider);

  ExtensionTestMessageListener listener("ready", true);
  const extensions::Extension* extension =
    LoadExtension(test_data_dir_.AppendASCII("systeminfo/storage"));
  GURL page_url = extension->GetResourceURL("test_storage_api.html");
  ui_test_utils::NavigateToURL(browser(), page_url);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  provider->Start();
  listener.Reply("go");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

} // namespace extensions
