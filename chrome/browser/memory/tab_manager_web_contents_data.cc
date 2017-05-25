// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager_web_contents_data.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

using base::TimeTicks;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(memory::TabManager::WebContentsData);

namespace memory {

TabManager::WebContentsData::WebContentsData(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      test_tick_clock_(nullptr),
      time_to_purge_(base::TimeDelta::FromMinutes(30)),
      is_purged_(false) {}

TabManager::WebContentsData::~WebContentsData() {}

void TabManager::WebContentsData::DidStartLoading() {
  // Marks the tab as no longer discarded if it has been reloaded from another
  // source (ie: context menu).
  SetDiscardState(false);
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
}

bool TabManager::WebContentsData::IsDiscarded() {
  return tab_data_.is_discarded;
}

void TabManager::WebContentsData::SetDiscardState(bool state) {
  if (tab_data_.is_discarded == state)
    return;

  if (!state) {
    static int reload_count = 0;
    tab_data_.last_reload_time = NowTicks();
    UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.ReloadCount",
                                ++reload_count, 1, 1000, 50);
    auto delta = tab_data_.last_reload_time - tab_data_.last_discard_time;
    // Capped to one day for now, will adjust if necessary.
    UMA_HISTOGRAM_CUSTOM_TIMES("TabManager.Discarding.DiscardToReloadTime",
                               delta, base::TimeDelta::FromSeconds(1),
                               base::TimeDelta::FromDays(1), 100);

    // Record the site engagement score if available.
    if (tab_data_.engagement_score >= 0.0) {
      UMA_HISTOGRAM_COUNTS_100("TabManager.Discarding.ReloadedEngagementScore",
                               tab_data_.engagement_score);
    }
    if (tab_data_.last_inactive_time != base::TimeTicks::UnixEpoch()) {
      delta = tab_data_.last_reload_time - tab_data_.last_inactive_time;
      UMA_HISTOGRAM_CUSTOM_TIMES("TabManager.Discarding.InactiveToReloadTime",
                                 delta, base::TimeDelta::FromSeconds(1),
                                 base::TimeDelta::FromDays(1), 100);
    }

  } else {
    static int discard_count = 0;
    UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.DiscardCount",
                                ++discard_count, 1, 1000, 50);
    tab_data_.last_discard_time = NowTicks();
    // Record the site engagement score if available.
    if (SiteEngagementService::IsEnabled()) {
      SiteEngagementService* service = SiteEngagementService::Get(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
      if (service) {
        tab_data_.engagement_score =
            service->GetScore(web_contents()->GetLastCommittedURL());
        UMA_HISTOGRAM_COUNTS_100(
            "TabManager.Discarding.DiscardedEngagementScore",
            tab_data_.engagement_score);
      }
    }
  }

  tab_data_.is_discarded = state;
  g_browser_process->GetTabManager()->OnDiscardedStateChange(web_contents(),
                                                             state);
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
    FromWebContents(new_contents)->test_tick_clock_ =
        FromWebContents(old_contents)->test_tick_clock_;
  }
}

void TabManager::WebContentsData::set_test_tick_clock(
    base::TickClock* test_tick_clock) {
  test_tick_clock_ = test_tick_clock;
}

TimeTicks TabManager::WebContentsData::NowTicks() const {
  if (!test_tick_clock_)
    return TimeTicks::Now();

  return test_tick_clock_->NowTicks();
}

TabManager::WebContentsData::Data::Data()
    : is_discarded(false),
      discard_count(0),
      is_recently_audible(false),
      last_audio_change_time(TimeTicks::UnixEpoch()),
      last_discard_time(TimeTicks::UnixEpoch()),
      last_reload_time(TimeTicks::UnixEpoch()),
      last_inactive_time(TimeTicks::UnixEpoch()),
      engagement_score(-1.0),
      is_auto_discardable(true) {}

bool TabManager::WebContentsData::Data::operator==(const Data& right) const {
  return is_discarded == right.is_discarded &&
         is_recently_audible == right.is_recently_audible &&
         last_audio_change_time == right.last_audio_change_time &&
         last_discard_time == right.last_discard_time &&
         last_reload_time == right.last_reload_time &&
         last_inactive_time == right.last_inactive_time &&
         engagement_score == right.engagement_score;
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

}  // namespace memory
