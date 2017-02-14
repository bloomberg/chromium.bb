// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/memory/tab_manager.h"
#include "chrome/browser/memory/tab_manager_web_contents_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

using content::OpenURLParams;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

namespace memory {

class TabManagerTest : public InProcessBrowserTest {
};

bool ObserveNavEntryCommitted(const GURL& expected_url,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  return content::Details<content::LoadCommittedDetails>(details)
             ->entry->GetURL() == expected_url;
}

IN_PROC_BROWSER_TEST_F(TabManagerTest, TabManagerBasics) {
  using content::WindowedNotificationObserver;
  TabManager* tab_manager = g_browser_process->GetTabManager();
  EXPECT_FALSE(tab_manager->recent_tab_discard());

  // Disable the protection of recent tabs.
  tab_manager->set_minimum_protection_time_for_tests(
      base::TimeDelta::FromMinutes(0));

  // Get three tabs open.
  WindowedNotificationObserver load1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open1(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open1);
  load1.Wait();

  WindowedNotificationObserver load2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open2(GURL(chrome::kChromeUICreditsURL), content::Referrer(),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open2);
  load2.Wait();

  WindowedNotificationObserver load3(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open3(GURL(chrome::kChromeUITermsURL), content::Referrer(),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open3);
  load3.Wait();

  auto* tsm = browser()->tab_strip_model();
  EXPECT_EQ(3, tsm->count());

  // Navigate the current (third) tab to a different URL, so we can test
  // back/forward later.
  WindowedNotificationObserver load4(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open4(GURL(chrome::kChromeUIVersionURL), content::Referrer(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open4);
  load4.Wait();

  // Navigate the third tab again, such that we have three navigation entries.
  WindowedNotificationObserver load5(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open5(GURL("chrome://dns"), content::Referrer(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open5);
  load5.Wait();

  EXPECT_EQ(3, tsm->count());

  // Discard a tab.  It should kill the first tab, since it was the oldest
  // and was not selected.
  EXPECT_TRUE(tab_manager->DiscardTabImpl());
  EXPECT_EQ(3, tsm->count());
  EXPECT_TRUE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(0)));
  EXPECT_FALSE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(1)));
  EXPECT_FALSE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(2)));
  EXPECT_TRUE(tab_manager->recent_tab_discard());

  // Run discard again, make sure it kills the second tab.
  EXPECT_TRUE(tab_manager->DiscardTabImpl());
  EXPECT_EQ(3, tsm->count());
  EXPECT_TRUE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(0)));
  EXPECT_TRUE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(1)));
  EXPECT_FALSE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(2)));

  // Kill the third tab. It should not kill the last tab, since it is active
  // tab.
  EXPECT_FALSE(tab_manager->DiscardTabImpl());
  EXPECT_TRUE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(0)));
  EXPECT_TRUE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(1)));
  EXPECT_FALSE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(2)));

  // Kill the third tab after making second tab active.
  tsm->ActivateTabAt(1, true);
  EXPECT_EQ(1, tsm->active_index());
  EXPECT_FALSE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(1)));
  tab_manager->DiscardWebContentsAt(2, tsm);
  EXPECT_TRUE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(2)));

  // Force creation of the FindBarController.
  browser()->GetFindBarController();

  // Select the first tab.  It should reload.
  WindowedNotificationObserver reload1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      base::Bind(&ObserveNavEntryCommitted,
                 GURL(chrome::kChromeUIChromeURLsURL)));
  chrome::SelectNumberedTab(browser(), 0);
  reload1.Wait();
  // Make sure the FindBarController gets the right WebContents.
  EXPECT_EQ(browser()->GetFindBarController()->web_contents(),
            tsm->GetActiveWebContents());
  EXPECT_EQ(0, tsm->active_index());
  EXPECT_FALSE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(0)));
  EXPECT_FALSE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(1)));
  EXPECT_TRUE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(2)));

  // Select the third tab. It should reload.
  WindowedNotificationObserver reload2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      base::Bind(&ObserveNavEntryCommitted, GURL("chrome://dns")));
  chrome::SelectNumberedTab(browser(), 2);
  reload2.Wait();
  EXPECT_EQ(2, tsm->active_index());
  EXPECT_FALSE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(0)));
  EXPECT_FALSE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(1)));
  EXPECT_FALSE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(2)));

  // Navigate the third tab back twice.  We used to crash here due to
  // crbug.com/121373.
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  EXPECT_FALSE(chrome::CanGoForward(browser()));
  WindowedNotificationObserver back1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      base::Bind(&ObserveNavEntryCommitted, GURL(chrome::kChromeUIVersionURL)));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back1.Wait();
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  EXPECT_TRUE(chrome::CanGoForward(browser()));
  WindowedNotificationObserver back2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      base::Bind(&ObserveNavEntryCommitted, GURL(chrome::kChromeUITermsURL)));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back2.Wait();
  EXPECT_FALSE(chrome::CanGoBack(browser()));
  EXPECT_TRUE(chrome::CanGoForward(browser()));
}

