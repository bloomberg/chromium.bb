// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/revisit/current_tab_matcher.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/sessions/core/serialized_navigation_entry.h"

namespace sync_driver {

CurrentTabMatcher::CurrentTabMatcher(const PageEquality& page_equality)
    : page_equality_(page_equality) {}

void CurrentTabMatcher::Check(const sessions::SessionTab* tab) {
  if (tab->navigations.empty()) {
    return;
  }
  const sessions::SerializedNavigationEntry& currentEntry =
      tab->navigations[tab->normalized_navigation_index()];
  // Cannot rely on SerializedNavigationEntry timestamps, they're
  // not set for foreign sessions. Instead rely on tab timestamps.
  if (page_equality_.IsSamePage(currentEntry.virtual_url()) &&
      (most_recent_match_ == nullptr ||
       tab->timestamp > most_recent_match_->timestamp)) {
    most_recent_match_ = tab;
  }
}

void CurrentTabMatcher::Emit(
    const PageVisitObserver::TransitionType transition) {
  if (most_recent_match_ == nullptr) {
    UMA_HISTOGRAM_ENUMERATION("Sync.PageRevisitTabMissTransition", transition,
                              PageVisitObserver::kTransitionTypeLast);
  } else {
    base::TimeDelta age(base::Time::Now() - most_recent_match_->timestamp);
    UMA_HISTOGRAM_CUSTOM_TIMES("Sync.PageRevisitTabMatchAge", age,
                               base::TimeDelta::FromSeconds(1),
                               base::TimeDelta::FromDays(14), 100);
    UMA_HISTOGRAM_ENUMERATION("Sync.PageRevisitTabMatchTransition", transition,
                              PageVisitObserver::kTransitionTypeLast);
  }
}

}  // namespace sync_driver
