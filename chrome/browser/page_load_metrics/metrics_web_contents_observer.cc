// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/common/page_load_metrics/page_load_metrics_messages.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/page_transition_types.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    page_load_metrics::MetricsWebContentsObserver);

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
const char kCommitToCompleteNoTimingIPCs[] =
    "PageLoad.Internal.CommitToComplete.NoTimingIPCs";
const char kPageLoadCompletedAfterAppBackground[] =
    "PageLoad.Internal.PageLoadCompleted.AfterAppBackground";

}  // namespace internal

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

void RecordInternalError(InternalErrorLoadEvent event) {
  UMA_HISTOGRAM_ENUMERATION(internal::kErrorEvents, event, ERR_LAST_ENTRY);
}

void RecordAppBackgroundPageLoadCompleted(bool completed_after_background) {
  UMA_HISTOGRAM_BOOLEAN(internal::kPageLoadCompletedAfterAppBackground,
                        completed_after_background);
}

// TODO(csharrison): Add a case for client side redirects, which is what JS
// initiated window.location / window.history navigations get set to.
UserAbortType AbortTypeForPageTransition(ui::PageTransition transition) {
  if (transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
    return ABORT_CLIENT_REDIRECT;
  }
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD))
    return ABORT_RELOAD;
  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK)
    return ABORT_FORWARD_BACK;
  if (ui::PageTransitionIsNewNavigation(transition))
    return ABORT_NEW_NAVIGATION;
  NOTREACHED()
      << "AbortTypeForPageTransition received unexpected ui::PageTransition: "
      << transition;
  return ABORT_OTHER;
}

void LogAbortChainSameURLHistogram(int aborted_chain_size_same_url) {
  if (aborted_chain_size_same_url > 0) {
    UMA_HISTOGRAM_COUNTS(internal::kAbortChainSizeSameURL,
                         aborted_chain_size_same_url);
  }
}

void DispatchObserverTimingCallbacks(PageLoadMetricsObserver* observer,
                                     const PageLoadTiming& last_timing,
                                     const PageLoadTiming& new_timing,
                                     const PageLoadMetadata& last_metadata,
                                     const PageLoadExtraInfo& extra_info) {
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
  if (extra_info.metadata.behavior_flags != last_metadata.behavior_flags)
    observer->OnLoadingBehaviorObserved(extra_info);
}

// TODO(crbug.com/617904): Browser initiated navigations should have
// HasUserGesture() set to true. Update this once we get enough data from just
// renderer initiated aborts.
bool IsNavigationUserInitiated(content::NavigationHandle* handle) {
  return handle->HasUserGesture();
}

}  // namespace

PageLoadTracker::PageLoadTracker(
    bool in_foreground,
    PageLoadMetricsEmbedderInterface* embedder_interface,
    const GURL& currently_committed_url,
    content::NavigationHandle* navigation_handle,
    int aborted_chain_size,
    int aborted_chain_size_same_url)
    : did_stop_tracking_(false),
      app_entered_background_(false),
      navigation_start_(navigation_handle->NavigationStart()),
      url_(navigation_handle->GetURL()),
      abort_type_(ABORT_NONE),
      abort_user_initiated_(false),
      started_in_foreground_(in_foreground),
      page_transition_(navigation_handle->GetPageTransition()),
      num_cache_requests_(0),
      num_network_requests_(0),
      user_gesture_(IsNavigationUserInitiated(navigation_handle)),
      aborted_chain_size_(aborted_chain_size),
      aborted_chain_size_same_url_(aborted_chain_size_same_url),
      embedder_interface_(embedder_interface) {
  DCHECK(!navigation_handle->HasCommitted());
  embedder_interface_->RegisterObservers(this);
  for (const auto& observer : observers_) {
    observer->OnStart(navigation_handle, currently_committed_url,
                      started_in_foreground_);
  }
}

