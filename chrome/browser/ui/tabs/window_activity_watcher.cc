// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/window_activity_watcher.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_metrics_event.pb.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

using WindowMetrics = WindowActivityWatcher::WindowMetrics;
using metrics::WindowMetricsEvent;

struct WindowActivityWatcher::WindowMetrics {
  bool operator==(const WindowMetrics& other) {
    return window_id == other.window_id && show_state == other.show_state &&
           type == other.type;
  }

  bool operator!=(const WindowMetrics& other) { return !operator==(other); }

  // ID for the window, unique within the Chrome session.
  SessionID::id_type window_id;
  WindowMetricsEvent::Type type;
  // TODO(michaelpg): Observe the show state and log when it changes.
  WindowMetricsEvent::ShowState show_state;
};

namespace {

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

  if (browser->window()->IsFullscreen())
    window_metrics.show_state = WindowMetricsEvent::SHOW_STATE_FULLSCREEN;
  else if (browser->window()->IsMinimized())
    window_metrics.show_state = WindowMetricsEvent::SHOW_STATE_MINIMIZED;
  else if (browser->window()->IsMaximized())
    window_metrics.show_state = WindowMetricsEvent::SHOW_STATE_MAXIMIZED;
  else
    window_metrics.show_state = WindowMetricsEvent::SHOW_STATE_NORMAL;
  return window_metrics;
}

// Logs a UKM entry with the corresponding metrics.
void LogUkmEntry(const WindowMetrics& window_metrics) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  ukm::builders::TabManager_WindowMetrics entry(ukm::AssignNewSourceId());
  entry.SetWindowId(window_metrics.window_id)
      .SetShowState(window_metrics.show_state)
      .SetType(window_metrics.type)
      .Record(ukm_recorder);
}

}  // namespace

// static
WindowActivityWatcher* WindowActivityWatcher::GetInstance() {
  CR_DEFINE_STATIC_LOCAL(WindowActivityWatcher, instance, ());
  return &instance;
}

void WindowActivityWatcher::CreateOrUpdateWindowMetrics(
    const Browser* browser) {
  DCHECK(!browser->profile()->IsOffTheRecord());
  SessionID::id_type window_id = browser->session_id().id();
  const auto& existing_metrics_it = window_metrics_.find(window_id);

  // We only need to create a new UKM entry if the metrics have changed or
  // this browser hasn't been logged yet.
  WindowMetrics new_metrics = CreateMetrics(browser);
  if (existing_metrics_it == window_metrics_.end() ||
      existing_metrics_it->second != new_metrics) {
    LogUkmEntry(new_metrics);
    window_metrics_[window_id] = new_metrics;
  }
}

WindowActivityWatcher::WindowActivityWatcher() : browser_list_observer_(this) {
  browser_list_observer_.Add(BrowserList::GetInstance());
}

WindowActivityWatcher::~WindowActivityWatcher() = default;

void WindowActivityWatcher::OnBrowserAdded(Browser* browser) {
  // Don't track incognito browsers. This is also enforced by UKM.
  if (!browser->profile()->IsOffTheRecord())
    CreateOrUpdateWindowMetrics(browser);
}

void WindowActivityWatcher::OnBrowserRemoved(Browser* browser) {
  if (!browser->profile()->IsOffTheRecord())
    window_metrics_.erase(browser->session_id().id());
}

void WindowActivityWatcher::OnBrowserSetLastActive(Browser* browser) {
  if (!browser->profile()->IsOffTheRecord())
    CreateOrUpdateWindowMetrics(browser);
}

void WindowActivityWatcher::OnBrowserNoLongerActive(Browser* browser) {
  if (!browser->profile()->IsOffTheRecord())
    CreateOrUpdateWindowMetrics(browser);
}
