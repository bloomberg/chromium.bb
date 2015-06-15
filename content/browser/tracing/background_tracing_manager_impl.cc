// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_manager_impl.h"

#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/public/browser/background_tracing_preemptive_config.h"
#include "content/public/browser/background_tracing_reactive_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/common/content_client.h"

namespace content {

namespace {

base::LazyInstance<BackgroundTracingManagerImpl>::Leaky g_controller =
    LAZY_INSTANCE_INITIALIZER;

const char kMetaDataConfigKey[] = "config";
const char kMetaDataVersionKey[] = "product_version";

// These values are used for a histogram. Do not reorder.
enum BackgroundTracingMetrics {
  SCENARIO_ACTIVATION_REQUESTED = 0,
  SCENARIO_ACTIVATED_SUCCESSFULLY = 1,
  RECORDING_ENABLED = 2,
  PREEMPTIVE_TRIGGERED = 3,
  REACTIVE_TRIGGERED = 4,
  FINALIZATION_ALLOWED = 5,
  FINALIZATION_DISALLOWED = 6,
  FINALIZATION_STARTED = 7,
  FINALIZATION_COMPLETE = 8,
  NUMBER_OF_BACKGROUND_TRACING_METRICS,
};

void RecordBackgroundTracingMetric(BackgroundTracingMetrics metric) {
  UMA_HISTOGRAM_ENUMERATION("Tracing.Background.ScenarioState", metric,
                            NUMBER_OF_BACKGROUND_TRACING_METRICS);
}

}  // namespace

BackgroundTracingManagerImpl::TraceDataEndpointWrapper::
    TraceDataEndpointWrapper(base::Callback<
        void(scoped_refptr<base::RefCountedString>)> done_callback)
    : done_callback_(done_callback) {
}

BackgroundTracingManagerImpl::TraceDataEndpointWrapper::
    ~TraceDataEndpointWrapper() {
}

void BackgroundTracingManagerImpl::TraceDataEndpointWrapper::
    ReceiveTraceFinalContents(const std::string& file_contents) {
  std::string tmp = file_contents;
  scoped_refptr<base::RefCountedString> contents_ptr =
      base::RefCountedString::TakeString(&tmp);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(done_callback_, contents_ptr));
}

BackgroundTracingManagerImpl::TracingTimer::TracingTimer(
    StartedFinalizingCallback callback) : callback_(callback) {
}

BackgroundTracingManagerImpl::TracingTimer::~TracingTimer() {
}

void BackgroundTracingManagerImpl::TracingTimer::StartTimer() {
  const int kTimeoutSecs = 10;
  tracing_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(kTimeoutSecs),
      this, &BackgroundTracingManagerImpl::TracingTimer::TracingTimerFired);
}

void BackgroundTracingManagerImpl::TracingTimer::CancelTimer() {
  tracing_timer_.Stop();
}

void BackgroundTracingManagerImpl::TracingTimer::TracingTimerFired() {
  BackgroundTracingManagerImpl::GetInstance()->BeginFinalizing(callback_);
}

void BackgroundTracingManagerImpl::TracingTimer::FireTimerForTesting() {
  CancelTimer();
  TracingTimerFired();
}

BackgroundTracingManager* BackgroundTracingManager::GetInstance() {
  return BackgroundTracingManagerImpl::GetInstance();
}

BackgroundTracingManagerImpl* BackgroundTracingManagerImpl::GetInstance() {
  return g_controller.Pointer();
}

BackgroundTracingManagerImpl::BackgroundTracingManagerImpl()
    : delegate_(GetContentClient()->browser()->GetTracingDelegate()),
      is_gathering_(false),
      is_tracing_(false),
      requires_anonymized_data_(true),
      trigger_handle_ids_(0) {
  data_endpoint_wrapper_ = new TraceDataEndpointWrapper(
      base::Bind(&BackgroundTracingManagerImpl::OnFinalizeStarted,
                 base::Unretained(this)));
}

BackgroundTracingManagerImpl::~BackgroundTracingManagerImpl() {
  NOTREACHED();
}

void BackgroundTracingManagerImpl::WhenIdle(
    base::Callback<void()> idle_callback) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  idle_callback_ = idle_callback;
}

