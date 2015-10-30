// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager_web_contents_data.h"

#include "base/metrics/histogram.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

using base::TimeTicks;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(memory::TabManager::WebContentsData);

namespace memory {

TabManager::WebContentsData::WebContentsData(content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

TabManager::WebContentsData::~WebContentsData() {}

void TabManager::WebContentsData::DidStartLoading() {
  // Marks the tab as no longer discarded if it has been reloaded from another
  // source (ie: context menu).
  SetDiscardState(false);
}

bool TabManager::WebContentsData::IsDiscarded() {
  return tab_data_.is_discarded_;
}

void TabManager::WebContentsData::SetDiscardState(bool state) {
  if (tab_data_.is_discarded_ && !state) {
    static int reload_count = 0;
    UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.ReloadCount",
                                ++reload_count, 1, 1000, 50);
    auto delta = base::TimeTicks::Now() - tab_data_.last_discard_time_;
    // Capped to one day for now, will adjust if necessary.
    UMA_HISTOGRAM_CUSTOM_TIMES("TabManager.Discarding.DiscardToReloadTime",
                               delta, base::TimeDelta::FromSeconds(1),
                               base::TimeDelta::FromDays(1), 100);
  } else if (!tab_data_.is_discarded_ && state) {
    static int discard_count = 0;
    UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.DiscardCount",
                                ++discard_count, 1, 1000, 50);
    tab_data_.last_discard_time_ = base::TimeTicks::Now();
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

TabManager::WebContentsData::Data::Data()
    : is_discarded_(false),
      discard_count_(0),
      is_recently_audible_(false),
      last_audio_change_time_(TimeTicks::UnixEpoch()) {}

}  // namespace memory
