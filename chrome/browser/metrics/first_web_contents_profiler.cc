// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(OS_ANDROID)

#include "chrome/browser/metrics/first_web_contents_profiler.h"

#include <string>

#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_info.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/metrics/profiler/tracking_synchronizer.h"
#include "components/metrics/proto/profiler_event.pb.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"

namespace {

// The initial delay for responsiveness prober in milliseconds.
const int kInitialDelayMs = 20;

// The following is the multiplier is used to delay the probe for
// responsiveness.
const double kBackoffMultiplier = 1.5;

// The maximum backoff delay in milliseconds.
const int kMaxDelayMs = 250;

void DelayedRecordUIResponsiveness(
    base::HistogramBase* responsiveness_histogram,
    base::HistogramBase* unresponsiveness_histogram,
    base::Time start_recording_time,
    base::TimeDelta next_task_delay);

// Records the elapsed time for a task to execute a UI task under 1/60s after
// the first WebContent was painted at least once. If after few tries it is
// unable to execute under 1/60s it records the execution time of a task.
void RecordUIResponsiveness(base::HistogramBase* responsiveness_histogram,
                            base::HistogramBase* unresponsiveness_histogram,
                            base::Time start_recording_time,
                            base::Time task_posted_time,
                            base::TimeDelta next_task_delay) {
  DCHECK(!start_recording_time.is_null());
  DCHECK(!task_posted_time.is_null());
  base::Time now = base::Time::Now();
  base::TimeDelta elapsed = now - task_posted_time;

  // Task executed in less then 1/60s.
  if (elapsed.InMilliseconds() <= (1000 / 60)) {
    responsiveness_histogram->AddTime(now - start_recording_time);
  } else if (next_task_delay.InMilliseconds() > kMaxDelayMs) {
    // Records elapsed time to execute last task.
    unresponsiveness_histogram->AddTime(elapsed);
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&DelayedRecordUIResponsiveness, responsiveness_histogram,
                   unresponsiveness_histogram, start_recording_time,
                   next_task_delay * kBackoffMultiplier),
        next_task_delay);
  }
}

// Used for posting |RecordUIResponsiveness| without delay, so that
// |RecordUIResponsiveness| can do more accurate time calculation for elapsed
// time of the task to complete.
void DelayedRecordUIResponsiveness(
    base::HistogramBase* responsiveness_histogram,
    base::HistogramBase* unresponsiveness_histogram,
    base::Time start_recording_time,
    base::TimeDelta next_task_delay) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&RecordUIResponsiveness, responsiveness_histogram,
                            unresponsiveness_histogram, start_recording_time,
                            base::Time::Now(), next_task_delay));
}

// Sends the first task for measuring UI Responsiveness.
void MeasureUIResponsiveness(base::HistogramBase* responsiveness_histogram,
                             base::HistogramBase* unresponsiveness_histogram) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&RecordUIResponsiveness, responsiveness_histogram,
                 unresponsiveness_histogram, base::Time::Now(),
                 base::Time::Now(),
                 base::TimeDelta::FromMilliseconds(kInitialDelayMs)));
}

}  // namespace

scoped_ptr<FirstWebContentsProfiler>
FirstWebContentsProfiler::CreateProfilerForFirstWebContents(
    Delegate* delegate) {
  DCHECK(delegate);
  for (chrome::BrowserIterator iterator; !iterator.done(); iterator.Next()) {
    Browser* browser = *iterator;
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (web_contents) {
      return scoped_ptr<FirstWebContentsProfiler>(
          new FirstWebContentsProfiler(web_contents, delegate));
    }
  }
  return nullptr;
}

FirstWebContentsProfiler::FirstWebContentsProfiler(
    content::WebContents* web_contents,
    Delegate* delegate)
    : content::WebContentsObserver(web_contents),
      initial_entry_committed_(false),
      collected_paint_metric_(false),
      collected_load_metric_(false),
      delegate_(delegate),
      responsiveness_histogram_(NULL),
      responsiveness_1sec_histogram_(NULL),
      responsiveness_10sec_histogram_(NULL),
      unresponsiveness_histogram_(NULL),
      unresponsiveness_1sec_histogram_(NULL),
      unresponsiveness_10sec_histogram_(NULL) {
  InitHistograms();
}