bool BackgroundTracingManagerImpl::IsSupportedConfig(
    BackgroundTracingConfig* config) {
  // No config is just fine, we just don't do anything.
  if (!config)
    return true;

  if (config->mode == BackgroundTracingConfig::PREEMPTIVE_TRACING_MODE) {
    BackgroundTracingPreemptiveConfig* preemptive_config =
        static_cast<BackgroundTracingPreemptiveConfig*>(config);
    const std::vector<BackgroundTracingPreemptiveConfig::MonitoringRule>&
        configs = preemptive_config->configs;
    for (size_t i = 0; i < configs.size(); ++i) {
      if (configs[i].type !=
          BackgroundTracingPreemptiveConfig::
              MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED)
        return false;
    }
  }

  if (config->mode == BackgroundTracingConfig::REACTIVE_TRACING_MODE) {
    BackgroundTracingReactiveConfig* reactive_config =
        static_cast<BackgroundTracingReactiveConfig*>(config);
    const std::vector<BackgroundTracingReactiveConfig::TracingRule>&
        configs = reactive_config->configs;
    for (size_t i = 0; i < configs.size(); ++i) {
      if (configs[i].type !=
          BackgroundTracingReactiveConfig::TRACE_FOR_10S_OR_TRIGGER_OR_FULL)
        return false;
    }
  }

  return true;
}

bool BackgroundTracingManagerImpl::SetActiveScenario(
    scoped_ptr<BackgroundTracingConfig> config,
    const BackgroundTracingManager::ReceiveCallback& receive_callback,
    DataFiltering data_filtering) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  RecordBackgroundTracingMetric(SCENARIO_ACTIVATION_REQUESTED);

  if (is_tracing_)
    return false;

  bool requires_anonymized_data = (data_filtering == ANONYMIZE_DATA);

  // If the I/O thread isn't running, this is a startup scenario and
  // we have to wait until initialization is finished to validate that the
  // scenario can run.
  if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    // TODO(oysteine): Retry when time_until_allowed has elapsed.
    if (config && delegate_ &&
        !delegate_->IsAllowedToBeginBackgroundScenario(
            *config.get(), requires_anonymized_data)) {
      return false;
    }
  } else {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&BackgroundTracingManagerImpl::ValidateStartupScenario,
                   base::Unretained(this)));
  }

  if (!IsSupportedConfig(config.get()))
    return false;

  // No point in tracing if there's nowhere to send it.
  if (config && receive_callback.is_null())
    return false;

  config_ = config.Pass();
  receive_callback_ = receive_callback;
  requires_anonymized_data_ = requires_anonymized_data;

  EnableRecordingIfConfigNeedsIt();

  RecordBackgroundTracingMetric(SCENARIO_ACTIVATED_SUCCESSFULLY);
  return true;
}

bool BackgroundTracingManagerImpl::HasActiveScenarioForTesting() {
  return config_;
}

void BackgroundTracingManagerImpl::ValidateStartupScenario() {
  if (!config_ || !delegate_)
    return;

  if (!delegate_->IsAllowedToBeginBackgroundScenario(
          *config_.get(), requires_anonymized_data_)) {
    AbortScenario();
  }
}

void BackgroundTracingManagerImpl::EnableRecordingIfConfigNeedsIt() {
  if (!config_)
    return;

  if (config_->mode == BackgroundTracingConfig::PREEMPTIVE_TRACING_MODE) {
    EnableRecording(GetCategoryFilterStringForCategoryPreset(
        static_cast<BackgroundTracingPreemptiveConfig*>(config_.get())
            ->category_preset),
        base::trace_event::RECORD_CONTINUOUSLY);
  }
  // There is nothing to do in case of reactive tracing.
}

