// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_active_scenario.h"

#include <set>
#include <utility>

#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/tracing_features.h"

using base::trace_event::TraceConfig;
using Metrics = content::BackgroundTracingManagerImpl::Metrics;

namespace content {

class BackgroundTracingActiveScenario::TracingTimer {
 public:
  TracingTimer(BackgroundTracingActiveScenario* scenario,
               BackgroundTracingManager::StartedFinalizingCallback callback)
      : scenario_(scenario), callback_(callback) {}
  ~TracingTimer() = default;

  void StartTimer(int seconds) {
    tracing_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(seconds), this,
                         &TracingTimer::TracingTimerFired);
  }
  void CancelTimer() { tracing_timer_.Stop(); }

  void FireTimerForTesting() {
    CancelTimer();
    TracingTimerFired();
  }

 private:
  void TracingTimerFired() { scenario_->BeginFinalizing(callback_); }

  BackgroundTracingActiveScenario* scenario_;
  base::OneShotTimer tracing_timer_;
  BackgroundTracingManager::StartedFinalizingCallback callback_;
};

BackgroundTracingActiveScenario::BackgroundTracingActiveScenario(
    std::unique_ptr<const BackgroundTracingConfigImpl> config,
    bool requires_anonymized_data,
    BackgroundTracingManager::ReceiveCallback receive_callback,
    base::OnceClosure on_aborted_callback)
    : config_(std::move(config)),
      requires_anonymized_data_(requires_anonymized_data),
      scenario_state_(State::kIdle),
      receive_callback_(std::move(receive_callback)),
      triggered_named_event_handle_(-1),
      on_aborted_callback_(std::move(on_aborted_callback)),
      weak_ptr_factory_(this) {
  DCHECK(config_ && !config_->rules().empty());
  for (const auto& rule : config_->rules()) {
    rule->Install();
  }
}

BackgroundTracingActiveScenario::~BackgroundTracingActiveScenario() = default;

const BackgroundTracingConfigImpl* BackgroundTracingActiveScenario::GetConfig()
    const {
  return config_.get();
}

void BackgroundTracingActiveScenario::SetState(State new_state) {
  auto old_state = scenario_state_;
  scenario_state_ = new_state;

  if ((old_state == State::kTracing) &&
      base::trace_event::TraceLog::GetInstance()->IsEnabled()) {
    // Leaving the kTracing state means we're supposed to have fully
    // shut down tracing at this point. Since StartTracing directly enables
    // tracing in TraceLog, in addition to going through Mojo, there's an
    // edge-case where tracing is rapidly stopped after starting, too quickly
    // for the TraceEventAgent of the browser process to register itself,
    // which means that we're left in a state where the Mojo interface doesn't
    // think we're tracing but TraceLog is still enabled. If that's the case,
    // we abort tracing here.
    auto record_mode =
        (config_->tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE)
            ? base::trace_event::RECORD_CONTINUOUSLY
            : base::trace_event::RECORD_UNTIL_FULL;
    TraceConfig config =
        BackgroundTracingConfigImpl::GetConfigForCategoryPreset(
            config_->category_preset(), record_mode);

    uint8_t modes = base::trace_event::TraceLog::RECORDING_MODE;
    if (!config.event_filters().empty())
      modes |= base::trace_event::TraceLog::FILTERING_MODE;
    base::trace_event::TraceLog::GetInstance()->SetDisabled(modes);
  }

  if (scenario_state_ == State::kAborted) {
    std::move(on_aborted_callback_).Run();
  }
}

void BackgroundTracingActiveScenario::FireTimerForTesting() {
  DCHECK(tracing_timer_);
  tracing_timer_->FireTimerForTesting();
}

void BackgroundTracingActiveScenario::SetRuleTriggeredCallbackForTesting(
    const base::RepeatingClosure& callback) {
  rule_triggered_callback_for_testing_ = callback;
}

void BackgroundTracingActiveScenario::StartTracingIfConfigNeedsIt() {
  DCHECK(config_);
  if (config_->tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE) {
    StartTracing(config_->category_preset(),
                 base::trace_event::RECORD_CONTINUOUSLY);
    return;
  }

  // There is nothing to do in case of reactive tracing.
}

