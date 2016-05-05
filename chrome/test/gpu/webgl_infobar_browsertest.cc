// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/gl/gl_implementation.h"

namespace {

void SimulateGPUCrash(Browser* browser) {
  // None of the ui_test_utils entry points supports what we need to
  // do here: navigate with the PAGE_TRANSITION_FROM_ADDRESS_BAR flag,
  // without waiting for the navigation. It would be painful to change
  // either of the NavigateToURL entry points to support these two
  // constraints, so we use chrome::Navigate directly.
  chrome::NavigateParams params(
      browser,
      GURL(content::kChromeUIGpuCrashURL),
      ui::PageTransitionFromInt(
          ui::PAGE_TRANSITION_TYPED |
          ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  params.disposition = NEW_BACKGROUND_TAB;
  chrome::Navigate(&params);
}

}  // namespace

class WebGLInfoBarTest : public InProcessBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    base::FilePath test_dir;
    ASSERT_TRUE(PathService::Get(content::DIR_TEST_DATA, &test_dir));
    gpu_test_dir_ = test_dir.AppendASCII("gpu");
  }
  base::FilePath gpu_test_dir_;
};

// This test is flaky. http://crbug.com/324555
IN_PROC_BROWSER_TEST_F(WebGLInfoBarTest, DISABLED_ContextLossRaisesInfoBar) {
#undef MAYBE_ContextLossRaisesInfoBard
  if (gpu::GPUTestBotConfig::CurrentConfigMatches("XP"))
    return;

  // Load page and wait for it to load.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(
      browser(),
      content::GetFileUrlWithQuery(
          gpu_test_dir_.AppendASCII("webgl.html"), "query=kill"));
  observer.Wait();

  content::WindowedNotificationObserver infobar_added(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
        content::NotificationService::AllSources());
  SimulateGPUCrash(browser());
  infobar_added.Wait();
  EXPECT_EQ(1u,
            InfoBarService::FromWebContents(
                browser()->tab_strip_model()->GetActiveWebContents())->
                    infobar_count());
}

// This test is flaky. http://crbug.com/324555
IN_PROC_BROWSER_TEST_F(WebGLInfoBarTest, DISABLED_ContextLossInfoBarReload) {
  if (gpu::GPUTestBotConfig::CurrentConfigMatches("XP"))
    return;

  content::DOMMessageQueue message_queue;

  // Load page and wait for it to load.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(
      browser(),
      content::GetFileUrlWithQuery(
          gpu_test_dir_.AppendASCII("webgl.html"),
          "query=kill_after_notification"));
  observer.Wait();

  std::string m;
  ASSERT_TRUE(message_queue.WaitForMessage(&m));
  EXPECT_EQ("\"LOADED\"", m);

  message_queue.ClearQueue();

  content::WindowedNotificationObserver infobar_added(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
        content::NotificationService::AllSources());
  SimulateGPUCrash(browser());
  infobar_added.Wait();
  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_EQ(1u, infobar_service->infobar_count());
  infobars::InfoBarDelegate* delegate =
      infobar_service->infobar_at(0)->delegate();
  ASSERT_TRUE(delegate->AsThreeDAPIInfoBarDelegate());
  delegate->AsConfirmInfoBarDelegate()->Cancel();

  // The page should reload and another message sent to the
  // DomAutomationController.
  m.clear();
  ASSERT_TRUE(message_queue.WaitForMessage(&m));
  EXPECT_EQ("\"LOADED\"", m);
}

// There isn't any point in adding a test which calls Accept() on the
// ThreeDAPIInfoBarDelegate; doing so doesn't remove the infobar, and
// there's no concrete event that could be observed in response.
