// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui/zoom/zoom_controller.h"

#include "base/prefs/pref_service.h"
#include "base/process/kill.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using ui_zoom::ZoomController;
using ui_zoom::ZoomObserver;

bool operator==(const ZoomController::ZoomChangedEventData& lhs,
                const ZoomController::ZoomChangedEventData& rhs) {
  return lhs.web_contents == rhs.web_contents &&
         lhs.old_zoom_level == rhs.old_zoom_level &&
         lhs.new_zoom_level == rhs.new_zoom_level &&
         lhs.zoom_mode == rhs.zoom_mode &&
         lhs.can_show_bubble == rhs.can_show_bubble;
}

class ZoomChangedWatcher : public ZoomObserver {
 public:
  ZoomChangedWatcher(
      content::WebContents* web_contents,
      const ZoomController::ZoomChangedEventData& expected_event_data)
      : web_contents_(web_contents),
        expected_event_data_(expected_event_data),
        message_loop_runner_(new content::MessageLoopRunner) {
    ZoomController::FromWebContents(web_contents)->AddObserver(this);
  }
  ~ZoomChangedWatcher() override {}

  void Wait() { message_loop_runner_->Run(); }

  void OnZoomChanged(
      const ZoomController::ZoomChangedEventData& event_data) override {
    if (event_data == expected_event_data_)
      message_loop_runner_->Quit();
  }

 private:
  content::WebContents* web_contents_;
  ZoomController::ZoomChangedEventData expected_event_data_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(ZoomChangedWatcher);
};

typedef InProcessBrowserTest ZoomControllerBrowserTest;

#if defined(OS_ANDROID)
#define MAYBE_CrashedTabsDoNotChangeZoom DISABLED_CrashedTabsDoNotChangeZoom
#else
#define MAYBE_CrashedTabsDoNotChangeZoom CrashedTabsDoNotChangeZoom
#endif
IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest,
                       MAYBE_CrashedTabsDoNotChangeZoom) {
  // At the start of the test we are at a tab displaying about:blank.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);

  double old_zoom_level = zoom_controller->GetZoomLevel();
  double new_zoom_level = old_zoom_level + 0.5;

  content::RenderProcessHost* host = web_contents->GetRenderProcessHost();
  {
    content::RenderProcessHostWatcher crash_observer(
        host, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    host->Shutdown(0, false);
    crash_observer.Wait();
  }
  EXPECT_FALSE(web_contents->GetRenderViewHost()->IsRenderViewLive());

  // The following attempt to change the zoom level for a crashed tab should
  // fail.
  zoom_controller->SetZoomLevel(new_zoom_level);
  EXPECT_FLOAT_EQ(old_zoom_level, zoom_controller->GetZoomLevel());
}

IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest, OnPreferenceChanged) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  double new_default_zoom_level = 1.0;
  // Since this page uses the default zoom level, the changes to the default
  // zoom level will change the zoom level for this web_contents.
  ZoomController::ZoomChangedEventData zoom_change_data(
      web_contents,
      new_default_zoom_level,
      new_default_zoom_level,
      ZoomController::ZOOM_MODE_DEFAULT,
      true);
  ZoomChangedWatcher zoom_change_watcher(web_contents, zoom_change_data);
  // TODO(wjmaclean): Convert this to call partition-specific zoom level prefs
  // when they become available.
  browser()->profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(
      new_default_zoom_level);
  // Because this test relies on a round-trip IPC to/from the renderer process,
  // we need to wait for it to propagate.
  zoom_change_watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest, ErrorPagesCanZoom) {
  ui_test_utils::NavigateToURL(browser(), GURL("http://kjfhkjsdf.com"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);
  EXPECT_EQ(
      content::PAGE_TYPE_ERROR,
      web_contents->GetController().GetLastCommittedEntry()->GetPageType());

  double old_zoom_level = zoom_controller->GetZoomLevel();
  double new_zoom_level = old_zoom_level + 0.5;

  // The following attempt to change the zoom level for an error page should
  // fail.
  zoom_controller->SetZoomLevel(new_zoom_level);
  EXPECT_FLOAT_EQ(new_zoom_level, zoom_controller->GetZoomLevel());
}

IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest, Observe) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  double new_zoom_level = 1.0;
  // When the event is initiated from HostZoomMap, the old zoom level is not
  // available.
  ZoomController::ZoomChangedEventData zoom_change_data(
      web_contents,
      new_zoom_level,
      new_zoom_level,
      ZoomController::ZOOM_MODE_DEFAULT,
      true);  // We have a non-empty host, so this will be 'true'.
  ZoomChangedWatcher zoom_change_watcher(web_contents, zoom_change_data);

  content::HostZoomMap* host_zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(
          web_contents->GetBrowserContext());

  host_zoom_map->SetZoomLevelForHost("about:blank", new_zoom_level);
  zoom_change_watcher.Wait();
}

// Regression test: crbug.com/438979.
IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest,
                       SettingsZoomAfterSigninWorks) {
  GURL signin_url(
      std::string(chrome::kChromeUIChromeSigninURL).append("?source=0"));
  // We open the signin page in a new tab so that the ZoomController is
  // created against the HostZoomMap of the special StoragePartition that
  // backs the signin page. When we subsequently navigate away from the
  // signin page, the HostZoomMap changes, and we need to test that the
  // ZoomController correctly detects this.
  // TODO(wjmaclean): It would be nice if detecting when the signin page is
  // fully loaded without needing a hard-coded value.
  const int kLoadStopsBeforeSigninPageIsFullyLoaded = 3;
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), signin_url, kLoadStopsBeforeSigninPageIsFullyLoaded,
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(
      content::PAGE_TYPE_ERROR,
      web_contents->GetController().GetLastCommittedEntry()->GetPageType());

  EXPECT_EQ(signin_url, web_contents->GetLastCommittedURL());
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);

  content::HostZoomMap* host_zoom_map_signin =
      content::HostZoomMap::GetForWebContents(web_contents);

  GURL settings_url(chrome::kChromeUISettingsURL);
  ui_test_utils::NavigateToURL(browser(), settings_url);
  EXPECT_NE(
      content::PAGE_TYPE_ERROR,
      web_contents->GetController().GetLastCommittedEntry()->GetPageType());

  // Verify new tab was created.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  // Verify that the settings page is using the same WebContents.
  EXPECT_EQ(web_contents, browser()->tab_strip_model()->GetActiveWebContents());
  // TODO(wjmaclean): figure out why this next line fails, i.e. why does this
  // test not properly trigger a navigation to the settings page.
  EXPECT_EQ(settings_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(zoom_controller, ZoomController::FromWebContents(web_contents));

  // We expect the navigation from the chrome sign in page to the settings
  // page to invoke a storage partition switch, and thus a different HostZoomMap
  // for the web_contents.
  content::HostZoomMap* host_zoom_map_settings =
      content::HostZoomMap::GetForWebContents(web_contents);
  EXPECT_NE(host_zoom_map_signin, host_zoom_map_settings);

  // If we zoom the new page, it should still generate a ZoomController event.
  double old_zoom_level = zoom_controller->GetZoomLevel();
  double new_zoom_level = old_zoom_level + 0.5;

  ZoomController::ZoomChangedEventData zoom_change_data(
      web_contents,
      old_zoom_level,
      new_zoom_level,
      ZoomController::ZOOM_MODE_DEFAULT,
      true);  // We have a non-empty host, so this will be 'true'.
  ZoomChangedWatcher zoom_change_watcher(web_contents, zoom_change_data);
  zoom_controller->SetZoomLevel(new_zoom_level);
  zoom_change_watcher.Wait();
}
