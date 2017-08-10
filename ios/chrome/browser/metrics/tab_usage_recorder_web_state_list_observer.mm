// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/tab_usage_recorder_web_state_list_observer.h"

#include "base/logging.h"
#include "ios/chrome/browser/metrics/tab_usage_recorder.h"
#include "ios/chrome/browser/tabs/legacy_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabUsageRecorderWebStateListObserver::TabUsageRecorderWebStateListObserver(
    TabUsageRecorder* tab_usage_recorder)
    : tab_usage_recorder_(tab_usage_recorder) {
  DCHECK(tab_usage_recorder_);
}

TabUsageRecorderWebStateListObserver::~TabUsageRecorderWebStateListObserver() =
    default;

void TabUsageRecorderWebStateListObserver::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  if (!activating)
    return;

  Tab* tab = LegacyTabHelper::GetTabForWebState(web_state);
  tab_usage_recorder_->TabCreatedForSelection(tab);
}

void TabUsageRecorderWebStateListObserver::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    bool user_action) {
  if (!user_action)
    return;

  tab_usage_recorder_->RecordTabSwitched(
      old_web_state ? LegacyTabHelper::GetTabForWebState(old_web_state) : nil,
      new_web_state ? LegacyTabHelper::GetTabForWebState(new_web_state) : nil);
}
