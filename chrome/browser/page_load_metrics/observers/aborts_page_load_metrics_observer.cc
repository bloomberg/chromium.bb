// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"

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
const char kHistogramAbortUnknownNavigationBeforeCommit[] =
    "PageLoad.AbortTiming.UnknownNavigation.BeforeCommit";

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

const char kHistogramAbortForwardBackDuringParse[] =
    "PageLoad.AbortTiming.ForwardBackNavigation.DuringParse";
const char kHistogramAbortReloadDuringParse[] =
    "PageLoad.AbortTiming.Reload.DuringParse";
const char kHistogramAbortNewNavigationDuringParse[] =
    "PageLoad.AbortTiming.NewNavigation.DuringParse";
const char kHistogramAbortStopDuringParse[] =
    "PageLoad.AbortTiming.Stop.DuringParse";
const char kHistogramAbortCloseDuringParse[] =
    "PageLoad.AbortTiming.Close.DuringParse";

}  // namespace internal

namespace {

void RecordAbortBeforeCommit(UserAbortType abort_type,
                             base::TimeDelta time_to_abort) {
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
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortCloseBeforeCommit,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_OTHER:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortOtherBeforeCommit,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_UNKNOWN_NAVIGATION:
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramAbortUnknownNavigationBeforeCommit,
          time_to_abort);
      return;
    case UserAbortType::ABORT_NONE:
    case UserAbortType::ABORT_LAST_ENTRY:
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

void RecordAbortAfterCommitBeforePaint(UserAbortType abort_type,
                                       base::TimeDelta time_to_abort) {
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
    case UserAbortType::ABORT_UNKNOWN_NAVIGATION:
      NOTREACHED() << "Received UserAbortType::ABORT_UNKNOWN_NAVIGATION for "
                      "committed load.";
      return;
    case UserAbortType::ABORT_OTHER:
      NOTREACHED() << "Received UserAbortType::ABORT_OTHER for committed load.";
      return;
    case UserAbortType::ABORT_NONE:
    case UserAbortType::ABORT_LAST_ENTRY:
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

void RecordAbortDuringParse(UserAbortType abort_type,
                            base::TimeDelta time_to_abort) {
  switch (abort_type) {
    case UserAbortType::ABORT_RELOAD:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortReloadDuringParse,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_FORWARD_BACK:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortForwardBackDuringParse,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_NEW_NAVIGATION:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortNewNavigationDuringParse,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_STOP:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortStopDuringParse,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_CLOSE:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortCloseDuringParse,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_UNKNOWN_NAVIGATION:
      NOTREACHED() << "Received UserAbortType::ABORT_UNKNOWN_NAVIGATION for "
                      "committed load.";
      return;
    case UserAbortType::ABORT_OTHER:
      NOTREACHED() << "Received UserAbortType::ABORT_OTHER for committed load.";
      return;
    case UserAbortType::ABORT_NONE:
    case UserAbortType::ABORT_LAST_ENTRY:
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

bool ShouldTrackMetrics(
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  UserAbortType abort_type = extra_info.abort_type;
  if (abort_type == UserAbortType::ABORT_NONE)
    return false;

  DCHECK(extra_info.time_to_abort);

  // Don't log abort times if the page was backgrounded before the abort event.
  if (!WasStartedInForegroundOptionalEventInForeground(extra_info.time_to_abort,
                                                       extra_info))
    return false;

  return true;
}

}  // namespace

AbortsPageLoadMetricsObserver::AbortsPageLoadMetricsObserver() {}

void AbortsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (!ShouldTrackMetrics(extra_info))
    return;

  // If we did not receive any timing IPCs from the render process, we can't
  // know for certain if the page was truly aborted before paint, or if the
  // abort happened before we received the IPC from the render process. Thus, we
  // do not log aborts for these page loads. Tracked page loads that receive no
  // timing IPCs are tracked via the ERR_NO_IPCS_RECEIVED error code in the
  // PageLoad.Events.InternalError histogram, so we can keep track of how often
  // this happens.
  if (timing.IsEmpty())
    return;

  const base::TimeDelta& time_to_abort = extra_info.time_to_abort.value();
  if (timing.parse_start && time_to_abort >= timing.parse_start &&
      (!timing.parse_stop || timing.parse_stop >= time_to_abort)) {
    RecordAbortDuringParse(extra_info.abort_type, time_to_abort);
  }
  if (!timing.first_paint || timing.first_paint >= time_to_abort) {
    RecordAbortAfterCommitBeforePaint(extra_info.abort_type, time_to_abort);
  }
}

void AbortsPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (!ShouldTrackMetrics(extra_info))
    return;

  RecordAbortBeforeCommit(extra_info.abort_type,
                          extra_info.time_to_abort.value());
}
