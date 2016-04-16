// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/leak_detector_controller.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/variations/variations_associated_data.h"

namespace metrics {

namespace {

// Reads parameters for the field trial variation. Any parameters not present in
// the variation info or that cannot be parsed will be filled in with default
// values. Returns a MemoryLeakReportProto with the parameter fields filled in.
MemoryLeakReportProto::Params GetVariationParameters() {
  // Variation parameter names.
  const char kFieldTrialName[] = "RuntimeMemoryLeakDetector";
  const char kSamplingRateParam[] = "sampling_rate";
  const char kMaxStackDepthParam[] = "max_stack_depth";
  const char kAnalysisIntervalKbParam[] = "analysis_interval_kb";
  const char kSizeSuspicionThresholdParam[] = "size_suspicion_threshold";
  const char kCallStackSuspicionThresholdParam[] =
      "call_stack_suspicion_threshold";

  // Default parameter values.
  double kDefaultSamplingRate = 1.0f / 256;
  size_t kDefaultMaxStackDepth = 4;
  uint64_t kDefaultAnalysisIntervalKb = 32768;
  uint32_t kDefaultSizeSuspicionThreshold = 4;
  uint32_t kDefaultCallStackSuspicionThreshold = 4;

  double sampling_rate = 0;
  size_t max_stack_depth = 0;
  uint64_t analysis_interval_kb = 0;
  uint32_t size_suspicion_threshold = 0;
  uint32_t call_stack_suspicion_threshold = 0;

  // Get a mapping of param names to param values.
  std::map<std::string, std::string> params;
  variations::GetVariationParams(kFieldTrialName, &params);

  // Even if the variation param data does not exist and |params| ends up empty,
  // the below code will assign default values.
  auto iter = params.find(kSamplingRateParam);
  if (iter == params.end() ||
      !base::StringToDouble(iter->second, &sampling_rate)) {
    sampling_rate = kDefaultSamplingRate;
  }

  iter = params.find(kMaxStackDepthParam);
  if (iter == params.end() ||
      !base::StringToSizeT(iter->second, &max_stack_depth)) {
    max_stack_depth = kDefaultMaxStackDepth;
  }

  iter = params.find(kAnalysisIntervalKbParam);
  if (iter == params.end() ||
      !base::StringToUint64(iter->second, &analysis_interval_kb)) {
    analysis_interval_kb = kDefaultAnalysisIntervalKb;
  }

  iter = params.find(kSizeSuspicionThresholdParam);
  if (iter == params.end() ||
      !base::StringToUint(iter->second, &size_suspicion_threshold)) {
    size_suspicion_threshold = kDefaultSizeSuspicionThreshold;
  }

  iter = params.find(kCallStackSuspicionThresholdParam);
  if (iter == params.end() ||
      !base::StringToUint(iter->second, &call_stack_suspicion_threshold)) {
    call_stack_suspicion_threshold = kDefaultCallStackSuspicionThreshold;
  }

  MemoryLeakReportProto_Params result;
  result.set_sampling_rate(sampling_rate);
  result.set_max_stack_depth(max_stack_depth);
  result.set_analysis_interval_bytes(analysis_interval_kb * 1024);
  result.set_size_suspicion_threshold(size_suspicion_threshold);
  result.set_call_stack_suspicion_threshold(call_stack_suspicion_threshold);
  return result;
}

}  // namespace

LeakDetectorController::LeakDetectorController()
    : params_(GetVariationParameters()) {
  LeakDetector* detector = LeakDetector::GetInstance();
  detector->AddObserver(this);

  // Leak detector parameters are stored in |params_|.
  detector->Init(params_);
}

LeakDetectorController::~LeakDetectorController() {
  DCHECK(thread_checker_.CalledOnValidThread());
  LeakDetector::GetInstance()->RemoveObserver(this);
}

void LeakDetectorController::OnLeaksFound(
    const std::vector<MemoryLeakReportProto>& reports) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (const auto& report : reports) {
    // Store the report and insert stored parameters.
    stored_reports_.push_back(report);
    stored_reports_.back().mutable_params()->CopyFrom(params_);
  }
}

void LeakDetectorController::GetLeakReports(
    std::vector<MemoryLeakReportProto>* reports) {
  DCHECK(thread_checker_.CalledOnValidThread());
  *reports = std::move(stored_reports_);
}

}  // namespace metrics
