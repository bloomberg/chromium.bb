// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
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

// document.write blocking histograms
const char kHistogramDocWriteBlockParseStartToFirstContentfulPaint[] =
    "PageLoad.Clients.DocWrite.Block.Timing2.ParseStartToFirstContentfulPaint";
const char kHistogramDocWriteBlockParseBlockedOnScript[] =
    "PageLoad.Clients.DocWrite.Block.Timing2.ParseBlockedOnScriptLoad";
const char kHistogramDocWriteBlockParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.Clients.DocWrite.Block.Timing2."
    "ParseBlockedOnScriptLoadFromDocumentWrite";
const char kHistogramDocWriteBlockParseDuration[] =
    "PageLoad.Clients.DocWrite.Block.Timing2.ParseDuration";
const char kHistogramDocWriteBlockParseBlockedOnScriptParseComplete[] =
    "PageLoad.Clients.DocWrite.Block.Timing2.ParseBlockedOnScriptLoad."
    "ParseComplete";
const char kDocWriteBlockParseBlockedOnScriptLoadDocumentWriteParseComplete[] =
    "PageLoad.Clients.DocWrite.Block.Timing2."
    "ParseBlockedOnScriptLoadFromDocumentWrite.ParseComplete";
const char kHistogramDocWriteBlockReloadCount[] =
    "PageLoad.Clients.DocWrite.Block.ReloadCount";

const char kBackgroundHistogramDocWriteBlockParseBlockedOnScript[] =
    "PageLoad.Clients.DocWrite.Block.Timing2.ParseBlockedOnScriptLoad."
    "Background";
const char kBackgroundHistogramDocWriteBlockParseBlockedOnScriptComplete[] =
    "PageLoad.Clients.DocWrite.Block.Timing2.ParseBlockedOnScriptLoad."
    "ParseComplete.Background";
const char kBackgroundDocWriteBlockParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.Clients.DocWrite.Block.Timing2."
    "ParseBlockedOnScriptLoadFromDocumentWrite.Background";
const char kBackgroundDocWriteBlockParseBlockedOnScriptLoadDocWriteComplete[] =
    "PageLoad.Clients.DocWrite.Block.Timing2."
    "ParseBlockedOnScriptLoadFromDocumentWrite.ParseComplete.Background";
const char kBackgroundHistogramDocWriteBlockParseDuration[] =
    "PageLoad.Clients.DocWrite.Block.Timing2.ParseDuration.Background";

// Histograms that are logged immediately on receiving timing/metadata update.
const char kHistogramDocWriteFirstContentfulPaintImmediate[] =
    "PageLoad.Clients.DocWrite.Evaluator.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramDocWriteParseStartToFirstContentfulPaintImmediate[] =
    "PageLoad.Clients.DocWrite.Evaluator.PaintTiming."
    "ParseStartToFirstContentfulPaint";
const char kHistogramDocWriteParseDurationImmediate[] =
    "PageLoad.Clients.DocWrite.Evaluator.ParseTiming.ParseDuration";
const char kHistogramDocWriteParseBlockedOnScriptImmediate[] =
    "PageLoad.Clients.DocWrite.Evaluator.ParseTiming.ParseBlockedOnScriptLoad";
const char kHistogramDocWriteParseBlockedOnScriptLoadDocumentWriteImmediate[] =
    "PageLoad.Clients.DocWrite.Evaluator.ParseTiming."
    "ParseBlockedOnScriptLoadFromDocumentWrite";
const char kBackgroundHistogramDocWriteFirstContentfulPaintImmediate[] =
    "PageLoad.Clients.DocWrite.Evaluator.PaintTiming."
    "NavigationToFirstContentfulPaint."
    "Background";
const char kBackgroundHistogramDocWriteParseDurationImmediate[] =
    "PageLoad.Clients.DocWrite.Evaluator.ParseTiming.ParseDuration.Background";
const char kBackgroundHistogramDocWriteParseBlockedOnScriptImmediate[] =
    "PageLoad.Clients.DocWrite.Evaluator.ParseTiming.ParseBlockedOnScriptLoad."
    "Background";
const char
    kBackgroundHistogramDocWriteParseBlockedOnScriptLoadDocumentWriteImmediate
        [] = "PageLoad.Clients.DocWrite.Evaluator.ParseTiming."
             "ParseBlockedOnScriptLoadFromDocumentWrite.Background";

