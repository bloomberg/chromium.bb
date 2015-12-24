// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_metrics_messages.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/rappor/rappor_service.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/page_transition_types.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    page_load_metrics::MetricsWebContentsObserver);

namespace page_load_metrics {

namespace {

// The url we see from the renderer side is not always the same as what
// we see from the browser side (e.g. chrome://newtab). We want to be
// sure here that we aren't logging UMA for internal pages.
bool IsRelevantNavigation(
    content::NavigationHandle* navigation_handle,
    const GURL& browser_url,
    const std::string& mime_type) {
  DCHECK(navigation_handle->HasCommitted());
  return navigation_handle->IsInMainFrame() &&
         !navigation_handle->IsSamePage() &&
         !navigation_handle->IsErrorPage() &&
         navigation_handle->GetURL().SchemeIsHTTPOrHTTPS() &&
         browser_url.SchemeIsHTTPOrHTTPS() &&
         (mime_type == "text/html" || mime_type == "application/xhtml+xml");
}

bool IsValidPageLoadTiming(const PageLoadTiming& timing) {
  if (timing.IsEmpty())
    return false;

  // If we have a non-empty timing, it should always have a navigation start.
  if (timing.navigation_start.is_null()) {
    NOTREACHED();
    return false;
  }

  // If we have a DOM content loaded event, we should have a response start.
  if (!timing.dom_content_loaded_event_start.is_zero() &&
      timing.response_start > timing.dom_content_loaded_event_start) {
    NOTREACHED();
    return false;
  }

  // If we have a load event, we should have both a response start and a DCL.
  // TODO(csharrison) crbug.com/536203 shows that sometimes we can get a load
  // event without a DCL. Figure out if we can change this condition to use a
  // NOTREACHED in the condition.
  if (!timing.load_event_start.is_zero() &&
      (timing.dom_content_loaded_event_start.is_zero() ||
       timing.response_start > timing.load_event_start ||
       timing.dom_content_loaded_event_start > timing.load_event_start)) {
    return false;
  }

  return true;
}

void RecordInternalError(InternalErrorLoadEvent event) {
  UMA_HISTOGRAM_ENUMERATION(kErrorEvents, event, ERR_LAST_ENTRY);
}

UserAbortType AbortTypeForPageTransition(ui::PageTransition transition) {
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD))
    return ABORT_RELOAD;
  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK)
    return ABORT_FORWARD_BACK;
  if (ui::PageTransitionIsNewNavigation(transition))
    return ABORT_NEW_NAVIGATION;
  NOTREACHED();
  return ABORT_OTHER;
}

// The number of buckets in the bitfield histogram. These buckets are described
// in rappor.xml in PageLoad.CoarseTiming.NavigationToFirstContentfulPaint.
// The bucket flag is defined by 1 << bucket_index, and is the bitfield
// representing which timing bucket the page load falls into, i.e. 000010
// would be the bucket flag showing that the page took between 2 and 4 seconds
// to load.
const size_t kNumRapporHistogramBuckets = 6;

uint64_t RapporHistogramBucketIndex(const base::TimeDelta& time) {
  int64 seconds = time.InSeconds();
  if (seconds < 2)
    return 0;
  if (seconds < 4)
    return 1;
  if (seconds < 8)
    return 2;
  if (seconds < 16)
    return 3;
  if (seconds < 32)
    return 4;
  return 5;
}

}  // namespace

PageLoadTracker::PageLoadTracker(
    bool in_foreground,
    PageLoadMetricsEmbedderInterface* embedder_interface,
    content::NavigationHandle* navigation_handle)
    : renderer_tracked_(false),
      has_commit_(false),
      navigation_start_(navigation_handle->NavigationStart()),
      abort_type_(ABORT_NONE),
      started_in_foreground_(in_foreground),
      embedder_interface_(embedder_interface) {
  embedder_interface_->RegisterObservers(this);
  for (const auto& observer : observers_) {
    observer->OnStart(navigation_handle);
  }
}

