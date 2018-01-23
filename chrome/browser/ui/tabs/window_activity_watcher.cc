// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/window_activity_watcher.h"

#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_metrics_event.pb.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

using metrics::WindowMetricsEvent;

namespace {

struct WindowMetrics {
  bool operator==(const WindowMetrics& other) {
    return window_id == other.window_id && type == other.type &&
           show_state == other.show_state && is_active == other.is_active &&
           tab_count == other.tab_count;
  }

  bool operator!=(const WindowMetrics& other) { return !operator==(other); }

  // ID for the window, unique within the Chrome session.
  SessionID::id_type window_id;
  WindowMetricsEvent::Type type;
  // TODO(michaelpg): Observe the show state and log when it changes.
  WindowMetricsEvent::ShowState show_state;

  // True if this is the active (frontmost) window.
  bool is_active;

  // Number of tabs in the tab strip.
  int tab_count;
};

// Sets metric values that are dependent on the current window state.
void UpdateMetrics(const Browser* browser, WindowMetrics* window_metrics) {
  if (browser->window()->IsFullscreen())
    window_metrics->show_state = WindowMetricsEvent::SHOW_STATE_FULLSCREEN;
  else if (browser->window()->IsMinimized())
    window_metrics->show_state = WindowMetricsEvent::SHOW_STATE_MINIMIZED;
  else if (browser->window()->IsMaximized())
    window_metrics->show_state = WindowMetricsEvent::SHOW_STATE_MAXIMIZED;
  else
    window_metrics->show_state = WindowMetricsEvent::SHOW_STATE_NORMAL;

  window_metrics->is_active = browser->window()->IsActive();
  window_metrics->tab_count = browser->tab_strip_model()->count();
}

// Returns a populated WindowMetrics for the browser.
WindowMetrics CreateMetrics(const Browser* browser) {
  WindowMetrics window_metrics;
  window_metrics.window_id = browser->session_id().id();

  switch (browser->type()) {
    case Browser::TYPE_TABBED:
      window_metrics.type = WindowMetricsEvent::TYPE_TABBED;
      break;
    case Browser::TYPE_POPUP:
      window_metrics.type = WindowMetricsEvent::TYPE_POPUP;
      break;
  }

  UpdateMetrics(browser, &window_metrics);
  return window_metrics;
}

// Logs a UKM entry with the metrics from |window_metrics|.
void LogWindowMetricsUkmEntry(const WindowMetrics& window_metrics) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  ukm::builders::TabManager_WindowMetrics entry(ukm::AssignNewSourceId());
  entry.SetWindowId(window_metrics.window_id)
      .SetIsActive(window_metrics.is_active)
      .SetShowState(window_metrics.show_state)
      .SetType(window_metrics.type);

  // Bucketize values for privacy considerations. Use a spacing factor that
  // ensures values up to 3 can be logged exactly; after that, the precise
  // number becomes less significant.
  int tab_count = window_metrics.tab_count;
  if (tab_count > 3)
    tab_count = ukm::GetExponentialBucketMin(tab_count, 1.5);
  entry.SetTabCount(tab_count);

  entry.Record(ukm_recorder);
}

}  // namespace

// Observes a browser window's tab strip and logs a WindowMetrics UKM event for
// the window upon changes to metrics like TabCount.
class WindowActivityWatcher::BrowserWatcher : public TabStripModelObserver {
 public:
  explicit BrowserWatcher(Browser* browser)
      : browser_(browser), observer_(this) {
    DCHECK(!browser->profile()->IsOffTheRecord());
    MaybeLogWindowMetricsUkmEntry();
    observer_.Add(browser->tab_strip_model());
  }

  ~BrowserWatcher() override = default;

  // Logs a new WindowMetrics entry to the UKM recorder if the entry would be
  // different than the last one we logged.
  void MaybeLogWindowMetricsUkmEntry() {
    // Do nothing if the window has no tabs (which can happen when a window is
    // opened, before a tab is added) or is being closed.
    if (browser_->tab_strip_model()->empty() ||
        browser_->tab_strip_model()->closing_all()) {
      return;
    }

    if (!last_window_metrics_) {
      last_window_metrics_ = CreateMetrics(browser_);
      LogWindowMetricsUkmEntry(last_window_metrics_.value());
      return;
    }

    // Copy old state to compare with.
    WindowMetrics old_metrics = last_window_metrics_.value();
    UpdateMetrics(browser_, &last_window_metrics_.value());

    // We only need to create a new UKM entry if the metrics have changed.
    if (old_metrics != last_window_metrics_.value())
      LogWindowMetricsUkmEntry(last_window_metrics_.value());
  }

 private:
  // TabStripModelObserver:
  void TabInsertedAt(TabStripModel* tab_strip_model,
                     content::WebContents* contents,
                     int index,
                     bool foreground) override {
    MaybeLogWindowMetricsUkmEntry();
  }
  void TabDetachedAt(content::WebContents* contents, int index) override {
    MaybeLogWindowMetricsUkmEntry();
  }

  // The browser whose tab strip we track. WindowActivityWatcher should ensure
  // this outlives us.
  Browser* browser_;

  // The most recent WindowMetrics entry logged. We only log a new UKM entry if
  // some metric value has changed.
  base::Optional<WindowMetrics> last_window_metrics_;

  // Used to update the tab count for browser windows.
  ScopedObserver<TabStripModel, TabStripModelObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWatcher);
};

// static
WindowActivityWatcher* WindowActivityWatcher::GetInstance() {
  CR_DEFINE_STATIC_LOCAL(WindowActivityWatcher, instance, ());
  return &instance;
}

WindowActivityWatcher::WindowActivityWatcher() {
  BrowserList::AddObserver(this);
  for (Browser* browser : *BrowserList::GetInstance())
    OnBrowserAdded(browser);
}

WindowActivityWatcher::~WindowActivityWatcher() {
  BrowserList::RemoveObserver(this);
}

bool WindowActivityWatcher::ShouldTrackBrowser(Browser* browser) {
  // Don't track incognito browsers. This is also enforced by UKM.
  return !browser->profile()->IsOffTheRecord();
}

void WindowActivityWatcher::OnBrowserAdded(Browser* browser) {
  if (ShouldTrackBrowser(browser))
    browser_watchers_[browser] = std::make_unique<BrowserWatcher>(browser);
}

void WindowActivityWatcher::OnBrowserRemoved(Browser* browser) {
  browser_watchers_.erase(browser);
}

void WindowActivityWatcher::OnBrowserSetLastActive(Browser* browser) {
  if (ShouldTrackBrowser(browser))
    browser_watchers_[browser]->MaybeLogWindowMetricsUkmEntry();
}

void WindowActivityWatcher::OnBrowserNoLongerActive(Browser* browser) {
  if (ShouldTrackBrowser(browser))
    browser_watchers_[browser]->MaybeLogWindowMetricsUkmEntry();
}
