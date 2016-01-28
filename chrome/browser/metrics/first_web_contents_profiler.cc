// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(OS_ANDROID)

#include "chrome/browser/metrics/first_web_contents_profiler.h"

#include <string>

#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/metrics/profiler/tracking_synchronizer.h"
#include "components/metrics/proto/profiler_event.pb.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/navigation_handle.h"

// static
void FirstWebContentsProfiler::Start() {
  for (auto* browser : *BrowserList::GetInstance()) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (web_contents) {
      // FirstWebContentsProfiler owns itself and is also bound to
      // |web_contents|'s lifetime by observing WebContentsDestroyed().
      new FirstWebContentsProfiler(web_contents);
      return;
    }
  }
}

FirstWebContentsProfiler::FirstWebContentsProfiler(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      collected_paint_metric_(false),
      collected_load_metric_(false),
      collected_main_navigation_start_metric_(false),
      collected_main_navigation_finished_metric_(false) {}

void FirstWebContentsProfiler::DidFirstVisuallyNonEmptyPaint() {
  if (collected_paint_metric_)
    return;
  if (startup_metric_utils::WasNonBrowserUIDisplayed()) {
    FinishedCollectingMetrics(FinishReason::ABANDON_BLOCKING_UI);
    return;
  }

  collected_paint_metric_ = true;
  startup_metric_utils::RecordFirstWebContentsNonEmptyPaint(
      base::TimeTicks::Now());

  metrics::TrackingSynchronizer::OnProfilingPhaseCompleted(
      metrics::ProfilerEventProto::EVENT_FIRST_NONEMPTY_PAINT);

  if (IsFinishedCollectingMetrics())
    FinishedCollectingMetrics(FinishReason::DONE);
}

void FirstWebContentsProfiler::DocumentOnLoadCompletedInMainFrame() {
  if (collected_load_metric_)
    return;
  if (startup_metric_utils::WasNonBrowserUIDisplayed()) {
    FinishedCollectingMetrics(FinishReason::ABANDON_BLOCKING_UI);
    return;
  }

  collected_load_metric_ = true;
  startup_metric_utils::RecordFirstWebContentsMainFrameLoad(
      base::TimeTicks::Now());

  if (IsFinishedCollectingMetrics())
    FinishedCollectingMetrics(FinishReason::DONE);
}

void FirstWebContentsProfiler::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (collected_main_navigation_start_metric_)
    return;
  if (startup_metric_utils::WasNonBrowserUIDisplayed()) {
    FinishedCollectingMetrics(FinishReason::ABANDON_BLOCKING_UI);
    return;
  }

  // The first navigation has to be the main frame's.
  DCHECK(navigation_handle->IsInMainFrame());

  collected_main_navigation_start_metric_ = true;
  startup_metric_utils::RecordFirstWebContentsMainNavigationStart(
      base::TimeTicks::Now());
}

void FirstWebContentsProfiler::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (collected_main_navigation_finished_metric_) {
    // Abandon profiling on a top-level navigation to a different page as it:
    //   (1) is no longer a fair timing; and
    //   (2) can cause http://crbug.com/525209 where one of the timing
    //       heuristics (e.g. first paint) didn't fire for the initial content
    //       but fires after a lot of idle time when the user finally navigates
    //       to another page that does trigger it.
    if (navigation_handle->IsInMainFrame() &&
        navigation_handle->HasCommitted() &&
        !navigation_handle->IsSamePage()) {
      FinishedCollectingMetrics(FinishReason::ABANDON_NEW_NAVIGATION);
    }
    return;
  }

  if (startup_metric_utils::WasNonBrowserUIDisplayed()) {
    FinishedCollectingMetrics(FinishReason::ABANDON_BLOCKING_UI);
    return;
  }

  // The first navigation has to be the main frame's.
  DCHECK(navigation_handle->IsInMainFrame());

  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage()) {
    FinishedCollectingMetrics(FinishReason::ABANDON_NAVIGATION_ERROR);
    return;
  }

  collected_main_navigation_finished_metric_ = true;
  startup_metric_utils::RecordFirstWebContentsMainNavigationFinished(
      base::TimeTicks::Now());
}

void FirstWebContentsProfiler::WasHidden() {
  // Stop profiling if the content gets hidden as its load may be deprioritized
  // and timing it becomes meaningless.
  FinishedCollectingMetrics(FinishReason::ABANDON_CONTENT_HIDDEN);
}

void FirstWebContentsProfiler::WebContentsDestroyed() {
  FinishedCollectingMetrics(FinishReason::ABANDON_CONTENT_DESTROYED);
}

bool FirstWebContentsProfiler::IsFinishedCollectingMetrics() {
  return collected_paint_metric_ && collected_load_metric_;
}

void FirstWebContentsProfiler::FinishedCollectingMetrics(
    FinishReason finish_reason) {
  UMA_HISTOGRAM_ENUMERATION("Startup.FirstWebContents.FinishReason",
                            finish_reason, FinishReason::ENUM_MAX);
  if (!collected_paint_metric_) {
    UMA_HISTOGRAM_ENUMERATION("Startup.FirstWebContents.FinishReason_NoPaint",
                              finish_reason, FinishReason::ENUM_MAX);
  }
  if (!collected_load_metric_) {
    UMA_HISTOGRAM_ENUMERATION("Startup.FirstWebContents.FinishReason_NoLoad",
                              finish_reason, FinishReason::ENUM_MAX);
  }

  delete this;
}

#endif  // !defined(OS_ANDROID)