void BackgroundTracingActiveScenario::StartTracing(
    BackgroundTracingConfigImpl::CategoryPreset preset,
    base::trace_event::TraceRecordMode record_mode) {
  TraceConfig config = BackgroundTracingConfigImpl::GetConfigForCategoryPreset(
      preset, record_mode);
  if (requires_anonymized_data_)
    config.EnableArgumentFilter();
#if defined(OS_ANDROID)
  // Set low trace buffer size on Android in order to upload small trace files.
  if (config_->tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE) {
    config.SetTraceBufferSizeInEvents(20000);
    config.SetTraceBufferSizeInKb(500);
  }
#else
  // TODO(crbug.com/941318): Re-enable startup tracing for Android once all
  // Perfetto-related deadlocks are resolved.
  if (!TracingControllerImpl::GetInstance()->IsTracing() &&
      tracing::TracingUsesPerfettoBackend()) {
    // TODO(oysteine): This should pass in |requires_anonymized_data_| instead
    // of false only when using consumer API with proto output. But, for JSON
    // output we still need this to be false since filtering happens in the JSON
    // exporter.
    tracing::TraceEventDataSource::GetInstance()->SetupStartupTracing(
        /*privacy_filtering_enabled=*/false);
  }
#endif

  if (!TracingControllerImpl::GetInstance()->StartTracing(
          config,
          base::BindOnce(
              &BackgroundTracingManagerImpl::OnStartTracingDone,
              base::Unretained(BackgroundTracingManagerImpl::GetInstance()),
              preset))) {
    AbortScenario();
    return;
  }

  SetState(State::kTracing);

  // Activate the categories immediately. StartTracing eventually does this
  // itself, but asynchronously via PostTask, and in the meantime events will be
  // dropped. This ensures that we start recording events for those categories
  // immediately.
  uint8_t modes = base::trace_event::TraceLog::RECORDING_MODE;
  if (!config.event_filters().empty())
    modes |= base::trace_event::TraceLog::FILTERING_MODE;
  base::trace_event::TraceLog::GetInstance()->SetEnabled(config, modes);

  BackgroundTracingManagerImpl::RecordMetric(Metrics::RECORDING_ENABLED);
}

void BackgroundTracingActiveScenario::BeginFinalizing(
    BackgroundTracingManager::StartedFinalizingCallback callback) {
  triggered_named_event_handle_ = -1;
  tracing_timer_.reset();

  scoped_refptr<TracingControllerImpl::TraceDataEndpoint> trace_data_endpoint;
  bool is_allowed_finalization =
      BackgroundTracingManagerImpl::GetInstance()->IsAllowedFinalization();
  base::RepeatingClosure started_finalizing_closure;
  if (!callback.is_null()) {
    started_finalizing_closure =
        base::BindRepeating(callback, is_allowed_finalization);
  }

  if (is_allowed_finalization) {
    trace_data_endpoint = TracingControllerImpl::CreateCompressedStringEndpoint(
        TracingControllerImpl::CreateCallbackEndpoint(base::BindRepeating(
            &BackgroundTracingActiveScenario::OnTracingStopped,
            weak_ptr_factory_.GetWeakPtr(),
            std::move(started_finalizing_closure))),
        true /* compress_with_background_priority */);
    BackgroundTracingManagerImpl::RecordMetric(Metrics::FINALIZATION_ALLOWED);
  } else {
    trace_data_endpoint =
        TracingControllerImpl::CreateCallbackEndpoint(base::BindRepeating(
            [](base::RepeatingClosure closure,
               base::WeakPtr<BackgroundTracingActiveScenario> active_scenario,
               std::unique_ptr<const base::DictionaryValue> metadata,
               base::RefCountedString* file_contents) {
              if (active_scenario) {
                active_scenario->SetState(State::kAborted);
              }

              if (closure) {
                std::move(closure).Run();
              }
            },
            std::move(started_finalizing_closure),
            weak_ptr_factory_.GetWeakPtr()));
    BackgroundTracingManagerImpl::RecordMetric(
        Metrics::FINALIZATION_DISALLOWED);
  }

  TracingControllerImpl::GetInstance()->StopTracing(trace_data_endpoint);
}

