// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "net/test/test_server.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"

namespace {

const char kSimplePage[] = "files/sidebar/simple_page.html";

class SidebarTest : public ExtensionBrowserTest {
 public:
  SidebarTest() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalExtensionApis);
  }

 protected:
  // InProcessBrowserTest overrides.
  virtual void SetUpOnMainThread() {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Load test sidebar extension.
    FilePath extension_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extension_path));
    extension_path = extension_path.AppendASCII("sidebar");

    ASSERT_TRUE(LoadExtension(extension_path));

    // For now content_id == extension_id.
    content_id_ = last_loaded_extension_id_;
  }

  void ShowSidebarForCurrentTab() {
    ShowSidebar(browser()->GetSelectedTabContents());
  }

  void ExpandSidebarForCurrentTab() {
    ExpandSidebar(browser()->GetSelectedTabContents());
  }

  void CollapseSidebarForCurrentTab() {
    CollapseSidebar(browser()->GetSelectedTabContents());
  }

  void HideSidebarForCurrentTab() {
    HideSidebar(browser()->GetSelectedTabContents());
  }

  void NavigateSidebarForCurrentTabTo(const std::string& test_page) {
    GURL url = test_server()->GetURL(test_page);

    TabContents* tab = browser()->GetSelectedTabContents();

    SidebarManager* sidebar_manager = SidebarManager::GetInstance();

    sidebar_manager->NavigateSidebar(tab, content_id_, url);

    SidebarContainer* sidebar_container =
        sidebar_manager->GetSidebarContainerFor(tab, content_id_);

    TabContents* client_contents = sidebar_container->sidebar_contents();
    ui_test_utils::WaitForNavigation(&client_contents->controller());
  }

  void ShowSidebar(TabContents* tab) {
    SidebarManager* sidebar_manager = SidebarManager::GetInstance();
    sidebar_manager->ShowSidebar(tab, content_id_);
  }

  void ExpandSidebar(TabContents* tab) {
    SidebarManager* sidebar_manager = SidebarManager::GetInstance();
    sidebar_manager->ExpandSidebar(tab, content_id_);
    if (browser()->GetSelectedTabContents() == tab)
      EXPECT_GT(browser_view()->GetSidebarWidth(), 0);
  }

  void CollapseSidebar(TabContents* tab) {
    SidebarManager* sidebar_manager = SidebarManager::GetInstance();
    sidebar_manager->CollapseSidebar(tab, content_id_);
    if (browser()->GetSelectedTabContents() == tab)
      EXPECT_EQ(0, browser_view()->GetSidebarWidth());
  }

  void HideSidebar(TabContents* tab) {
    SidebarManager* sidebar_manager = SidebarManager::GetInstance();
    sidebar_manager->HideSidebar(tab, content_id_);
    if (browser()->GetSelectedTabContents() == tab)
      EXPECT_EQ(0, browser_view()->GetSidebarWidth());
  }

  TabContents* tab_contents(int i) {
    return browser()->GetTabContentsAt(i);
  }

  BrowserView* browser_view() const {
    return static_cast<BrowserView*>(browser()->window());
  }

 private:
  std::string content_id_;
};

IN_PROC_BROWSER_TEST_F(SidebarTest, OpenClose) {
  ShowSidebarForCurrentTab();

  ExpandSidebarForCurrentTab();
  CollapseSidebarForCurrentTab();

  ExpandSidebarForCurrentTab();
  CollapseSidebarForCurrentTab();

  ExpandSidebarForCurrentTab();
  CollapseSidebarForCurrentTab();

  HideSidebarForCurrentTab();

  ShowSidebarForCurrentTab();

  ExpandSidebarForCurrentTab();
  CollapseSidebarForCurrentTab();

  HideSidebarForCurrentTab();
}

IN_PROC_BROWSER_TEST_F(SidebarTest, SwitchingTabs) {
  ShowSidebarForCurrentTab();
  ExpandSidebarForCurrentTab();

  browser()->NewTab();

  // Make sure sidebar is not visbile for the newly opened tab.
  EXPECT_EQ(0, browser_view()->GetSidebarWidth());

  // Switch back to the first tab.
  browser()->SelectNumberedTab(0);

  // Make sure it is visible now.
  EXPECT_GT(browser_view()->GetSidebarWidth(), 0);

  HideSidebarForCurrentTab();
}

IN_PROC_BROWSER_TEST_F(SidebarTest, SidebarOnInactiveTab) {
  ShowSidebarForCurrentTab();
  ExpandSidebarForCurrentTab();

  browser()->NewTab();

  // Hide sidebar on inactive (first) tab.
  HideSidebar(tab_contents(0));

  // Switch back to the first tab.
  browser()->SelectNumberedTab(0);

  // Make sure sidebar is not visbile anymore.
  EXPECT_EQ(0, browser_view()->GetSidebarWidth());

  // Show sidebar on inactive (second) tab.
  ShowSidebar(tab_contents(1));
  ExpandSidebar(tab_contents(1));
  // Make sure sidebar is not visible yet.
  EXPECT_EQ(0, browser_view()->GetSidebarWidth());

  // Switch back to the second tab.
  browser()->SelectNumberedTab(1);
  // Make sure sidebar is visible now.
  EXPECT_GT(browser_view()->GetSidebarWidth(), 0);

  HideSidebarForCurrentTab();
}

IN_PROC_BROWSER_TEST_F(SidebarTest, SidebarNavigate) {
  ASSERT_TRUE(test_server()->Start());

  ShowSidebarForCurrentTab();

  NavigateSidebarForCurrentTabTo(kSimplePage);

  HideSidebarForCurrentTab();
}

}  // namespace

