// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/leak_detector/leak_detector_controller.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "components/metrics/leak_detector/gnu_build_id_reader.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"

namespace metrics {

namespace {

using ParamsMap = std::map<std::string, std::string>;

// Returns a mapping of param names to param values, obtained from the
// variations system. Both names and values are strings.
ParamsMap GetRawVariationParams() {
  const char kFieldTrialName[] = "RuntimeMemoryLeakDetector";
  ParamsMap result;
  variations::GetVariationParams(kFieldTrialName, &result);
  return result;
}

// Reads the raw variation parameters and parses them to obtain values for the
// parameters used by the memory leak detector itself. Any parameters not
// present in the variation info or that cannot be parsed will be filled in with
// default values. Returns a MemoryLeakReportProto with the parameter fields
// filled in.
MemoryLeakReportProto::Params GetLeakDetectorParams() {
  // Variation parameter names.
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

  const ParamsMap params = GetRawVariationParams();

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

// Parses the parameters related to randomly enabling leak detector on different
// processes, from the raw parameter strings provided by the variations.
// Args:
// - browser_probability: probability that the leak detector will be enabled on
//                        browser process (spawned once per session).
// - renderer_probability: probability that the leak detector will be enabled on
//                         renderer process (spawned many times per session).
// - max_renderer_processes_enabled: The maximum number of renderer processes on
//                                   which the leak detector can be enabled
//                                   simultaneously.
//
// Probabilities are in the range [0, 1]. Anything higher or lower will not be
// clamped but it will not affect the outcome, since these probabilities are
// compared against the value of base::RandDouble() (aka the "dice roll"), which
// will be within this range.
void GetLeakDetectorEnableParams(double* browser_probability,
                                 double* renderer_probability,
                                 int* max_renderer_processes_enabled) {
  const char kBrowserEnableProbabilityParam[] =
      "browser_process_enable_probability";
  const char kRendererEnableProbabilityParam[] =
      "renderer_process_enable_probability";
  const char kMaxRendererProcessesEnabledParam[] =
      "max_renderer_processes_enabled";
  const double kDefaultProbability = 0.0;
  const int kDefaultNumProcessesEnabled = 0;

  const ParamsMap params = GetRawVariationParams();
  auto iter = params.find(kBrowserEnableProbabilityParam);
  if (iter == params.end() ||
      !base::StringToDouble(iter->second, browser_probability)) {
    *browser_probability = kDefaultProbability;
  }
  iter = params.find(kRendererEnableProbabilityParam);
  if (iter == params.end() ||
      !base::StringToDouble(iter->second, renderer_probability)) {
    *renderer_probability = kDefaultProbability;
  }
  iter = params.find(kMaxRendererProcessesEnabledParam);
  if (iter == params.end() ||
      !base::StringToInt(iter->second, max_renderer_processes_enabled)) {
    *max_renderer_processes_enabled = kDefaultNumProcessesEnabled;
  }
}

}  // namespace

LeakDetectorController::LeakDetectorController()
    : params_(GetLeakDetectorParams()),
      browser_process_enable_probability_(0),
      renderer_process_enable_probability_(0),
      max_renderer_processes_with_leak_detector_enabled_(0),
      num_renderer_processes_with_leak_detector_enabled_(0),
      enable_collect_memory_usage_step_(true),
      waiting_for_collect_memory_usage_step_(false),
      weak_ptr_factory_(this) {
  // Read the build ID once and store it.
  leak_detector::gnu_build_id_reader::ReadBuildID(&build_id_);

  GetLeakDetectorEnableParams(
      &browser_process_enable_probability_,
      &renderer_process_enable_probability_,
      &max_renderer_processes_with_leak_detector_enabled_);

  // Register the LeakDetectorController with the remote controller, so this
  // class can send/receive data to/from remote processes.
  LeakDetectorRemoteController::SetLocalControllerInstance(this);

  // Conditionally launch browser process based on probability.
  if (base::RandDouble() < browser_process_enable_probability_) {
    LeakDetector* detector = LeakDetector::GetInstance();
    detector->AddObserver(this);

    // Leak detector parameters are stored in |params_|.
    detector->Init(params_, content::BrowserThread::GetTaskRunnerForThread(
                                content::BrowserThread::UI));
  }
}

LeakDetectorController::~LeakDetectorController() {
  DCHECK(thread_checker_.CalledOnValidThread());
  LeakDetector::GetInstance()->RemoveObserver(this);
  LeakDetectorRemoteController::SetLocalControllerInstance(nullptr);
}

void LeakDetectorController::GetLeakReports(
    std::vector<MemoryLeakReportProto>* reports) {
  DCHECK(thread_checker_.CalledOnValidThread());
  reports->swap(stored_reports_);
  stored_reports_.clear();
}

void LeakDetectorController::OnLeaksFound(
    const std::vector<MemoryLeakReportProto>& reports) {
  StoreLeakReports(reports, MemoryLeakReportProto::BROWSER_PROCESS);
}

MemoryLeakReportProto_Params
LeakDetectorController::GetParamsAndRecordRequest() {
  if (ShouldRandomlyEnableLeakDetectorOnRendererProcess()) {
    ++num_renderer_processes_with_leak_detector_enabled_;
    return params_;
  }
  // If the leak detector is not to be enabled on the remote process, send an
  // empty MemoryLeakReportProto_Params protobuf. The remote process will not
  // initialize the leak detector since |sampling_rate| is 0.
  return MemoryLeakReportProto_Params();
}

void LeakDetectorController::SendLeakReports(
    const std::vector<MemoryLeakReportProto>& reports) {
  StoreLeakReports(reports, MemoryLeakReportProto::RENDERER_PROCESS);
}

void LeakDetectorController::OnRemoteProcessShutdown() {
  DCHECK_GT(num_renderer_processes_with_leak_detector_enabled_, 0);
  --num_renderer_processes_with_leak_detector_enabled_;
}

LeakDetectorController::TotalMemoryGrowthTracker::TotalMemoryGrowthTracker()
    : total_usage_kb_(0) {}

LeakDetectorController::TotalMemoryGrowthTracker::~TotalMemoryGrowthTracker() {}

bool LeakDetectorController::TotalMemoryGrowthTracker::UpdateSample(
    base::ProcessId pid,
    int sample,
    int* diff) {
  total_usage_kb_ += sample;
  return false;
}

bool LeakDetectorController::ShouldRandomlyEnableLeakDetectorOnRendererProcess()
    const {
  return base::RandDouble() < renderer_process_enable_probability_ &&
         num_renderer_processes_with_leak_detector_enabled_ <
             max_renderer_processes_with_leak_detector_enabled_;
}

void LeakDetectorController::OnMemoryDetailCollectionDone(size_t index) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(waiting_for_collect_memory_usage_step_);
  waiting_for_collect_memory_usage_step_ = false;