bool BackgroundTracingManagerImpl::IsAbleToTriggerTracing(
    TriggerHandle handle) const {
  if (!config_)
    return false;

  // If the last trace is still uploading, we don't allow a new one to trigger.
  if (is_gathering_)
    return false;

  if (!IsTriggerHandleValid(handle)) {
    return false;
  }

  std::string trigger_name = GetTriggerNameFromHandle(handle);

  if (config_->mode == BackgroundTracingConfig::PREEMPTIVE_TRACING_MODE) {
    BackgroundTracingPreemptiveConfig* preemptive_config =
        static_cast<BackgroundTracingPreemptiveConfig*>(config_.get());

    const std::vector<BackgroundTracingPreemptiveConfig::MonitoringRule>&
        configs = preemptive_config->configs;

    for (size_t i = 0; i < configs.size(); ++i) {
      if (configs[i].type != BackgroundTracingPreemptiveConfig::
                                 MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED)
        continue;

      if (trigger_name == configs[i].named_trigger_info.trigger_name) {
        return true;
      }
    }
  } else {
    BackgroundTracingReactiveConfig* reactive_config =
        static_cast<BackgroundTracingReactiveConfig*>(config_.get());

    const std::vector<BackgroundTracingReactiveConfig::TracingRule>&
        configs = reactive_config->configs;

    for (size_t i = 0; i < configs.size(); ++i) {
      if (configs[i].type !=
              BackgroundTracingReactiveConfig::
                  TRACE_FOR_10S_OR_TRIGGER_OR_FULL)
        continue;
      if (trigger_name == configs[i].trigger_name) {
        return true;
      }
    }
  }
  return false;
}

void BackgroundTracingManagerImpl::TriggerNamedEvent(
    BackgroundTracingManagerImpl::TriggerHandle handle,
    StartedFinalizingCallback callback) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&BackgroundTracingManagerImpl::TriggerNamedEvent,
                   base::Unretained(this), handle, callback));
    return;
  }

  if (!IsAbleToTriggerTracing(handle)) {
    if (!callback.is_null())
      callback.Run(false);
    return;
  }

  if (config_->mode == BackgroundTracingConfig::PREEMPTIVE_TRACING_MODE) {
    RecordBackgroundTracingMetric(PREEMPTIVE_TRIGGERED);
    BeginFinalizing(callback);
  } else {
    RecordBackgroundTracingMetric(REACTIVE_TRIGGERED);
    if (is_tracing_) {
      tracing_timer_->CancelTimer();
      BeginFinalizing(callback);
      return;
    }

    // It was not already tracing, start a new trace.
    BackgroundTracingReactiveConfig* reactive_config =
        static_cast<BackgroundTracingReactiveConfig*>(config_.get());
    const std::vector<BackgroundTracingReactiveConfig::TracingRule>&
        configs = reactive_config->configs;
    std::string trigger_name = GetTriggerNameFromHandle(handle);
    for (size_t i = 0; i < configs.size(); ++i) {
      if (configs[i].trigger_name == trigger_name) {
        EnableRecording(
            GetCategoryFilterStringForCategoryPreset(
                configs[i].category_preset),
            base::trace_event::RECORD_UNTIL_FULL);
        tracing_timer_.reset(new TracingTimer(callback));
        tracing_timer_->StartTimer();
        break;
      }
    }
  }
}

BackgroundTracingManagerImpl::TriggerHandle
BackgroundTracingManagerImpl::RegisterTriggerType(const char* trigger_name) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  trigger_handle_ids_ += 1;

  trigger_handles_.insert(
      std::pair<TriggerHandle, std::string>(trigger_handle_ids_, trigger_name));

  return static_cast<TriggerHandle>(trigger_handle_ids_);
}

bool BackgroundTracingManagerImpl::IsTriggerHandleValid(
    BackgroundTracingManager::TriggerHandle handle) const {
  return trigger_handles_.find(handle) != trigger_handles_.end();
}

std::string BackgroundTracingManagerImpl::GetTriggerNameFromHandle(
    BackgroundTracingManager::TriggerHandle handle) const {
  CHECK(IsTriggerHandleValid(handle));
  return trigger_handles_.find(handle)->second;
}

void BackgroundTracingManagerImpl::GetTriggerNameList(
    std::vector<std::string>* trigger_names) {
  for (std::map<TriggerHandle, std::string>::iterator it =
           trigger_handles_.begin();
       it != trigger_handles_.end(); ++it)
    trigger_names->push_back(it->second);
}

void BackgroundTracingManagerImpl::InvalidateTriggerHandlesForTesting() {
  trigger_handles_.clear();
}

void BackgroundTracingManagerImpl::SetTracingEnabledCallbackForTesting(
    const base::Closure& callback) {
  tracing_enabled_callback_for_testing_ = callback;
};

void BackgroundTracingManagerImpl::FireTimerForTesting() {
  tracing_timer_->FireTimerForTesting();
}