PageLoadTracker::~PageLoadTracker() {
  if (app_entered_background_) {
    RecordAppBackgroundPageLoadCompleted(true);
  }

  if (did_stop_tracking_)
    return;

  if (commit_time_.is_null()) {
    if (!failed_provisional_load_info_)
      RecordInternalError(ERR_NO_COMMIT_OR_FAILED_PROVISIONAL_LOAD);

    // Don't include any aborts that resulted in a new navigation, as the chain
    // length will be included in the aborter PageLoadTracker.
    if (abort_type_ != ABORT_RELOAD && abort_type_ != ABORT_FORWARD_BACK &&
        abort_type_ != ABORT_NEW_NAVIGATION) {
      LogAbortChainHistograms(nullptr);
    }
  } else if (timing_.IsEmpty()) {
    RecordInternalError(ERR_NO_IPCS_RECEIVED);
    PAGE_LOAD_HISTOGRAM(internal::kCommitToCompleteNoTimingIPCs,
                        base::TimeTicks::Now() - commit_time_);
  }

  const PageLoadExtraInfo info = ComputePageLoadExtraInfo();
  for (const auto& observer : observers_) {
    if (failed_provisional_load_info_) {
      observer->OnFailedProvisionalLoad(*failed_provisional_load_info_, info);
    } else if (info.time_to_commit) {
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
  DCHECK(!commit_time_.is_null());

  // Note that histograms could be separated out by this commit's transition
  // type, but for simplicity they will all be bucketed together.
  LogAbortChainSameURLHistogram(aborted_chain_size_same_url_);

  ui::PageTransition committed_transition =
      final_navigation->GetPageTransition();
  switch (AbortTypeForPageTransition(committed_transition)) {
    case ABORT_RELOAD:
      UMA_HISTOGRAM_COUNTS(internal::kAbortChainSizeReload,
                           aborted_chain_size_);
      return;
    case ABORT_FORWARD_BACK:
      UMA_HISTOGRAM_COUNTS(internal::kAbortChainSizeForwardBack,
                           aborted_chain_size_);
      return;
    // TODO(csharrison): Refactor this code so it is based on the WillStart*
    // code path instead of the committed load code path. Then, for every abort
    // chain, log a histogram of the counts of each of these metrics. For now,
    // merge client redirects with new navigations, which was (basically) the
    // previous behavior.
    case ABORT_CLIENT_REDIRECT:
    case ABORT_NEW_NAVIGATION:
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

  for (const auto& observer : observers_)
    observer->OnHidden();
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

  for (const auto& observer : observers_)
    observer->OnShown();
}

void PageLoadTracker::Commit(content::NavigationHandle* navigation_handle) {
  // TODO(bmcquade): To improve accuracy, consider adding commit time to
  // NavigationHandle. Taking a timestamp here should be close enough for now.
  commit_time_ = base::TimeTicks::Now();
  ClampBrowserTimestampIfInterProcessTimeTickSkew(&commit_time_);
  url_ = navigation_handle->GetURL();
  // Some transitions (like CLIENT_REDIRECT) are only known at commit time.
  page_transition_ = navigation_handle->GetPageTransition();
  user_gesture_ = navigation_handle->HasUserGesture();
  for (const auto& observer : observers_) {
    observer->OnCommit(navigation_handle);
  }
  LogAbortChainHistograms(navigation_handle);
}

void PageLoadTracker::FailedProvisionalLoad(
    content::NavigationHandle* navigation_handle) {
  DCHECK(!failed_provisional_load_info_);
  failed_provisional_load_info_.reset(new FailedProvisionalLoadInfo(
      base::TimeTicks::Now() - navigation_handle->NavigationStart(),
      navigation_handle->GetNetErrorCode()));
}

void PageLoadTracker::Redirect(content::NavigationHandle* navigation_handle) {
  for (const auto& observer : observers_) {
    observer->OnRedirect(navigation_handle);
  }
}

void PageLoadTracker::OnInputEvent(const blink::WebInputEvent& event) {
  for (const auto& observer : observers_) {
    observer->OnUserInput(event);
  }
}

void PageLoadTracker::FlushMetricsOnAppEnterBackground() {
  if (!app_entered_background_) {
    RecordAppBackgroundPageLoadCompleted(false);
    app_entered_background_ = true;
  }
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
      (metadata_.behavior_flags & new_metadata.behavior_flags) ==
      metadata_.behavior_flags;
  if (IsValidPageLoadTiming(new_timing) && valid_timing_descendent &&
      valid_behavior_descendent) {
    // There are some subtle ordering constraints here. GetPageLoadMetricsInfo()
    // must be called before DispatchObserverTimingCallbacks, but its
    // implementation depends on the state of metadata_, so we need to update
    // metadata_ before calling GetPageLoadMetricsInfo. Thus, we make a copy of
    // timing here, update timing_ and metadata_, and then proceed to dispatch
    // the observer timing callbacks.
    const PageLoadTiming last_timing = timing_;
    timing_ = new_timing;

    const PageLoadMetadata last_metadata = metadata_;
    metadata_ = new_metadata;
    const PageLoadExtraInfo info = ComputePageLoadExtraInfo();
    for (const auto& observer : observers_) {
      DispatchObserverTimingCallbacks(observer.get(), last_timing, new_timing,
                                      last_metadata, info);
    }
    return true;
  }
  return false;
}

void PageLoadTracker::OnLoadedSubresource(bool was_cached) {
  if (was_cached) {
    ++num_cache_requests_;
  } else {
    ++num_network_requests_;
  }
}

void PageLoadTracker::StopTracking() {
  did_stop_tracking_ = true;
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
  base::Optional<base::TimeDelta> time_to_abort;
  base::Optional<base::TimeDelta> time_to_commit;

  if (!background_time_.is_null()) {
    DCHECK_GE(background_time_, navigation_start_);
    first_background_time = background_time_ - navigation_start_;
  }

  if (!foreground_time_.is_null()) {
    DCHECK_GE(foreground_time_, navigation_start_);
    first_foreground_time = foreground_time_ - navigation_start_;
  }

  if (abort_type_ != ABORT_NONE) {
    DCHECK_GE(abort_time_, navigation_start_);
    time_to_abort = abort_time_ - navigation_start_;
  } else {
    DCHECK(abort_time_.is_null());
  }

  if (!commit_time_.is_null()) {
    DCHECK_GE(commit_time_, navigation_start_);
    time_to_commit = commit_time_ - navigation_start_;
  }

  // abort_type_ == ABORT_NONE implies !abort_user_initiated_.
  DCHECK(abort_type_ != ABORT_NONE || !abort_user_initiated_);
  return PageLoadExtraInfo(
      first_background_time, first_foreground_time, started_in_foreground_,
      user_gesture_, commit_time_.is_null() ? GURL() : url_, time_to_commit,
      abort_type_, abort_user_initiated_, time_to_abort, num_cache_requests_,
      num_network_requests_, metadata_);
}

void PageLoadTracker::NotifyAbort(UserAbortType abort_type,
                                  bool user_initiated,
                                  base::TimeTicks timestamp,
                                  bool is_certainly_browser_timestamp) {
  DCHECK_NE(abort_type, ABORT_NONE);
  // Use UpdateAbort to update an already notified PageLoadTracker.
  if (abort_type_ != ABORT_NONE)
    return;

  UpdateAbortInternal(abort_type, user_initiated, timestamp,
                      is_certainly_browser_timestamp);
}

void PageLoadTracker::UpdateAbort(UserAbortType abort_type,
                                  bool user_initiated,
                                  base::TimeTicks timestamp,
                                  bool is_certainly_browser_timestamp) {
  DCHECK_NE(abort_type, ABORT_NONE);
  DCHECK_NE(abort_type, ABORT_OTHER);
  DCHECK_EQ(abort_type_, ABORT_OTHER);

  // For some aborts (e.g. navigations), the initiated timestamp can be earlier
  // than the timestamp that aborted the load. Taking the minimum gives the
  // closest user initiated time known.
  UpdateAbortInternal(abort_type, user_initiated,
                      std::min(abort_time_, timestamp),
                      is_certainly_browser_timestamp);
}

bool PageLoadTracker::IsLikelyProvisionalAbort(
    base::TimeTicks abort_cause_time) const {
  // Note that |abort_cause_time - abort_time| can be negative.
  return abort_type_ == ABORT_OTHER &&
         (abort_cause_time - abort_time_).InMilliseconds() < 100;
}

bool PageLoadTracker::MatchesOriginalNavigation(
    content::NavigationHandle* navigation_handle) {
  // Neither navigation should have committed.
  DCHECK(!navigation_handle->HasCommitted());
  DCHECK(commit_time_.is_null());
  return navigation_handle->GetURL() == url_;
}

void PageLoadTracker::UpdateAbortInternal(UserAbortType abort_type,
                                          bool user_initiated,
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
    RecordInternalError(ERR_ABORT_BEFORE_NAVIGATION_START);
    abort_type_ = ABORT_NONE;
    abort_time_ = base::TimeTicks();
    return;
  }
  abort_type_ = abort_type;
  abort_time_ = timestamp;
  abort_user_initiated_ = user_initiated && abort_type != ABORT_CLIENT_REDIRECT;

  if (is_certainly_browser_timestamp) {
    ClampBrowserTimestampIfInterProcessTimeTickSkew(&abort_time_);
  }
}

// static
MetricsWebContentsObserver::MetricsWebContentsObserver(
    content::WebContents* web_contents,
    std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface)
    : content::WebContentsObserver(web_contents),
      in_foreground_(false),
      embedder_interface_(std::move(embedder_interface)),
      has_navigated_(false) {
  RegisterInputEventObserver(web_contents->GetRenderViewHost());
}

MetricsWebContentsObserver* MetricsWebContentsObserver::CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface) {
  DCHECK(web_contents);

  MetricsWebContentsObserver* metrics = FromWebContents(web_contents);
  if (!metrics) {
    metrics = new MetricsWebContentsObserver(web_contents,
                                             std::move(embedder_interface));
    web_contents->SetUserData(UserDataKey(), metrics);
  }
  return metrics;
}

MetricsWebContentsObserver::~MetricsWebContentsObserver() {
  // TODO(csharrison): Use a more user-initiated signal for CLOSE.
  NotifyAbortAllLoads(ABORT_CLOSE, false);
}

void MetricsWebContentsObserver::RegisterInputEventObserver(
    content::RenderViewHost* host) {
  if (host != nullptr)
    host->GetWidget()->AddInputEventObserver(this);
}

void MetricsWebContentsObserver::UnregisterInputEventObserver(
    content::RenderViewHost* host) {
  if (host != nullptr)
    host->GetWidget()->RemoveInputEventObserver(this);
}

void MetricsWebContentsObserver::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  UnregisterInputEventObserver(old_host);
  RegisterInputEventObserver(new_host);
}

