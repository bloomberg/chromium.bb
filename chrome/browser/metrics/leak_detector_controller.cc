// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/leak_detector_controller.h"

#include <algorithm>

#include "base/logging.h"

namespace metrics {

namespace {

// Default parameters for LeakDetector.
const float kSamplingRate = 1.0f / 256;
const int kMaxStackDepth = 4;
const uint64_t kAnalysisIntervalBytes = 32 * 1024 * 1024;
const int kSizeSuspicionThreshold = 4;
const int kCallStackSuspicionThreshold = 4;

}  // namespace

LeakDetectorController::LeakDetectorController()
    : detector_(kSamplingRate,
                kMaxStackDepth,
                kAnalysisIntervalBytes,
                kSizeSuspicionThreshold,
                kCallStackSuspicionThreshold) {
  detector_.AddObserver(this);
}

LeakDetectorController::~LeakDetectorController() {
  DCHECK(thread_checker_.CalledOnValidThread());
  detector_.RemoveObserver(this);
}

void LeakDetectorController::OnLeakFound(
    const LeakDetector::LeakReport& report) {
  DCHECK(thread_checker_.CalledOnValidThread());

  stored_reports_.push_back(MemoryLeakReportProto());
  MemoryLeakReportProto* proto = &stored_reports_.back();

  // Copy the contents of the report to the protobuf.
  proto->set_size_bytes(report.alloc_size_bytes);
  proto->mutable_call_stack()->Reserve(report.call_stack.size());
  for (uintptr_t call_stack_entry : report.call_stack)
    proto->mutable_call_stack()->Add(call_stack_entry);

  // Store the LeakDetector parameters in the protobuf.
  proto->set_sampling_rate(kSamplingRate);
  proto->set_max_stack_depth(kMaxStackDepth);
  proto->set_analysis_interval_bytes(kAnalysisIntervalBytes);
  proto->set_size_suspicion_threshold(kSizeSuspicionThreshold);
  proto->set_call_stack_suspicion_threshold(kCallStackSuspicionThreshold);
}

void LeakDetectorController::GetLeakReports(
    std::vector<MemoryLeakReportProto>* reports) {
  DCHECK(thread_checker_.CalledOnValidThread());
  *reports = std::move(stored_reports_);
}

}  // namespace metrics
