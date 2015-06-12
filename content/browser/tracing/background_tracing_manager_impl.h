// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_MANAGER_IMPL_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_MANAGER_IMPL_H_

#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/background_tracing_manager.h"

namespace content {

class TracingDelegate;

class BackgroundTracingManagerImpl : public content::BackgroundTracingManager {
 public:
  static BackgroundTracingManagerImpl* GetInstance();

  bool SetActiveScenario(scoped_ptr<BackgroundTracingConfig>,
                         const ReceiveCallback&,
                         DataFiltering data_filtering) override;
  void WhenIdle(IdleCallback idle_callback) override;

  void TriggerNamedEvent(TriggerHandle, StartedFinalizingCallback) override;
  TriggerHandle RegisterTriggerType(const char* trigger_name) override;
  void GetTriggerNameList(std::vector<std::string>* trigger_names) override;

  void InvalidateTriggerHandlesForTesting() override;
  void SetTracingEnabledCallbackForTesting(
      const base::Closure& callback) override;
  void FireTimerForTesting() override;
  bool HasActiveScenarioForTesting() override;

 private:
  BackgroundTracingManagerImpl();
  ~BackgroundTracingManagerImpl() override;

  void EnableRecording(std::string, base::trace_event::TraceRecordMode);
  void EnableRecordingIfConfigNeedsIt();
  void OnFinalizeStarted(scoped_refptr<base::RefCountedString>);
  void OnFinalizeComplete();
  void BeginFinalizing(StartedFinalizingCallback);
  void ValidateStartupScenario();
  void AbortScenario();

  std::string GetTriggerNameFromHandle(TriggerHandle handle) const;
  bool IsTriggerHandleValid(TriggerHandle handle) const;

  bool IsAbleToTriggerTracing(TriggerHandle handle) const;
  bool IsSupportedConfig(BackgroundTracingConfig* config);

  std::string GetCategoryFilterStringForCategoryPreset(
      BackgroundTracingConfig::CategoryPreset) const;

  class TraceDataEndpointWrapper
      : public content::TracingController::TraceDataEndpoint {
   public:
    TraceDataEndpointWrapper(base::Callback<
        void(scoped_refptr<base::RefCountedString>)> done_callback);

    void ReceiveTraceFinalContents(const std::string& file_contents) override;

   private:
    ~TraceDataEndpointWrapper() override;

    base::Callback<void(scoped_refptr<base::RefCountedString>)> done_callback_;
  };

  class TracingTimer {
   public:
    explicit TracingTimer(StartedFinalizingCallback);
    ~TracingTimer();

    void StartTimer();
    void CancelTimer();

    void FireTimerForTesting();

   private:
    void TracingTimerFired();

    base::OneShotTimer<TracingTimer> tracing_timer_;
    StartedFinalizingCallback callback_;
  };

  scoped_ptr<TracingDelegate> delegate_;
  scoped_ptr<content::BackgroundTracingConfig> config_;
  scoped_refptr<TraceDataEndpointWrapper> data_endpoint_wrapper_;
  std::map<TriggerHandle, std::string> trigger_handles_;
  scoped_ptr<TracingTimer> tracing_timer_;
  ReceiveCallback receive_callback_;

  bool is_gathering_;
  bool is_tracing_;
  bool requires_anonymized_data_;
  int trigger_handle_ids_;

  IdleCallback idle_callback_;
  base::Closure tracing_enabled_callback_for_testing_;

  friend struct base::DefaultLazyInstanceTraits<BackgroundTracingManagerImpl>;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_MANAGER_IMPL_H_