bool MetricsWebContentsObserver::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(MetricsWebContentsObserver, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(PageLoadMetricsMsg_TimingUpdated, OnTimingUpdated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MetricsWebContentsObserver::WillStartNavigationRequest(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  std::unique_ptr<PageLoadTracker> last_aborted =
      NotifyAbortedProvisionalLoadsNewNavigation(navigation_handle);

  int chain_size_same_url = 0;
  int chain_size = 0;
  if (last_aborted) {
    if (last_aborted->MatchesOriginalNavigation(navigation_handle)) {
      chain_size_same_url = last_aborted->aborted_chain_size_same_url() + 1;
    } else if (last_aborted->aborted_chain_size_same_url() > 0) {
      LogAbortChainSameURLHistogram(
          last_aborted->aborted_chain_size_same_url());
    }
    chain_size = last_aborted->aborted_chain_size() + 1;
  }

  if (!ShouldTrackNavigation(navigation_handle))
    return;

  // TODO(bmcquade): add support for tracking prerendered pages when they become
  // visible.
  if (embedder_interface_->IsPrerendering(web_contents()))
    return;

  // Pass in the last committed url to the PageLoadTracker. If the MWCO has
  // never observed a committed load, use the last committed url from this
  // WebContent's opener. This is more accurate than using referrers due to
  // referrer sanitizing and origin referrers. Note that this could potentially
  // be inaccurate if the opener has since navigated.
  content::WebContents* opener = web_contents()->GetOpener();
  const GURL& opener_url =
      !has_navigated_ && opener
          ? web_contents()->GetOpener()->GetLastCommittedURL()
          : GURL::EmptyGURL();
  const GURL& currently_committed_url =
      committed_load_ ? committed_load_->committed_url() : opener_url;
  has_navigated_ = true;

  // We can have two provisional loads in some cases. E.g. a same-site
  // navigation can have a concurrent cross-process navigation started
  // from the omnibox.
  DCHECK_GT(2ul, provisional_loads_.size());
  // Passing raw pointers to observers_ and embedder_interface_ is safe because
  // the MetricsWebContentsObserver owns them both list and they are torn down
  // after the PageLoadTracker. The PageLoadTracker does not hold on to
  // committed_load_ or navigation_handle beyond the scope of the constructor.
  provisional_loads_.insert(std::make_pair(
      navigation_handle,
      base::WrapUnique(new PageLoadTracker(
          in_foreground_, embedder_interface_.get(), currently_committed_url,
          navigation_handle, chain_size, chain_size_same_url))));
}

void MetricsWebContentsObserver::OnRequestComplete(
    content::ResourceType resource_type,
    bool was_cached,
    int net_error) {
  // For simplicity, only count subresources. Navigations are hard to attribute
  // here because we won't have a committed load by the time data streams in
  // from the IO thread.
  if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME &&
      net_error != net::OK) {
    return;
  }
  if (!committed_load_)
    return;
  committed_load_->OnLoadedSubresource(was_cached);
}

const PageLoadExtraInfo
MetricsWebContentsObserver::GetPageLoadExtraInfoForCommittedLoad() {
  DCHECK(committed_load_);
  return committed_load_->ComputePageLoadExtraInfo();
}

void MetricsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  std::unique_ptr<PageLoadTracker> finished_nav(
      std::move(provisional_loads_[navigation_handle]));
  provisional_loads_.erase(navigation_handle);

  const bool should_track =
      finished_nav && ShouldTrackNavigation(navigation_handle);

  if (finished_nav && !should_track)
    finished_nav->StopTracking();

  if (navigation_handle->HasCommitted()) {
    // Ignore same-page navigations.
    if (navigation_handle->IsSamePage())
      return;

    // Notify other loads that they may have been aborted by this committed
    // load. is_certainly_browser_timestamp is set to false because
    // NavigationStart() could be set in either the renderer or browser process.
    NotifyAbortAllLoadsWithTimestamp(
        AbortTypeForPageTransition(navigation_handle->GetPageTransition()),
        IsNavigationUserInitiated(navigation_handle),
        navigation_handle->NavigationStart(), false);

    if (should_track) {
      HandleCommittedNavigationForTrackedLoad(navigation_handle,
                                              std::move(finished_nav));
    } else {
      committed_load_.reset();
    }
  } else if (should_track) {
    HandleFailedNavigationForTrackedLoad(navigation_handle,
                                         std::move(finished_nav));
  }
}

