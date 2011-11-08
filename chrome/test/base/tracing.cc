// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/tracing.h"

#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/trace_controller.h"

namespace {

class InProcessTraceController : public TraceSubscriber {
 public:
  static InProcessTraceController* GetInstance() {
    return Singleton<InProcessTraceController>::get();
  }

  InProcessTraceController() {}
  virtual ~InProcessTraceController() {}

  bool BeginTracing(const std::string& categories) {
    return TraceController::GetInstance()->BeginTracing(this, categories);
  }

  bool EndTracing(std::string* json_trace_output) {
    using namespace base::debug;

    TraceResultBuffer::SimpleOutput output;
    trace_buffer_.SetOutputCallback(output.GetCallback());

    trace_buffer_.Start();
    if (!TraceController::GetInstance()->EndTracingAsync(this))
      return false;
    // Wait for OnEndTracingComplete() to quit the message loop.
    // OnTraceDataCollected may be called multiple times while blocking here.
    ui_test_utils::RunMessageLoop();
    trace_buffer_.Finish();
    trace_buffer_.SetOutputCallback(TraceResultBuffer::OutputCallback());

    *json_trace_output = output.json_output;
    return true;
  }

 private:
  friend struct DefaultSingletonTraits<InProcessTraceController>;

  // TraceSubscriber
  virtual void OnEndTracingComplete() OVERRIDE {
    MessageLoopForUI::current()->Quit();
  }

  // TraceSubscriber
  virtual void OnTraceDataCollected(const std::string& trace_fragment)
      OVERRIDE {
    trace_buffer_.AddFragment(trace_fragment);
  }

  // For collecting trace data asynchronously.
  base::debug::TraceResultBuffer trace_buffer_;

  DISALLOW_COPY_AND_ASSIGN(InProcessTraceController);
};

}  // namespace

namespace tracing {

bool BeginTracing(const std::string& categories) {
  return InProcessTraceController::GetInstance()->BeginTracing(categories);
}

bool EndTracing(std::string* json_trace_output) {
  return InProcessTraceController::GetInstance()->EndTracing(json_trace_output);
}

}  // namespace tracing

