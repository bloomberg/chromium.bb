// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "net/base/mock_host_resolver.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

using content::WebContents;

#if defined(OS_WIN)
// http://crbug.com/118478
#define MAYBE_WebNavigationIFrame DISABLED_WebNavigationIFrame
#define MAYBE_WebNavigationFailures DISABLED_WebNavigationFailures
#define MAYBE_WebNavigationForwardBack DISABLED_WebNavigationForwardBack
#define MAYBE_WebNavigationClientRedirect DISABLED_WebNavigationClientRedirect
#define MAYBE_WebNavigationGetFrame DISABLED_WebNavigationGetFrame
#define MAYBE_WebNavigationSimpleLoad DISABLED_WebNavigationSimpleLoad
#define MAYBE_WebNavigationReferenceFragment \
    DISABLED_WebNavigationReferenceFragment
#define MAYBE_WebNavigationOpenTab DISABLED_WebNavigationOpenTab
#else
#define MAYBE_WebNavigationIFrame WebNavigationIFrame
#define MAYBE_WebNavigationFailures WebNavigationFailures
#define MAYBE_WebNavigationForwardBack WebNavigationForwardBack
#define MAYBE_WebNavigationClientRedirect WebNavigationClientRedirect
#define MAYBE_WebNavigationGetFrame WebNavigationGetFrame
#define MAYBE_WebNavigationSimpleLoad WebNavigationSimpleLoad
#define MAYBE_WebNavigationReferenceFragment WebNavigationReferenceFragment
#define MAYBE_WebNavigationOpenTab WebNavigationOpenTab
#endif

namespace extensions {

namespace {

class TestRenderViewContextMenu : public RenderViewContextMenu {
 public:
  TestRenderViewContextMenu(WebContents* web_contents,
                            const content::ContextMenuParams& params)
      : RenderViewContextMenu(web_contents, params) {
  }
  virtual ~TestRenderViewContextMenu() {}

 private:
  virtual void PlatformInit() {}
  virtual void PlatformCancel() {}
  virtual bool GetAcceleratorForCommandId(int, ui::Accelerator*) {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewContextMenu);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigation) {
  FrameNavigationState::set_allow_extension_scheme(true);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_api.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_WebNavigationGetFrame) {
  FrameNavigationState::set_allow_extension_scheme(true);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_getFrame.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_WebNavigationClientRedirect) {
  FrameNavigationState::set_allow_extension_scheme(true);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_clientRedirect.html"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationServerRedirect) {
  FrameNavigationState::set_allow_extension_scheme(true);
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_serverRedirect.html"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_WebNavigationForwardBack) {
  FrameNavigationState::set_allow_extension_scheme(true);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_forwardBack.html"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_WebNavigationIFrame) {
  FrameNavigationState::set_allow_extension_scheme(true);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_iframe.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_WebNavigationOpenTab) {
  FrameNavigationState::set_allow_extension_scheme(true);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_openTab.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_WebNavigationReferenceFragment) {
  FrameNavigationState::set_allow_extension_scheme(true);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_referenceFragment.html"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_WebNavigationSimpleLoad) {
  FrameNavigationState::set_allow_extension_scheme(true);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_simpleLoad.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_WebNavigationFailures) {
  FrameNavigationState::set_allow_extension_scheme(true);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_failures.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationUserAction) {
  FrameNavigationState::set_allow_extension_scheme(true);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_userAction.html")) << message_;

  WebContents* tab = browser()->GetSelectedWebContents();
  ui_test_utils::WaitForLoadStop(tab);

  ResultCatcher catcher;

  ExtensionService* service = browser()->profile()->GetExtensionService();
  const Extension* extension =
      service->GetExtensionById(last_loaded_extension_id_, false);
  GURL url = extension->GetResourceURL("userAction/a.html");

  ui_test_utils::NavigateToURL(browser(), url);

  // This corresponds to "Open link in new tab".
  content::ContextMenuParams params;
  params.is_editable = false;
  params.media_type = WebKit::WebContextMenuData::MediaTypeNone;
  params.page_url = url;
  params.frame_id = WebNavigationTabObserver::Get(tab)->
      frame_navigation_state().GetMainFrameID();
  params.link_url = extension->GetResourceURL("userAction/b.html");

  TestRenderViewContextMenu menu(tab, params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationRequestOpenTab) {
  FrameNavigationState::set_allow_extension_scheme(true);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionSubtest("webnavigation", "test_requestOpenTab.html"))
      << message_;

  WebContents* tab = browser()->GetSelectedWebContents();
  ui_test_utils::WaitForLoadStop(tab);

  ResultCatcher catcher;

  ExtensionService* service = browser()->profile()->GetExtensionService();
  const Extension* extension =
      service->GetExtensionById(last_loaded_extension_id_, false);
  GURL url = extension->GetResourceURL("requestOpenTab/a.html");

  ui_test_utils::NavigateToURL(browser(), url);

  // There's a link on a.html. Middle-click on it to open it in a new tab.
  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonMiddle;
  mouse_event.x = 7;
  mouse_event.y = 7;
  mouse_event.clickCount = 1;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationTargetBlank) {
  FrameNavigationState::set_allow_extension_scheme(true);
  ASSERT_TRUE(StartTestServer());

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionSubtest("webnavigation", "test_targetBlank.html"))
      << message_;

  WebContents* tab = browser()->GetSelectedWebContents();
  ui_test_utils::WaitForLoadStop(tab);

  ResultCatcher catcher;

  GURL url = test_server()->GetURL(
      "files/extensions/api_test/webnavigation/targetBlank/a.html");

  browser::NavigateParams params(browser(), url, content::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // There's a link with target=_blank on a.html. Click on it to open it in a
  // new tab.
  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  mouse_event.x = 7;
  mouse_event.y = 7;
  mouse_event.clickCount = 1;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationTargetBlankIncognito) {
  FrameNavigationState::set_allow_extension_scheme(true);
  ASSERT_TRUE(StartTestServer());

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLegacyExtensionManifests);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionSubtest(
      "webnavigation", "test_targetBlank.html",
      ExtensionApiTest::kFlagEnableIncognito)) << message_;

  ResultCatcher catcher;

  GURL url = test_server()->GetURL(
      "files/extensions/api_test/webnavigation/targetBlank/a.html");

  ui_test_utils::OpenURLOffTheRecord(browser()->profile(), url);
  WebContents* tab = browser::FindTabbedBrowser(
      browser()->profile()->GetOffTheRecordProfile(), false)->
          GetSelectedWebContents();

  // There's a link with target=_blank on a.html. Click on it to open it in a
  // new tab.
  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  mouse_event.x = 7;
  mouse_event.y = 7;
  mouse_event.clickCount = 1;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