PageLoadTracker::~PageLoadTracker() {
  PageLoadExtraInfo info = GetPageLoadMetricsInfo();
  RecordRappor(info);
  RecordTimingHistograms(info);
  for (const auto& observer : observers_) {
    observer->OnComplete(timing_, info);
  }
}

void PageLoadTracker::WebContentsHidden() {
  // Only log the first time we background in a given page load.
  if (started_in_foreground_ && background_time_.is_null())
    background_time_ = base::TimeTicks::Now();
}

void PageLoadTracker::WebContentsShown() {
  // Only log the first time we foreground in a given page load.
  // Don't log foreground time if we started foregrounded.
  if (!started_in_foreground_ && foreground_time_.is_null())
    foreground_time_ = base::TimeTicks::Now();
}

void PageLoadTracker::Commit(content::NavigationHandle* navigation_handle) {
  has_commit_ = true;
  url_ = navigation_handle->GetURL();
  // We log the event that this load started. Because we don't know if a load is
  // relevant or if it will commit before now, we have to log this event at
  // commit time.
  if (renderer_tracked())
    RecordCommittedEvent(RELEVANT_LOAD_STARTED, !started_in_foreground_);

  for (const auto& observer : observers_) {
    observer->OnCommit(navigation_handle);
  }
}

void PageLoadTracker::Redirect(content::NavigationHandle* navigation_handle) {
  for (const auto& observer : observers_) {
    observer->OnRedirect(navigation_handle);
  }
}

bool PageLoadTracker::UpdateTiming(const PageLoadTiming& new_timing) {
  // Throw away IPCs that are not relevant to the current navigation.
  // Two timing structures cannot refer to the same navigation if they indicate
  // that a navigation started at different times, so a new timing struct with a
  // different start time from an earlier struct is considered invalid.
  bool valid_timing_descendent =
      timing_.navigation_start.is_null() ||
      timing_.navigation_start == new_timing.navigation_start;
  if (IsValidPageLoadTiming(new_timing) && valid_timing_descendent) {
    timing_ = new_timing;
    return true;
  }
  return false;
}

bool PageLoadTracker::HasBackgrounded() {
  return !started_in_foreground_ || !background_time_.is_null();
}

void PageLoadTracker::set_renderer_tracked(bool renderer_tracked) {
  renderer_tracked_ = renderer_tracked;
}

void PageLoadTracker::AddObserver(
    scoped_ptr<PageLoadMetricsObserver> observer) {
  observers_.push_back(std::move(observer));
}

PageLoadExtraInfo PageLoadTracker::GetPageLoadMetricsInfo() {
  base::TimeDelta first_background_time;
  base::TimeDelta first_foreground_time;
  base::TimeDelta time_to_abort;
  if (!background_time_.is_null() && started_in_foreground_)
    first_background_time = background_time_ - navigation_start_;
  if (!foreground_time_.is_null() && !started_in_foreground_)
    first_foreground_time = foreground_time_ - navigation_start_;
  if (abort_type_ != ABORT_NONE) {
    DCHECK_GT(abort_time_, navigation_start_);
    time_to_abort = abort_time_ - navigation_start_;
  }
  return PageLoadExtraInfo(first_background_time, first_foreground_time,
                           started_in_foreground_, has_commit_, abort_type_,
                           time_to_abort);
}

const GURL& PageLoadTracker::committed_url() {
  DCHECK(has_commit_);
  return url_;
}

void PageLoadTracker::NotifyAbort(UserAbortType abort_type,
                                  const base::TimeTicks& timestamp) {
  DCHECK_NE(abort_type, ABORT_NONE);
  // Use UpdateAbort to update an already notified PageLoadTracker.
  if (abort_type_ != ABORT_NONE)
    return;

  UpdateAbortInternal(abort_type, timestamp);
}