// Handle a pre-commit error. Navigations that result in an error page will be
// ignored. Note that downloads/204s will result in HasCommitted() returning
// false.
// TODO(csharrison): Track changes to NavigationHandle for signals when this is
// the case (HTTP response headers).
void MetricsWebContentsObserver::HandleFailedNavigationForTrackedLoad(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<PageLoadTracker> tracker) {
  tracker->FailedProvisionalLoad(navigation_handle);

  net::Error error = navigation_handle->GetNetErrorCode();

  // net::OK: This case occurs when the NavigationHandle finishes and reports
  // !HasCommitted(), but reports no net::Error. This should not occur
  // pre-PlzNavigate, but afterwards it should represent the navigation stopped
  // by the user before it was ready to commit.
  // net::ERR_ABORTED: An aborted provisional load has error
  // net::ERR_ABORTED. Note that this can come from some non user-initiated
  // errors, such as downloads, or 204 responses. See crbug.com/542369.
  if ((error == net::OK) || (error == net::ERR_ABORTED)) {
    tracker->NotifyAbort(ABORT_OTHER, false, base::TimeTicks::Now(), true);
    aborted_provisional_loads_.push_back(std::move(tracker));
  }
}

void MetricsWebContentsObserver::HandleCommittedNavigationForTrackedLoad(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<PageLoadTracker> tracker) {
  if (!navigation_handle->HasUserGesture() &&
      (navigation_handle->GetPageTransition() &
       ui::PAGE_TRANSITION_CLIENT_REDIRECT) != 0 &&
      committed_load_)
    committed_load_->NotifyClientRedirectTo(*tracker);

  committed_load_ = std::move(tracker);
  committed_load_->Commit(navigation_handle);
}