const char kHistogramDocWriteBlockFirstContentfulPaintImmediate[] =
    "PageLoad.Clients.DocWrite.Block.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramDocWriteBlockParseStartToFirstContentfulPaintImmediate[] =
    "PageLoad.Clients.DocWrite.Block.PaintTiming."
    "ParseStartToFirstContentfulPaint";
const char kHistogramDocWriteBlockParseBlockedOnScriptImmediate[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseBlockedOnScriptLoad";
const char
    kHistogramDocWriteBlockParseBlockedOnScriptLoadDocumentWriteImmediate[] =
        "PageLoad.Clients.DocWrite.Block.ParseTiming."
        "ParseBlockedOnScriptLoadFromDocumentWrite";
const char kHistogramDocWriteBlockParseDurationImmediate[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseDuration";

const char kBackgroundHistogramDocWriteBlockParseBlockedOnScriptImmediate[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseBlockedOnScriptLoad."
    "Background";
const char
    kBackgroundDocWriteBlockParseBlockedOnScriptLoadDocumentWriteImmediate[] =
        "PageLoad.Clients.DocWrite.Block.ParseTiming."
        "ParseBlockedOnScriptLoadFromDocumentWrite.Background";
const char kBackgroundHistogramDocWriteBlockParseDurationImmediate[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseDuration.Background";
}  // namespace internal

DocumentWritePageLoadMetricsObserver::DocumentWritePageLoadMetricsObserver()
    : doc_write_block_reload_observed_(false) {}

void DocumentWritePageLoadMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (info.metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::WebLoadingBehaviorDocumentWriteEvaluator) {
    LogDocumentWriteEvaluatorFirstContentfulPaint(timing, info);
  }
  if (info.metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::WebLoadingBehaviorDocumentWriteBlock) {
    LogDocumentWriteBlockFirstContentfulPaint(timing, info);
  }
}

void DocumentWritePageLoadMetricsObserver::OnParseStop(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (info.metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::WebLoadingBehaviorDocumentWriteEvaluator) {
    LogDocumentWriteEvaluatorParseStop(timing, info);
  }
  if (info.metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::WebLoadingBehaviorDocumentWriteBlock) {
    LogDocumentWriteBlockParseStop(timing, info);
  }
}

void DocumentWritePageLoadMetricsObserver::OnLoadingBehaviorObserved(
    const page_load_metrics::PageLoadExtraInfo& info) {
  if ((info.metadata.behavior_flags &
       blink::WebLoadingBehaviorFlag::
           WebLoadingBehaviorDocumentWriteBlockReload) &&
      !doc_write_block_reload_observed_) {
    DCHECK(
        !(info.metadata.behavior_flags &
          blink::WebLoadingBehaviorFlag::WebLoadingBehaviorDocumentWriteBlock));
    UMA_HISTOGRAM_COUNTS(internal::kHistogramDocWriteBlockReloadCount, 1);
    doc_write_block_reload_observed_ = true;
  }
}

void DocumentWritePageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (timing.IsEmpty())
    return;

  if (info.metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::WebLoadingBehaviorDocumentWriteEvaluator) {
    LogDocumentWriteEvaluatorData(timing, info);
  }
  if (info.metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::WebLoadingBehaviorDocumentWriteBlock) {
    LogDocumentWriteBlockData(timing, info);
  }
}

void DocumentWritePageLoadMetricsObserver::
    LogDocumentWriteEvaluatorFirstContentfulPaint(
        const page_load_metrics::PageLoadTiming& timing,
        const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteFirstContentfulPaintImmediate,
        timing.first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteParseStartToFirstContentfulPaintImmediate,
        timing.first_contentful_paint.value() - timing.parse_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteFirstContentfulPaintImmediate,
        timing.first_contentful_paint.value());
  }
}

void DocumentWritePageLoadMetricsObserver::LogDocumentWriteEvaluatorParseStop(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  base::TimeDelta parse_duration =
      timing.parse_stop.value() - timing.parse_start.value();
  if (WasStartedInForegroundOptionalEventInForeground(timing.parse_stop,
                                                      info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteParseDurationImmediate,
                        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteParseBlockedOnScriptImmediate,
        timing.parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::
            kHistogramDocWriteParseBlockedOnScriptLoadDocumentWriteImmediate,
        timing.parse_blocked_on_script_load_from_document_write_duration
            .value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteParseDurationImmediate,
        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteParseBlockedOnScriptImmediate,
        timing.parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::
            kBackgroundHistogramDocWriteParseBlockedOnScriptLoadDocumentWriteImmediate,
        timing.parse_blocked_on_script_load_from_document_write_duration
            .value());
  }
}

