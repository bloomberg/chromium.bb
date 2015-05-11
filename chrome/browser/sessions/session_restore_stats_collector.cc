// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_stats_collector.h"

#include <string>

#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

using content::NavigationController;
using content::RenderWidgetHost;
using content::WebContents;

// static
void SessionRestoreStatsCollector::TrackTabs(
    const std::vector<SessionRestoreDelegate::RestoredTab>& tabs,
    const base::TimeTicks& restore_started) {
  if (!shared_collector_)
    shared_collector_ = new SessionRestoreStatsCollector(restore_started);

  shared_collector_->AddTabs(tabs);
}

// static
void SessionRestoreStatsCollector::TrackActiveTabs(
    const std::vector<SessionRestoreDelegate::RestoredTab>& tabs,
    const base::TimeTicks& restore_started) {
  if (!shared_collector_)
    shared_collector_ = new SessionRestoreStatsCollector(restore_started);

  std::vector<SessionRestoreDelegate::RestoredTab> active_tabs;
  for (auto tab : tabs) {
    if (tab.is_active())
      active_tabs.push_back(tab);
  }
  shared_collector_->AddTabs(active_tabs);
}

SessionRestoreStatsCollector::SessionRestoreStatsCollector(
    const base::TimeTicks& restore_started)
    : got_first_foreground_load_(false),
      got_first_paint_(false),
      restore_started_(restore_started),
      tab_count_(0),
      max_parallel_tab_loads_(0) {
  this_retainer_ = this;
  registrar_.Add(
      this, content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
      content::NotificationService::AllSources());
}

SessionRestoreStatsCollector::~SessionRestoreStatsCollector() {
  DCHECK((got_first_paint_ || render_widget_hosts_to_paint_.empty()) &&
         tabs_tracked_.empty() && render_widget_hosts_loading_.empty());
  DCHECK(shared_collector_ == this);
  shared_collector_ = nullptr;
}

void SessionRestoreStatsCollector::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_START: {
      // Add this render_widget_host to the set of those we're waiting for
      // paints on. We want to only record stats for paints that occur after
      // a load has finished.
      NavigationController* tab =
          content::Source<NavigationController>(source).ptr();
      RenderWidgetHost* render_widget_host = GetRenderWidgetHost(tab);
      DCHECK(render_widget_host);
      render_widget_hosts_loading_.insert(render_widget_host);
      break;
    }
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      RemoveTab(&web_contents->GetController());
      break;
    }
    case content::NOTIFICATION_LOAD_STOP: {
      NavigationController* tab =
          content::Source<NavigationController>(source).ptr();
      RenderWidgetHost* render_widget_host = GetRenderWidgetHost(tab);
      render_widget_hosts_to_paint_.insert(render_widget_host);
      RemoveTab(tab);
      if (!got_first_foreground_load_ && render_widget_host &&
          render_widget_host->GetView() &&
          render_widget_host->GetView()->IsShowing()) {
        got_first_foreground_load_ = true;
        base::TimeDelta time_to_load =
            base::TimeTicks::Now() - restore_started_;
        UMA_HISTOGRAM_CUSTOM_TIMES("SessionRestore.ForegroundTabFirstLoaded",
                                   time_to_load,
                                   base::TimeDelta::FromMilliseconds(10),
                                   base::TimeDelta::FromSeconds(100), 100);
        // Record a time for the number of tabs, to help track down
        // contention.
        std::string time_for_count = base::StringPrintf(
            "SessionRestore.ForegroundTabFirstLoaded_%d", tab_count_);
        base::HistogramBase* counter_for_count =
            base::Histogram::FactoryTimeGet(
                time_for_count, base::TimeDelta::FromMilliseconds(10),
                base::TimeDelta::FromSeconds(100), 100,
                base::Histogram::kUmaTargetedHistogramFlag);
        counter_for_count->AddTime(time_to_load);
      }
      break;
    }
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE: {
      RenderWidgetHost* render_widget_host =
          content::Source<RenderWidgetHost>(source).ptr();
      if (!got_first_paint_ && render_widget_host->GetView() &&
          render_widget_host->GetView()->IsShowing()) {
        if (render_widget_hosts_to_paint_.find(render_widget_host) !=
            render_widget_hosts_to_paint_.end()) {
          // Got a paint for one of our renderers, so record time.
          got_first_paint_ = true;
          base::TimeDelta time_to_paint =
              base::TimeTicks::Now() - restore_started_;
          // TODO(danduong): to remove this with 467680, to make sure we
          // don't forget to clean this up.
          UMA_HISTOGRAM_CUSTOM_TIMES("SessionRestore.ForegroundTabFirstPaint",
                                     time_to_paint,
                                     base::TimeDelta::FromMilliseconds(10),
                                     base::TimeDelta::FromSeconds(100), 100);
          // Record a time for the number of tabs, to help track down
          // contention.
          std::string time_for_count = base::StringPrintf(
              "SessionRestore.ForegroundTabFirstPaint_%d", tab_count_);
          base::HistogramBase* counter_for_count =
              base::Histogram::FactoryTimeGet(
                  time_for_count, base::TimeDelta::FromMilliseconds(10),
                  base::TimeDelta::FromSeconds(100), 100,
                  base::Histogram::kUmaTargetedHistogramFlag);
          counter_for_count->AddTime(time_to_paint);
          UMA_HISTOGRAM_CUSTOM_TIMES("SessionRestore.ForegroundTabFirstPaint2",
                                     time_to_paint,
                                     base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMinutes(16), 50);
          // Record a time for the number of tabs, to help track down
          // contention.
          std::string time_for_count2 = base::StringPrintf(
              "SessionRestore.ForegroundTabFirstPaint2_%d", tab_count_);
          base::HistogramBase* counter_for_count2 =
              base::Histogram::FactoryTimeGet(
                  time_for_count2, base::TimeDelta::FromMilliseconds(100),
                  base::TimeDelta::FromMinutes(16), 50,
                  base::Histogram::kUmaTargetedHistogramFlag);
          counter_for_count2->AddTime(time_to_paint);
        } else if (render_widget_hosts_loading_.find(render_widget_host) ==
                   render_widget_hosts_loading_.end()) {
          // If this is a host for a tab we're not loading some other tab
          // has rendered and there's no point tracking the time. This could
          // happen because the user opened a different tab or restored tabs
          // to an already existing browser and an existing tab painted.
          got_first_paint_ = true;
        }
      }
      break;
    }
    default:
      NOTREACHED() << "Unknown notification received:" << type;
      break;
  }

  // Check if we are done and if so, reset |this_retainer_| as the collector no
  // longer needs to stay alive.
  if ((got_first_paint_ || render_widget_hosts_to_paint_.empty()) &&
      tabs_tracked_.empty() && render_widget_hosts_loading_.empty())
    this_retainer_ = nullptr;
}

