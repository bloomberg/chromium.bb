// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"

namespace internal {
const char kHistogramDocWriteFirstContentfulPaint[] =
    "PageLoad.Clients.DocWrite.Evaluator.Timing2."
    "NavigationToFirstContentfulPaint";
const char kHistogramDocWriteParseStartToFirstContentfulPaint[] =
    "PageLoad.Clients.DocWrite.Evaluator.Timing2."
    "ParseStartToFirstContentfulPaint";
const char kHistogramDocWriteParseDuration[] =
    "PageLoad.Clients.DocWrite.Evaluator.Timing2.ParseDuration";
const char kHistogramDocWriteParseBlockedOnScript[] =
    "PageLoad.Clients.DocWrite.Evaluator.Timing2.ParseBlockedOnScriptLoad";
const char kHistogramDocWriteParseBlockedOnScriptParseComplete[] =
    "PageLoad.Clients.DocWrite.Evaluator.Timing2.ParseBlockedOnScriptLoad."
    "ParseComplete";
const char kHistogramDocWriteParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.Clients.DocWrite.Evaluator.Timing2."
    "ParseBlockedOnScriptLoadFromDocumentWrite";
const char
    kHistogramDocWriteParseBlockedOnScriptLoadDocumentWriteParseComplete[] =
        "PageLoad.Clients.DocWrite.Evaluator.Timing2."
        "ParseBlockedOnScriptLoadFromDocumentWrite.ParseComplete";

const char kBackgroundHistogramDocWriteFirstContentfulPaint[] =
    "PageLoad.Clients.DocWrite.Evaluator.Timing2."
    "NavigationToFirstContentfulPaint."
    "Background";
const char kBackgroundHistogramDocWriteParseDuration[] =
    "PageLoad.Clients.DocWrite.Evaluator.Timing2.ParseDuration.Background";
const char kBackgroundHistogramDocWriteParseBlockedOnScript[] =
    "PageLoad.Clients.DocWrite.Evaluator.Timing2.ParseBlockedOnScriptLoad."
    "Background";
const char kBackgroundHistogramDocWriteParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.Clients.DocWrite.Evaluator.Timing2."
    "ParseBlockedOnScriptLoadFromDocumentWrite.Background";

}  // namespace internal

DocumentWritePageLoadMetricsObserver::DocumentWritePageLoadMetricsObserver() {}

void DocumentWritePageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (info.time_to_commit.is_zero() || timing.IsEmpty())
    return;
  if (info.metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::WebLoadingBehaviorDocumentWriteEvaluator) {
    LogDocumentWriteEvaluatorData(timing, info);
  }
}

void DocumentWritePageLoadMetricsObserver::LogDocumentWriteEvaluatorData(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  bool foreground_paint = WasStartedInForegroundEventInForeground(
      timing.first_contentful_paint, info);
  if (!timing.first_contentful_paint.is_zero()) {
    if (foreground_paint) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteFirstContentfulPaint,
                          timing.first_contentful_paint);
    } else {
      PAGE_LOAD_HISTOGRAM(
          internal::kBackgroundHistogramDocWriteFirstContentfulPaint,
          timing.first_contentful_paint);
    }
  }

  // Log parse based metrics.
  if (!timing.parse_start.is_zero()) {
    if (foreground_paint) {
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramDocWriteParseStartToFirstContentfulPaint,
          timing.first_contentful_paint - timing.parse_start);
    }

    const bool incomplete_parse_in_foreground =
        timing.parse_stop.is_zero() && info.started_in_foreground &&
        info.first_background_time.is_zero();
    if (incomplete_parse_in_foreground ||
        WasStartedInForegroundEventInForeground(timing.parse_stop, info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteParseBlockedOnScript,
                          timing.parse_blocked_on_script_load_duration);
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramDocWriteParseBlockedOnScriptLoadDocumentWrite,
          timing.parse_blocked_on_script_load_from_document_write_duration);
    } else {
      PAGE_LOAD_HISTOGRAM(
          internal::kBackgroundHistogramDocWriteParseBlockedOnScript,
          timing.parse_blocked_on_script_load_duration);
      PAGE_LOAD_HISTOGRAM(
          internal::
              kBackgroundHistogramDocWriteParseBlockedOnScriptLoadDocumentWrite,
          timing.parse_blocked_on_script_load_from_document_write_duration);
    }

    // These metrics require a full parse.
    if (!timing.parse_stop.is_zero()) {
      base::TimeDelta parse_duration = timing.parse_stop - timing.parse_start;
      if (WasStartedInForegroundEventInForeground(timing.parse_stop, info)) {
        PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteParseDuration,
                            parse_duration);
        PAGE_LOAD_HISTOGRAM(
            internal::kHistogramDocWriteParseBlockedOnScriptParseComplete,
            timing.parse_blocked_on_script_load_duration);
        PAGE_LOAD_HISTOGRAM(
            internal::
                kHistogramDocWriteParseBlockedOnScriptLoadDocumentWriteParseComplete,
            timing.parse_blocked_on_script_load_from_document_write_duration);
      } else {
        PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramDocWriteParseDuration,
                            parse_duration);
      }
    }
  }
}
