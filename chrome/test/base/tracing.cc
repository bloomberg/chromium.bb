// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/tracing.h"

#include "base/debug/trace_event.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/trace_controller.h"
#include "content/public/browser/trace_subscriber.h"

namespace {

class InProcessTraceController : public content::TraceSubscriber {
 public:
  static InProcessTraceController* GetInstance() {
    return Singleton<InProcessTraceController>::get();
  }

  InProcessTraceController() {}
  virtual ~InProcessTraceController() {}

  bool BeginTracing(const std::string& categories) {
    return content::TraceController::GetInstance()->BeginTracing(
        this, categories);
  }

  bool EndTracing(std::string* json_trace_output) {
    using namespace base::debug;

    TraceResultBuffer::SimpleOutput output;
    trace_buffer_.SetOutputCallback(output.GetCallback());

    trace_buffer_.Start();
    if (!content::TraceController::GetInstance()->EndTracingAsync(this))
      return false;
    // Wait for OnEndTracingComplete() to quit the message loop.
    // OnTraceDataCollected may be called multiple times while blocking here.
    message_loop_runner_ = new ui_test_utils::MessageLoopRunner;
    message_loop_runner_->Run();
    trace_buffer_.Finish();
    trace_buffer_.SetOutputCallback(TraceResultBuffer::OutputCallback());

    *json_trace_output = output.json_output;
    return true;
  }

 private:
  friend struct DefaultSingletonTraits<InProcessTraceController>;

  // TraceSubscriber
  virtual void OnEndTracingComplete() OVERRIDE {
    message_loop_runner_->Quit();
  }

  // TraceSubscriber
  virtual void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& trace_fragment) OVERRIDE {
    trace_buffer_.AddFragment(trace_fragment->data());
  }

  // For collecting trace data asynchronously.
  base::debug::TraceResultBuffer trace_buffer_;

  scoped_refptr<ui_test_utils::MessageLoopRunner> message_loop_runner_;

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

