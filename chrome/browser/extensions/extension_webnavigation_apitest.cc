// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_webnavigation_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "net/base/mock_host_resolver.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "webkit/glue/context_menu.h"

namespace {

class TestRenderViewContextMenu : public RenderViewContextMenu {
 public:
  TestRenderViewContextMenu(TabContents* tab_contents,
                            const ContextMenuParams& params)
      : RenderViewContextMenu(tab_contents, params) {
  }
  virtual ~TestRenderViewContextMenu() {}

 private:
  virtual void PlatformInit() {}
  virtual bool GetAcceleratorForCommandId(int, ui::Accelerator*) {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewContextMenu);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigation) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_api.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationGetFrame) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_getFrame.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationClientRedirect) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_clientRedirect.html"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationServerRedirect) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_serverRedirect.html"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationForwardBack) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_forwardBack.html"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationIFrame) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_iframe.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationOpenTab) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_openTab.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationReferenceFragment) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_referenceFragment.html"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationSimpleLoad) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_simpleLoad.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationFailures) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_failures.html")) << message_;
}

// Fails almost consistently on Mac only.  http://crbug.com/94932
#if defined(OS_MACOSX)
#define MAYBE_WebNavigationUserAction FAILS_WebNavigationUserAction
#else
#define MAYBE_WebNavigationUserAction WebNavigationUserAction
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_WebNavigationUserAction) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  FrameNavigationState::set_allow_extension_scheme(true);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_userAction.html")) << message_;

  ResultCatcher catcher;

  ExtensionService* service = browser()->profile()->GetExtensionService();
  const Extension* extension =
      service->GetExtensionById(last_loaded_extension_id_, false);
  GURL url = extension->GetResourceURL("userAction/a.html");

  ui_test_utils::NavigateToURL(browser(), url);

  // This corresponds to "Open link in new tab".
  TabContents* tab = browser()->GetSelectedTabContents();
  ContextMenuParams params;
  params.is_editable = false;
  params.media_type = WebKit::WebContextMenuData::MediaTypeNone;
  params.page_url = url;
  params.frame_id =
      ExtensionWebNavigationTabObserver::Get(tab)->
          frame_navigation_state().GetMainFrameID();
  params.link_url = extension->GetResourceURL("userAction/b.html");

  TestRenderViewContextMenu menu(tab, params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}
