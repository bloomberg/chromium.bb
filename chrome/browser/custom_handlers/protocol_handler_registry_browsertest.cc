// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/web/web_context_menu_data.h"

using content::WebContents;

class RegisterProtocolHandlerBrowserTest : public InProcessBrowserTest {
 public:
  RegisterProtocolHandlerBrowserTest() { }

  TestRenderViewContextMenu* CreateContextMenu(GURL url) {
    content::ContextMenuParams params;
    params.media_type = blink::WebContextMenuData::kMediaTypeNone;
    params.link_url = url;
    params.unfiltered_link_url = url;
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    params.page_url = web_contents->GetController().GetActiveEntry()->GetURL();
#if defined(OS_MACOSX)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif  // OS_MACOSX
    TestRenderViewContextMenu* menu = new TestRenderViewContextMenu(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
        params);
    menu->Init();
    return menu;
  }

  void AddProtocolHandler(const std::string& protocol,
                          const GURL& url) {
    ProtocolHandler handler = ProtocolHandler::CreateProtocolHandler(protocol,
                                                                     url);
    ProtocolHandlerRegistry* registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(
            browser()->profile());
    // Fake that this registration is happening on profile startup. Otherwise
    // it'll try to register with the OS, which causes DCHECKs on Windows when
    // running as admin on Windows 7.
    registry->is_loading_ = true;
    registry->OnAcceptRegisterProtocolHandler(handler);
    registry->is_loading_ = false;
    ASSERT_TRUE(registry->IsHandledProtocol(protocol));
  }
  void RemoveProtocolHandler(const std::string& protocol,
                             const GURL& url) {
    ProtocolHandler handler = ProtocolHandler::CreateProtocolHandler(protocol,
                                                                     url);
    ProtocolHandlerRegistry* registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(
            browser()->profile());
    registry->RemoveHandler(handler);
    ASSERT_FALSE(registry->IsHandledProtocol(protocol));
  }
};

IN_PROC_BROWSER_TEST_F(RegisterProtocolHandlerBrowserTest,
    ContextMenuEntryAppearsForHandledUrls) {
  std::unique_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenu(GURL("http://www.google.com/")));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));

  AddProtocolHandler(std::string("web+search"),
                     GURL("http://www.google.com/%s"));
  GURL url("web+search:testing");
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_EQ(1u, registry->GetHandlersFor(url.scheme()).size());
  menu.reset(CreateContextMenu(url));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));
}

IN_PROC_BROWSER_TEST_F(RegisterProtocolHandlerBrowserTest,
    UnregisterProtocolHandler) {
  std::unique_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenu(GURL("http://www.google.com/")));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));

  AddProtocolHandler(std::string("web+search"),
                     GURL("http://www.google.com/%s"));
  GURL url("web+search:testing");
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_EQ(1u, registry->GetHandlersFor(url.scheme()).size());
  menu.reset(CreateContextMenu(url));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));
  RemoveProtocolHandler(std::string("web+search"),
                        GURL("http://www.google.com/%s"));
  ASSERT_EQ(0u, registry->GetHandlersFor(url.scheme()).size());
  menu.reset(CreateContextMenu(url));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKWITH));
}

IN_PROC_BROWSER_TEST_F(RegisterProtocolHandlerBrowserTest, CustomHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL handler_url = embedded_test_server()->GetURL("/custom_handler_foo.html");
  AddProtocolHandler("foo", handler_url);

  ui_test_utils::NavigateToURL(browser(), GURL("foo:test"));

  ASSERT_EQ(handler_url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}
