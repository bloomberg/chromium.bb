// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/zoom/zoom_controller.h"

#include "base/prefs/pref_service.h"
#include "base/process/kill.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

typedef InProcessBrowserTest ZoomControllerBrowserTest;

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
  virtual ~ZoomChangedWatcher() {}

  void Wait() { message_loop_runner_->Run(); }

  virtual void OnZoomChanged(
      const ZoomController::ZoomChangedEventData& event_data) OVERRIDE {
    if (event_data == expected_event_data_)
      message_loop_runner_->Quit();
  }

 private:
  content::WebContents* web_contents_;
  ZoomController::ZoomChangedEventData expected_event_data_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(ZoomChangedWatcher);
};

// TODO(wjmaclean): Enable this on Android when we can kill the process there.
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
    base::KillProcess(host->GetHandle(), 0, false);
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
  browser()->profile()->GetPrefs()->SetDouble(prefs::kDefaultZoomLevel,
                                              new_default_zoom_level);
  // Because this test relies on a round-trip IPC to/from the renderer process,
  // we need to wait for it to propagate.
  zoom_change_watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(ZoomControllerBrowserTest, ErrorPagesDoNotZoom) {
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
  EXPECT_FLOAT_EQ(old_zoom_level, zoom_controller->GetZoomLevel());
}
