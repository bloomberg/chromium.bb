// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/page_load_metrics/metrics_render_frame_observer.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/renderer/page_load_metrics/page_timing_metrics_sender.h"
#include "chrome/renderer/page_load_metrics/renderer_page_track_decider.h"
#include "chrome/renderer/searchbox/search_bouncer.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPerformance.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

base::TimeDelta ClampDelta(double event, double start) {
  if (event - start < 0)
    event = start;
  return base::Time::FromDoubleT(event) - base::Time::FromDoubleT(start);
}

}  //  namespace

MetricsRenderFrameObserver::MetricsRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

MetricsRenderFrameObserver::~MetricsRenderFrameObserver() {}

void MetricsRenderFrameObserver::DidChangePerformanceTiming() {
  // Only track timing metrics for main frames.
  if (IsMainFrame())
    SendMetrics();
}

void MetricsRenderFrameObserver::DidObserveLoadingBehavior(
    blink::WebLoadingBehaviorFlag behavior) {
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->DidObserveLoadingBehavior(behavior);
}

void MetricsRenderFrameObserver::FrameDetached() {
  page_timing_metrics_sender_.reset();
}

void MetricsRenderFrameObserver::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_page_navigation) {
  // Same-page navigations (e.g. an in-document navigation from a fragment
  // link) aren't full page loads, since they don't go to network to load the
  // main HTML resource. DidStartProvisionalLoad doesn't get invoked for same
  // page navigations, so we may still have an active
  // page_timing_metrics_sender_ at this point.
  if (is_same_page_navigation)
    return;

  // Make sure to release the sender for a previous navigation, if we have one.
  page_timing_metrics_sender_.reset();

  // We only create a PageTimingMetricsSender if the page meets the criteria for
  // sending and recording metrics. Once page_timing_metrics_sender_ is
  // non-null, we will send metrics for the current page at some later time, as
  // those metrics become available.
  if (ShouldSendMetrics()) {
    PageLoadTiming timing;
    if (IsMainFrame()) {
      // Only populate PageLoadTiming for the main frame.
      timing = GetTiming();
      DCHECK(!timing.navigation_start.is_null());
    }
    page_timing_metrics_sender_.reset(
        new PageTimingMetricsSender(this, routing_id(), CreateTimer(), timing));
  }
}

void MetricsRenderFrameObserver::SendMetrics() {
  if (!page_timing_metrics_sender_)
    return;
  if (HasNoRenderFrame())
    return;
  PageLoadTiming timing(GetTiming());
  page_timing_metrics_sender_->Send(timing);
}

bool MetricsRenderFrameObserver::ShouldSendMetrics() const {
  if (HasNoRenderFrame())
    return false;
  const blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  const blink::WebDocument& document = frame->document();
  return RendererPageTrackDecider(&document, frame->dataSource()).ShouldTrack();
}

PageLoadTiming MetricsRenderFrameObserver::GetTiming() const {
  const blink::WebPerformance& perf =
      render_frame()->GetWebFrame()->performance();

  PageLoadTiming timing;
  double start = perf.navigationStart();
  timing.navigation_start = base::Time::FromDoubleT(start);
  if (perf.responseStart() > 0.0)
    timing.response_start = ClampDelta(perf.responseStart(), start);
  if (perf.domContentLoadedEventStart() > 0.0) {
    timing.dom_content_loaded_event_start =
        ClampDelta(perf.domContentLoadedEventStart(), start);
  }
  if (perf.loadEventStart() > 0.0)
    timing.load_event_start = ClampDelta(perf.loadEventStart(), start);
  if (perf.firstLayout() > 0.0)
    timing.first_layout = ClampDelta(perf.firstLayout(), start);
  if (perf.firstPaint() > 0.0)
    timing.first_paint = ClampDelta(perf.firstPaint(), start);
  if (perf.firstTextPaint() > 0.0)
    timing.first_text_paint = ClampDelta(perf.firstTextPaint(), start);
  if (perf.firstImagePaint() > 0.0)
    timing.first_image_paint = ClampDelta(perf.firstImagePaint(), start);
  if (perf.firstContentfulPaint() > 0.0) {
    timing.first_contentful_paint =
        ClampDelta(perf.firstContentfulPaint(), start);
  }
  if (perf.firstMeaningfulPaint() > 0.0) {
    timing.first_meaningful_paint =
        ClampDelta(perf.firstMeaningfulPaint(), start);
  }
  if (perf.parseStart() > 0.0)
    timing.parse_start = ClampDelta(perf.parseStart(), start);
  if (perf.parseStop() > 0.0)
    timing.parse_stop = ClampDelta(perf.parseStop(), start);
  if (timing.parse_start) {
    // If we started parsing, record all parser durations such as the amount of
    // time blocked on script load, even if those values are zero.
    timing.parse_blocked_on_script_load_duration =
        base::TimeDelta::FromSecondsD(perf.parseBlockedOnScriptLoadDuration());
    timing.parse_blocked_on_script_load_from_document_write_duration =
        base::TimeDelta::FromSecondsD(
            perf.parseBlockedOnScriptLoadFromDocumentWriteDuration());
    timing.parse_blocked_on_script_execution_duration =
        base::TimeDelta::FromSecondsD(
            perf.parseBlockedOnScriptExecutionDuration());
    timing.parse_blocked_on_script_execution_from_document_write_duration =
        base::TimeDelta::FromSecondsD(
            perf.parseBlockedOnScriptExecutionFromDocumentWriteDuration());
  }

  if (perf.authorStyleSheetParseDurationBeforeFCP() > 0.0) {
    timing.style_sheet_timing.author_style_sheet_parse_duration_before_fcp =
        base::TimeDelta::FromSecondsD(
            perf.authorStyleSheetParseDurationBeforeFCP());
  }
  if (perf.updateStyleDurationBeforeFCP() > 0.0) {
    timing.style_sheet_timing.update_style_duration_before_fcp =
        base::TimeDelta::FromSecondsD(perf.updateStyleDurationBeforeFCP());
  }
  return timing;
}

std::unique_ptr<base::Timer> MetricsRenderFrameObserver::CreateTimer() const {
  return base::WrapUnique(new base::OneShotTimer);
}

bool MetricsRenderFrameObserver::HasNoRenderFrame() const {
  bool no_frame = !render_frame() || !render_frame()->GetWebFrame();
  DCHECK(!no_frame);
  return no_frame;
}

void MetricsRenderFrameObserver::OnDestruct() {
  delete this;
}

bool MetricsRenderFrameObserver::IsMainFrame() const {
  return render_frame()->IsMainFrame();
}

}  // namespace page_load_metrics
