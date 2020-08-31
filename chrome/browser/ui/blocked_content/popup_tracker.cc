// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_tracker.h"

#include <algorithm>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {

int CappedUserInteractions(int user_interactions, int max_interactions) {
  return std::min(user_interactions, max_interactions);
}

}  // namespace

PopupTracker* PopupTracker::CreateForWebContents(
    content::WebContents* contents,
    content::WebContents* opener,
    WindowOpenDisposition disposition) {
  DCHECK(contents);
  DCHECK(opener);
  auto* tracker = FromWebContents(contents);
  if (!tracker) {
    tracker = new PopupTracker(contents, opener, disposition);
    contents->SetUserData(UserDataKey(), base::WrapUnique(tracker));
  }
  return tracker;
}

PopupTracker::~PopupTracker() = default;

PopupTracker::PopupTracker(content::WebContents* contents,
                           content::WebContents* opener,
                           WindowOpenDisposition disposition)
    : content::WebContentsObserver(contents),
      scoped_observer_(this),
      visibility_tracker_(
          base::DefaultTickClock::GetInstance(),
          contents->GetVisibility() != content::Visibility::HIDDEN),
      opener_source_id_(ukm::GetSourceIdForWebContentsDocument(opener)),
      window_open_disposition_(disposition) {
  if (auto* popup_opener = PopupOpenerTabHelper::FromWebContents(opener))
    popup_opener->OnOpenedPopup(this);

  auto* observer_manager =
      subresource_filter::SubresourceFilterObserverManager::FromWebContents(
          contents);
  if (observer_manager) {
    scoped_observer_.Add(observer_manager);
  }
}

void PopupTracker::WebContentsDestroyed() {
  base::TimeDelta total_foreground_duration =
      visibility_tracker_.GetForegroundDuration();
  if (first_load_visible_time_start_) {
    base::TimeDelta first_load_visible_time =
        first_load_visible_time_
            ? *first_load_visible_time_
            : total_foreground_duration - *first_load_visible_time_start_;
    UMA_HISTOGRAM_LONG_TIMES(
        "ContentSettings.Popups.FirstDocumentEngagementTime2",
        first_load_visible_time);
  }
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "ContentSettings.Popups.EngagementTime", total_foreground_duration,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(6), 50);
  if (web_contents()->GetClosedByUserGesture()) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "ContentSettings.Popups.EngagementTime.GestureClose",
        total_foreground_duration, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromHours(6), 50);
  }

  if (opener_source_id_ != ukm::kInvalidSourceId) {
    const int kMaxInteractions = 100;
    const int kMaxSubcatagoryInteractions = 50;
    ukm::builders::Popup_Closed(opener_source_id_)
        .SetEngagementTime(ukm::GetExponentialBucketMinForUserTiming(
            total_foreground_duration.InMilliseconds()))
        .SetUserInitiatedClose(web_contents()->GetClosedByUserGesture())
        .SetTrusted(is_trusted_)
        .SetSafeBrowsingStatus(static_cast<int>(safe_browsing_status_))
        .SetWindowOpenDisposition(static_cast<int>(window_open_disposition_))
        .SetNumInteractions(
            CappedUserInteractions(num_interactions_, kMaxInteractions))
        .SetNumActivationInteractions(CappedUserInteractions(
            num_activation_events_, kMaxSubcatagoryInteractions))
        .SetNumGestureScrollBeginInteractions(CappedUserInteractions(
            num_gesture_scroll_begin_events_, kMaxSubcatagoryInteractions))
        .Record(ukm::UkmRecorder::Get());
  }
}

void PopupTracker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!first_load_visible_time_start_) {
    first_load_visible_time_start_ =
        visibility_tracker_.GetForegroundDuration();
  } else if (!first_load_visible_time_) {
    first_load_visible_time_ = visibility_tracker_.GetForegroundDuration() -
                               *first_load_visible_time_start_;
  }
}

void PopupTracker::OnVisibilityChanged(content::Visibility visibility) {
  // TODO(csharrison): Consider handling OCCLUDED tabs the same way as HIDDEN
  // tabs.
  if (visibility == content::Visibility::HIDDEN)
    visibility_tracker_.OnHidden();
  else
    visibility_tracker_.OnShown();
}

void PopupTracker::DidGetUserInteraction(
    const blink::WebInputEvent::Type type) {
  // TODO(csharrison): It would be nice if ctrl-W could be filtered out here,
  // but the initial ctrl key press is registered as a kRawKeyDown.
  num_interactions_++;

  if (type == blink::WebInputEvent::Type::kGestureScrollBegin) {
    num_gesture_scroll_begin_events_++;
  } else {
    num_activation_events_++;
  }
}

// This method will always be called before the DidFinishNavigation associated
// with this handle.
// The exception is a navigation restoring a page from back-forward cache --
// in that case don't issue any requests, therefore we don't get any
// safe browsing callbacks. See the comment above for the mitigation.
void PopupTracker::OnSafeBrowsingChecksComplete(
    content::NavigationHandle* navigation_handle,
    const subresource_filter::SubresourceFilterSafeBrowsingClient::CheckResult&
        result) {
  DCHECK(navigation_handle->IsInMainFrame());
  safe_browsing_status_ = PopupSafeBrowsingStatus::kSafe;
  if (result.threat_type ==
          safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING ||
      result.threat_type == safe_browsing::SBThreatType::
                                SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING ||
      result.threat_type ==
          safe_browsing::SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER) {
    safe_browsing_status_ = PopupSafeBrowsingStatus::kUnsafe;
  }
}

void PopupTracker::OnSubresourceFilterGoingAway() {
  scoped_observer_.RemoveAll();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PopupTracker)