void FirstWebContentsProfiler::DidFirstVisuallyNonEmptyPaint() {
  if (collected_paint_metric_)
    return;
  if (startup_metric_utils::WasNonBrowserUIDisplayed()) {
    FinishedCollectingMetrics(FinishReason::ABANDON_BLOCKING_UI);
    return;
  }

  collected_paint_metric_ = true;
  startup_metric_utils::RecordFirstWebContentsNonEmptyPaint(base::Time::Now());

  metrics::TrackingSynchronizer::OnProfilingPhaseCompleted(
      metrics::ProfilerEventProto::EVENT_FIRST_NONEMPTY_PAINT);

  // Measures responsiveness now.
  MeasureUIResponsiveness(responsiveness_histogram_,
                          unresponsiveness_histogram_);

  // As it was observed that sometimes the task queue can be free immediately
  // after the first paint but get overloaded shortly thereafter, here we
  // measures responsiveness after 1 second and 10 seconds to observe the
  // possible effect of those late tasks.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MeasureUIResponsiveness, responsiveness_1sec_histogram_,
                 unresponsiveness_1sec_histogram_),
      base::TimeDelta::FromSeconds(1));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MeasureUIResponsiveness, responsiveness_10sec_histogram_,
                 unresponsiveness_10sec_histogram_),
      base::TimeDelta::FromSeconds(10));

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
  startup_metric_utils::RecordFirstWebContentsMainFrameLoad(base::Time::Now());

  if (IsFinishedCollectingMetrics())
    FinishedCollectingMetrics(FinishReason::DONE);
}

void FirstWebContentsProfiler::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  // Abandon profiling on any navigation to a different page as it:
  //   (1) is no longer a fair timing; and
  //   (2) can cause http://crbug.com/525209 where one of the timing heuristics
  //       (e.g. first paint) didn't fire for the initial content but fires
  //       after a lot of idle time when the user finally navigates to another
  //       page that does trigger it.
  if (load_details.is_navigation_to_different_page()) {
    if (initial_entry_committed_)
      FinishedCollectingMetrics(FinishReason::ABANDON_NAVIGATION);
    else
      initial_entry_committed_ = true;
  }
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
  delegate_->ProfilerFinishedCollectingMetrics();
}

void FirstWebContentsProfiler::InitHistograms() {
  const std::string responsiveness_histogram_name =
      "Startup.FirstWebContents.UIResponsive";
  responsiveness_histogram_ = base::Histogram::FactoryTimeGet(
      responsiveness_histogram_name, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(60), 100,
      base::Histogram::kUmaTargetedHistogramFlag);

  const std::string responsiveness_1sec_histogram_name =
      "Startup.FirstWebContents.UIResponsive_1sec";
  responsiveness_1sec_histogram_ = base::Histogram::FactoryTimeGet(
      responsiveness_1sec_histogram_name, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(60), 100,
      base::Histogram::kUmaTargetedHistogramFlag);

  const std::string responsiveness_10sec_histogram_name =
      "Startup.FirstWebContents.UIResponsive_10sec";
  responsiveness_10sec_histogram_ = base::Histogram::FactoryTimeGet(
      responsiveness_10sec_histogram_name, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(60), 100,
      base::Histogram::kUmaTargetedHistogramFlag);

  const std::string unresponsiveness_histogram_name =
      "Startup.FirstWebContents.UINotResponsive";
  unresponsiveness_histogram_ = base::Histogram::FactoryTimeGet(
      unresponsiveness_histogram_name, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(60), 100,
      base::Histogram::kUmaTargetedHistogramFlag);

  const std::string unresponsiveness_1sec_histogram_name =
      "Startup.FirstWebContents.UINotResponsive_1sec";
  unresponsiveness_1sec_histogram_ = base::Histogram::FactoryTimeGet(
      unresponsiveness_1sec_histogram_name,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(60),
      100, base::Histogram::kUmaTargetedHistogramFlag);

  const std::string unresponsiveness_10sec_histogram_name =
      "Startup.FirstWebContents.UINotResponsive_10sec";
  unresponsiveness_10sec_histogram_ = base::Histogram::FactoryTimeGet(
      unresponsiveness_10sec_histogram_name,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(60),
      100, base::Histogram::kUmaTargetedHistogramFlag);
}
#endif  // !defined(OS_ANDROID)
