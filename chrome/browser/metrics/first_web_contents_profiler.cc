// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(OS_ANDROID)

#include "chrome/browser/metrics/first_web_contents_profiler.h"

#include "base/metrics/histogram_macros.h"
#include "base/process/process_info.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_version_info.h"

namespace {

const int kHistogramMinTimeMilliseconds = 200;
const int kHistogramMaxTimeSeconds = 45;

// There is a lot of noise in the dev channel. One possible cause is that the
// bucket ranges are too wide. The bucket count is chosen such that the size of
// the bucket is 1% of its minimum. Notice that this also means that
// subsequent buckets will satisfy Min(bucket N + 1) / Min(bucket N) = 1.01.
// Then we want X, such that 1.01 ^ X = 45 * 1000 / 200. Some quick math shows
// that X = 544.
const int kHistogramBucketCount = 544;

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

bool FirstWebContentsProfiler::ShouldCollectMetrics() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  return channel == chrome::VersionInfo::CHANNEL_CANARY ||
         channel == chrome::VersionInfo::CHANNEL_DEV;
}

FirstWebContentsProfiler::FirstWebContentsProfiler(
    content::WebContents* web_contents,
    Delegate* delegate)
    : content::WebContentsObserver(web_contents),
      collected_paint_metric_(false),
      collected_load_metric_(false),
      delegate_(delegate) {
  process_creation_time_ = base::CurrentProcessInfo::CreationTime();
}

void FirstWebContentsProfiler::DidFirstVisuallyNonEmptyPaint() {
  if (collected_paint_metric_)
    return;

  collected_paint_metric_ = true;
  if (!process_creation_time_.is_null()) {
    base::TimeDelta elapsed = base::Time::Now() - process_creation_time_;

    // TODO(erikchen): Revisit these metrics once data has been collected to
    // determine whether using more buckets reduces noise and provides higher
    // quality information.
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Startup.Experimental.FirstWebContents.NonEmptyPaint."
        "ManyBuckets",
        elapsed,
        base::TimeDelta::FromMilliseconds(kHistogramMinTimeMilliseconds),
        base::TimeDelta::FromSeconds(kHistogramMaxTimeSeconds),
        kHistogramBucketCount);
    UMA_HISTOGRAM_LONG_TIMES_100(
        "Startup.Experimental.FirstWebContents.NonEmptyPaint.StandardBuckets",
        elapsed);
  }

  if (IsFinishedCollectingMetrics())
    FinishedCollectingMetrics();
}

void FirstWebContentsProfiler::DocumentOnLoadCompletedInMainFrame() {
  if (collected_load_metric_)
    return;

  collected_load_metric_ = true;
  if (!process_creation_time_.is_null()) {
    base::TimeDelta elapsed = base::Time::Now() - process_creation_time_;

    // TODO(erikchen): Revisit these metrics once data has been collected to
    // determine whether using more buckets reduces noise and provides higher
    // quality information.
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Startup.Experimental.FirstWebContents.MainFrameLoad."
        "ManyBuckets",
        elapsed,
        base::TimeDelta::FromMilliseconds(kHistogramMinTimeMilliseconds),
        base::TimeDelta::FromSeconds(kHistogramMaxTimeSeconds),
        kHistogramBucketCount);
    UMA_HISTOGRAM_LONG_TIMES_100(
        "Startup.Experimental.FirstWebContents.MainFrameLoad.StandardBuckets",
        elapsed);
  }

  if (IsFinishedCollectingMetrics())
    FinishedCollectingMetrics();
}

void FirstWebContentsProfiler::WebContentsDestroyed() {
  FinishedCollectingMetrics();
}

bool FirstWebContentsProfiler::IsFinishedCollectingMetrics() {
  return collected_paint_metric_ && collected_load_metric_;
}

void FirstWebContentsProfiler::FinishedCollectingMetrics() {
  delegate_->ProfilerFinishedCollectingMetrics();
}

#endif  // !defined(OS_ANDROID)