void MetricsWebContentsObserver::NavigationStopped() {
  // TODO(csharrison): Use a more user-initiated signal for STOP.
  NotifyAbortAllLoads(ABORT_STOP, false);
}

void MetricsWebContentsObserver::OnInputEvent(
    const blink::WebInputEvent& event) {
  // Ignore browser navigation or reload which comes with type Undefined.
  if (event.type == blink::WebInputEvent::Type::Undefined)
    return;

  if (committed_load_)
    committed_load_->OnInputEvent(event);
}

void MetricsWebContentsObserver::FlushMetricsOnAppEnterBackground() {
  if (committed_load_)
    committed_load_->FlushMetricsOnAppEnterBackground();
  for (const auto& kv : provisional_loads_) {
    kv.second->FlushMetricsOnAppEnterBackground();
  }
  for (const auto& tracker : aborted_provisional_loads_) {
    tracker->FlushMetricsOnAppEnterBackground();
  }
}

void MetricsWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;
  auto it = provisional_loads_.find(navigation_handle);
  if (it == provisional_loads_.end())
    return;
  it->second->Redirect(navigation_handle);
}

void MetricsWebContentsObserver::WasShown() {
  if (in_foreground_)
    return;
  in_foreground_ = true;
  if (committed_load_)
    committed_load_->WebContentsShown();
  for (const auto& kv : provisional_loads_) {
    kv.second->WebContentsShown();
  }
}

