// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"

namespace {

class TestRenderViewContextMenu : public RenderViewContextMenu {
 public:
  TestRenderViewContextMenu(TabContents* tab_contents, ContextMenuParams params)
      : RenderViewContextMenu(tab_contents, params) { }

  virtual void PlatformInit() { }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
    return false;
  }

  bool IsItemPresent(int command_id) {
    return menu_model_.GetIndexOfCommandId(command_id) != -1;
  }
};

class RegisterProtocolHandlerBrowserTest : public InProcessBrowserTest {
 public:
  RegisterProtocolHandlerBrowserTest() { }

  TestRenderViewContextMenu* CreateContextMenu(GURL url) {
    ContextMenuParams params;
    params.media_type = WebKit::WebContextMenuData::MediaTypeNone;
    params.link_url = url;
    TabContents* tab_contents = browser()->GetSelectedTabContents();
    params.page_url = tab_contents->controller().GetActiveEntry()->url();
#if defined(OS_MACOSX)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif  // OS_MACOSX
    TestRenderViewContextMenu* menu = new TestRenderViewContextMenu(
        browser()->GetSelectedTabContents(), params);
    menu->Init();
    return menu;
  }
};

IN_PROC_BROWSER_TEST_F(RegisterProtocolHandlerBrowserTest,
    ContextMenuEntryAppearsForHandledUrls) {
  scoped_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenu(GURL("http://www.google.com/")));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));

  ProtocolHandler handler = ProtocolHandler::CreateProtocolHandler(
        std::string("web+search"),
        GURL("http://www.google.com/%s"),
        UTF8ToUTF16(std::string("Test handler")));
  ProtocolHandlerRegistry* registry =
      browser()->profile()->GetProtocolHandlerRegistry();

  GURL url("web+search:testing");
  registry->OnAcceptRegisterProtocolHandler(handler);
  ASSERT_TRUE(registry->IsHandledProtocol("web+search"));
  ASSERT_EQ(registry->GetHandlersFor(url.scheme()).size(), (size_t) 1);
  menu.reset(CreateContextMenu(url));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));
}

}  // namespace
