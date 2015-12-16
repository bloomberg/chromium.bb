// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"

using page_load_metrics::UserAbortType;

namespace internal {

const char kHistogramAbortForwardBackBeforeCommit[] =
    "PageLoad.AbortTiming.ForwardBackNavigation.BeforeCommit";
const char kHistogramAbortReloadBeforeCommit[] =
    "PageLoad.AbortTiming.Reload.BeforeCommit";
const char kHistogramAbortNewNavigationBeforeCommit[] =
    "PageLoad.AbortTiming.NewNavigation.BeforeCommit";
const char kHistogramAbortStopBeforeCommit[] =
    "PageLoad.AbortTiming.Stop.BeforeCommit";
const char kHistogramAbortCloseBeforeCommit[] =
    "PageLoad.AbortTiming.Close.BeforeCommit";
const char kHistogramAbortOtherBeforeCommit[] =
    "PageLoad.AbortTiming.Other.BeforeCommit";
const char kHistogramAbortForwardBackBeforePaint[] =
    "PageLoad.AbortTiming.ForwardBackNavigation.AfterCommit.BeforePaint";
const char kHistogramAbortReloadBeforePaint[] =
    "PageLoad.AbortTiming.Reload.AfterCommit.BeforePaint";
const char kHistogramAbortNewNavigationBeforePaint[] =
    "PageLoad.AbortTiming.NewNavigation.AfterCommit.BeforePaint";
const char kHistogramAbortStopBeforePaint[] =
    "PageLoad.AbortTiming.Stop.AfterCommit.BeforePaint";
const char kHistogramAbortCloseBeforePaint[] =
    "PageLoad.AbortTiming.Close.AfterCommit.BeforePaint";

}  // namespace internal

AbortsPageLoadMetricsObserver::AbortsPageLoadMetricsObserver() {}

void AbortsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  UserAbortType abort_type = extra_info.abort_type;
  if (abort_type == UserAbortType::ABORT_NONE)
    return;

  const base::TimeDelta& time_to_abort = extra_info.time_to_abort;
  DCHECK(!time_to_abort.is_zero());

  // Loads are not considered aborts if they painted before the abort event.
  if (!timing.first_paint.is_zero() && timing.first_paint < time_to_abort)
    return;

  // Don't log abort times if the page was backgrounded before the abort event.
  if (!EventOccurredInForeground(time_to_abort, extra_info))
    return;

  // If |timing.IsEmpty()|, then this load was not tracked by the renderer. It
  // is impossible to know whether the abort signals came before the page
  // painted.
  if (extra_info.has_commit && !timing.IsEmpty()) {
    switch (abort_type) {
      case UserAbortType::ABORT_RELOAD:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortReloadBeforePaint,
                            time_to_abort);
        return;
      case UserAbortType::ABORT_FORWARD_BACK:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortForwardBackBeforePaint,
                            time_to_abort);
        return;
      case UserAbortType::ABORT_NEW_NAVIGATION:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortNewNavigationBeforePaint,
                            time_to_abort);
        return;
      case UserAbortType::ABORT_STOP:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortStopBeforePaint,
                            time_to_abort);
        return;
      case UserAbortType::ABORT_CLOSE:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortCloseBeforePaint,
                            time_to_abort);
        return;
      case UserAbortType::ABORT_OTHER:
        DLOG(FATAL)
            << "Received UserAbortType::ABORT_OTHER for committed load.";
        return;
      case UserAbortType::ABORT_NONE:
      case UserAbortType::ABORT_LAST_ENTRY:
        NOTREACHED();
        return;
    }
  } else if (extra_info.has_commit) {
    // This load was not tracked by the renderer.
    return;
  }
  // Handle provisional aborts.
  DCHECK(timing.IsEmpty());
  switch (abort_type) {
    case UserAbortType::ABORT_RELOAD:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortReloadBeforeCommit,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_FORWARD_BACK:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortForwardBackBeforeCommit,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_NEW_NAVIGATION:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortNewNavigationBeforeCommit,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_STOP:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortStopBeforeCommit,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_CLOSE:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortCloseBeforePaint,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_OTHER:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortOtherBeforeCommit,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_NONE:
    case UserAbortType::ABORT_LAST_ENTRY:
      NOTREACHED();
      return;
  }
}
