// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/api/system_info_display/display_info_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/common/chrome_switches.h"

namespace extensions {

using api::system_info_display::Bounds;
using api::system_info_display::DisplayUnitInfo;

class MockDisplayInfoProvider : public DisplayInfoProvider {
 public:
  virtual bool QueryInfo(DisplayInfo* info) OVERRIDE {
    info->clear();
    for (int i = 0; i < 2; i++) {
      linked_ptr<DisplayUnitInfo> unit(new DisplayUnitInfo());
      unit->id = "DISPLAY";
      unit->name = "DISPLAY NAME";
      unit->is_primary = i == 0 ? true : false;
      unit->is_internal = i == 0 ? true : false;
      unit->is_enabled = true;
      unit->dpi_x = 96.0;
      unit->dpi_y = 96.0;
      unit->bounds.left = 0;
      unit->bounds.top = 0;
      unit->bounds.width = 1280;
      unit->bounds.height = 720;
      unit->work_area.left = 0;
      unit->work_area.top = 0;
      unit->work_area.width = 960;
      unit->work_area.height = 720;
      info->push_back(unit);
    }
    return true;
  }
 private:
  virtual ~MockDisplayInfoProvider() {}

};

class SystemInfoDisplayApiTest: public ExtensionApiTest {
 public:
  SystemInfoDisplayApiTest() {}
  virtual ~SystemInfoDisplayApiTest() {}

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

IN_PROC_BROWSER_TEST_F(SystemInfoDisplayApiTest, Display) {
  // The |provider| will be owned by the singleton instance.
  scoped_refptr<MockDisplayInfoProvider> provider =
      new MockDisplayInfoProvider();
  DisplayInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunPlatformAppTest("systeminfo/display")) << message_;
}

} // namespace extensions
