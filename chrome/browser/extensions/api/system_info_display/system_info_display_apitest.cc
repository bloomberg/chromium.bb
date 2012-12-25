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

using api::experimental_system_info_display::DisplayUnitInfo;

class MockDisplayInfoProvider : public DisplayInfoProvider {
 public:
  virtual bool QueryInfo(DisplayInfo* info) OVERRIDE {
    info->clear();
    for (int i = 0; i < 2; i++) {
      linked_ptr<DisplayUnitInfo> unit(new DisplayUnitInfo());
      unit->id = "DISPLAY";
      unit->index = i;
      unit->is_primary = i == 0 ? true : false;
      unit->avail_top = 0;
      unit->avail_left = 0;
      unit->avail_width = 960;
      unit->avail_height = 720;
      unit->color_depth = 32;
      unit->pixel_depth = 32;
      unit->height = 1280;
      unit->width = 720;
      unit->absolute_top_offset = 0;
      unit->absolute_left_offset = 1280;
      unit->dpi_x = 96;
      unit->dpi_y = 96;
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
  ~SystemInfoDisplayApiTest() {}

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

IN_PROC_BROWSER_TEST_F(SystemInfoDisplayApiTest, Display) {
  // The |provider| will be owned by the singleton instance.
  scoped_refptr<MockDisplayInfoProvider> provider =
      new MockDisplayInfoProvider();
  DisplayInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunPlatformAppTest("systeminfo/display")) << message_;
}

} // namespace extensions
