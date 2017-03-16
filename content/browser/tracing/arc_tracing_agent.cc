// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/arc_tracing_agent.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"

namespace content {

namespace {

constexpr char kArcTracingAgentName[] = "arc";
constexpr char kArcTraceLabel[] = "ArcTraceEvents";

void OnStopTracing(bool success) {
  DLOG_IF(WARNING, !success) << "Failed to stop ARC tracing.";
}

class ArcTracingAgentImpl : public ArcTracingAgent {
 public:
  // base::trace_event::TracingAgent overrides:
  std::string GetTracingAgentName() override { return kArcTracingAgentName; }

  std::string GetTraceEventLabel() override { return kArcTraceLabel; }

  void StartAgentTracing(const base::trace_event::TraceConfig& trace_config,
                         const StartAgentTracingCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    // delegate_ may be nullptr if ARC is not enabled on the system. In such
    // case, simply do nothing.
    if (!delegate_) {
      // Use PostTask as the convention of TracingAgent. The caller expects
      // callback to be called after this function returns.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(callback, GetTracingAgentName(), false));
      return;
    }

    delegate_->StartTracing(trace_config,
                            base::Bind(callback, GetTracingAgentName()));
  }

  void StopAgentTracing(const StopAgentTracingCallback& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (delegate_)
      delegate_->StopTracing(base::Bind(OnStopTracing));

    // Trace data is collect via systrace (debugd) in dev-mode. Simply
    // return empty data here.
    std::string no_data;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, GetTracingAgentName(), GetTraceEventLabel(),
                   base::RefCountedString::TakeString(&no_data)));
  }

  // ArcTracingAgent overrides:
  void SetDelegate(Delegate* delegate) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    delegate_ = delegate;
  }

  static ArcTracingAgentImpl* GetInstance() {
    return base::Singleton<ArcTracingAgentImpl>::get();
  }

 private:
  // This allows constructor and destructor to be private and usable only
  // by the Singleton class.
  friend struct base::DefaultSingletonTraits<ArcTracingAgentImpl>;

  ArcTracingAgentImpl() = default;
  ~ArcTracingAgentImpl() override = default;

  Delegate* delegate_ = nullptr;  // Owned by ArcServiceLauncher.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ArcTracingAgentImpl);
};

}  // namespace

// static
ArcTracingAgent* ArcTracingAgent::GetInstance() {
  return ArcTracingAgentImpl::GetInstance();
}

ArcTracingAgent::ArcTracingAgent() = default;
ArcTracingAgent::~ArcTracingAgent() = default;

ArcTracingAgent::Delegate::~Delegate() = default;

}  // namespace content
