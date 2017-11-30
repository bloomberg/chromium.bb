// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_ACTIVITY_WATCHER_H_
#define CHROME_BROWSER_UI_TABS_TAB_ACTIVITY_WATCHER_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class TabMetricsLogger;

// Observes background tab activity in order to log UKMs for tabs. Metrics will
// be compared against tab reactivation/close events to determine the end state
// of each background tab.
class TabActivityWatcher : public TabStripModelObserver,
                           public BrowserTabStripTrackerDelegate {
 public:
  // Helper class to observe WebContents.
  class WebContentsData;

  TabActivityWatcher();
  ~TabActivityWatcher() override;

  // TODO(michaelpg): Track more events.

  // Forces logging even when a timeout would have prevented it.
  void DisableLogTimeoutForTest();

  // Returns the single instance, creating it if necessary.
  static TabActivityWatcher* GetInstance();

  // Begins watching the |web_contents|. The watching will stop automatically
  // when the WebContents is destroyed.
  static void WatchWebContents(content::WebContents* web_contents);

 private:
  // TabStripModelObserver:
  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index) override;
  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(Browser* browser) override;

  // Called from WebContentsData when a tab is being hidden.
  void OnWasHidden(content::WebContents* web_contents);

  // Called from WebContentsData when a tab has stopped loading.
  void OnDidStopLoading(content::WebContents* web_contents);

  // Logs the tab with |web_contents| if the tab hasn't been logged for the same
  // source ID within a timeout window.
  void MaybeLogTab(content::WebContents* web_contents);

  std::unique_ptr<TabMetricsLogger> tab_metrics_logger_;

  // Manages registration of this class as an observer of all TabStripModels as
  // browsers are created and destroyed.
  BrowserTabStripTracker browser_tab_strip_tracker_;

  // Time before a tab with the same SourceId can be logged again.
  base::TimeDelta per_source_log_timeout_;

  DISALLOW_COPY_AND_ASSIGN(TabActivityWatcher);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_ACTIVITY_WATCHER_H_
