// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"

using page_load_metrics::UserAbortType;

namespace internal {

const char kHistogramAbortClientRedirectBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.ClientRedirect.BeforeCommit";
const char kHistogramAbortForwardBackBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.ForwardBackNavigation.BeforeCommit";
const char kHistogramAbortReloadBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.Reload.BeforeCommit";
const char kHistogramAbortNewNavigationBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.NewNavigation.BeforeCommit";
const char kHistogramAbortStopBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.Stop.BeforeCommit";
const char kHistogramAbortCloseBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.Close.BeforeCommit";
const char kHistogramAbortBackgroundBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.Background.BeforeCommit";
const char kHistogramAbortOtherBeforeCommit[] =
    "PageLoad.Experimental.AbortTiming.Other.BeforeCommit";

const char kHistogramAbortClientRedirectBeforePaint[] =
    "PageLoad.Experimental.AbortTiming.ClientRedirect.AfterCommit.BeforePaint";
const char kHistogramAbortForwardBackBeforePaint[] =
    "PageLoad.Experimental.AbortTiming.ForwardBackNavigation.AfterCommit."
    "BeforePaint";
const char kHistogramAbortReloadBeforePaint[] =
    "PageLoad.Experimental.AbortTiming.Reload.AfterCommit.BeforePaint";
const char kHistogramAbortNewNavigationBeforePaint[] =
    "PageLoad.Experimental.AbortTiming.NewNavigation.AfterCommit.BeforePaint";
const char kHistogramAbortStopBeforePaint[] =
    "PageLoad.Experimental.AbortTiming.Stop.AfterCommit.BeforePaint";
const char kHistogramAbortCloseBeforePaint[] =
    "PageLoad.Experimental.AbortTiming.Close.AfterCommit.BeforePaint";
const char kHistogramAbortBackgroundBeforePaint[] =
    "PageLoad.Experimental.AbortTiming.Background.AfterCommit.BeforePaint";

const char kHistogramAbortClientRedirectDuringParse[] =
    "PageLoad.Experimental.AbortTiming.ClientRedirect.DuringParse";
const char kHistogramAbortForwardBackDuringParse[] =
    "PageLoad.Experimental.AbortTiming.ForwardBackNavigation.DuringParse";
const char kHistogramAbortReloadDuringParse[] =
    "PageLoad.Experimental.AbortTiming.Reload.DuringParse";
const char kHistogramAbortNewNavigationDuringParse[] =
    "PageLoad.Experimental.AbortTiming.NewNavigation.DuringParse";
const char kHistogramAbortStopDuringParse[] =
    "PageLoad.Experimental.AbortTiming.Stop.DuringParse";
const char kHistogramAbortCloseDuringParse[] =
    "PageLoad.Experimental.AbortTiming.Close.DuringParse";
const char kHistogramAbortBackgroundDuringParse[] =
    "PageLoad.Experimental.AbortTiming.Background.DuringParse";

// These metrics should be temporary until we have landed on a one-size-fits-all
// abort metric.
const char kHistogramAbortNewNavigationUserInitiated[] =
    "PageLoad.Experimental.AbortTiming.NewNavigation.BeforeCommit."
    "UserInitiated";
const char kHistogramAbortForwardBackUserInitiated[] =
    "PageLoad.Experimental.AbortTiming.ForwardBackNavigation.BeforeCommit."
    "UserInitiated";
const char kHistogramAbortReloadUserInitiated[] =
    "PageLoad.Experimental.AbortTiming.Reload.BeforeCommit.UserInitiated";

}  // namespace internal

namespace {

bool IsAbortUserInitiated(const page_load_metrics::PageLoadExtraInfo& info) {
  // We consider an abort to be user initiated if the abort was triggered by a
  // user action, and the page load being aborted was also user initiated. A
  // user may abort a non-user-initiated page load, but we exclude these from
  // our user initiated abort tracking since it's less clear that such an abort
  // is interesting from a user perspective.
  return info.abort_user_initiated && info.user_initiated;
}

void RecordAbortBeforeCommit(UserAbortType abort_type,
                             bool user_initiated,
                             base::TimeDelta time_to_abort) {
  switch (abort_type) {
    case UserAbortType::ABORT_RELOAD:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortReloadBeforeCommit,
                          time_to_abort);
      if (user_initiated) {
        PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortReloadUserInitiated,
                            time_to_abort);
      }
      return;
    case UserAbortType::ABORT_FORWARD_BACK:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortForwardBackBeforeCommit,
                          time_to_abort);
      if (user_initiated) {
        PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortForwardBackUserInitiated,
                            time_to_abort);
      }
      return;
    case UserAbortType::ABORT_CLIENT_REDIRECT:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortClientRedirectBeforeCommit,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_NEW_NAVIGATION:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortNewNavigationBeforeCommit,
                          time_to_abort);
      if (user_initiated) {
        PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortNewNavigationUserInitiated,
                            time_to_abort);
      }
      return;
    case UserAbortType::ABORT_STOP:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortStopBeforeCommit,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_CLOSE:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortCloseBeforeCommit,
                          time_to_abort);
      return;
    case UserAbortType::ABORT_BACKGROUND:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortBackgroundBeforeCommit,
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
    case UserAbortType::ABORT_CLIENT_REDIRECT:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortClientRedirectBeforePaint,
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
    case UserAbortType::ABORT_BACKGROUND:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortBackgroundBeforePaint,
                          time_to_abort);
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
    case UserAbortType::ABORT_CLIENT_REDIRECT:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortClientRedirectDuringParse,
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
    case UserAbortType::ABORT_BACKGROUND:
      PAGE_LOAD_HISTOGRAM(internal::kHistogramAbortBackgroundDuringParse,
                          time_to_abort);
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
                          IsAbortUserInitiated(extra_info),
                          extra_info.time_to_abort.value());
}