void BackgroundTracingManagerImpl::EnableRecording(
    std::string category_filter_str,
    base::trace_event::TraceRecordMode record_mode) {
  base::trace_event::TraceConfig trace_config(category_filter_str, record_mode);
  if (requires_anonymized_data_)
    trace_config.EnableArgumentFilter();

  is_tracing_ = TracingController::GetInstance()->EnableRecording(
      trace_config, tracing_enabled_callback_for_testing_);
  RecordBackgroundTracingMetric(RECORDING_ENABLED);
}

void BackgroundTracingManagerImpl::OnFinalizeStarted(
    scoped_refptr<base::RefCountedString> file_contents) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  RecordBackgroundTracingMetric(FINALIZATION_STARTED);
  UMA_HISTOGRAM_MEMORY_KB("Tracing.Background.FinalizingTraceSizeInKB",
                          file_contents->size() / 1024);

  if (!receive_callback_.is_null()) {
    receive_callback_.Run(
        file_contents, GenerateMetadataDict(),
        base::Bind(&BackgroundTracingManagerImpl::OnFinalizeComplete,
                   base::Unretained(this)));
  }
}

void BackgroundTracingManagerImpl::OnFinalizeComplete() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&BackgroundTracingManagerImpl::OnFinalizeComplete,
                   base::Unretained(this)));
    return;
  }

  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  is_gathering_ = false;

  if (!idle_callback_.is_null())
    idle_callback_.Run();

  // Now that a trace has completed, we may need to enable recording again.
  // TODO(oysteine): Retry later if IsAllowedToBeginBackgroundScenario fails.
  if (!delegate_ ||
      delegate_->IsAllowedToBeginBackgroundScenario(
          *config_.get(), requires_anonymized_data_)) {
    EnableRecordingIfConfigNeedsIt();
  }

  RecordBackgroundTracingMetric(FINALIZATION_COMPLETE);
}

scoped_ptr<base::DictionaryValue>
BackgroundTracingManagerImpl::GenerateMetadataDict() const {
  // Grab the product version.
  std::string product_version = GetContentClient()->GetProduct();

  // Serialize the config into json.
  scoped_ptr<base::DictionaryValue> config_dict(new base::DictionaryValue());

  BackgroundTracingConfig::IntoDict(config_.get(), config_dict.get());

  scoped_ptr<base::DictionaryValue> metadata_dict(new base::DictionaryValue());
  metadata_dict->Set(kMetaDataConfigKey, config_dict.Pass());
  metadata_dict->SetString(kMetaDataVersionKey, product_version);

  return metadata_dict.Pass();
}

void BackgroundTracingManagerImpl::BeginFinalizing(
    StartedFinalizingCallback callback) {
  is_gathering_ = true;
  is_tracing_ = false;

  bool is_allowed_finalization =
      !delegate_ || (config_ &&
                     delegate_->IsAllowedToEndBackgroundScenario(
                         *config_.get(), requires_anonymized_data_));

  scoped_refptr<TracingControllerImpl::TraceDataSink> trace_data_sink;
  if (is_allowed_finalization) {
    trace_data_sink = content::TracingController::CreateCompressedStringSink(
        data_endpoint_wrapper_);
    RecordBackgroundTracingMetric(FINALIZATION_ALLOWED);

    if (auto metadata_dict = GenerateMetadataDict()) {
      std::string results;
      if (base::JSONWriter::Write(*metadata_dict.get(), &results))
        trace_data_sink->SetMetadata(results);
    }
  } else {
    RecordBackgroundTracingMetric(FINALIZATION_DISALLOWED);
  }

  content::TracingController::GetInstance()->DisableRecording(trace_data_sink);

  if (!callback.is_null())
    callback.Run(is_allowed_finalization);
}

void BackgroundTracingManagerImpl::AbortScenario() {
  is_tracing_ = false;
  config_.reset();

  content::TracingController::GetInstance()->DisableRecording(nullptr);
}

std::string
BackgroundTracingManagerImpl::GetCategoryFilterStringForCategoryPreset(
    BackgroundTracingConfig::CategoryPreset preset) const {
  switch (preset) {
    case BackgroundTracingConfig::CategoryPreset::BENCHMARK:
      return "benchmark,"
             "disabled-by-default-toplevel.flow,"
             "disabled-by-default-ipc.flow";
    case BackgroundTracingConfig::CategoryPreset::BENCHMARK_DEEP:
      return "*,disabled-by-default-blink.debug.layout";
  }
  NOTREACHED();
  return "";
}

}  // namspace content