void PageLoadTracker::UpdateAbort(UserAbortType abort_type,
                                  const base::TimeTicks& timestamp) {
  DCHECK_NE(abort_type, ABORT_NONE);
  DCHECK_NE(abort_type, ABORT_OTHER);
  DCHECK_EQ(abort_type_, ABORT_OTHER);

  // For some aborts (e.g. navigations), the initiated timestamp can be earlier
  // than the timestamp that aborted the load. Taking the minimum gives the
  // closest user initiated time known.
  UpdateAbortInternal(abort_type, std::min(abort_time_, timestamp));
}

void PageLoadTracker::UpdateAbortInternal(UserAbortType abort_type,
                                          const base::TimeTicks& timestamp) {
  // When a provisional navigation commits, that navigation's start time is
  // interpreted as the abort time for other provisional loads in the tab.
  // However, this only makes sense if the committed load started after the
  // aborted provisional loads started. Thus we ignore cases where the committed
  // load started before the aborted provisional load, as this would result in
  // recording a negative time-to-abort. The real issue here is that we have to
  // infer the cause of aborts. It would be better if the navigation code could
  // instead report the actual cause of an aborted navigation. See crbug/571647
  // for details.
  if (timestamp <= navigation_start_) {
    RecordInternalError(ERR_ABORT_BEFORE_NAVIGATION_START);
    abort_type_ = ABORT_NONE;
    abort_time_ = base::TimeTicks();
    return;
  }
  abort_type_ = abort_type;
  abort_time_ = timestamp;
}

void PageLoadTracker::RecordTimingHistograms(const PageLoadExtraInfo& info) {
  if (!info.first_background_time.is_zero() &&
      !EventOccurredInForeground(timing_.first_paint, info)) {
    if (has_commit_) {
      PAGE_LOAD_HISTOGRAM(kHistogramBackgroundBeforePaint,
                          info.first_background_time);
    } else {
      PAGE_LOAD_HISTOGRAM(kHistogramBackgroundBeforeCommit,
                          info.first_background_time);
    }
  }

  // The rest of the histograms require the load to have commit and be relevant.
  if (!has_commit_ || !renderer_tracked())
    return;

  // The rest of the timing histograms require us to have received IPCs from the
  // renderer. Record UMA for how often this occurs (usually for quickly aborted
  // loads). For now, don't update observers if this is the case.
  if (timing_.IsEmpty()) {
    RecordInternalError(ERR_NO_IPCS_RECEIVED);
    return;
  }

  if (!timing_.dom_content_loaded_event_start.is_zero()) {
    if (EventOccurredInForeground(timing_.dom_content_loaded_event_start,
                                  info)) {
      PAGE_LOAD_HISTOGRAM(kHistogramDomContentLoaded,
                          timing_.dom_content_loaded_event_start);
    } else {
      PAGE_LOAD_HISTOGRAM(kBackgroundHistogramDomContentLoaded,
                          timing_.dom_content_loaded_event_start);
    }
  }
  if (!timing_.load_event_start.is_zero()) {
    if (EventOccurredInForeground(timing_.load_event_start, info)) {
      PAGE_LOAD_HISTOGRAM(kHistogramLoad, timing_.load_event_start);
    } else {
      PAGE_LOAD_HISTOGRAM(kBackgroundHistogramLoad, timing_.load_event_start);
    }
  }
  if (timing_.first_layout.is_zero()) {
    RecordCommittedEvent(RELEVANT_LOAD_FAILED_BEFORE_FIRST_LAYOUT,
                         HasBackgrounded());
  } else {
    if (EventOccurredInForeground(timing_.first_layout, info)) {
      PAGE_LOAD_HISTOGRAM(kHistogramFirstLayout, timing_.first_layout);
      RecordCommittedEvent(RELEVANT_LOAD_SUCCESSFUL_FIRST_LAYOUT, false);
    } else {
      PAGE_LOAD_HISTOGRAM(kBackgroundHistogramFirstLayout,
                          timing_.first_layout);
      RecordCommittedEvent(RELEVANT_LOAD_SUCCESSFUL_FIRST_LAYOUT, true);
    }
  }
  if (!timing_.first_paint.is_zero()) {
    if (EventOccurredInForeground(timing_.first_paint, info)) {
      PAGE_LOAD_HISTOGRAM(kHistogramFirstPaint, timing_.first_paint);
    } else {
      PAGE_LOAD_HISTOGRAM(kBackgroundHistogramFirstPaint, timing_.first_paint);
    }
  }
  if (!timing_.first_text_paint.is_zero()) {
    if (EventOccurredInForeground(timing_.first_text_paint, info)) {
      PAGE_LOAD_HISTOGRAM(kHistogramFirstTextPaint, timing_.first_text_paint);
    } else {
      PAGE_LOAD_HISTOGRAM(kBackgroundHistogramFirstTextPaint,
                          timing_.first_text_paint);
    }
  }
  if (!timing_.first_image_paint.is_zero()) {
    if (EventOccurredInForeground(timing_.first_image_paint, info)) {
      PAGE_LOAD_HISTOGRAM(kHistogramFirstImagePaint, timing_.first_image_paint);
    } else {
      PAGE_LOAD_HISTOGRAM(kBackgroundHistogramFirstImagePaint,
                          timing_.first_image_paint);
    }
  }
  base::TimeDelta first_contentful_paint = GetFirstContentfulPaint(timing_);
  if (!first_contentful_paint.is_zero()) {
    if (EventOccurredInForeground(first_contentful_paint, info)) {
      PAGE_LOAD_HISTOGRAM(kHistogramFirstContentfulPaint,
                          first_contentful_paint);
      // Bucket these histograms into high/low resolution clock systems. This
      // might point us to directions that will de-noise some UMA.
      if (base::TimeTicks::IsHighResolution()) {
        PAGE_LOAD_HISTOGRAM(kHistogramFirstContentfulPaintHigh,
                            first_contentful_paint);
      } else {
        PAGE_LOAD_HISTOGRAM(kHistogramFirstContentfulPaintLow,
                            first_contentful_paint);
      }
    } else {
      PAGE_LOAD_HISTOGRAM(kBackgroundHistogramFirstContentfulPaint,
                          first_contentful_paint);
    }
  }

  // Log time to first foreground / time to first background. Log counts that we
  // started a relevant page load in the foreground / background.
  if (!info.first_background_time.is_zero())
    PAGE_LOAD_HISTOGRAM(kHistogramFirstBackground, info.first_background_time);
  else if (!info.first_foreground_time.is_zero())
    PAGE_LOAD_HISTOGRAM(kHistogramFirstForeground, info.first_foreground_time);
}

