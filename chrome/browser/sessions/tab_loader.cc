// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_loader.h"

#include <algorithm>
#include <string>

#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

using content::NavigationController;
using content::RenderWidgetHost;
using content::WebContents;

// static
TabLoader* TabLoader::GetTabLoader(base::TimeTicks restore_started) {
  if (!shared_tab_loader_)
    shared_tab_loader_ = new TabLoader(restore_started);
  return shared_tab_loader_;
}

void TabLoader::ScheduleLoad(NavigationController* controller) {
  CheckNotObserving(controller);
  DCHECK(controller);
  DCHECK(find(tabs_to_load_.begin(), tabs_to_load_.end(), controller) ==
         tabs_to_load_.end());
  tabs_to_load_.push_back(controller);
  RegisterForNotifications(controller);
}

void TabLoader::TabIsLoading(NavigationController* controller) {
  CheckNotObserving(controller);
  DCHECK(controller);
  DCHECK(find(tabs_loading_.begin(), tabs_loading_.end(), controller) ==
         tabs_loading_.end());
  tabs_loading_.insert(controller);
  RenderWidgetHost* render_widget_host = GetRenderWidgetHost(controller);
  DCHECK(render_widget_host);
  render_widget_hosts_loading_.insert(render_widget_host);
  RegisterForNotifications(controller);
}

void TabLoader::StartLoading() {
  // When multiple profiles are using the same TabLoader, another profile might
  // already have started loading. In that case, the tabs scheduled for loading
  // by this profile are already in the loading queue, and they will get loaded
  // eventually.
  if (delegate_)
    return;

  registrar_.Add(
      this, content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
      content::NotificationService::AllSources());
  this_retainer_ = this;
  // Create a TabLoaderDelegate which will allow OS specific behavior for tab
  // loading.
  if (!delegate_) {
    delegate_ = TabLoaderDelegate::Create(this);
    // There is already at least one tab loading (the active tab). As such we
    // only have to start the timeout timer here.
    StartTimer();
  }
}

void TabLoader::SetTabLoadingEnabled(bool enable_tab_loading) {
  if (enable_tab_loading == loading_enabled_)
    return;
  loading_enabled_ = enable_tab_loading;
  if (loading_enabled_)
    LoadNextTab();
  else
    force_load_timer_.Stop();
}

TabLoader::TabLoader(base::TimeTicks restore_started)
    : memory_pressure_listener_(
          base::Bind(&TabLoader::OnMemoryPressure, base::Unretained(this))),
      force_load_delay_multiplier_(1),
      loading_enabled_(true),
      got_first_foreground_load_(false),
      got_first_paint_(false),
      tab_count_(0),
      restore_started_(restore_started),
      max_parallel_tab_loads_(0) {
}

TabLoader::~TabLoader() {
  DCHECK((got_first_paint_ || render_widget_hosts_to_paint_.empty()) &&
         tabs_loading_.empty() && tabs_to_load_.empty());
  shared_tab_loader_ = nullptr;
}

void TabLoader::LoadNextTab() {
  // LoadNextTab should only get called after we have started the tab
  // loading.
  CHECK(delegate_);
  if (!tabs_to_load_.empty()) {
    NavigationController* tab = tabs_to_load_.front();
    DCHECK(tab);
    tabs_loading_.insert(tab);
    if (tabs_loading_.size() > max_parallel_tab_loads_)
      max_parallel_tab_loads_ = tabs_loading_.size();
    tabs_to_load_.pop_front();
    tab->LoadIfNecessary();
    content::WebContents* contents = tab->GetWebContents();
    if (contents) {
      Browser* browser = chrome::FindBrowserWithWebContents(contents);
      if (browser &&
          browser->tab_strip_model()->GetActiveWebContents() != contents) {
        // By default tabs are marked as visible. As only the active tab is
        // visible we need to explicitly tell non-active tabs they are hidden.
        // Without this call non-active tabs are not marked as backgrounded.
        //
        // NOTE: We need to do this here rather than when the tab is added to
        // the Browser as at that time not everything has been created, so that
        // the call would do nothing.
        contents->WasHidden();
      }
    }
  }

  if (!tabs_to_load_.empty())
    StartTimer();
}

void TabLoader::StartTimer() {
  force_load_timer_.Stop();
  force_load_timer_.Start(FROM_HERE,
                          delegate_->GetTimeoutBeforeLoadingNextTab() *
                              force_load_delay_multiplier_,
                          this, &TabLoader::ForceLoadTimerFired);
}

