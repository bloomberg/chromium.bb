// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"

namespace internal {
const char kHistogramDocWriteFirstContentfulPaint[] =
    "PageLoad.Clients.DocWrite.Evaluator.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramDocWriteParseStartToFirstContentfulPaint[] =
    "PageLoad.Clients.DocWrite.Evaluator.PaintTiming."
    "ParseStartToFirstContentfulPaint";
const char kHistogramDocWriteParseDuration[] =
    "PageLoad.Clients.DocWrite.Evaluator.ParseTiming.ParseDuration";
const char kHistogramDocWriteParseBlockedOnScriptLoad[] =
    "PageLoad.Clients.DocWrite.Evaluator.ParseTiming.ParseBlockedOnScriptLoad";
const char kHistogramDocWriteParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.Clients.DocWrite.Evaluator.ParseTiming."
    "ParseBlockedOnScriptLoadFromDocumentWrite";
const char kHistogramDocWriteParseBlockedOnScriptExecution[] =
    "PageLoad.Clients.DocWrite.Evaluator.ParseTiming."
    "ParseBlockedOnScriptExecution";
const char kHistogramDocWriteParseBlockedOnScriptExecutionDocumentWrite[] =
    "PageLoad.Clients.DocWrite.Evaluator.ParseTiming."
    "ParseBlockedOnScriptExecutionFromDocumentWrite";
const char kBackgroundHistogramDocWriteFirstContentfulPaint[] =
    "PageLoad.Clients.DocWrite.Evaluator.PaintTiming."
    "NavigationToFirstContentfulPaint."
    "Background";
const char kBackgroundHistogramDocWriteParseDuration[] =
    "PageLoad.Clients.DocWrite.Evaluator.ParseTiming.ParseDuration.Background";
const char kBackgroundHistogramDocWriteParseBlockedOnScriptLoad[] =
    "PageLoad.Clients.DocWrite.Evaluator.ParseTiming.ParseBlockedOnScriptLoad."
    "Background";
const char kBackgroundHistogramDocWriteParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.Clients.DocWrite.Evaluator.ParseTiming."
    "ParseBlockedOnScriptLoadFromDocumentWrite.Background";

const char kHistogramDocWriteBlockFirstContentfulPaint[] =
    "PageLoad.Clients.DocWrite.Block.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramDocWriteBlockParseStartToFirstContentfulPaint[] =
    "PageLoad.Clients.DocWrite.Block.PaintTiming."
    "ParseStartToFirstContentfulPaint";
const char kHistogramDocWriteBlockParseBlockedOnScriptLoad[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseBlockedOnScriptLoad";
const char kHistogramDocWriteBlockParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming."
    "ParseBlockedOnScriptLoadFromDocumentWrite";
const char kHistogramDocWriteBlockParseBlockedOnScriptExecution[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseBlockedOnScriptExecution";
const char kHistogramDocWriteBlockParseBlockedOnScriptExecutionDocumentWrite[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming."
    "ParseBlockedOnScriptExecutionFromDocumentWrite";
const char kHistogramDocWriteBlockParseDuration[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseDuration";

const char kBackgroundHistogramDocWriteBlockParseBlockedOnScriptLoad[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseBlockedOnScriptLoad."
    "Background";
const char kBackgroundDocWriteBlockParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming."
    "ParseBlockedOnScriptLoadFromDocumentWrite.Background";
const char kBackgroundHistogramDocWriteBlockParseDuration[] =
    "PageLoad.Clients.DocWrite.Block.ParseTiming.ParseDuration.Background";

const char kHistogramDocWriteBlockCount[] =
    "PageLoad.Clients.DocWrite.Block.Count";
const char kHistogramDocWriteBlockReloadCount[] =
    "PageLoad.Clients.DocWrite.Block.ReloadCount";
const char kHistogramDocWriteBlockLoadingBehavior[] =
    "PageLoad.Clients.DocWrite.Block.DocumentWriteLoadingBehavior";

const char kUkmDocWriteBlockName[] = "Intervention.DocumentWrite.ScriptBlock";
const char kUkmDocWriteBlockReload[] = "Disabled.Reload";
const char kUkmParseBlockedOnScriptLoadDocumentWrite[] =
    "ParseTiming.ParseBlockedOnScriptLoadFromDocumentWrite";
const char kUkmParseBlockedOnScriptExecutionDocumentWrite[] =
    "ParseTiming.ParseBlockedOnScriptExecutionFromDocumentWrite";

}  // namespace internal

void DocumentWritePageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (info.main_frame_metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::
          kWebLoadingBehaviorDocumentWriteEvaluator) {
    LogDocumentWriteEvaluatorFirstContentfulPaint(timing, info);
  }
  if (info.main_frame_metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorDocumentWriteBlock) {
    LogDocumentWriteBlockFirstContentfulPaint(timing, info);
  }
}

void DocumentWritePageLoadMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing,
        const page_load_metrics::PageLoadExtraInfo& info) {
  if (info.main_frame_metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::
          kWebLoadingBehaviorDocumentWriteEvaluator) {
    LogDocumentWriteEvaluatorFirstMeaningfulPaint(timing, info);
  }
  if (info.main_frame_metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorDocumentWriteBlock) {
    LogDocumentWriteBlockFirstMeaningfulPaint(timing, info);
  }
}

void DocumentWritePageLoadMetricsObserver::OnParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (info.main_frame_metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::
          kWebLoadingBehaviorDocumentWriteEvaluator) {
    LogDocumentWriteEvaluatorParseStop(timing, info);
  }
  if (info.main_frame_metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorDocumentWriteBlock) {
    LogDocumentWriteBlockParseStop(timing, info);
  }
}

// static
void DocumentWritePageLoadMetricsObserver::LogLoadingBehaviorMetrics(
    DocumentWritePageLoadMetricsObserver::DocumentWriteLoadingBehavior behavior,
    ukm::SourceId source_id) {
  UMA_HISTOGRAM_ENUMERATION(
      internal::kHistogramDocWriteBlockLoadingBehavior, behavior,
      DocumentWritePageLoadMetricsObserver::LOADING_BEHAVIOR_MAX);

  // We only log the block and reload behaviors in UKM.
  if (behavior != LOADING_BEHAVIOR_BLOCK &&
      behavior != LOADING_BEHAVIOR_RELOAD) {
    return;
  }
  ukm::UkmRecorder* ukm_recorder = g_browser_process->ukm_recorder();
  if (ukm_recorder) {
    std::unique_ptr<ukm::UkmEntryBuilder> builder =
        ukm_recorder->GetEntryBuilder(source_id,
                                      internal::kUkmDocWriteBlockName);
    if (behavior == LOADING_BEHAVIOR_RELOAD) {
      builder->AddMetric(internal::kUkmDocWriteBlockReload, true);
    }
  }
}

void DocumentWritePageLoadMetricsObserver::OnLoadingBehaviorObserved(
    const page_load_metrics::PageLoadExtraInfo& info) {
  if ((info.main_frame_metadata.behavior_flags &
       blink::WebLoadingBehaviorFlag::
           kWebLoadingBehaviorDocumentWriteBlockReload) &&
      !doc_write_block_reload_observed_) {
    DCHECK(!(
        info.main_frame_metadata.behavior_flags &
        blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorDocumentWriteBlock));
    UMA_HISTOGRAM_COUNTS(internal::kHistogramDocWriteBlockReloadCount, 1);
    LogLoadingBehaviorMetrics(LOADING_BEHAVIOR_RELOAD, info.source_id);
    doc_write_block_reload_observed_ = true;
  }
  if ((info.main_frame_metadata.behavior_flags &
       blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorDocumentWriteBlock) &&
      !doc_write_block_observed_) {
    UMA_HISTOGRAM_BOOLEAN(internal::kHistogramDocWriteBlockCount, true);
    LogLoadingBehaviorMetrics(LOADING_BEHAVIOR_BLOCK, info.source_id);
    doc_write_block_observed_ = true;
  }
  if ((info.main_frame_metadata.behavior_flags &
       blink::WebLoadingBehaviorFlag::
           kWebLoadingBehaviorDocumentWriteBlockDifferentScheme) &&
      !doc_write_same_site_diff_scheme_) {
    LogLoadingBehaviorMetrics(LOADING_BEHAVIOR_SAME_SITE_DIFF_SCHEME,
                              info.source_id);
    doc_write_same_site_diff_scheme_ = true;
  }
}

void DocumentWritePageLoadMetricsObserver::
    LogDocumentWriteEvaluatorFirstContentfulPaint(
        const page_load_metrics::mojom::PageLoadTiming& timing,
        const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteParseStartToFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value() -
            timing.parse_timing->parse_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value());
  }
}

