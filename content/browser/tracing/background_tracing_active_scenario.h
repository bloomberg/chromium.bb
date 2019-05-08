// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_ACTIVE_SCENARIO_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_ACTIVE_SCENARIO_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/public/browser/background_tracing_manager.h"

namespace base {
class RefCountedString;
}  // namespace base

namespace content {

class BackgroundTracingConfigImpl;

class BackgroundTracingActiveScenario {
 public:
  enum class State { kIdle, kTracing, kFinalizing, kUploading, kAborted };

  BackgroundTracingActiveScenario(
      std::unique_ptr<const BackgroundTracingConfigImpl> config,
      bool requires_anonymized_data,
      BackgroundTracingManager::ReceiveCallback receive_callback,
      base::OnceClosure on_aborted_callback);
  ~BackgroundTracingActiveScenario();

  void StartTracingIfConfigNeedsIt();
  void AbortScenario();

  const BackgroundTracingConfigImpl* GetConfig() const;
  void GenerateMetadataDict(base::DictionaryValue* metadata_dict);
  State state() const { return scenario_state_; }
  bool requires_anonymized_data() const { return requires_anonymized_data_; }

  void TriggerNamedEvent(
      BackgroundTracingManager::TriggerHandle handle,
      BackgroundTracingManager::StartedFinalizingCallback callback);
  void OnHistogramTrigger(const std::string& histogram_name);
  void OnRuleTriggered(
      const BackgroundTracingRule* triggered_rule,
      BackgroundTracingManager::StartedFinalizingCallback callback);

  // For testing
  CONTENT_EXPORT void FireTimerForTesting();
  CONTENT_EXPORT void SetRuleTriggeredCallbackForTesting(
      const base::RepeatingClosure& callback);

 private:
  void StartTracing(BackgroundTracingConfigImpl::CategoryPreset,
                    base::trace_event::TraceRecordMode);

  void BeginFinalizing(
      BackgroundTracingManager::StartedFinalizingCallback callback);
  void OnTracingStopped(base::RepeatingClosure started_finalizing_closure,
                        std::unique_ptr<const base::DictionaryValue> metadata,
                        base::RefCountedString*);

  void OnFinalizeComplete(bool success);

  BackgroundTracingRule* GetRuleAbleToTriggerTracing(
      const std::string& trigger_name);

  void SetState(State new_state);

  std::unique_ptr<const BackgroundTracingConfigImpl> config_;
  bool requires_anonymized_data_;
  State scenario_state_;
  std::unique_ptr<base::DictionaryValue> last_triggered_rule_;
  base::RepeatingClosure rule_triggered_callback_for_testing_;
  BackgroundTracingManager::ReceiveCallback receive_callback_;
  BackgroundTracingManager::TriggerHandle triggered_named_event_handle_;
  base::OnceClosure on_aborted_callback_;

  class TracingTimer;
  std::unique_ptr<TracingTimer> tracing_timer_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<BackgroundTracingActiveScenario> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingActiveScenario);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_ACTIVE_SCENARIO_H_