void BackgroundTracingActiveScenario::OnTracingStopped(
    base::RepeatingClosure started_finalizing_closure,
    std::unique_ptr<const base::DictionaryValue> metadata,
    base::RefCountedString* file_contents) {
  SetState(State::kFinalizing);
  BackgroundTracingManagerImpl::RecordMetric(Metrics::FINALIZATION_STARTED);
  UMA_HISTOGRAM_MEMORY_KB("Tracing.Background.FinalizingTraceSizeInKB",
                          file_contents->size() / 1024);

  if (!receive_callback_.is_null()) {
    receive_callback_.Run(
        file_contents, std::move(metadata),
        base::BindOnce(&BackgroundTracingActiveScenario::OnFinalizeComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (!started_finalizing_closure.is_null()) {
    std::move(started_finalizing_closure).Run();
  }
}

void BackgroundTracingActiveScenario::OnFinalizeComplete(bool success) {
  if (success) {
    BackgroundTracingManagerImpl::RecordMetric(Metrics::UPLOAD_SUCCEEDED);
  } else {
    BackgroundTracingManagerImpl::RecordMetric(Metrics::UPLOAD_FAILED);
  }

  SetState(State::kIdle);

  // Now that a trace has completed, we may need to enable recording again.
  StartTracingIfConfigNeedsIt();
}

void BackgroundTracingActiveScenario::AbortScenario() {
  if ((state() != State::kTracing)) {
    // Setting the kAborted state will cause |this| to be destroyed.
    SetState(State::kAborted);
    return;
  }

  auto trace_data_endpoint =
      TracingControllerImpl::CreateCallbackEndpoint(base::BindRepeating(
          [](base::WeakPtr<BackgroundTracingActiveScenario> active_scenario,
             std::unique_ptr<const base::DictionaryValue>,
             base::RefCountedString*) {
            if (active_scenario) {
              active_scenario->SetState(State::kAborted);
            }
          },
          weak_ptr_factory_.GetWeakPtr()));

  TracingControllerImpl::GetInstance()->StopTracing(trace_data_endpoint);
}

void BackgroundTracingActiveScenario::TriggerNamedEvent(
    BackgroundTracingManager::TriggerHandle handle,
    BackgroundTracingManager::StartedFinalizingCallback callback) {
  std::string trigger_name =
      BackgroundTracingManagerImpl::GetInstance()->GetTriggerNameFromHandle(
          handle);
  auto* triggered_rule = GetRuleAbleToTriggerTracing(trigger_name);
  if (!triggered_rule) {
    if (!callback.is_null()) {
      std::move(callback).Run(false);
    }
    return;
  }

  // A different reactive config than the running one tried to trigger.
  if ((config_->tracing_mode() == BackgroundTracingConfigImpl::REACTIVE &&
       (state() == State::kTracing) &&
       triggered_named_event_handle_ != handle)) {
    if (!callback.is_null()) {
      std::move(callback).Run(false);
    }
    return;
  }

  triggered_named_event_handle_ = handle;
  OnRuleTriggered(triggered_rule, std::move(callback));
}

void BackgroundTracingActiveScenario::OnHistogramTrigger(
    const std::string& histogram_name) {
  for (const auto& rule : config_->rules()) {
    if (rule->ShouldTriggerNamedEvent(histogram_name)) {
      OnRuleTriggered(rule.get(),
                      BackgroundTracingManager::StartedFinalizingCallback());
    }
  }
}

void BackgroundTracingActiveScenario::OnRuleTriggered(
    const BackgroundTracingRule* triggered_rule,
    BackgroundTracingManager::StartedFinalizingCallback callback) {
  double trigger_chance = triggered_rule->trigger_chance();
  if (trigger_chance < 1.0 && base::RandDouble() > trigger_chance) {
    if (!callback.is_null()) {
      std::move(callback).Run(false);
    }
    return;
  }

  last_triggered_rule_.reset(new base::DictionaryValue);
  triggered_rule->IntoDict(last_triggered_rule_.get());

  int trace_delay = triggered_rule->GetTraceDelay();

  if (config_->tracing_mode() == BackgroundTracingConfigImpl::REACTIVE) {
    // In reactive mode, a trigger starts tracing, or finalizes tracing
    // immediately if it's already running.
    BackgroundTracingManagerImpl::RecordMetric(Metrics::REACTIVE_TRIGGERED);

    if (state() != State::kTracing) {
      // It was not already tracing, start a new trace.
      StartTracing(triggered_rule->category_preset(),
                   base::trace_event::RECORD_UNTIL_FULL);
    } else {
      // Some reactive configs that trigger again while tracing should just
      // end right away (to not capture multiple navigations, for example).
      // For others we just want to ignore the repeated trigger.
      if (triggered_rule->stop_tracing_on_repeated_reactive()) {
        trace_delay = -1;
      } else {
        if (!callback.is_null()) {
          std::move(callback).Run(false);
        }
        return;
      }
    }
  } else {
    // In preemptive mode, a trigger starts finalizing a trace if one is
    // running and we haven't got a finalization timer running,
    // otherwise we do nothing.
    if ((state() != State::kTracing) || tracing_timer_) {
      if (!callback.is_null()) {
        std::move(callback).Run(false);
      }
      return;
    }

    BackgroundTracingManagerImpl::RecordMetric(Metrics::PREEMPTIVE_TRIGGERED);
  }

  if (trace_delay < 0) {
    BeginFinalizing(std::move(callback));
  } else {
    tracing_timer_ = std::make_unique<TracingTimer>(this, std::move(callback));
    tracing_timer_->StartTimer(trace_delay);
  }

  if (!rule_triggered_callback_for_testing_.is_null()) {
    rule_triggered_callback_for_testing_.Run();
  }
}

BackgroundTracingRule*
BackgroundTracingActiveScenario::GetRuleAbleToTriggerTracing(
    const std::string& trigger_name) {
  // If the last trace is still uploading, we don't allow a new one to trigger.
  if (state() == State::kFinalizing) {
    return nullptr;
  }

  for (const auto& rule : config_->rules()) {
    if (rule->ShouldTriggerNamedEvent(trigger_name)) {
      return rule.get();
    }
  }

  return nullptr;
}

void BackgroundTracingActiveScenario::GenerateMetadataDict(
    base::DictionaryValue* metadata_dict) {
  auto config_dict = std::make_unique<base::DictionaryValue>();
  config_->IntoDict(config_dict.get());
  metadata_dict->Set("config", std::move(config_dict));
  metadata_dict->SetString("scenario_name", config_->scenario_name());

  if (last_triggered_rule_) {
    metadata_dict->Set("last_triggered_rule", std::move(last_triggered_rule_));
  }
}

}  // namespace content