void PageLoadTracker::RecordProvisionalEvent(ProvisionalLoadEvent event) {
  if (HasBackgrounded()) {
    UMA_HISTOGRAM_ENUMERATION(kBackgroundProvisionalEvents, event,
                              PROVISIONAL_LOAD_LAST_ENTRY);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kProvisionalEvents, event,
                              PROVISIONAL_LOAD_LAST_ENTRY);
  }
}

// RecordCommittedEvent needs a backgrounded input because we need to special
// case a few events that need either precise timing measurements, or different
// logic than simply "Did I background before logging this event?"
void PageLoadTracker::RecordCommittedEvent(CommittedRelevantLoadEvent event,
                                           bool backgrounded) {
  if (backgrounded) {
    UMA_HISTOGRAM_ENUMERATION(kBackgroundCommittedEvents, event,
                              RELEVANT_LOAD_LAST_ENTRY);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kCommittedEvents, event,
                              RELEVANT_LOAD_LAST_ENTRY);
  }
}

void PageLoadTracker::RecordRappor(const PageLoadExtraInfo& info) {
  if (!info.has_commit)
    return;
  DCHECK(!committed_url().is_empty());
  rappor::RapporService* rappor_service =
      embedder_interface_->GetRapporService();
  if (!rappor_service)
    return;
  base::TimeDelta first_contentful_paint = GetFirstContentfulPaint(timing_);
  // Log the eTLD+1 of sites that show poor loading performance.
  if (EventOccurredInForeground(first_contentful_paint, info)) {
    scoped_ptr<rappor::Sample> sample =
        rappor_service->CreateSample(rappor::UMA_RAPPOR_TYPE);
    sample->SetStringField(
        "Domain", rappor::GetDomainAndRegistrySampleFromGURL(committed_url()));
    uint64_t bucket_index = RapporHistogramBucketIndex(first_contentful_paint);
    sample->SetFlagsField("Bucket", uint64_t(1) << bucket_index,
                          kNumRapporHistogramBuckets);
    // The IsSlow flag is just a one bit boolean if the first layout was > 10s.
    sample->SetFlagsField("IsSlow", first_contentful_paint.InSecondsF() >= 10,
                          1);
    rappor_service->RecordSampleObj(kRapporMetricsNameCoarseTiming,
                                     sample.Pass());
  }
}

