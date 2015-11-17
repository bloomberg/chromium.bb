// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager_web_contents_data.h"

#include "base/metrics/histogram.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

using base::TimeTicks;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(memory::TabManager::WebContentsData);

namespace memory {

TabManager::WebContentsData::WebContentsData(content::WebContents* web_contents)
    : WebContentsObserver(web_contents), test_tick_clock_(nullptr) {}

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
  if (tab_data_.discard_count_ > 0 && !tab_data_.is_discarded_) {
    auto delta = NowTicks() - tab_data_.last_reload_time_;
    // Capped to one day for now, will adjust if necessary.
    UMA_HISTOGRAM_CUSTOM_TIMES("TabManager.Discarding.ReloadToCloseTime", delta,
                               base::TimeDelta::FromSeconds(1),
                               base::TimeDelta::FromDays(1), 100);
  }
}

bool TabManager::WebContentsData::IsDiscarded() {
  return tab_data_.is_discarded_;
}

void TabManager::WebContentsData::SetDiscardState(bool state) {
  if (tab_data_.is_discarded_ && !state) {
    static int reload_count = 0;
    tab_data_.last_reload_time_ = NowTicks();
    UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.ReloadCount",
                                ++reload_count, 1, 1000, 50);
    auto delta = tab_data_.last_reload_time_ - tab_data_.last_discard_time_;
    // Capped to one day for now, will adjust if necessary.
    UMA_HISTOGRAM_CUSTOM_TIMES("TabManager.Discarding.DiscardToReloadTime",
                               delta, base::TimeDelta::FromSeconds(1),
                               base::TimeDelta::FromDays(1), 100);

    if (tab_data_.last_inactive_time_ != base::TimeTicks::UnixEpoch()) {
      delta = tab_data_.last_reload_time_ - tab_data_.last_inactive_time_;
      UMA_HISTOGRAM_CUSTOM_TIMES("TabManager.Discarding.InactiveToReloadTime",
                                 delta, base::TimeDelta::FromSeconds(1),
                                 base::TimeDelta::FromDays(1), 100);
    }

  } else if (!tab_data_.is_discarded_ && state) {
    static int discard_count = 0;
    UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.DiscardCount",
                                ++discard_count, 1, 1000, 50);
    tab_data_.last_discard_time_ = NowTicks();
  }

  tab_data_.is_discarded_ = state;
}

int TabManager::WebContentsData::DiscardCount() {
  return tab_data_.discard_count_;
}

void TabManager::WebContentsData::IncrementDiscardCount() {
  tab_data_.discard_count_++;
}

bool TabManager::WebContentsData::IsRecentlyAudible() {
  return tab_data_.is_recently_audible_;
}

void TabManager::WebContentsData::SetRecentlyAudible(bool state) {
  tab_data_.is_recently_audible_ = state;
}

TimeTicks TabManager::WebContentsData::LastAudioChangeTime() {
  return tab_data_.last_audio_change_time_;
}

void TabManager::WebContentsData::SetLastAudioChangeTime(TimeTicks timestamp) {
  tab_data_.last_audio_change_time_ = timestamp;
}

TimeTicks TabManager::WebContentsData::LastInactiveTime() {
  return tab_data_.last_inactive_time_;
}

void TabManager::WebContentsData::SetLastInactiveTime(TimeTicks timestamp) {
  tab_data_.last_inactive_time_ = timestamp;
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

TimeTicks TabManager::WebContentsData::NowTicks() {
  if (!test_tick_clock_)
    return TimeTicks::Now();

  return test_tick_clock_->NowTicks();
}

TabManager::WebContentsData::Data::Data()
    : is_discarded_(false),
      discard_count_(0),
      is_recently_audible_(false),
      last_audio_change_time_(TimeTicks::UnixEpoch()),
      last_discard_time_(TimeTicks::UnixEpoch()),
      last_reload_time_(TimeTicks::UnixEpoch()),
      last_inactive_time_(TimeTicks::UnixEpoch()) {}

bool TabManager::WebContentsData::Data::operator==(const Data& right) const {
  return is_discarded_ == right.is_discarded_ &&
         is_recently_audible_ == right.is_recently_audible_ &&
         last_audio_change_time_ == right.last_audio_change_time_ &&
         last_discard_time_ == right.last_discard_time_ &&
         last_reload_time_ == right.last_reload_time_ &&
         last_inactive_time_ == right.last_inactive_time_;
}

bool TabManager::WebContentsData::Data::operator!=(const Data& right) const {
  return !(*this == right);
}

}  // namespace memory
