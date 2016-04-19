// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_loader.h"

#include <algorithm>
#include <string>

#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/sessions/session_restore_stats_collector.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

using content::NavigationController;
using content::RenderWidgetHost;
using content::WebContents;

void TabLoader::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      HandleTabClosedOrLoaded(&web_contents->GetController());
      break;
    }
    case content::NOTIFICATION_LOAD_STOP: {
      NavigationController* controller =
          content::Source<NavigationController>(source).ptr();
      HandleTabClosedOrLoaded(controller);
      break;
    }
    default:
      NOTREACHED() << "Unknown notification received:" << type;
  }
  // Delete ourselves when we are done.
  if (tabs_loading_.empty() && tabs_to_load_.empty())
    this_retainer_ = nullptr;
}

void TabLoader::SetTabLoadingEnabled(bool enable_tab_loading) {
  // TODO(chrisha): Make the SessionRestoreStatsCollector aware that tab loading
  // was explicitly stopped or restarted. This can make be used to invalidate
  // various metrics.
  if (enable_tab_loading == loading_enabled_)
    return;
  loading_enabled_ = enable_tab_loading;
  if (loading_enabled_) {
    LoadNextTab();
  } else {
    force_load_timer_.Stop();
  }
}

// static
void TabLoader::RestoreTabs(const std::vector<RestoredTab>& tabs,
                            const base::TimeTicks& restore_started) {
  if (!shared_tab_loader_)
    shared_tab_loader_ = new TabLoader(restore_started);

  shared_tab_loader_->stats_collector_->TrackTabs(tabs);
  shared_tab_loader_->StartLoading(tabs);
}

TabLoader::TabLoader(base::TimeTicks restore_started)
    : memory_pressure_listener_(
          base::Bind(&TabLoader::OnMemoryPressure, base::Unretained(this))),
      force_load_delay_multiplier_(1),
      loading_enabled_(true),
      restore_started_(restore_started) {
  stats_collector_ = new SessionRestoreStatsCollector(
      restore_started,
      base::WrapUnique(
          new SessionRestoreStatsCollector::UmaStatsReportingDelegate()));
  shared_tab_loader_ = this;
  this_retainer_ = this;
}

TabLoader::~TabLoader() {
  DCHECK(tabs_loading_.empty() && tabs_to_load_.empty());
  DCHECK(shared_tab_loader_ == this);
  shared_tab_loader_ = nullptr;
}

void TabLoader::StartLoading(const std::vector<RestoredTab>& tabs) {
  // Add the tabs to the list of tabs loading/to load and register them for
  // notifications. Also, restore the favicons of the background tabs (the title
  // has already been set by now).This avoids having blank icons in case the
  // restore is halted due to memory pressure. Also, when multiple tabs are
  // restored to a single window, the title may not appear, and the user will
  // have no way of finding out which tabs corresponds to which page if the icon
  // is a generic grey one.
  for (auto& restored_tab : tabs) {
    if (!restored_tab.is_active()) {
      tabs_to_load_.push_back(&restored_tab.contents()->GetController());
      favicon::ContentFaviconDriver* favicon_driver =
          favicon::ContentFaviconDriver::FromWebContents(
              restored_tab.contents());
      favicon_driver->FetchFavicon(favicon_driver->GetActiveURL());
    } else {
      tabs_loading_.insert(&restored_tab.contents()->GetController());
    }
    RegisterForNotifications(&restored_tab.contents()->GetController());
  }

  // When multiple profiles are using the same TabLoader, another profile might
  // already have started loading. In that case, the tabs scheduled for loading
  // by this profile are already in the loading queue, and they will get loaded
  // eventually.
  if (delegate_)
    return;

  // Create a TabLoaderDelegate which will allow OS specific behavior for tab
  // loading.
  if (!delegate_) {
    delegate_ = TabLoaderDelegate::Create(this);
    // There is already at least one tab loading (the active tab). As such we
    // only have to start the timeout timer here. But, don't restore background
    // tabs if the system is under memory pressure.
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level =
        CurrentMemoryPressureLevel();

    if (memory_pressure_level !=
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
      OnMemoryPressure(memory_pressure_level);
      return;
    }

    StartFirstTimer();
  }
}

void TabLoader::LoadNextTab() {
  // LoadNextTab should only get called after we have started the tab
  // loading.
  CHECK(delegate_);

  // Abort if loading is not enabled.
  if (!loading_enabled_)
    return;

  if (!tabs_to_load_.empty()) {
    // Check the memory pressure before restoring the next tab, and abort if
    // there is pressure. This is important on the Mac because of the sometimes
    // large delay between a memory pressure event and receiving a notification
    // of that event (in that case tab restore can trigger memory pressure but
    // will complete before the notification arrives).
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level =
        CurrentMemoryPressureLevel();
    if (memory_pressure_level !=
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
      OnMemoryPressure(memory_pressure_level);
      return;
    }

    NavigationController* controller = tabs_to_load_.front();
    DCHECK(controller);
    tabs_loading_.insert(controller);
    tabs_to_load_.pop_front();
    controller->LoadIfNecessary();
    content::WebContents* contents = controller->GetWebContents();
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

void TabLoader::StartFirstTimer() {
  force_load_timer_.Stop();
  force_load_timer_.Start(FROM_HERE,
                          delegate_->GetFirstTabLoadingTimeout(),
                          this, &TabLoader::ForceLoadTimerFired);
}

void TabLoader::StartTimer() {
  force_load_timer_.Stop();
  force_load_timer_.Start(FROM_HERE,
                          delegate_->GetTimeoutBeforeLoadingNextTab() *
                              force_load_delay_multiplier_,
                          this, &TabLoader::ForceLoadTimerFired);
}

void TabLoader::RemoveTab(NavigationController* controller) {
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    content::Source<WebContents>(controller->GetWebContents()));
  registrar_.Remove(this, content::NOTIFICATION_LOAD_STOP,
                    content::Source<NavigationController>(controller));

  TabsLoading::iterator i = tabs_loading_.find(controller);
  if (i != tabs_loading_.end())
    tabs_loading_.erase(i);

  TabsToLoad::iterator j =
      find(tabs_to_load_.begin(), tabs_to_load_.end(), controller);
  if (j != tabs_to_load_.end())
    tabs_to_load_.erase(j);
}

void TabLoader::ForceLoadTimerFired() {
  force_load_delay_multiplier_ *= 2;
  LoadNextTab();
}

void TabLoader::RegisterForNotifications(NavigationController* controller) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(controller->GetWebContents()));
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::Source<NavigationController>(controller));
}

void TabLoader::HandleTabClosedOrLoaded(NavigationController* controller) {
  RemoveTab(controller);
  if (delegate_)
    LoadNextTab();
}

base::MemoryPressureListener::MemoryPressureLevel
    TabLoader::CurrentMemoryPressureLevel() {
  if (base::MemoryPressureMonitor::Get())
    return base::MemoryPressureMonitor::Get()->GetCurrentPressureLevel();

  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
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
    NavigationController* tab = tabs_to_load_.front();
    tabs_to_load_.pop_front();
    RemoveTab(tab);

    // Notify the stats collector that a tab's loading has been deferred due to
    // memory pressure.
    stats_collector_->DeferTab(tab);
  }
  // By calling |LoadNextTab| explicitly, we make sure that the
  // |NOTIFICATION_SESSION_RESTORE_DONE| event gets sent.
  LoadNextTab();
}

// static
TabLoader* TabLoader::shared_tab_loader_ = nullptr;
