// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats_tracker.h"

#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace metrics {

// static
const char TabStatsTracker::UmaStatsReportingDelegate::
    kNumberOfTabsOnResumeHistogramName[] = "Tabs.NumberOfTabsOnResume";

// static
void TabStatsTracker::Initialize() {
  // Calls GetInstance() to initialize the static instance.
  GetInstance();
}

// static
TabStatsTracker* TabStatsTracker::GetInstance() {
  static TabStatsTracker* instance = new TabStatsTracker();
  return instance;
}

TabStatsTracker::TabStatsTracker()
    : total_tabs_count_(0U),
      browser_count_(0U),
      reporting_delegate_(base::MakeUnique<UmaStatsReportingDelegate>()) {
  // Get the list of existing browsers/tabs. There shouldn't be any if this is
  // initialized at startup but this will ensure that the count stay accurate if
  // the initialization gets moved to after the creation of the first tab.
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    ++browser_count_;
    browser->tab_strip_model()->AddObserver(this);
    total_tabs_count_ += browser->tab_strip_model()->count();
  }
  browser_list->AddObserver(this);
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor != nullptr)
    power_monitor->AddObserver(this);
}

TabStatsTracker::~TabStatsTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    --browser_count_;
    browser->tab_strip_model()->RemoveObserver(this);
    total_tabs_count_ -= browser->tab_strip_model()->count();
  }
  browser_list->RemoveObserver(this);

  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor != nullptr)
    power_monitor->RemoveObserver(this);
}

void TabStatsTracker::OnBrowserAdded(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++browser_count_;
  browser->tab_strip_model()->AddObserver(this);
}

void TabStatsTracker::OnBrowserRemoved(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  --browser_count_;
  browser->tab_strip_model()->RemoveObserver(this);
}

void TabStatsTracker::TabInsertedAt(TabStripModel* model,
                                    content::WebContents* web_contents,
                                    int index,
                                    bool foreground) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++total_tabs_count_;
}

void TabStatsTracker::TabClosingAt(TabStripModel* model,
                                   content::WebContents* web_contents,
                                   int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  --total_tabs_count_;
}

void TabStatsTracker::OnResume() {
  reporting_delegate_->ReportTabCountOnResume(total_tabs_count_);
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportTabCountOnResume(
    size_t tab_count) {
  // Don't report the number of tabs on resume if Chrome is running in
  // background with no visible window.
  if (g_browser_process && g_browser_process->background_mode_manager()
                               ->IsBackgroundWithoutWindows()) {
    return;
  }
  UMA_HISTOGRAM_COUNTS_10000(kNumberOfTabsOnResumeHistogramName, tab_count);
}

}  // namespace metrics
