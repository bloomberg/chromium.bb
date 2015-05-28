// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_HISTOGRAMS_H_
#define CC_BASE_HISTOGRAMS_H_

#include "base/compiler_specific.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "cc/base/cc_export.h"

namespace cc {

// Emits UMA histogram trackers for time spent as well as area (in pixels)
// processed per unit time. Time is measured in microseconds, and work in
// pixels per millisecond.
//
// Usage:
//   // Outside of a method, perhaps in a namespace.
//   DEFINE_SCOPED_UMA_HISTOGRAM_AREA_TIMER(ScopedReticulateSplinesTimer,
//                                          "ReticulateSplinesUs",
//                                          "ReticulateSplinesPixelsPerMs");
//
//   // Inside a method.
//   ScopedReticulateSplinesTimer timer;
//   timer.AddArea(some_rect.size().GetArea());
#define DEFINE_SCOPED_UMA_HISTOGRAM_AREA_TIMER(class_name, time_histogram, \
                                               area_histogram)             \
  class class_name : public ::cc::ScopedUMAHistogramAreaTimerBase {        \
   public:                                                                 \
    ~class_name();                                                         \
  };                                                                       \
  class_name::~class_name() {                                              \
    Sample time_sample;                                                    \
    Sample area_sample;                                                    \
    if (GetHistogramValues(&time_sample, &area_sample)) {                  \
      UMA_HISTOGRAM_COUNTS(time_histogram, time_sample);                   \
      UMA_HISTOGRAM_COUNTS(area_histogram, area_sample);                   \
    }                                                                      \
  }

class CC_EXPORT ScopedUMAHistogramAreaTimerBase {
 public:
  void AddArea(int area) { area_ += area; }
  void SetArea(int area) { area_ = area; }

 protected:
  using Sample = base::HistogramBase::Sample;

  ScopedUMAHistogramAreaTimerBase();
  ~ScopedUMAHistogramAreaTimerBase();

  // Returns true if histograms should be recorded (i.e. values are valid).
  bool GetHistogramValues(Sample* time_microseconds,
                          Sample* pixels_per_ms) const;

 private:
  static bool GetHistogramValues(base::TimeDelta elapsed,
                                 int area,
                                 Sample* time_microseconds,
                                 Sample* pixels_per_ms);

  base::ElapsedTimer timer_;
  base::CheckedNumeric<int> area_;

  friend class ScopedUMAHistogramAreaTimerBaseTest;
  DISALLOW_COPY_AND_ASSIGN(ScopedUMAHistogramAreaTimerBase);
};

}  // namespace cc

#endif  // CC_BASE_HISTOGRAMS_H_