// On Linux, memory pressure listener is not implemented yet.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

// Test that the MemoryPressureListener event is properly triggering a tab
// discard upon |MEMORY_PRESSURE_LEVEL_CRITICAL| event.
IN_PROC_BROWSER_TEST_F(TabManagerTest, OomPressureListener) {
  TabManager* tab_manager = g_browser_process->GetTabManager();

  // Disable the protection of recent tabs.
  tab_manager->set_minimum_protection_time_for_tests(
      base::TimeDelta::FromMinutes(0));

  // Get three tabs open.
  content::WindowedNotificationObserver load1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open1(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open1);
  load1.Wait();

  content::WindowedNotificationObserver load2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open2(GURL(chrome::kChromeUICreditsURL), content::Referrer(),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open2);
  load2.Wait();
  EXPECT_FALSE(tab_manager->recent_tab_discard());

  // Nothing should happen with a moderate memory pressure event.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_FALSE(tab_manager->recent_tab_discard());

  // A critical memory pressure event should discard a tab.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  // Coming here, an asynchronous operation will collect system stats. Once in,
  // a tab should get discarded. As such we need to give it 10s time to discard.
  const int kTimeoutTimeInMS = 10000;
  const int kIntervalTimeInMS = 5;
  int timeout = kTimeoutTimeInMS / kIntervalTimeInMS;
  while (--timeout) {
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(kIntervalTimeInMS));
    base::RunLoop().RunUntilIdle();
    if (tab_manager->recent_tab_discard())
      break;
  }
  EXPECT_TRUE(tab_manager->recent_tab_discard());
}

#endif

IN_PROC_BROWSER_TEST_F(TabManagerTest, InvalidOrEmptyURL) {
  TabManager* tab_manager = g_browser_process->GetTabManager();

  // Disable the protection of recent tabs.
  tab_manager->set_minimum_protection_time_for_tests(
      base::TimeDelta::FromMinutes(0));

  // Open two tabs. Wait for the foreground one to load but do not wait for the
  // background one.
  content::WindowedNotificationObserver load1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open1(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open1);
  load1.Wait();

  content::WindowedNotificationObserver load2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open2(GURL(chrome::kChromeUICreditsURL), content::Referrer(),
                      WindowOpenDisposition::NEW_BACKGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open2);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // This shouldn't be able to discard a tab as the background tab has not yet
  // started loading (its URL is not committed).
  EXPECT_FALSE(tab_manager->DiscardTabImpl());

  // Wait for the background tab to load which then allows it to be discarded.
  load2.Wait();
  EXPECT_TRUE(tab_manager->DiscardTabImpl());
}

// Makes sure that PDF pages are protected.
IN_PROC_BROWSER_TEST_F(TabManagerTest, ProtectPDFPages) {
  TabManager* tab_manager = g_browser_process->GetTabManager();

  // Start the embedded test server so we can get served the required PDF page.
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  embedded_test_server()->StartAcceptingConnections();

  // Get two tabs open, the first one being a PDF page and the second one being
  // the foreground tab.
  GURL url1 = embedded_test_server()->GetURL("/pdf/test.pdf");
  ui_test_utils::NavigateToURL(browser(), url1);

  GURL url2(chrome::kChromeUIAboutURL);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // No discarding should be possible as the only background tab is displaying a
  // PDF page, hence protected.
  EXPECT_FALSE(tab_manager->DiscardTabImpl());
}

// Makes sure that recently opened or used tabs are protected, depending on the
// value of of |minimum_protection_time_|.
// TODO(georgesak): Move this to a unit test instead (requires change to API).
IN_PROC_BROWSER_TEST_F(TabManagerTest, ProtectRecentlyUsedTabs) {
  // TODO(georgesak): Retrieve this value from tab_manager.h once it becomes a
  // constant (as of now, it gets set through variations).
  const int kProtectionTime = 5;
  TabManager* tab_manager = g_browser_process->GetTabManager();

  base::SimpleTestTickClock test_clock_;
  tab_manager->set_test_tick_clock(&test_clock_);

  auto* tsm = browser()->tab_strip_model();

  // Set the minimum time of protection.
  tab_manager->set_minimum_protection_time_for_tests(
      base::TimeDelta::FromMinutes(kProtectionTime));

  // Open 2 tabs, the second one being in the background.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIAboutURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(2, tsm->count());

  // Advance the clock for less than the protection time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(kProtectionTime / 2));

  // Should not be able to discard a tab.
  ASSERT_FALSE(tab_manager->DiscardTabImpl());

  // Advance the clock for more than the protection time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(kProtectionTime / 2 + 2));

  // Should be able to discard the background tab now.
  EXPECT_TRUE(tab_manager->DiscardTabImpl());

  // Activate the 2nd tab.
  tsm->ActivateTabAt(1, true);
  EXPECT_EQ(1, tsm->active_index());

  // Advance the clock for less than the protection time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(kProtectionTime / 2));

  // Should not be able to discard a tab.
  ASSERT_FALSE(tab_manager->DiscardTabImpl());

  // Advance the clock for more than the protection time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(kProtectionTime / 2 + 2));

  // Should be able to discard the background tab now.
  EXPECT_TRUE(tab_manager->DiscardTabImpl());

  // This is necessary otherwise the test crashes in
  // WebContentsData::WebContentsDestroyed.
  tsm->CloseAllTabs();
}

