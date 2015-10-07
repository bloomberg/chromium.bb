// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "components/page_load_metrics/common/page_load_metrics_messages.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    page_load_metrics::MetricsWebContentsObserver);

namespace page_load_metrics {

namespace {

bool IsValidPageLoadTiming(const PageLoadTiming& timing) {
  if (timing.IsEmpty())
    return false;

  // If we have a non-empty timing, it should always have a navigation start.
  DCHECK(!timing.navigation_start.is_null());

  // If we have a DOM content loaded event, we should have a response start.
  DCHECK_IMPLIES(
      !timing.dom_content_loaded_event_start.is_zero(),
      timing.response_start <= timing.dom_content_loaded_event_start);

  // If we have a load event, we should have both a response start and a DCL.
  // TODO(csharrison) crbug.com/536203 shows that sometimes we can get a load
  // event without a DCL. Figure out if we can change this condition to use a
  // DCHECK instead.
  if (!timing.load_event_start.is_zero() &&
      (timing.dom_content_loaded_event_start.is_zero() ||
       timing.response_start > timing.load_event_start ||
       timing.dom_content_loaded_event_start > timing.load_event_start)) {
    return false;
  }

  return true;
}

base::Time WallTimeFromTimeTicks(base::TimeTicks time) {
  return base::Time::FromDoubleT(
      (time - base::TimeTicks::UnixEpoch()).InSecondsF());
}

}  // namespace

#define PAGE_LOAD_HISTOGRAM(name, sample)                           \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample,                          \
                             base::TimeDelta::FromMilliseconds(10), \
                             base::TimeDelta::FromMinutes(10), 100)

PageLoadTracker::PageLoadTracker(bool in_foreground)
    : has_commit_(false), started_in_foreground_(in_foreground) {
  RecordEvent(PAGE_LOAD_STARTED);
}

PageLoadTracker::~PageLoadTracker() {
  // Even a load that failed a provisional load should log
  // that it aborted before first layout.
  if (timing_.first_layout.is_zero())
    RecordEvent(PAGE_LOAD_ABORTED_BEFORE_FIRST_LAYOUT);

  if (has_commit_)
    RecordTimingHistograms();
}

void PageLoadTracker::WebContentsHidden() {
  // Only log the first time we background in a given page load.
  if (background_time_.is_null()) {
    background_time_ = base::TimeTicks::Now();
  }
}

void PageLoadTracker::Commit() {
  has_commit_ = true;
}

bool PageLoadTracker::UpdateTiming(const PageLoadTiming& timing) {
  // Throw away IPCs that are not relevant to the current navigation.
  if (!timing_.navigation_start.is_null() &&
      timing_.navigation_start != timing.navigation_start) {
    // TODO(csharrison) uma log a counter here
    return false;
  }
  if (IsValidPageLoadTiming(timing)) {
    timing_ = timing;
    return true;
  }
  return false;
}

void PageLoadTracker::RecordTimingHistograms() {
  DCHECK(has_commit_);
  // This method is similar to how blink converts TimeTicks to epoch time.
  // There may be slight inaccuracies due to inter-process timestamps, but
  // this solution is the best we have right now.
  base::TimeDelta background_delta;
  if (started_in_foreground_) {
    background_delta = background_time_.is_null()
        ? base::TimeDelta::Max()
        : WallTimeFromTimeTicks(background_time_) - timing_.navigation_start;
  }

  if (!timing_.dom_content_loaded_event_start.is_zero()) {
    if (timing_.dom_content_loaded_event_start < background_delta) {
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Timing2.NavigationToDOMContentLoadedEventFired",
          timing_.dom_content_loaded_event_start);
    } else {
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Timing2.NavigationToDOMContentLoadedEventFired.BG",
          timing_.dom_content_loaded_event_start);
    }
  }
  if (!timing_.load_event_start.is_zero()) {
    if (timing_.load_event_start < background_delta) {
      PAGE_LOAD_HISTOGRAM("PageLoad.Timing2.NavigationToLoadEventFired",
                          timing_.load_event_start);
    } else {
      PAGE_LOAD_HISTOGRAM("PageLoad.Timing2.NavigationToLoadEventFired.BG",
                          timing_.load_event_start);
    }
  }
  if (!timing_.first_layout.is_zero()) {
    if (timing_.first_layout < background_delta) {
      PAGE_LOAD_HISTOGRAM("PageLoad.Timing2.NavigationToFirstLayout",
                          timing_.first_layout);
      RecordEvent(PAGE_LOAD_SUCCESSFUL_FIRST_LAYOUT_FOREGROUND);
    } else {
      PAGE_LOAD_HISTOGRAM("PageLoad.Timing2.NavigationToFirstLayout.BG",
                          timing_.first_layout);
      RecordEvent(PAGE_LOAD_SUCCESSFUL_FIRST_LAYOUT_BACKGROUND);
    }
  }
  if (!timing_.first_text_paint.is_zero()) {
    if (timing_.first_text_paint < background_delta) {
      PAGE_LOAD_HISTOGRAM("PageLoad.Timing2.NavigationToFirstTextPaint",
                          timing_.first_text_paint);
    } else {
      PAGE_LOAD_HISTOGRAM("PageLoad.Timing2.NavigationToFirstTextPaint.BG",
                          timing_.first_text_paint);
    }
  }
}

