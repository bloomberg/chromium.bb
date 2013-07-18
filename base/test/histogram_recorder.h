// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_HISTOGRAM_RECORDER_H_
#define BASE_TEST_HISTOGRAM_RECORDER_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_samples.h"

namespace base {

// This class acts as a differential reader for histogram samples, enabling
// tests to check that metrics were sent as they should be.
class HistogramRecorder {
 public:
  HistogramRecorder();
  virtual ~HistogramRecorder();

  // Returns the histogram data accumulated since this instance was created.
  // Returns NULL if no samples are available.
  scoped_ptr<HistogramSamples> GetHistogramSamplesSinceCreation(
      const std::string& histogram_name);

 private:
  // Used to determine the histogram changes made during this instance's
  // lifecycle.
  std::map<std::string, HistogramSamples*> original_samples_;
};

}  // namespace base

#endif  // BASE_TEST_HISTOGRAM_RECORDER_H_