void TabLoader::Observe(int type,
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
      if (!got_first_paint_) {
        RenderWidgetHost* render_widget_host =
            GetRenderWidgetHost(&web_contents->GetController());
        render_widget_hosts_loading_.erase(render_widget_host);
      }
      HandleTabClosedOrLoaded(&web_contents->GetController());
      break;
    }
    case content::NOTIFICATION_LOAD_STOP: {
      NavigationController* tab =
          content::Source<NavigationController>(source).ptr();
      RenderWidgetHost* render_widget_host = GetRenderWidgetHost(tab);
      render_widget_hosts_to_paint_.insert(render_widget_host);
      HandleTabClosedOrLoaded(tab);
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
  }
  // Delete ourselves when we're not waiting for any more notifications. If this
  // was not the last reference, a SessionRestoreImpl holding a reference will
  // eventually call StartLoading (which assigns this_retainer_), or drop the
  // reference without initiating a load.
  if ((got_first_paint_ || render_widget_hosts_to_paint_.empty()) &&
      tabs_loading_.empty() && tabs_to_load_.empty())
    this_retainer_ = nullptr;
}

void TabLoader::RemoveTab(NavigationController* tab) {
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    content::Source<WebContents>(tab->GetWebContents()));
  registrar_.Remove(this, content::NOTIFICATION_LOAD_STOP,
                    content::Source<NavigationController>(tab));
  registrar_.Remove(this, content::NOTIFICATION_LOAD_START,
                    content::Source<NavigationController>(tab));

  TabsLoading::iterator i = tabs_loading_.find(tab);
  if (i != tabs_loading_.end())
    tabs_loading_.erase(i);

  TabsToLoad::iterator j =
      find(tabs_to_load_.begin(), tabs_to_load_.end(), tab);
  if (j != tabs_to_load_.end())
    tabs_to_load_.erase(j);
}

void TabLoader::ForceLoadTimerFired() {
  force_load_delay_multiplier_ *= 2;
  LoadNextTab();
}

RenderWidgetHost* TabLoader::GetRenderWidgetHost(NavigationController* tab) {
  WebContents* web_contents = tab->GetWebContents();
  if (web_contents) {
    content::RenderWidgetHostView* render_widget_host_view =
        web_contents->GetRenderWidgetHostView();
    if (render_widget_host_view)
      return render_widget_host_view->GetRenderWidgetHost();
  }
  return nullptr;
}

void TabLoader::RegisterForNotifications(NavigationController* controller) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(controller->GetWebContents()));
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::Source<NavigationController>(controller));
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::Source<NavigationController>(controller));
  ++tab_count_;
}

void TabLoader::HandleTabClosedOrLoaded(NavigationController* tab) {
  RemoveTab(tab);
  if (delegate_ && loading_enabled_)
    LoadNextTab();
  if (tabs_loading_.empty() && tabs_to_load_.empty()) {
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

void TabLoader::CheckNotObserving(NavigationController* controller) {
  const bool in_tabs_to_load = find(tabs_to_load_.begin(), tabs_to_load_.end(),
                                    controller) != tabs_to_load_.end();
  const bool in_tabs_loading = find(tabs_loading_.begin(), tabs_loading_.end(),
                                    controller) != tabs_loading_.end();
  const bool observing =
      registrar_.IsRegistered(
          this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
          content::Source<WebContents>(controller->GetWebContents())) ||
      registrar_.IsRegistered(
          this, content::NOTIFICATION_LOAD_STOP,
          content::Source<NavigationController>(controller)) ||
      registrar_.IsRegistered(
          this, content::NOTIFICATION_LOAD_START,
          content::Source<NavigationController>(controller));
  base::debug::Alias(&in_tabs_to_load);
  base::debug::Alias(&in_tabs_loading);
  base::debug::Alias(&observing);
  CHECK(!in_tabs_to_load && !in_tabs_loading && !observing);
}

void TabLoader::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  // When receiving a resource pressure level warning, we stop pre-loading more
  // tabs since we are running in danger of loading more tabs by throwing out
  // old ones.
  if (tabs_to_load_.empty())
    return;
  // Stop the timer and suppress any tab loads while we clean the list.
  SetTabLoadingEnabled(false);
  while (!tabs_to_load_.empty()) {
    NavigationController* controller = tabs_to_load_.front();
    tabs_to_load_.pop_front();
    RemoveTab(controller);
  }
  // By calling |LoadNextTab| explicitly, we make sure that the
  // |NOTIFICATION_SESSION_RESTORE_DONE| event gets sent.
  LoadNextTab();
}

// static
TabLoader* TabLoader::shared_tab_loader_ = nullptr;