void PageLoadTracker::RecordEvent(PageLoadEvent event) {
  UMA_HISTOGRAM_ENUMERATION(
      "PageLoad.EventCounts", event, PAGE_LOAD_LAST_ENTRY);
}

MetricsWebContentsObserver::MetricsWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), in_foreground_(false) {}

MetricsWebContentsObserver::~MetricsWebContentsObserver() {}

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
  // We can have two provisional loads in some cases. E.g. a same-site
  // navigation can have a concurrent cross-process navigation started
  // from the omnibox.
  DCHECK_GT(2ul, provisional_loads_.size());
  provisional_loads_.insert(
      navigation_handle, make_scoped_ptr(new PageLoadTracker(in_foreground_)));
}

void MetricsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  scoped_ptr<PageLoadTracker> finished_nav(
      provisional_loads_.take_and_erase(navigation_handle));
  DCHECK(finished_nav);

  // Handle a pre-commit error here. Navigations that result in an error page
  // will be ignored.
  if (!navigation_handle->HasCommitted()) {
    finished_nav->RecordEvent(PAGE_LOAD_FAILED_BEFORE_COMMIT);
    if (navigation_handle->GetNetErrorCode() == net::ERR_ABORTED)
      finished_nav->RecordEvent(PAGE_LOAD_ABORTED_BEFORE_COMMIT);
    return;
  }

  // Don't treat a same-page nav as a new page load.
  if (navigation_handle->IsSamePage())
    return;

  // Eagerly log the previous UMA even if we don't care about the current
  // navigation.
  committed_load_.reset();

  if (!IsRelevantNavigation(navigation_handle))
    return;

  committed_load_ = finished_nav.Pass();
  committed_load_->Commit();
}

void MetricsWebContentsObserver::WasShown() {
  in_foreground_ = true;
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
// we have one.
void MetricsWebContentsObserver::RenderProcessGone(
    base::TerminationStatus status) {
  committed_load_.reset();
}

void MetricsWebContentsObserver::OnTimingUpdated(
    content::RenderFrameHost* render_frame_host,
    const PageLoadTiming& timing) {
  if (!committed_load_)
    return;

  // We may receive notifications from frames that have been navigated away
  // from. We simply ignore them.
  if (render_frame_host != web_contents()->GetMainFrame())
    return;

  // For urls like chrome://newtab, the renderer and browser disagree,
  // so we have to double check that the renderer isn't sending data from a
  // bad url like https://www.google.com/_/chrome/newtab.
  if (!web_contents()->GetLastCommittedURL().SchemeIsHTTPOrHTTPS())
    return;

  committed_load_->UpdateTiming(timing);
}

bool MetricsWebContentsObserver::IsRelevantNavigation(
    content::NavigationHandle* navigation_handle) {
  // The url we see from the renderer side is not always the same as what
  // we see from the browser side (e.g. chrome://newtab). We want to be
  // sure here that we aren't logging UMA for internal pages.
  const GURL& browser_url = web_contents()->GetLastCommittedURL();
  return navigation_handle->IsInMainFrame() &&
         !navigation_handle->IsSamePage() &&
         !navigation_handle->IsErrorPage() &&
         navigation_handle->GetURL().SchemeIsHTTPOrHTTPS() &&
         browser_url.SchemeIsHTTPOrHTTPS();
}

}  // namespace page_load_metrics
