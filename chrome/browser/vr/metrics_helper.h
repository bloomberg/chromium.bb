// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_METRICS_HELPER_H_
#define CHROME_BROWSER_VR_METRICS_HELPER_H_

#include <string>

#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/vr/mode.h"

namespace vr {

// Helper to collect VR UMA metrics.
//
// For thread-safety, all functions must be called in sequence.
class MetricsHelper {
 public:
  MetricsHelper();
  ~MetricsHelper();

  void OnComponentReady();
  void OnEnter(Mode mode);
  void OnRegisteredComponent();

 private:
  base::Optional<base::Time>& GetEnterTime(Mode mode);
  void LogLatencyIfWaited(Mode mode, const base::Time& now);

  base::Optional<base::Time> enter_vr_time_;
  base::Optional<base::Time> enter_vr_browsing_time_;
  base::Optional<base::Time> enter_web_vr_time_;
  bool component_ready_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_METRICS_HELPER_H_