// Makes sure that tabs using media devices are protected.
IN_PROC_BROWSER_TEST_F(TabManagerTest, ProtectVideoTabs) {
  TabManager* tab_manager = g_browser_process->GetTabManager();

  // Disable the protection of recent tabs.
  tab_manager->set_minimum_protection_time_for_tests(
      base::TimeDelta::FromMinutes(0));

  // Open 2 tabs, the second one being in the background.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIAboutURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  auto* tab = browser()->tab_strip_model()->GetWebContentsAt(1);

  // Simulate that a video stream is now being captured.
  content::MediaStreamDevice fake_media_device(
      content::MEDIA_DEVICE_VIDEO_CAPTURE, "fake_media_device",
      "fake_media_device");
  content::MediaStreamDevices video_devices(1, fake_media_device);
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  dispatcher->SetTestVideoCaptureDevices(video_devices);
  std::unique_ptr<content::MediaStreamUI> video_stream_ui =
      dispatcher->GetMediaStreamCaptureIndicator()->RegisterMediaStream(
          tab, video_devices);
  video_stream_ui->OnStarted(base::Closure());

  // Should not be able to discard a tab.
  ASSERT_FALSE(tab_manager->DiscardTabImpl());

  // Remove the video stream.
  video_stream_ui.reset();

  // Should be able to discard the background tab now.
  EXPECT_TRUE(tab_manager->DiscardTabImpl());
}

IN_PROC_BROWSER_TEST_F(TabManagerTest, CanSuspendBackgroundedRenderer) {
  TabManager* tab_manager = g_browser_process->GetTabManager();

  // Open 2 tabs, the second one being in the background.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIAboutURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  auto* tab = browser()->tab_strip_model()->GetWebContentsAt(1);
  // Simulate that a video stream is now being captured.
  content::MediaStreamDevice fake_media_device(
      content::MEDIA_DEVICE_VIDEO_CAPTURE, "fake_media_device",
      "fake_media_device");
  content::MediaStreamDevices video_devices(1, fake_media_device);
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  dispatcher->SetTestVideoCaptureDevices(video_devices);
  std::unique_ptr<content::MediaStreamUI> video_stream_ui =
      dispatcher->GetMediaStreamCaptureIndicator()->RegisterMediaStream(
          tab, video_devices);
  video_stream_ui->OnStarted(base::Closure());

  // Should not be able to suspend a tab which plays a video.
  int render_process_id = tab->GetRenderProcessHost()->GetID();
  ASSERT_FALSE(tab_manager->CanSuspendBackgroundedRenderer(render_process_id));

  // Remove the video stream.
  video_stream_ui.reset();

  // Should be able to suspend the background tab now.
  EXPECT_TRUE(tab_manager->CanSuspendBackgroundedRenderer(render_process_id));
}

IN_PROC_BROWSER_TEST_F(TabManagerTest, AutoDiscardable) {
  using content::WindowedNotificationObserver;
  TabManager* tab_manager = g_browser_process->GetTabManager();

  // Disable the protection of recent tabs.
  tab_manager->set_minimum_protection_time_for_tests(
      base::TimeDelta::FromMinutes(0));

  // Get two tabs open.
  WindowedNotificationObserver load1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open1(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                      WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open1);
  load1.Wait();

  WindowedNotificationObserver load2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open2(GURL(chrome::kChromeUICreditsURL), content::Referrer(),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open2);
  load2.Wait();

  // Set the auto-discardable state of the first tab to false.
  auto* tsm = browser()->tab_strip_model();
  ASSERT_EQ(2, tsm->count());
  tab_manager->SetTabAutoDiscardableState(tsm->GetWebContentsAt(0), false);

  // Shouldn't discard the tab, since auto-discardable is deactivated.
  EXPECT_FALSE(tab_manager->DiscardTabImpl());

  // Reset auto-discardable state to true.
  tab_manager->SetTabAutoDiscardableState(tsm->GetWebContentsAt(0), true);

  // Now it should be able to discard the tab.
  EXPECT_TRUE(tab_manager->DiscardTabImpl());
  EXPECT_TRUE(tab_manager->IsTabDiscarded(tsm->GetWebContentsAt(0)));
}

}  // namespace memory

#endif  // OS_WIN || OS_MAXOSX || OS_LINUX
