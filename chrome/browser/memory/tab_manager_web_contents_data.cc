// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager_web_contents_data.h"

#include "base/metrics/histogram.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

using base::TimeTicks;
using content::WebContents;

namespace {

const char kDiscardStateKey[] = "WebContentsData";

}  // namespace

namespace memory {

// static
TabManager::WebContentsData* TabManager::WebContentsData::Get(
    WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TabManager::WebContentsData* discard_state = static_cast<WebContentsData*>(
      web_contents->GetUserData(&kDiscardStateKey));

  // If this function is called, we probably need to query/change the discard
  // state. Let's go ahead a add one.
  if (!discard_state) {
    discard_state = new WebContentsData;
    web_contents->SetUserData(&kDiscardStateKey, discard_state);
  }

  return discard_state;
}

// static
void TabManager::WebContentsData::Set(WebContents* web_contents,
                                      WebContentsData* state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  web_contents->SetUserData(&kDiscardStateKey, state);
}

// static
void TabManager::WebContentsData::CopyState(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  WebContentsData* old_state = Get(old_contents);
  WebContentsData* new_State = Get(new_contents);
  *new_State = *old_state;
}

// static
bool TabManager::WebContentsData::IsDiscarded(WebContents* web_contents) {
  return TabManager::WebContentsData::Get(web_contents)->is_discarded_;
}

// static
void TabManager::WebContentsData::SetDiscardState(WebContents* web_contents,
                                                  bool state) {
  WebContentsData* discard_state =
      TabManager::WebContentsData::Get(web_contents);
  if (discard_state->is_discarded_ && !state) {
    static int reload_count = 0;
    UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.ReloadCount",
                                ++reload_count, 1, 1000, 50);
    auto delta = base::TimeTicks::Now() - discard_state->last_discard_time_;
    // Capped to one day for now, will adjust if necessary.
    UMA_HISTOGRAM_CUSTOM_TIMES("TabManager.Discarding.DiscardToReloadTime",
                               delta, base::TimeDelta::FromSeconds(1),
                               base::TimeDelta::FromDays(1), 100);
  } else if (!discard_state->is_discarded_ && state) {
    static int discard_count = 0;
    UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.DiscardCount",
                                ++discard_count, 1, 1000, 50);
    discard_state->last_discard_time_ = base::TimeTicks::Now();
  }

  discard_state->is_discarded_ = state;
}

// static
int TabManager::WebContentsData::DiscardCount(WebContents* web_contents) {
  return TabManager::WebContentsData::Get(web_contents)->discard_count_;
}

// static
void TabManager::WebContentsData::IncrementDiscardCount(
    WebContents* web_contents) {
  TabManager::WebContentsData::Get(web_contents)->discard_count_++;
}

// static
bool TabManager::WebContentsData::IsRecentlyAudible(
    content::WebContents* web_contents) {
  return TabManager::WebContentsData::Get(web_contents)->is_recently_audible_;
}

// static
void TabManager::WebContentsData::SetRecentlyAudible(
    content::WebContents* web_contents,
    bool state) {
  TabManager::WebContentsData::Get(web_contents)->is_recently_audible_ = state;
}

// static
TimeTicks TabManager::WebContentsData::LastAudioChangeTime(
    content::WebContents* web_contents) {
  return TabManager::WebContentsData::Get(web_contents)
      ->last_audio_change_time_;
}

// static
void TabManager::WebContentsData::SetLastAudioChangeTime(
    content::WebContents* web_contents,
    TimeTicks timestamp) {
  TabManager::WebContentsData::Get(web_contents)->last_audio_change_time_ =
      timestamp;
}

TabManager::WebContentsData::WebContentsData()
    : is_discarded_(false),
      discard_count_(0),
      is_recently_audible_(false),
      last_audio_change_time_(TimeTicks::UnixEpoch()) {}

}  // namespace memory