  // The available physical memory must be translated from bytes
  // and the total memory usage must be translated from kb.
  MemoryLeakReportProto::MemoryUsageInfo mem_info;
  mem_info.set_available_ram_mb(
      base::SysInfo::AmountOfAvailablePhysicalMemory() / 1024 / 1024);
  mem_info.set_chrome_ram_usage_mb(
      total_memory_growth_tracker_.total_usage_kb() / 1024);

  // Update all reports that arrived in the meantime.
  for (; index < stored_reports_.size(); index++) {
    stored_reports_[index].mutable_memory_usage_info()->CopyFrom(mem_info);
  }
}

void LeakDetectorController::StoreLeakReports(
    const std::vector<MemoryLeakReportProto>& reports,
    MemoryLeakReportProto::ProcessType process_type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Postpone a task of collecting info about the memory usage.
  // When the task is done, all reports collected after this point
  // will be updated, unless GetLeakReports() gets called first.
  if (enable_collect_memory_usage_step_ &&
      !waiting_for_collect_memory_usage_step_) {
    waiting_for_collect_memory_usage_step_ = true;

    total_memory_growth_tracker_.reset();

    // Idea of using MetricsMemoryDetails is similar as in
    // ChromeMetricsServiceClient. However, here generation of histogram data
    // is suppressed.
    base::Closure callback =
        base::Bind(&LeakDetectorController::OnMemoryDetailCollectionDone,
                   weak_ptr_factory_.GetWeakPtr(), stored_reports_.size());

    scoped_refptr<MetricsMemoryDetails> details(
        new MetricsMemoryDetails(callback, &total_memory_growth_tracker_));
    details->set_generate_histograms(false);
    details->StartFetch();
  }

  for (const auto& report : reports) {
    // Store the report and insert stored parameters.
    stored_reports_.push_back(report);
    stored_reports_.back().mutable_params()->CopyFrom(params_);
    stored_reports_.back().set_source_process(process_type);
    stored_reports_.back().mutable_build_id()->assign(build_id_.begin(),
                                                      build_id_.end());
  }
}

}  // namespace metrics