void DocumentWritePageLoadMetricsObserver::LogDocumentWriteEvaluatorData(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  bool foreground_paint = WasStartedInForegroundOptionalEventInForeground(
      timing.first_contentful_paint, info);

  if (timing.first_contentful_paint) {
    if (foreground_paint) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteFirstContentfulPaint,
                          timing.first_contentful_paint.value());
    } else {
      PAGE_LOAD_HISTOGRAM(
          internal::kBackgroundHistogramDocWriteFirstContentfulPaint,
          timing.first_contentful_paint.value());
    }
  }

  // Log parse based metrics.
  if (!timing.parse_start)
    return;

  if (foreground_paint) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteParseStartToFirstContentfulPaint,
        timing.first_contentful_paint.value() - timing.parse_start.value());
  }

  if (WasParseInForeground(timing.parse_start, timing.parse_stop, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteParseBlockedOnScript,
                        timing.parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_blocked_on_script_load_from_document_write_duration
            .value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteParseBlockedOnScript,
        timing.parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::
            kBackgroundHistogramDocWriteParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_blocked_on_script_load_from_document_write_duration
            .value());
  }

  // These metrics require a full parse.
  if (!timing.parse_stop)
    return;

  base::TimeDelta parse_duration =
      timing.parse_stop.value() - timing.parse_start.value();
  if (WasStartedInForegroundOptionalEventInForeground(timing.parse_stop,
                                                      info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteParseDuration,
                        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteParseBlockedOnScriptParseComplete,
        timing.parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::
            kHistogramDocWriteParseBlockedOnScriptLoadDocumentWriteParseComplete,
        timing.parse_blocked_on_script_load_from_document_write_duration
            .value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramDocWriteParseDuration,
                        parse_duration);
  }
}

void DocumentWritePageLoadMetricsObserver::
    LogDocumentWriteBlockFirstContentfulPaint(
        const page_load_metrics::PageLoadTiming& timing,
        const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockFirstContentfulPaintImmediate,
        timing.first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::
            kHistogramDocWriteBlockParseStartToFirstContentfulPaintImmediate,
        timing.first_contentful_paint.value() - timing.parse_start.value());
  }
}

void DocumentWritePageLoadMetricsObserver::LogDocumentWriteBlockParseStop(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  base::TimeDelta parse_duration =
      timing.parse_stop.value() - timing.parse_start.value();
  if (WasStartedInForegroundOptionalEventInForeground(timing.parse_stop,
                                                      info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteBlockParseDurationImmediate,
                        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseBlockedOnScriptImmediate,
        timing.parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::
            kHistogramDocWriteBlockParseBlockedOnScriptLoadDocumentWriteImmediate,
        timing.parse_blocked_on_script_load_from_document_write_duration
            .value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteBlockParseDurationImmediate,
        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::
            kBackgroundHistogramDocWriteBlockParseBlockedOnScriptImmediate,
        timing.parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::
            kBackgroundDocWriteBlockParseBlockedOnScriptLoadDocumentWriteImmediate,
        timing.parse_blocked_on_script_load_from_document_write_duration
            .value());
  }
}

void DocumentWritePageLoadMetricsObserver::LogDocumentWriteBlockData(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!timing.parse_start) {
    return;
  }

  if (WasStartedInForegroundOptionalEventInForeground(
          timing.first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint,
        timing.first_contentful_paint.value() - timing.parse_start.value());
  }

  if (WasParseInForeground(timing.parse_start, timing.parse_stop, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteBlockParseBlockedOnScript,
                        timing.parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_blocked_on_script_load_from_document_write_duration
            .value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteBlockParseBlockedOnScript,
        timing.parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundDocWriteBlockParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_blocked_on_script_load_from_document_write_duration
            .value());
  }

  if (!timing.parse_stop) {
    return;
  }

  base::TimeDelta parse_duration =
      timing.parse_stop.value() - timing.parse_start.value();
  if (WasStartedInForegroundOptionalEventInForeground(timing.parse_stop,
                                                      info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteBlockParseDuration,
                        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseBlockedOnScriptParseComplete,
        timing.parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::
            kDocWriteBlockParseBlockedOnScriptLoadDocumentWriteParseComplete,
        timing.parse_blocked_on_script_load_from_document_write_duration
            .value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteBlockParseDuration,
        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteBlockParseBlockedOnScriptComplete,
        timing.parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::
            kBackgroundDocWriteBlockParseBlockedOnScriptLoadDocWriteComplete,
        timing.parse_blocked_on_script_load_from_document_write_duration
            .value());
  }
}
