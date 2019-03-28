// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PRESENTATION_TIME_RECORDER_H_
#define ASH_PUBLIC_CPP_PRESENTATION_TIME_RECORDER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"

namespace ash {

// PresentationTimeRecorder records the time between when an UI update is
// requested, and the requested UI change has been presented to the user
// (screen). This measure the longest presentation time for each commit by
// skipping updates made after the last request and next commit.  Use this if
// you want to measure the drawing performance in continuous operation that
// doesn't involve animations (such as window dragging). Call |RequestNext()|
// when you made modification to UI that should expect it will be presented.
// TODO(oshima): Move this to ash/metrics after crbug.com/942564 is resolved.
class ASH_PUBLIC_EXPORT PresentationTimeRecorder
    : public ui::CompositorObserver {
 public:
  explicit PresentationTimeRecorder(ui::Compositor* compositor);
  ~PresentationTimeRecorder() override;

  // Start recording next frame. It skips requesting next frame and returns
  // false if the previous frame has not been committed yet.
  bool RequestNext();

  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // Enable this to report the presentation time immediately with
  // fake value when RequestNext is called.
  static void SetReportPresentationTimeImmediatelyForTest(bool enable);

 protected:
  FRIEND_TEST_ALL_PREFIXES(PresentationTimeRecorderTest, Failure);

  int max_latency_ms() const { return max_latency_ms_; }
  int success_count() const { return success_count_; }

  // |delta| is the duration between the successful request time and
  // presentation time.
  virtual void ReportTime(base::TimeDelta delta) = 0;

  static bool GetReportPresentationTimeImmediatelyForTest();

 private:
  enum State {
    // The frame has been presented to the screen. This is the initial state.
    PRESENTED,
    // The presentation feedback has been requested.
    REQUESTED,
    // The changes to layers have been submitted, and waiting to be presented.
    COMMITTED,
  };
  void OnPresented(int count,
                   base::TimeTicks requested_time,
                   const gfx::PresentationFeedback& feedback);

  int average_latency_ms() const {
    return success_count_ ? total_latency_ms_ / success_count_ : 0;
  }
  int failure_ratio() const {
    return failure_count_
               ? (100 * failure_count_) / (success_count_ + failure_count_)
               : 0;
  }

  State state_ = PRESENTED;

  int success_count_ = 0;
  int failure_count_ = 0;
  int request_count_ = 0;
  int total_latency_ms_ = 0;
  int max_latency_ms_ = 0;

  ui::Compositor* compositor_ = nullptr;

  base::WeakPtrFactory<PresentationTimeRecorder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PresentationTimeRecorder);
};

// A utility class to record timing histograms of presentation time and max
// latency. The time range is 1 to 200 ms, with 50 buckets.
class ASH_PUBLIC_EXPORT PresentationTimeHistogramRecorder
    : public PresentationTimeRecorder {
 public:
  // |presentation_time_histogram_name| records latency reported on
  // |ReportTime()| and |max_latency_histogram_name| records the maximum latency
  // reported during the lifetime of this object.  Histogram names must be the
  // name of the UMA histogram defined in histograms.xml.
  PresentationTimeHistogramRecorder(
      ui::Compositor* compositor,
      const char* presentation_time_histogram_name,
      const char* max_latency_histogram_name);
  ~PresentationTimeHistogramRecorder() override;

  // PresentationTimeRecorder:
  void ReportTime(base::TimeDelta delta) override;

 private:
  base::HistogramBase* presentation_time_histogram_;
  std::string max_latency_histogram_name_;

  DISALLOW_COPY_AND_ASSIGN(PresentationTimeHistogramRecorder);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PRESENTATION_TIME_RECORDER_H_