// Note: The first meaningful paint calculation in the core observer filters
// out pages which had user interaction before the first meaningful paint.
// Because the counts of those instances are low (< 2%), just log everything
// here for simplicity. If this ends up being unreliable (the 2% is just from
// canary), the page_load_metrics API should be altered to return the values
// the consumer wants.
void DocumentWritePageLoadMetricsObserver::
    LogDocumentWriteEvaluatorFirstMeaningfulPaint(
        const page_load_metrics::mojom::PageLoadTiming& timing,
        const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.DocWrite.Evaluator.Experimental.PaintTiming."
        "ParseStartToFirstMeaningfulPaint",
        timing.paint_timing->first_meaningful_paint.value() -
            timing.parse_timing->parse_start.value());
  }
}

void DocumentWritePageLoadMetricsObserver::
    LogDocumentWriteBlockFirstMeaningfulPaint(
        const page_load_metrics::mojom::PageLoadTiming& timing,
        const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.DocWrite.Block.Experimental.PaintTiming."
        "ParseStartToFirstMeaningfulPaint",
        timing.paint_timing->first_meaningful_paint.value() -
            timing.parse_timing->parse_start.value());
  }
}

void DocumentWritePageLoadMetricsObserver::LogDocumentWriteEvaluatorParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  base::TimeDelta parse_duration = timing.parse_timing->parse_stop.value() -
                                   timing.parse_timing->parse_start.value();
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteParseDuration,
                        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteParseBlockedOnScriptLoad,
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_timing
            ->parse_blocked_on_script_load_from_document_write_duration
            .value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteParseBlockedOnScriptExecution,
        timing.parse_timing->parse_blocked_on_script_execution_duration
            .value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteParseBlockedOnScriptExecutionDocumentWrite,
        timing.parse_timing
            ->parse_blocked_on_script_execution_from_document_write_duration
            .value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramDocWriteParseDuration,
                        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteParseBlockedOnScriptLoad,
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::
            kBackgroundHistogramDocWriteParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_timing
            ->parse_blocked_on_script_load_from_document_write_duration
            .value());
  }
}

