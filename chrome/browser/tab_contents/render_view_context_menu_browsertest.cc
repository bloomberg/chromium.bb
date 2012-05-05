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
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

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

// Wait for a context menu to be shown, and then execute a command with the
// given command id in this menu, and close the menu again.
class ContextMenuNotificationObserver : public content::NotificationObserver {
 public:
  explicit ContextMenuNotificationObserver(int command_to_execute)
      : command_to_execute_(command_to_execute) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_CONTEXT_MENU_SHOWN,
                   content::NotificationService::AllSources());
  }

  virtual ~ContextMenuNotificationObserver() {}

 private:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(chrome::NOTIFICATION_CONTEXT_MENU_SHOWN, type);
    RenderViewContextMenu* context_menu =
        content::Source<RenderViewContextMenu>(source).ptr();
    context_menu->ExecuteCommand(command_to_execute_);
    context_menu->Cancel();
  }

  content::NotificationRegistrar registrar_;
  int command_to_execute_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuNotificationObserver);
};

class ContextMenuBrowserTest : public InProcessBrowserTest {
 public:
  ContextMenuBrowserTest() { }

  TestRenderViewContextMenu* CreateContextMenu(GURL unfiltered_url, GURL url) {
    content::ContextMenuParams params;
    params.media_type = WebKit::WebContextMenuData::MediaTypeNone;
    params.unfiltered_link_url = unfiltered_url;
    params.link_url = url;
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
};

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       OpenEntryPresentForNormalURLs) {
  scoped_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenu(GURL("http://www.google.com/"),
                        GURL("http://www.google.com/")));

  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
}

IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       OpenEntryAbsentForFilteredURLs) {
  scoped_ptr<TestRenderViewContextMenu> menu(
      CreateContextMenu(GURL("chrome://history"),
                        GURL()));

  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));
  ASSERT_FALSE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));
  ASSERT_TRUE(menu->IsItemPresent(IDC_CONTENT_CONTEXT_COPYLINKLOCATION));
}

// GTK requires a X11-level mouse event to open a context menu correctly.
#if defined(TOOLKIT_GTK)
#define MAYBE_RealMenu DISABLED_RealMenu
#elif defined(ADDRESS_SANITIZER)
// See http://crbug.com/126360.
#define MAYBE_RealMenu DISABLED_RealMenu
#else
#define MAYBE_RealMenu RealMenu
#endif
// Opens a link in a new tab via a "real" context menu.
IN_PROC_BROWSER_TEST_F(ContextMenuBrowserTest,
                       MAYBE_RealMenu) {
  ContextMenuNotificationObserver menu_observer(
      IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);
  ui_test_utils::WindowedTabAddedNotificationObserver tab_observer(
      content::NotificationService::AllSources());

  // Go to a page with a link
  ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<a href='about:blank'>link</a>"));

  // Open a context menu.
  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonRight;
  mouse_event.x = 15;
  mouse_event.y = 15;
  gfx::Rect offset;
  content::WebContents* tab = browser()->GetSelectedWebContents();
  tab->GetView()->GetContainerBounds(&offset);
  mouse_event.globalX = 15 + offset.x();
  mouse_event.globalY = 15 + offset.y();
  mouse_event.clickCount = 1;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);

  // The menu_observer will select "Open in new tab", wait for the new tab to
  // be added.
  tab_observer.Wait();
  tab = tab_observer.GetTab();
  ui_test_utils::WaitForLoadStop(tab);

  // Verify that it's the correct tab.
  EXPECT_EQ(GURL("about:blank"), tab->GetURL());
}

}  // namespace