void MetricsWebContentsObserver::WasHidden() {
  if (!in_foreground_)
    return;
  in_foreground_ = false;
  if (committed_load_)
    committed_load_->WebContentsHidden();
  for (const auto& kv : provisional_loads_) {
    kv.second->WebContentsHidden();
  }
}

// This will occur when the process for the main RenderFrameHost exits, either
// normally or from a crash. We eagerly log data from the last committed load if
// we have one. Don't notify aborts here because this is probably not user
// initiated. If it is (e.g. browser shutdown), other code paths will take care
// of notifying.
void MetricsWebContentsObserver::RenderProcessGone(
    base::TerminationStatus status) {
  // Other code paths will be run for normal renderer shutdown. Note that we
  // sometimes get the STILL_RUNNING value on fast shutdown.
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_STILL_RUNNING) {
    return;
  }

  // If this is a crash, eagerly log the aborted provisional loads and the
  // committed load. |provisional_loads_| don't need to be destroyed here
  // because their lifetime is tied to the NavigationHandle.
  committed_load_.reset();
  aborted_provisional_loads_.clear();
}

void MetricsWebContentsObserver::NotifyAbortAllLoads(UserAbortType abort_type,
                                                     bool user_initiated) {
  NotifyAbortAllLoadsWithTimestamp(abort_type, user_initiated,
                                   base::TimeTicks::Now(), true);
}

