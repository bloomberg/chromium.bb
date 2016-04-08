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
MemoryLeakReportProto GetVariationParameters() {
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

  MemoryLeakReportProto proto;
  proto.set_sampling_rate(sampling_rate);
  proto.set_max_stack_depth(max_stack_depth);
  proto.set_analysis_interval_bytes(analysis_interval_kb * 1024);
  proto.set_size_suspicion_threshold(size_suspicion_threshold);
  proto.set_call_stack_suspicion_threshold(call_stack_suspicion_threshold);
  return proto;
}

}  // namespace

LeakDetectorController::LeakDetectorController()
    : leak_report_proto_template_(GetVariationParameters()) {
  LeakDetector* detector = LeakDetector::GetInstance();
  detector->AddObserver(this);

  // Leak detector parameters are stored in |leak_report_proto_template_|.
  const MemoryLeakReportProto& proto = leak_report_proto_template_;
  detector->Init(proto.sampling_rate(), proto.max_stack_depth(),
                 proto.analysis_interval_bytes(),
                 proto.size_suspicion_threshold(),
                 proto.call_stack_suspicion_threshold());
}

LeakDetectorController::~LeakDetectorController() {
  DCHECK(thread_checker_.CalledOnValidThread());
  LeakDetector::GetInstance()->RemoveObserver(this);
}

void LeakDetectorController::OnLeakFound(
    const LeakDetector::LeakReport& report) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Initialize the new report with the protobuf template, which contains some
  // pre-filled data (parameter values).
  stored_reports_.push_back(leak_report_proto_template_);
  MemoryLeakReportProto* proto = &stored_reports_.back();

  // Fill in the remaining fields of the protobuf with data from |report|.
  proto->set_size_bytes(report.alloc_size_bytes);
  proto->mutable_call_stack()->Reserve(report.call_stack.size());
  for (uintptr_t call_stack_entry : report.call_stack)
    proto->mutable_call_stack()->Add(call_stack_entry);
}

void LeakDetectorController::GetLeakReports(
    std::vector<MemoryLeakReportProto>* reports) {
  DCHECK(thread_checker_.CalledOnValidThread());
  *reports = std::move(stored_reports_);
}

}  // namespace metrics