void SessionRestoreStatsCollector::AddTabs(
    const std::vector<SessionRestoreDelegate::RestoredTab>& tabs) {
  tab_count_ += tabs.size();
  for (auto& tab : tabs) {
    RegisterForNotifications(&tab.contents()->GetController());
    if (tab.is_active()) {
      RenderWidgetHost* render_widget_host =
          GetRenderWidgetHost(&tab.contents()->GetController());
      render_widget_hosts_loading_.insert(render_widget_host);
    }
  }
}

void SessionRestoreStatsCollector::RemoveTab(NavigationController* tab) {
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    content::Source<WebContents>(tab->GetWebContents()));
  registrar_.Remove(this, content::NOTIFICATION_LOAD_STOP,
                    content::Source<NavigationController>(tab));
  registrar_.Remove(this, content::NOTIFICATION_LOAD_START,
                    content::Source<NavigationController>(tab));
  if (render_widget_hosts_loading_.size() > max_parallel_tab_loads_)
    max_parallel_tab_loads_ = render_widget_hosts_loading_.size();
  RenderWidgetHost* render_widget_host = GetRenderWidgetHost(tab);
  render_widget_hosts_loading_.erase(render_widget_host);
  tabs_tracked_.erase(tab);

  // If there are no more tabs loading or being tracked, restore is done and
  // record the time. Note that we are not yet finished, as we might still be
  // waiting for our first paint, which can happen after all tabs are done
  // loading.
  // TODO(georgesak): review behaviour of ForegroundTabFirstPaint.
  if (tabs_tracked_.empty() && render_widget_hosts_loading_.empty()) {
    base::TimeDelta time_to_load = base::TimeTicks::Now() - restore_started_;
    UMA_HISTOGRAM_CUSTOM_TIMES("SessionRestore.AllTabsLoaded", time_to_load,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromSeconds(100), 100);
    // Record a time for the number of tabs, to help track down contention.
    std::string time_for_count =
        base::StringPrintf("SessionRestore.AllTabsLoaded_%d", tab_count_);
    base::HistogramBase* counter_for_count = base::Histogram::FactoryTimeGet(
        time_for_count, base::TimeDelta::FromMilliseconds(10),
        base::TimeDelta::FromSeconds(100), 100,
        base::Histogram::kUmaTargetedHistogramFlag);
    counter_for_count->AddTime(time_to_load);

    UMA_HISTOGRAM_COUNTS_100("SessionRestore.ParallelTabLoads",
                             max_parallel_tab_loads_);
  }
}

void SessionRestoreStatsCollector::RegisterForNotifications(
    NavigationController* tab) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(tab->GetWebContents()));
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::Source<NavigationController>(tab));
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::Source<NavigationController>(tab));
  tabs_tracked_.insert(tab);
}

RenderWidgetHost* SessionRestoreStatsCollector::GetRenderWidgetHost(
    NavigationController* tab) {
  WebContents* web_contents = tab->GetWebContents();
  if (web_contents) {
    content::RenderWidgetHostView* render_widget_host_view =
        web_contents->GetRenderWidgetHostView();
    if (render_widget_host_view)
      return render_widget_host_view->GetRenderWidgetHost();
  }
  return nullptr;
}

// static
SessionRestoreStatsCollector* SessionRestoreStatsCollector::shared_collector_ =
    nullptr;
