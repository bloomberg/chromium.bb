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
  DCHECK_IMPLIES(
      !timing.load_event_start.is_zero(),
      !timing.dom_content_loaded_event_start.is_zero() &&
      timing.response_start <= timing.load_event_start &&
      timing.dom_content_loaded_event_start <= timing.load_event_start);

  return true;
}

}  // namespace

MetricsWebContentsObserver::MetricsWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

// This object is tied to a single WebContents for its entire lifetime.
MetricsWebContentsObserver::~MetricsWebContentsObserver() {
  RecordTimingHistograms();
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

void MetricsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;
  if (navigation_handle->IsInMainFrame() && !navigation_handle->IsSamePage())
    RecordTimingHistograms();
  if (IsRelevantNavigation(navigation_handle))
    current_timing_.reset(new PageLoadTiming());
}

// This will occur when the process for the main RenderFrameHost exits.
// This will happen with a normal exit or a crash.
void MetricsWebContentsObserver::RenderProcessGone(
    base::TerminationStatus status) {
  RecordTimingHistograms();
}

#define PAGE_LOAD_HISTOGRAM(name, sample)                           \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample,                          \
                             base::TimeDelta::FromMilliseconds(10), \
                             base::TimeDelta::FromMinutes(10), 100);

void MetricsWebContentsObserver::OnTimingUpdated(
    content::RenderFrameHost* render_frame_host,
    const PageLoadTiming& timing) {
  if (!current_timing_)
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

  // Throw away IPCs that are not relevant to the current navigation.
  if (!current_timing_->navigation_start.is_null() &&
      timing.navigation_start != current_timing_->navigation_start) {
    // TODO(csharrison) uma log a counter here
    return;
  }

  *current_timing_ = timing;
}

void MetricsWebContentsObserver::RecordTimingHistograms() {
  if (!current_timing_ || !IsValidPageLoadTiming(*current_timing_))
    return;

  if (!current_timing_->dom_content_loaded_event_start.is_zero()) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Timing.NavigationToDOMContentLoadedEventFired",
        current_timing_->dom_content_loaded_event_start);
  }

  if (!current_timing_->load_event_start.is_zero()) {
    PAGE_LOAD_HISTOGRAM("PageLoad.Timing.NavigationToLoadEventFired",
                        current_timing_->load_event_start);
  }

  if (!current_timing_->first_layout.is_zero()) {
    PAGE_LOAD_HISTOGRAM("PageLoad.Timing.NavigationToFirstLayout",
                        current_timing_->first_layout);
  }
  current_timing_.reset();
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
