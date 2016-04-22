// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/tracing.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/test/test_utils.h"

namespace {

using content::BrowserThread;

class StringTraceSink : public content::TracingController::TraceDataSink {
 public:
  StringTraceSink(std::string* result, const base::Closure& callback)
      : result_(result), completion_callback_(callback) {}

  void AddTraceChunk(const std::string& chunk) override {
    *result_ += result_->empty() ? "[" : ",";
    *result_ += chunk;
  }
  void Close() override {
    if (!result_->empty())
      *result_ += "]";
    completion_callback_.Run();
  }

 private:
  ~StringTraceSink() override {}

  std::string* result_;
  base::Closure completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(StringTraceSink);
};

class InProcessTraceController {
 public:
  static InProcessTraceController* GetInstance() {
    return base::Singleton<InProcessTraceController>::get();
  }

  InProcessTraceController()
      : is_waiting_on_watch_(false),
        watch_notification_count_(0) {}
  virtual ~InProcessTraceController() {}

  bool BeginTracing(const std::string& category_patterns) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return content::TracingController::GetInstance()->StartTracing(
        base::trace_event::TraceConfig(category_patterns, ""),
        content::TracingController::StartTracingDoneCallback());
  }

  bool BeginTracingWithWatch(const std::string& category_patterns,
                             const std::string& category_name,
                             const std::string& event_name,
                             int num_occurrences) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(num_occurrences > 0);
    watch_notification_count_ = num_occurrences;
    if (!content::TracingController::GetInstance()->SetWatchEvent(
            category_name, event_name,
            base::Bind(&InProcessTraceController::OnWatchEventMatched,
                       base::Unretained(this)))) {
      return false;
    }
    if (!content::TracingController::GetInstance()->StartTracing(
            base::trace_event::TraceConfig(category_patterns, ""),
            base::Bind(&InProcessTraceController::OnEnableTracingComplete,
                       base::Unretained(this)))) {
      return false;
    }

    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    return true;
  }

  bool WaitForWatchEvent(base::TimeDelta timeout) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (watch_notification_count_ == 0)
      return true;

    if (!timeout.is_zero()) {
      timer_.Start(FROM_HERE, timeout, this,
                   &InProcessTraceController::Timeout);
    }

    is_waiting_on_watch_ = true;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    is_waiting_on_watch_ = false;

    return watch_notification_count_ == 0;
  }

  bool EndTracing(std::string* json_trace_output) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    using namespace base::debug;

    if (!content::TracingController::GetInstance()->StopTracing(
            new StringTraceSink(
                json_trace_output,
                base::Bind(&InProcessTraceController::OnTracingComplete,
                           base::Unretained(this))))) {
      return false;
    }
    // Wait for OnEndTracingComplete() to quit the message loop.
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();

    // Watch notifications can occur during this method's message loop run, but
    // not after, so clear them here.
    watch_notification_count_ = 0;
    return true;
  }

 private:
  friend struct base::DefaultSingletonTraits<InProcessTraceController>;

  void OnEnableTracingComplete() {
    message_loop_runner_->Quit();
  }

  void OnTracingComplete() { message_loop_runner_->Quit(); }

  void OnWatchEventMatched() {
    if (watch_notification_count_ == 0)
      return;
    if (--watch_notification_count_ == 0) {
      timer_.Stop();
      if (is_waiting_on_watch_)
        message_loop_runner_->Quit();
    }
  }

  void Timeout() {
    DCHECK(is_waiting_on_watch_);
    message_loop_runner_->Quit();
  }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  base::OneShotTimer timer_;

  bool is_waiting_on_watch_;
  int watch_notification_count_;

  DISALLOW_COPY_AND_ASSIGN(InProcessTraceController);
};

}  // namespace

namespace tracing {

bool BeginTracing(const std::string& category_patterns) {
  return InProcessTraceController::GetInstance()->BeginTracing(
      category_patterns);
}

bool BeginTracingWithWatch(const std::string& category_patterns,
                           const std::string& category_name,
                           const std::string& event_name,
                           int num_occurrences) {
  return InProcessTraceController::GetInstance()->BeginTracingWithWatch(
      category_patterns, category_name, event_name, num_occurrences);
}

bool WaitForWatchEvent(base::TimeDelta timeout) {
  return InProcessTraceController::GetInstance()->WaitForWatchEvent(timeout);
}

bool EndTracing(std::string* json_trace_output) {
  return InProcessTraceController::GetInstance()->EndTracing(json_trace_output);
}

}  // namespace tracing