void DocumentWritePageLoadMetricsObserver::
    LogDocumentWriteBlockFirstContentfulPaint(
        const page_load_metrics::mojom::PageLoadTiming& timing,
        const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteBlockFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value() -
            timing.parse_timing->parse_start.value());
  }
}

void DocumentWritePageLoadMetricsObserver::LogDocumentWriteBlockParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  base::TimeDelta parse_duration = timing.parse_timing->parse_stop.value() -
                                   timing.parse_timing->parse_start.value();
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDocWriteBlockParseDuration,
                        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseBlockedOnScriptLoad,
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_timing
            ->parse_blocked_on_script_load_from_document_write_duration
            .value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramDocWriteBlockParseBlockedOnScriptExecution,
        timing.parse_timing->parse_blocked_on_script_execution_duration
            .value());
    PAGE_LOAD_HISTOGRAM(
        internal::
            kHistogramDocWriteBlockParseBlockedOnScriptExecutionDocumentWrite,
        timing.parse_timing
            ->parse_blocked_on_script_execution_from_document_write_duration
            .value());

    ukm::UkmRecorder* ukm_recorder = g_browser_process->ukm_recorder();
    if (ukm_recorder) {
      std::unique_ptr<ukm::UkmEntryBuilder> builder =
          ukm_recorder->GetEntryBuilder(info.source_id,
                                        internal::kUkmDocWriteBlockName);
      builder->AddMetric(
          internal::kUkmParseBlockedOnScriptLoadDocumentWrite,
          timing.parse_timing
              ->parse_blocked_on_script_load_from_document_write_duration
              ->InMilliseconds());
      builder->AddMetric(
          internal::kUkmParseBlockedOnScriptExecutionDocumentWrite,
          timing.parse_timing
              ->parse_blocked_on_script_execution_from_document_write_duration
              ->InMilliseconds());
    }

  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteBlockParseDuration,
        parse_duration);
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramDocWriteBlockParseBlockedOnScriptLoad,
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundDocWriteBlockParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_timing
            ->parse_blocked_on_script_load_from_document_write_duration
            .value());
  }
}
