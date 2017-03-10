// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_tracker.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_embedder_interface.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "ui/base/page_transition_types.h"

// This macro invokes the specified method on each observer, passing the
// variable length arguments as the method's arguments, and removes the observer
// from the list of observers if the given method returns STOP_OBSERVING.
#define INVOKE_AND_PRUNE_OBSERVERS(observers, Method, ...)    \
  for (auto it = observers.begin(); it != observers.end();) { \
    if ((*it)->Method(__VA_ARGS__) ==                         \
        PageLoadMetricsObserver::STOP_OBSERVING) {            \
      it = observers.erase(it);                               \
    } else {                                                  \
      ++it;                                                   \
    }                                                         \
  }

namespace page_load_metrics {

namespace internal {

const char kErrorEvents[] = "PageLoad.Internal.ErrorCode";
const char kAbortChainSizeReload[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.Reload";
const char kAbortChainSizeForwardBack[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.ForwardBack";
const char kAbortChainSizeNewNavigation[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.NewNavigation";
const char kAbortChainSizeSameURL[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.SameURL";
const char kAbortChainSizeNoCommit[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.NoCommit";
const char kClientRedirectFirstPaintToNavigation[] =
    "PageLoad.Internal.ClientRedirect.FirstPaintToNavigation";
const char kClientRedirectWithoutPaint[] =
    "PageLoad.Internal.ClientRedirect.NavigationWithoutPaint";
const char kPageLoadCompletedAfterAppBackground[] =
    "PageLoad.Internal.PageLoadCompleted.AfterAppBackground";

}  // namespace internal

void RecordInternalError(InternalErrorLoadEvent event) {
  UMA_HISTOGRAM_ENUMERATION(internal::kErrorEvents, event, ERR_LAST_ENTRY);
}

// TODO(csharrison): Add a case for client side redirects, which is what JS
// initiated window.location / window.history navigations get set to.
PageEndReason EndReasonForPageTransition(ui::PageTransition transition) {
  if (transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
    return END_CLIENT_REDIRECT;
  }
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD))
    return END_RELOAD;
  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK)
    return END_FORWARD_BACK;
  if (ui::PageTransitionIsNewNavigation(transition))
    return END_NEW_NAVIGATION;
  NOTREACHED()
      << "EndReasonForPageTransition received unexpected ui::PageTransition: "
      << transition;
  return END_OTHER;
}

void LogAbortChainSameURLHistogram(int aborted_chain_size_same_url) {
  if (aborted_chain_size_same_url > 0) {
    UMA_HISTOGRAM_COUNTS(internal::kAbortChainSizeSameURL,
                         aborted_chain_size_same_url);
  }
}

bool IsNavigationUserInitiated(content::NavigationHandle* handle) {
  // TODO(crbug.com/617904): Browser initiated navigations should have
  // HasUserGesture() set to true. In the meantime, we consider all
  // browser-initiated navigations to be user initiated.
  //
  // TODO(crbug.com/637345): Some browser-initiated navigations incorrectly
  // report that they are renderer-initiated. We will currently report that
  // these navigations are not user initiated, when in fact they are user
  // initiated.
  return handle->HasUserGesture() || !handle->IsRendererInitiated();
}

namespace {

// Helper to allow use of Optional<> values in LOG() messages.
std::ostream& operator<<(std::ostream& os,
                         const base::Optional<base::TimeDelta>& opt) {
  if (opt)
    os << opt.value();
  else
    os << "(unset)";
  return os;
}

// If second is non-zero, first must also be non-zero and less than or equal to
// second.
bool EventsInOrder(const base::Optional<base::TimeDelta>& first,
                   const base::Optional<base::TimeDelta>& second) {
  if (!second) {
    return true;
  }
  return first && first <= second;
}

bool IsValidPageLoadTiming(const PageLoadTiming& timing) {
  if (timing.IsEmpty())
    return false;

  // If we have a non-empty timing, it should always have a navigation start.
  if (timing.navigation_start.is_null()) {
    NOTREACHED() << "Received null navigation_start.";
    return false;
  }

  // Verify proper ordering between the various timings.

  if (!EventsInOrder(timing.response_start, timing.parse_start)) {
    // We sometimes get a zero response_start with a non-zero parse start. See
    // crbug.com/590212.
    DLOG(ERROR) << "Invalid response_start " << timing.response_start
                << " for parse_start " << timing.parse_start;
    return false;
  }

  if (!EventsInOrder(timing.parse_start, timing.parse_stop)) {
    NOTREACHED() << "Invalid parse_start " << timing.parse_start
                 << " for parse_stop " << timing.parse_stop;
    return false;
  }

  if (timing.parse_stop) {
    const base::TimeDelta parse_duration =
        timing.parse_stop.value() - timing.parse_start.value();
    if (timing.parse_blocked_on_script_load_duration > parse_duration) {
      NOTREACHED() << "Invalid parse_blocked_on_script_load_duration "
                   << timing.parse_blocked_on_script_load_duration
                   << " for parse duration " << parse_duration;
      return false;
    }
    if (timing.parse_blocked_on_script_execution_duration > parse_duration) {
      NOTREACHED() << "Invalid parse_blocked_on_script_execution_duration "
                   << timing.parse_blocked_on_script_execution_duration
                   << " for parse duration " << parse_duration;
      return false;
    }
  }

  if (timing.parse_blocked_on_script_load_from_document_write_duration >
      timing.parse_blocked_on_script_load_duration) {
    NOTREACHED()
        << "Invalid parse_blocked_on_script_load_from_document_write_duration "
        << timing.parse_blocked_on_script_load_from_document_write_duration
        << " for parse_blocked_on_script_load_duration "
        << timing.parse_blocked_on_script_load_duration;
    return false;
  }

  if (timing.parse_blocked_on_script_execution_from_document_write_duration >
      timing.parse_blocked_on_script_execution_duration) {
    NOTREACHED()
        << "Invalid "
           "parse_blocked_on_script_execution_from_document_write_duration "
        << timing.parse_blocked_on_script_execution_from_document_write_duration
        << " for parse_blocked_on_script_execution_duration "
        << timing.parse_blocked_on_script_execution_duration;
    return false;
  }

  if (!EventsInOrder(timing.parse_stop,
                     timing.dom_content_loaded_event_start)) {
    NOTREACHED() << "Invalid parse_stop " << timing.parse_stop
                 << " for dom_content_loaded_event_start "
                 << timing.dom_content_loaded_event_start;
    return false;
  }

  if (!EventsInOrder(timing.dom_content_loaded_event_start,
                     timing.load_event_start)) {
    NOTREACHED() << "Invalid dom_content_loaded_event_start "
                 << timing.dom_content_loaded_event_start
                 << " for load_event_start " << timing.load_event_start;
    return false;
  }

  if (!EventsInOrder(timing.parse_start, timing.first_layout)) {
    NOTREACHED() << "Invalid parse_start " << timing.parse_start
                 << " for first_layout " << timing.first_layout;
    return false;
  }

  if (!EventsInOrder(timing.first_layout, timing.first_paint)) {
    // This can happen when we process an XHTML document that doesn't contain
    // well formed XML. See crbug.com/627607.
    DLOG(ERROR) << "Invalid first_layout " << timing.first_layout
                << " for first_paint " << timing.first_paint;
    return false;
  }

  if (!EventsInOrder(timing.first_paint, timing.first_text_paint)) {
    NOTREACHED() << "Invalid first_paint " << timing.first_paint
                 << " for first_text_paint " << timing.first_text_paint;
    return false;
  }

  if (!EventsInOrder(timing.first_paint, timing.first_image_paint)) {
    NOTREACHED() << "Invalid first_paint " << timing.first_paint
                 << " for first_image_paint " << timing.first_image_paint;
    return false;
  }

  if (!EventsInOrder(timing.first_paint, timing.first_contentful_paint)) {
    NOTREACHED() << "Invalid first_paint " << timing.first_paint
                 << " for first_contentful_paint "
                 << timing.first_contentful_paint;
    return false;
  }

  if (!EventsInOrder(timing.first_paint, timing.first_meaningful_paint)) {
    NOTREACHED() << "Invalid first_paint " << timing.first_paint
                 << " for first_meaningful_paint "
                 << timing.first_meaningful_paint;
    return false;
  }

  return true;
}

void RecordAppBackgroundPageLoadCompleted(bool completed_after_background) {
  UMA_HISTOGRAM_BOOLEAN(internal::kPageLoadCompletedAfterAppBackground,
                        completed_after_background);
}

void DispatchObserverTimingCallbacks(PageLoadMetricsObserver* observer,
                                     const PageLoadTiming& last_timing,
                                     const PageLoadTiming& new_timing,
                                     const PageLoadMetadata& last_metadata,
                                     const PageLoadExtraInfo& extra_info) {
  if (extra_info.main_frame_metadata.behavior_flags !=
      last_metadata.behavior_flags)
    observer->OnLoadingBehaviorObserved(extra_info);
  if (last_timing != new_timing)
    observer->OnTimingUpdate(new_timing, extra_info);
  if (new_timing.dom_content_loaded_event_start &&
      !last_timing.dom_content_loaded_event_start)
    observer->OnDomContentLoadedEventStart(new_timing, extra_info);
  if (new_timing.load_event_start && !last_timing.load_event_start)
    observer->OnLoadEventStart(new_timing, extra_info);
  if (new_timing.first_layout && !last_timing.first_layout)
    observer->OnFirstLayout(new_timing, extra_info);
  if (new_timing.first_paint && !last_timing.first_paint)
    observer->OnFirstPaint(new_timing, extra_info);
  if (new_timing.first_text_paint && !last_timing.first_text_paint)
    observer->OnFirstTextPaint(new_timing, extra_info);
  if (new_timing.first_image_paint && !last_timing.first_image_paint)
    observer->OnFirstImagePaint(new_timing, extra_info);
  if (new_timing.first_contentful_paint && !last_timing.first_contentful_paint)
    observer->OnFirstContentfulPaint(new_timing, extra_info);
  if (new_timing.first_meaningful_paint && !last_timing.first_meaningful_paint)
    observer->OnFirstMeaningfulPaint(new_timing, extra_info);
  if (new_timing.parse_start && !last_timing.parse_start)
    observer->OnParseStart(new_timing, extra_info);
  if (new_timing.parse_stop && !last_timing.parse_stop)
    observer->OnParseStop(new_timing, extra_info);
}

}  // namespace

PageLoadTracker::PageLoadTracker(
    bool in_foreground,
    PageLoadMetricsEmbedderInterface* embedder_interface,
    const GURL& currently_committed_url,
    content::NavigationHandle* navigation_handle,
    UserInitiatedInfo user_initiated_info,
    int aborted_chain_size,
    int aborted_chain_size_same_url)
    : did_stop_tracking_(false),
      app_entered_background_(false),
      navigation_start_(navigation_handle->NavigationStart()),
      url_(navigation_handle->GetURL()),
      start_url_(navigation_handle->GetURL()),
      did_commit_(false),
      page_end_reason_(END_NONE),
      page_end_user_initiated_info_(UserInitiatedInfo::NotUserInitiated()),
      started_in_foreground_(in_foreground),
      page_transition_(navigation_handle->GetPageTransition()),
      user_initiated_info_(user_initiated_info),
      aborted_chain_size_(aborted_chain_size),
      aborted_chain_size_same_url_(aborted_chain_size_same_url),
      embedder_interface_(embedder_interface) {
  DCHECK(!navigation_handle->HasCommitted());
  embedder_interface_->RegisterObservers(this);
  INVOKE_AND_PRUNE_OBSERVERS(observers_, OnStart, navigation_handle,
                             currently_committed_url, started_in_foreground_);
}

PageLoadTracker::~PageLoadTracker() {
  if (app_entered_background_) {
    RecordAppBackgroundPageLoadCompleted(true);
  }

  if (did_stop_tracking_)
    return;

  if (page_end_time_.is_null()) {
    RecordInternalError(ERR_NO_PAGE_LOAD_END_TIME);
    page_end_time_ = base::TimeTicks::Now();
  }

  if (!did_commit_) {
    if (!failed_provisional_load_info_)
      RecordInternalError(ERR_NO_COMMIT_OR_FAILED_PROVISIONAL_LOAD);

    // Don't include any aborts that resulted in a new navigation, as the chain
    // length will be included in the aborter PageLoadTracker.
    if (page_end_reason_ != END_RELOAD &&
        page_end_reason_ != END_FORWARD_BACK &&
        page_end_reason_ != END_NEW_NAVIGATION) {
      LogAbortChainHistograms(nullptr);
    }
  } else if (timing_.IsEmpty()) {
    RecordInternalError(ERR_NO_IPCS_RECEIVED);
  }

  const PageLoadExtraInfo info = ComputePageLoadExtraInfo();
  for (const auto& observer : observers_) {
    if (failed_provisional_load_info_) {
      observer->OnFailedProvisionalLoad(*failed_provisional_load_info_, info);
    } else if (did_commit_) {
      observer->OnComplete(timing_, info);
    }
  }
}

void PageLoadTracker::LogAbortChainHistograms(
    content::NavigationHandle* final_navigation) {
  if (aborted_chain_size_ == 0)
    return;
  // Note that this could be broken out by this navigation's abort type, if more
  // granularity is needed. Add one to the chain size to count the current
  // navigation. In the other cases, the current navigation is the final
  // navigation (which commits).
  if (!final_navigation) {
    UMA_HISTOGRAM_COUNTS(internal::kAbortChainSizeNoCommit,
                         aborted_chain_size_ + 1);
    LogAbortChainSameURLHistogram(aborted_chain_size_same_url_ + 1);
    return;
  }

  // The following is only executed for committing trackers.
  DCHECK(did_commit_);

  // Note that histograms could be separated out by this commit's transition
  // type, but for simplicity they will all be bucketed together.
  LogAbortChainSameURLHistogram(aborted_chain_size_same_url_);

  ui::PageTransition committed_transition =
      final_navigation->GetPageTransition();
  switch (EndReasonForPageTransition(committed_transition)) {
    case END_RELOAD:
      UMA_HISTOGRAM_COUNTS(internal::kAbortChainSizeReload,
                           aborted_chain_size_);
      return;
    case END_FORWARD_BACK:
      UMA_HISTOGRAM_COUNTS(internal::kAbortChainSizeForwardBack,
                           aborted_chain_size_);
      return;
    // TODO(csharrison): Refactor this code so it is based on the WillStart*
    // code path instead of the committed load code path. Then, for every abort
    // chain, log a histogram of the counts of each of these metrics. For now,
    // merge client redirects with new navigations, which was (basically) the
    // previous behavior.
    case END_CLIENT_REDIRECT:
    case END_NEW_NAVIGATION:
      UMA_HISTOGRAM_COUNTS(internal::kAbortChainSizeNewNavigation,
                           aborted_chain_size_);
      return;
    default:
      NOTREACHED()
          << "LogAbortChainHistograms received unexpected ui::PageTransition: "
          << committed_transition;
      return;
  }
}

void PageLoadTracker::WebContentsHidden() {
  // Only log the first time we background in a given page load.
  if (background_time_.is_null()) {
    // Make sure we either started in the foreground and haven't been
    // foregrounded yet, or started in the background and have already been
    // foregrounded.
    DCHECK_EQ(started_in_foreground_, foreground_time_.is_null());
    background_time_ = base::TimeTicks::Now();
    ClampBrowserTimestampIfInterProcessTimeTickSkew(&background_time_);
  }
  const PageLoadExtraInfo info = ComputePageLoadExtraInfo();
  INVOKE_AND_PRUNE_OBSERVERS(observers_, OnHidden, timing_, info);
}

void PageLoadTracker::WebContentsShown() {
  // Only log the first time we foreground in a given page load.
  if (foreground_time_.is_null()) {
    // Make sure we either started in the background and haven't been
    // backgrounded yet, or started in the foreground and have already been
    // backgrounded.
    DCHECK_NE(started_in_foreground_, background_time_.is_null());
    foreground_time_ = base::TimeTicks::Now();
    ClampBrowserTimestampIfInterProcessTimeTickSkew(&foreground_time_);
  }

  INVOKE_AND_PRUNE_OBSERVERS(observers_, OnShown);
}

void PageLoadTracker::WillProcessNavigationResponse(
    content::NavigationHandle* navigation_handle) {
  // PlzNavigate: NavigationHandle::GetGlobalRequestID() sometimes returns an
  // uninitialized GlobalRequestID. Bail early in this case. See
  // crbug.com/680841 for details.
  if (content::IsBrowserSideNavigationEnabled() &&
      navigation_handle->GetGlobalRequestID() == content::GlobalRequestID())
    return;

  DCHECK(!navigation_request_id_.has_value());
  navigation_request_id_ = navigation_handle->GetGlobalRequestID();
  DCHECK(navigation_request_id_.value() != content::GlobalRequestID());
}

void PageLoadTracker::Commit(content::NavigationHandle* navigation_handle) {
  did_commit_ = true;
  url_ = navigation_handle->GetURL();
  // Some transitions (like CLIENT_REDIRECT) are only known at commit time.
  page_transition_ = navigation_handle->GetPageTransition();
  user_initiated_info_.user_gesture = navigation_handle->HasUserGesture();

  INVOKE_AND_PRUNE_OBSERVERS(
      observers_, ShouldObserveMimeType,
      navigation_handle->GetWebContents()->GetContentsMimeType());

  INVOKE_AND_PRUNE_OBSERVERS(observers_, OnCommit, navigation_handle);
  LogAbortChainHistograms(navigation_handle);
}

void PageLoadTracker::FailedProvisionalLoad(
    content::NavigationHandle* navigation_handle,
    base::TimeTicks failed_load_time) {
  DCHECK(!failed_provisional_load_info_);
  failed_provisional_load_info_.reset(new FailedProvisionalLoadInfo(
      failed_load_time - navigation_handle->NavigationStart(),
      navigation_handle->GetNetErrorCode()));
}

void PageLoadTracker::Redirect(content::NavigationHandle* navigation_handle) {
  url_ = navigation_handle->GetURL();
  INVOKE_AND_PRUNE_OBSERVERS(observers_, OnRedirect, navigation_handle);
}

void PageLoadTracker::OnInputEvent(const blink::WebInputEvent& event) {
  input_tracker_.OnInputEvent(event);
  for (const auto& observer : observers_) {
    observer->OnUserInput(event);
  }
}

void PageLoadTracker::FlushMetricsOnAppEnterBackground() {
  if (!app_entered_background_) {
    RecordAppBackgroundPageLoadCompleted(false);
    app_entered_background_ = true;
  }

  const PageLoadExtraInfo info = ComputePageLoadExtraInfo();
  INVOKE_AND_PRUNE_OBSERVERS(observers_, FlushMetricsOnAppEnterBackground,
                             timing_, info);
}

void PageLoadTracker::NotifyClientRedirectTo(
    const PageLoadTracker& destination) {
  if (timing_.first_paint) {
    base::TimeTicks first_paint_time =
        navigation_start() + timing_.first_paint.value();
    base::TimeDelta first_paint_to_navigation;
    if (destination.navigation_start() > first_paint_time)
      first_paint_to_navigation =
          destination.navigation_start() - first_paint_time;
    PAGE_LOAD_HISTOGRAM(internal::kClientRedirectFirstPaintToNavigation,
                        first_paint_to_navigation);
  } else {
    UMA_HISTOGRAM_BOOLEAN(internal::kClientRedirectWithoutPaint, true);
  }
}

void PageLoadTracker::UpdateChildFrameMetadata(
    const PageLoadMetadata& child_metadata) {
  // Merge the child loading behavior flags with any we've already observed,
  // possibly from other child frames.
  const int last_child_loading_behavior_flags =
      child_frame_metadata_.behavior_flags;
  child_frame_metadata_.behavior_flags |= child_metadata.behavior_flags;
  if (last_child_loading_behavior_flags == child_frame_metadata_.behavior_flags)
    return;

  PageLoadExtraInfo extra_info(ComputePageLoadExtraInfo());
  for (const auto& observer : observers_) {
    observer->OnLoadingBehaviorObserved(extra_info);
  }
}

bool PageLoadTracker::UpdateTiming(const PageLoadTiming& new_timing,
                                   const PageLoadMetadata& new_metadata) {
  // Throw away IPCs that are not relevant to the current navigation.
  // Two timing structures cannot refer to the same navigation if they indicate
  // that a navigation started at different times, so a new timing struct with a
  // different start time from an earlier struct is considered invalid.
  bool valid_timing_descendent =
      timing_.navigation_start.is_null() ||
      timing_.navigation_start == new_timing.navigation_start;
  // Ensure flags sent previously are still present in the new metadata fields.
  bool valid_behavior_descendent =
      (main_frame_metadata_.behavior_flags & new_metadata.behavior_flags) ==
      main_frame_metadata_.behavior_flags;
  if (IsValidPageLoadTiming(new_timing) && valid_timing_descendent &&
      valid_behavior_descendent) {
    DCHECK(did_commit_);  // OnCommit() must be called first.
    // There are some subtle ordering constraints here. GetPageLoadMetricsInfo()
    // must be called before DispatchObserverTimingCallbacks, but its
    // implementation depends on the state of main_frame_metadata_, so we need
    // to update main_frame_metadata_ before calling GetPageLoadMetricsInfo.
    // Thus, we make a copy of timing here, update timing_ and
    // main_frame_metadata_, and then proceed to dispatch the observer timing
    // callbacks.
    const PageLoadTiming last_timing = timing_;
    timing_ = new_timing;

    const PageLoadMetadata last_metadata = main_frame_metadata_;
    main_frame_metadata_ = new_metadata;
    const PageLoadExtraInfo info = ComputePageLoadExtraInfo();
    for (const auto& observer : observers_) {
      DispatchObserverTimingCallbacks(observer.get(), last_timing, new_timing,
                                      last_metadata, info);
    }
    return true;
  }
  return false;
}

void PageLoadTracker::OnLoadedResource(
    const ExtraRequestInfo& extra_request_info) {
  for (const auto& observer : observers_) {
    observer->OnLoadedResource(extra_request_info);
  }
}

void PageLoadTracker::StopTracking() {
  did_stop_tracking_ = true;
  observers_.clear();
}

void PageLoadTracker::AddObserver(
    std::unique_ptr<PageLoadMetricsObserver> observer) {
  observers_.push_back(std::move(observer));
}

void PageLoadTracker::ClampBrowserTimestampIfInterProcessTimeTickSkew(
    base::TimeTicks* event_time) {
  DCHECK(event_time != nullptr);
  // Windows 10 GCE bot non-deterministically failed because TimeTicks::Now()
  // called in the browser process e.g. commit_time was less than
  // navigation_start_ that was populated in the renderer process because the
  // clock was not system-wide monotonic.
  // Note that navigation_start_ can also be set in the browser process in
  // some cases and in those cases event_time should never be <
  // navigation_start_. If it is due to a code error and it gets clamped in this
  // function, on high resolution systems it should lead to a dcheck failure.

  // TODO(shivanisha): Currently IsHighResolution is the best way to check
  // if the clock is system-wide monotonic. However IsHighResolution
  // does a broader check to see if the clock in use is high resolution
  // which also implies it is system-wide monotonic (on Windows).
  if (base::TimeTicks::IsHighResolution()) {
    DCHECK(event_time->is_null() || *event_time >= navigation_start_);
    return;
  }

  if (!event_time->is_null() && *event_time < navigation_start_) {
    RecordInternalError(ERR_INTER_PROCESS_TIME_TICK_SKEW);
    *event_time = navigation_start_;
  }
}

PageLoadExtraInfo PageLoadTracker::ComputePageLoadExtraInfo() {
  base::Optional<base::TimeDelta> first_background_time;
  base::Optional<base::TimeDelta> first_foreground_time;
  base::Optional<base::TimeDelta> page_end_time;

  if (!background_time_.is_null()) {
    DCHECK_GE(background_time_, navigation_start_);
    first_background_time = background_time_ - navigation_start_;
  }

  if (!foreground_time_.is_null()) {
    DCHECK_GE(foreground_time_, navigation_start_);
    first_foreground_time = foreground_time_ - navigation_start_;
  }

  if (page_end_reason_ != END_NONE) {
    DCHECK_GE(page_end_time_, navigation_start_);
    page_end_time = page_end_time_ - navigation_start_;
  } else {
    DCHECK(page_end_time_.is_null());
  }

  // page_end_reason_ == END_NONE implies page_end_user_initiated_info_ is not
  // user initiated.
  DCHECK(page_end_reason_ != END_NONE ||
         (!page_end_user_initiated_info_.browser_initiated &&
          !page_end_user_initiated_info_.user_gesture &&
          !page_end_user_initiated_info_.user_input_event));
  return PageLoadExtraInfo(
      navigation_start_, first_background_time, first_foreground_time,
      started_in_foreground_, user_initiated_info_, url(), start_url_,
      did_commit_, page_end_reason_, page_end_user_initiated_info_,
      page_end_time, main_frame_metadata_, child_frame_metadata_);
}

bool PageLoadTracker::HasMatchingNavigationRequestID(
    const content::GlobalRequestID& request_id) const {
  DCHECK(request_id != content::GlobalRequestID());
  return navigation_request_id_.has_value() &&
         navigation_request_id_.value() == request_id;
}

void PageLoadTracker::NotifyPageEnd(PageEndReason page_end_reason,
                                    UserInitiatedInfo user_initiated_info,
                                    base::TimeTicks timestamp,
                                    bool is_certainly_browser_timestamp) {
  DCHECK_NE(page_end_reason, END_NONE);
  // Use UpdatePageEnd to update an already notified PageLoadTracker.
  if (page_end_reason_ != END_NONE)
    return;

  UpdatePageEndInternal(page_end_reason, user_initiated_info, timestamp,
                        is_certainly_browser_timestamp);
}

void PageLoadTracker::UpdatePageEnd(PageEndReason page_end_reason,
                                    UserInitiatedInfo user_initiated_info,
                                    base::TimeTicks timestamp,
                                    bool is_certainly_browser_timestamp) {
  DCHECK_NE(page_end_reason, END_NONE);
  DCHECK_NE(page_end_reason, END_OTHER);
  DCHECK_EQ(page_end_reason_, END_OTHER);
  DCHECK(!page_end_time_.is_null());
  if (page_end_time_.is_null() || page_end_reason_ != END_OTHER)
    return;

  // For some aborts (e.g. navigations), the initiated timestamp can be earlier
  // than the timestamp that aborted the load. Taking the minimum gives the
  // closest user initiated time known.
  UpdatePageEndInternal(page_end_reason, user_initiated_info,
                        std::min(page_end_time_, timestamp),
                        is_certainly_browser_timestamp);
}

bool PageLoadTracker::IsLikelyProvisionalAbort(
    base::TimeTicks abort_cause_time) const {
  // Note that |abort_cause_time - page_end_time_| can be negative.
  return page_end_reason_ == END_OTHER &&
         (abort_cause_time - page_end_time_).InMilliseconds() < 100;
}

bool PageLoadTracker::MatchesOriginalNavigation(
    content::NavigationHandle* navigation_handle) {
  // Neither navigation should have committed.
  DCHECK(!navigation_handle->HasCommitted());
  DCHECK(!did_commit_);
  return navigation_handle->GetURL() == start_url_;
}

void PageLoadTracker::UpdatePageEndInternal(
    PageEndReason page_end_reason,
    UserInitiatedInfo user_initiated_info,
    base::TimeTicks timestamp,
    bool is_certainly_browser_timestamp) {
  // When a provisional navigation commits, that navigation's start time is
  // interpreted as the abort time for other provisional loads in the tab.
  // However, this only makes sense if the committed load started after the
  // aborted provisional loads started. Thus we ignore cases where the committed
  // load started before the aborted provisional load, as this would result in
  // recording a negative time-to-abort. The real issue here is that we have to
  // infer the cause of aborts. It would be better if the navigation code could
  // instead report the actual cause of an aborted navigation. See crbug/571647
  // for details.
  if (timestamp < navigation_start_) {
    RecordInternalError(ERR_END_BEFORE_NAVIGATION_START);
    page_end_reason_ = END_NONE;
    page_end_time_ = base::TimeTicks();
    return;
  }
  page_end_reason_ = page_end_reason;
  page_end_time_ = timestamp;
  // A client redirect can never be user initiated. Due to the way Blink
  // implements user gesture tracking, where all events that occur within 1
  // second after a user interaction are considered to be triggered by user
  // activation (based on HTML spec:
  // https://html.spec.whatwg.org/multipage/interaction.html#triggered-by-user-activation),
  // these navs may sometimes be reported as user initiated by Blink. Thus, we
  // explicitly filter these types of aborts out when deciding if the abort was
  // user initiated.
  if (page_end_reason != END_CLIENT_REDIRECT)
    page_end_user_initiated_info_ = user_initiated_info;

  if (is_certainly_browser_timestamp) {
    ClampBrowserTimestampIfInterProcessTimeTickSkew(&page_end_time_);
  }
}

}  // namespace page_load_metrics
