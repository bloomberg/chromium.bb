// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/discard_metrics_util.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

using base::TimeTicks;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    resource_coordinator::TabManager::WebContentsData);

namespace resource_coordinator {

TabManager::WebContentsData::WebContentsData(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      time_to_purge_(base::TimeDelta::FromMinutes(30)),
      is_purged_(false),
      ukm_source_id_(0) {}

TabManager::WebContentsData::~WebContentsData() {}

void TabManager::WebContentsData::DidStartLoading() {
  // Marks the tab as no longer discarded if it has been reloaded from another
  // source (ie: context menu).
  SetDiscardState(false);
}

void TabManager::WebContentsData::DidStopLoading() {
  if (IsPageAlmostIdleSignalEnabled())
    return;
  NotifyTabIsLoaded();
}

void TabManager::WebContentsData::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Only change to the loading state if there is a navigation in the main
  // frame. DidStartLoading() happens before this, but at that point we don't
  // know if the load is happening in the main frame or an iframe.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  SetTabLoadingState(TAB_IS_LOADING);
  g_browser_process->GetTabManager()
      ->stats_collector()
      ->OnDidStartMainFrameNavigation(web_contents());
}

void TabManager::WebContentsData::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  SetIsInSessionRestore(false);
  g_browser_process->GetTabManager()->OnDidFinishNavigation(navigation_handle);

  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInMainFrame()) {
    return;
  }

  tab_data_.navigation_time = navigation_handle->NavigationStart();
  ukm_source_id_ = ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                                          ukm::SourceIdType::NAVIGATION_ID);
}

void TabManager::WebContentsData::WasShown() {
  if (tab_data_.last_inactive_time.is_null())
    return;
  ReportUKMWhenBackgroundTabIsClosedOrForegrounded(true);
}

void TabManager::WebContentsData::WebContentsDestroyed() {
  // If Chrome is shutting down, ignore this event.
  if (g_browser_process->IsShuttingDown())
    return;

  // If the tab has been previously discarded but is not currently discarded
  // (ie. it has been reloaded), we want to record the time it took between the
  // reload event and the closing of the tab.
  if (tab_data_.discard_count > 0 && !tab_data_.is_discarded) {
    auto delta = NowTicks() - tab_data_.last_reload_time;
    // Capped to one day for now, will adjust if necessary.
    UMA_HISTOGRAM_CUSTOM_TIMES("TabManager.Discarding.ReloadToCloseTime", delta,
                               base::TimeDelta::FromSeconds(1),
                               base::TimeDelta::FromDays(1), 100);
  }

  ReportUKMWhenTabIsClosed();

  if (!web_contents()->IsVisible() && !tab_data_.last_inactive_time.is_null())
    ReportUKMWhenBackgroundTabIsClosedOrForegrounded(false);

  SetTabLoadingState(TAB_IS_NOT_LOADING);
  SetIsInSessionRestore(false);
  g_browser_process->GetTabManager()->OnWebContentsDestroyed(web_contents());
}

void TabManager::WebContentsData::NotifyTabIsLoaded() {
  // We may already be in the stopped state if this is being invoked due to an
  // iframe loading new content.
  if (tab_data_.tab_loading_state != TAB_IS_LOADED) {
    SetTabLoadingState(TAB_IS_LOADED);
    g_browser_process->GetTabManager()->OnTabIsLoaded(web_contents());
  }
}

bool TabManager::WebContentsData::IsDiscarded() {
  return tab_data_.is_discarded;
}

void TabManager::WebContentsData::SetDiscardState(bool is_discarded) {
  if (tab_data_.is_discarded == is_discarded)
    return;

  if (is_discarded) {
    tab_data_.last_discard_time = NowTicks();
    RecordTabDiscarded();
  } else {
    tab_data_.last_reload_time = NowTicks();
    RecordTabReloaded(tab_data_.last_inactive_time, tab_data_.last_discard_time,
                      tab_data_.last_reload_time);
  }

  tab_data_.is_discarded = is_discarded;
  g_browser_process->GetTabManager()->OnDiscardedStateChange(web_contents(),
                                                             is_discarded);
}

int TabManager::WebContentsData::DiscardCount() {
  return tab_data_.discard_count;
}

void TabManager::WebContentsData::IncrementDiscardCount() {
  tab_data_.discard_count++;
}

bool TabManager::WebContentsData::IsRecentlyAudible() {
  return tab_data_.is_recently_audible;
}

void TabManager::WebContentsData::SetRecentlyAudible(bool state) {
  tab_data_.is_recently_audible = state;
}

TimeTicks TabManager::WebContentsData::LastAudioChangeTime() {
  return tab_data_.last_audio_change_time;
}

void TabManager::WebContentsData::SetLastAudioChangeTime(TimeTicks timestamp) {
  tab_data_.last_audio_change_time = timestamp;
}

TimeTicks TabManager::WebContentsData::LastInactiveTime() {
  return tab_data_.last_inactive_time;
}

void TabManager::WebContentsData::SetLastInactiveTime(TimeTicks timestamp) {
  tab_data_.last_inactive_time = timestamp;
}

// static
void TabManager::WebContentsData::CopyState(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  // Only copy if an existing state is found.
  if (FromWebContents(old_contents)) {
    CreateForWebContents(new_contents);
    FromWebContents(new_contents)->tab_data_ =
        FromWebContents(old_contents)->tab_data_;
  }
}

void TabManager::WebContentsData::ReportUKMWhenTabIsClosed() {
  if (!ukm_source_id_)
    return;
  auto duration = NowTicks() - tab_data_.navigation_time;
  ukm::builders::TabManager_TabLifetime(ukm_source_id_)
      .SetTimeSinceNavigation(duration.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void TabManager::WebContentsData::
    ReportUKMWhenBackgroundTabIsClosedOrForegrounded(bool is_foregrounded) {
  if (!ukm_source_id_)
    return;
  auto duration = NowTicks() - tab_data_.last_inactive_time;
  ukm::builders::TabManager_Background_ForegroundedOrClosed(ukm_source_id_)
      .SetTimeFromBackgrounded(duration.InMilliseconds())
      .SetIsForegrounded(is_foregrounded)
      .Record(ukm::UkmRecorder::Get());
}

TabManager::WebContentsData::Data::Data()
    : is_discarded(false),
      discard_count(0),
      is_recently_audible(false),
      is_auto_discardable(true),
      tab_loading_state(TAB_IS_NOT_LOADING),
      is_in_session_restore(false),
      is_restored_in_foreground(false) {}

bool TabManager::WebContentsData::Data::operator==(const Data& right) const {
  return is_discarded == right.is_discarded &&
         is_recently_audible == right.is_recently_audible &&
         last_audio_change_time == right.last_audio_change_time &&
         last_discard_time == right.last_discard_time &&
         last_reload_time == right.last_reload_time &&
         last_inactive_time == right.last_inactive_time &&
         tab_loading_state == right.tab_loading_state &&
         is_in_session_restore == right.is_in_session_restore &&
         is_restored_in_foreground == right.is_restored_in_foreground;
}

bool TabManager::WebContentsData::Data::operator!=(const Data& right) const {
  return !(*this == right);
}

void TabManager::WebContentsData::SetAutoDiscardableState(bool state) {
  if (tab_data_.is_auto_discardable == state)
    return;

  tab_data_.is_auto_discardable = state;
  g_browser_process->GetTabManager()->OnAutoDiscardableStateChange(
      web_contents(), state);
}

bool TabManager::WebContentsData::IsAutoDiscardable() {
  return tab_data_.is_auto_discardable;
}

}  // namespace resource_coordinator