void MetricsWebContentsObserver::NotifyAbortAllLoadsWithTimestamp(
    UserAbortType abort_type,
    bool user_initiated,
    base::TimeTicks timestamp,
    bool is_certainly_browser_timestamp) {
  if (committed_load_) {
    committed_load_->NotifyAbort(abort_type, user_initiated, timestamp,
                                 is_certainly_browser_timestamp);
  }
  for (const auto& kv : provisional_loads_) {
    kv.second->NotifyAbort(abort_type, user_initiated, timestamp,
                           is_certainly_browser_timestamp);
  }
  for (const auto& tracker : aborted_provisional_loads_) {
    if (tracker->IsLikelyProvisionalAbort(timestamp)) {
      tracker->UpdateAbort(abort_type, user_initiated, timestamp,
                           is_certainly_browser_timestamp);
    }
  }
  aborted_provisional_loads_.clear();
}

std::unique_ptr<PageLoadTracker>
MetricsWebContentsObserver::NotifyAbortedProvisionalLoadsNewNavigation(
    content::NavigationHandle* new_navigation) {
  // If there are multiple aborted loads that can be attributed to this one,
  // just count the latest one for simplicity. Other loads will fall into the
  // OTHER bucket, though there shouldn't be very many.
  if (aborted_provisional_loads_.size() == 0)
    return nullptr;
  if (aborted_provisional_loads_.size() > 1)
    RecordInternalError(ERR_NAVIGATION_SIGNALS_MULIPLE_ABORTED_LOADS);

  std::unique_ptr<PageLoadTracker> last_aborted_load =
      std::move(aborted_provisional_loads_.back());
  aborted_provisional_loads_.pop_back();

  base::TimeTicks timestamp = new_navigation->NavigationStart();
  if (last_aborted_load->IsLikelyProvisionalAbort(timestamp)) {
    last_aborted_load->UpdateAbort(
        AbortTypeForPageTransition(new_navigation->GetPageTransition()),
        IsNavigationUserInitiated(new_navigation), timestamp, false);
  }

  aborted_provisional_loads_.clear();
  return last_aborted_load;
}

void MetricsWebContentsObserver::OnTimingUpdated(
    content::RenderFrameHost* render_frame_host,
    const PageLoadTiming& timing,
    const PageLoadMetadata& metadata) {
  // We may receive notifications from frames that have been navigated away
  // from. We simply ignore them.
  if (render_frame_host != web_contents()->GetMainFrame()) {
    RecordInternalError(ERR_IPC_FROM_WRONG_FRAME);
    return;
  }

  // While timings arriving for the wrong frame are expected, we do not expect
  // any of the errors below. Thus, we track occurrences of all errors below,
  // rather than returning early after encountering an error.

  bool error = false;
  if (!committed_load_) {
    RecordInternalError(ERR_IPC_WITH_NO_RELEVANT_LOAD);
    error = true;
  }

  if (!web_contents()->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    RecordInternalError(ERR_IPC_FROM_BAD_URL_SCHEME);
    error = true;
  }

  if (error)
    return;

  if (!committed_load_->UpdateTiming(timing, metadata)) {
    // If the page load tracker cannot update its timing, something is wrong
    // with the IPC (it's from another load, or it's invalid in some other way).
    // We expect this to be a rare occurrence.
    RecordInternalError(ERR_BAD_TIMING_IPC);
  }
}

bool MetricsWebContentsObserver::ShouldTrackNavigation(
    content::NavigationHandle* navigation_handle) const {
  DCHECK(navigation_handle->IsInMainFrame());
  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return false;
  if (embedder_interface_->IsNewTabPageUrl(navigation_handle->GetURL()))
    return false;
  if (navigation_handle->HasCommitted()) {
    if (navigation_handle->IsSamePage() || navigation_handle->IsErrorPage())
      return false;
    const std::string& mime_type = web_contents()->GetContentsMimeType();
    if (mime_type != "text/html" && mime_type != "application/xhtml+xml")
      return false;
  }
  return true;
}

}  // namespace page_load_metrics