// static
MetricsWebContentsObserver::MetricsWebContentsObserver(
    content::WebContents* web_contents,
    scoped_ptr<PageLoadMetricsEmbedderInterface> embedder_interface)
    : content::WebContentsObserver(web_contents),
      in_foreground_(false),
      embedder_interface_(embedder_interface.Pass()) {}

MetricsWebContentsObserver* MetricsWebContentsObserver::CreateForWebContents(
    content::WebContents* web_contents,
    scoped_ptr<PageLoadMetricsEmbedderInterface> embedder_interface) {
  DCHECK(web_contents);

  MetricsWebContentsObserver* metrics = FromWebContents(web_contents);
  if (!metrics) {
    metrics =
        new MetricsWebContentsObserver(web_contents, embedder_interface.Pass());
    web_contents->SetUserData(UserDataKey(), metrics);
  }
  return metrics;
}

MetricsWebContentsObserver::~MetricsWebContentsObserver() {
  NotifyAbortAllLoads(ABORT_CLOSE);
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

void MetricsWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;
  if (embedder_interface_->IsPrerendering(web_contents()))
    return;
  // We can have two provisional loads in some cases. E.g. a same-site
  // navigation can have a concurrent cross-process navigation started
  // from the omnibox.
  DCHECK_GT(2ul, provisional_loads_.size());
  // Passing raw pointers to observers_ and embedder_interface_ is safe because
  // the MetricsWebContentsObserver owns them both list and they are torn down
  // after the PageLoadTracker.
  provisional_loads_.insert(std::make_pair(
      navigation_handle,
      make_scoped_ptr(new PageLoadTracker(
          in_foreground_, embedder_interface_.get(), navigation_handle))));
}

void MetricsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  scoped_ptr<PageLoadTracker> finished_nav(
      std::move(provisional_loads_[navigation_handle]));
  provisional_loads_.erase(navigation_handle);

  // There's a chance a navigation could have started before we were added to a
  // tab. Bail out early if this is the case.
  if (!finished_nav)
    return;

  // Handle a pre-commit error here. Navigations that result in an error page
  // will be ignored. Note that downloads/204s will result in HasCommitted()
  // returning false.
  // TODO(csharrison): Track changes to NavigationHandle for signals when this
  // is the case (HTTP response headers).
  if (!navigation_handle->HasCommitted()) {
    net::Error error = navigation_handle->GetNetErrorCode();
    ProvisionalLoadEvent event = error == net::OK ? PROVISIONAL_LOAD_STOPPED
        : error == net::ERR_ABORTED ? PROVISIONAL_LOAD_ERR_ABORTED
        : PROVISIONAL_LOAD_ERR_FAILED_NON_ABORT;
    finished_nav->RecordProvisionalEvent(event);
    if (event != PROVISIONAL_LOAD_ERR_FAILED_NON_ABORT) {
      finished_nav->NotifyAbort(ABORT_OTHER, base::TimeTicks::Now());
      aborted_provisional_loads_.push_back(std::move(finished_nav));
    }
    return;
  }
  finished_nav->RecordProvisionalEvent(PROVISIONAL_LOAD_COMMITTED);

  // Don't treat a same-page nav as a new page load.
  if (navigation_handle->IsSamePage())
    return;

  // Notify other loads that they may have been aborted by this committed load.
  // Note that by using the committed navigation start as the abort cause, we
  // lose data on provisional loads that were aborted by other provisional
  // loads. Those will either be listed as ABORT_OTHER or as being aborted by
  // this load.
  NotifyAbortAllLoadsWithTimestamp(
      AbortTypeForPageTransition(navigation_handle->GetPageTransition()),
      navigation_handle->NavigationStart());

  committed_load_ = finished_nav.Pass();
  aborted_provisional_loads_.clear();

  const GURL& browser_url = web_contents()->GetLastCommittedURL();
  const std::string& mime_type = web_contents()->GetContentsMimeType();
  DCHECK(!browser_url.is_empty());
  DCHECK(!mime_type.empty());
  committed_load_->set_renderer_tracked(
      IsRelevantNavigation(navigation_handle, browser_url, mime_type));

  committed_load_->Commit(navigation_handle);
}

