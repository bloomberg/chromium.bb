// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"

using content::WebContents;

namespace {

class TestRenderViewContextMenu : public RenderViewContextMenu {
 public:
  TestRenderViewContextMenu(WebContents* web_contents,
                            content::ContextMenuParams params)
      : RenderViewContextMenu(web_contents, params) { }

  virtual void PlatformInit() { }
  virtual void PlatformCancel() { }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
    return false;
  }

  bool IsItemPresent(int command_id) {
    return menu_model_.GetIndexOfCommandId(command_id) != -1;
  }
};

}  // namespace

class RegisterProtocolHandlerBrowserTest : public InProcessBrowserTest {
 public:
  RegisterProtocolHandlerBrowserTest() { }

  TestRenderViewContextMenu* CreateContextMenu(GURL url) {
    content::ContextMenuParams params;
    params.media_type = WebKit::WebContextMenuData::MediaTypeNone;
    params.link_url = url;
    params.unfiltered_link_url = url;
    WebContents* web_contents = browser()->GetSelectedWebContents();
    params.page_url = web_contents->GetController().GetActiveEntry()->GetURL();
#if defined(OS_MACOSX)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif  // OS_MACOSX
    TestRenderViewContextMenu* menu = new TestRenderViewContextMenu(
        browser()->GetSelectedWebContents(), params);
    menu->Init();
    return menu;
  }

  void AddProtocolHandler(const std::string& protocol,
                          const GURL& url,
                          const string16& title) {
    ProtocolHandler handler = ProtocolHandler::CreateProtocolHandler(
          protocol, url, title);
    ProtocolHandlerRegistry* registry =
        browser()->profile()->GetProtocolHandlerRegistry();
    // Fake that this registration is happening on profile startup. Otherwise
    // it'll try to register with the OS, which causes DCHECKs on Windows when
    // running as admin on Windows 7.
    registry->is_loading_ = true;
    registry->OnAcceptRegisterProtocolHandler(handler);
    registry->is_loading_ = false;
    ASSERT_TRUE(registry->IsHandledProtocol(protocol));
  }

};

IN_PROC_BROWSER_TEST_F(RegisterProtocolHandlerBrowserTest,
    ContextMenuEntryAppearsForHandledUrls) {
  scoped_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenu(GURL("http://www.google.com/")));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));

  AddProtocolHandler(std::string("web+search"),
                     GURL("http://www.google.com/%s"),
                     UTF8ToUTF16(std::string("Test handler")));
  GURL url("web+search:testing");
  ASSERT_EQ(1u,
            browser()->profile()->GetProtocolHandlerRegistry()->GetHandlersFor(
                url.scheme()).size());
  menu.reset(CreateContextMenu(url));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));
}

IN_PROC_BROWSER_TEST_F(RegisterProtocolHandlerBrowserTest, CustomHandler) {
  ASSERT_TRUE(test_server()->Start());
  GURL handler_url = test_server()->GetURL("files/custom_handler_foo.html");
  AddProtocolHandler("foo", handler_url,
                     UTF8ToUTF16(std::string("Test foo Handler")));

  ui_test_utils::NavigateToURL(browser(), GURL("foo:test"));

  ASSERT_EQ(handler_url, browser()->GetSelectedWebContents()->GetURL());
}