void MetricsWebContentsObserver::NavigationStopped() {
  NotifyAbortAllLoads(ABORT_STOP);
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
  in_foreground_ = true;
  if (committed_load_)
    committed_load_->WebContentsShown();
  for (const auto& kv : provisional_loads_) {
    kv.second->WebContentsShown();
  }
}

void MetricsWebContentsObserver::WasHidden() {
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

void MetricsWebContentsObserver::NotifyAbortAllLoads(UserAbortType abort_type) {
  NotifyAbortAllLoadsWithTimestamp(abort_type, base::TimeTicks::Now());
}

void MetricsWebContentsObserver::NotifyAbortAllLoadsWithTimestamp(
    UserAbortType abort_type,
    base::TimeTicks timestamp) {
  if (committed_load_)
    committed_load_->NotifyAbort(abort_type, timestamp);
  for (const auto& kv : provisional_loads_) {
    kv.second->NotifyAbort(abort_type, timestamp);
  }
  // If a better signal than ABORT_OTHER came in the last 100ms, treat it as the
  // cause of the abort (some ABORT_OTHER signals occur before the true signal).
  // Note that |abort_type| can only be ABORT_OTHER for provisional loads. While
  // this  heuristic is coarse, it works better and is simpler than other
  // feasible methods. See https://goo.gl/WKRG98.
  for (const auto& tracker : aborted_provisional_loads_) {
    // Note that |timestamp - abort_time| can be negative.
    if (tracker->abort_type() == ABORT_OTHER &&
        (timestamp - tracker->abort_time()).InMilliseconds() < 100) {
      tracker->UpdateAbort(abort_type, timestamp);
    }
  }
}

void MetricsWebContentsObserver::OnTimingUpdated(
    content::RenderFrameHost* render_frame_host,
    const PageLoadTiming& timing) {
  bool error = false;
  if (!committed_load_ || !committed_load_->renderer_tracked()) {
    RecordInternalError(ERR_IPC_WITH_NO_RELEVANT_LOAD);
    error = true;
  }

  // We may receive notifications from frames that have been navigated away
  // from. We simply ignore them.
  if (render_frame_host != web_contents()->GetMainFrame()) {
    RecordInternalError(ERR_IPC_FROM_WRONG_FRAME);
    error = true;
  }

  // For urls like chrome://newtab, the renderer and browser disagree,
  // so we have to double check that the renderer isn't sending data from a
  // bad url like https://www.google.com/_/chrome/newtab.
  if (!web_contents()->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    RecordInternalError(ERR_IPC_FROM_BAD_URL_SCHEME);
    error = true;
  }

  if (error)
    return;

  if (!committed_load_->UpdateTiming(timing)) {
    // If the page load tracker cannot update its timing, something is wrong
    // with the IPC (it's from another load, or it's invalid in some other way).
    // We expect this to be a rare occurrence.
    RecordInternalError(ERR_BAD_TIMING_IPC);
  }
}

}  // namespace page_load_metrics
