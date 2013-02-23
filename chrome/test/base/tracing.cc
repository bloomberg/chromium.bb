// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/tracing.h"

#include "base/debug/trace_event.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/trace_controller.h"
#include "content/public/browser/trace_subscriber.h"
#include "content/public/test/test_utils.h"

namespace {

using content::BrowserThread;

class InProcessTraceController : public content::TraceSubscriber {
 public:
  static InProcessTraceController* GetInstance() {
    return Singleton<InProcessTraceController>::get();
  }

  InProcessTraceController()
      : is_waiting_on_watch_(false),
        watch_notification_count_(0) {}
  virtual ~InProcessTraceController() {}

  bool BeginTracing(const std::string& categories) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return content::TraceController::GetInstance()->BeginTracing(
        this, categories, base::debug::TraceLog::RECORD_UNTIL_FULL);
  }

  bool BeginTracingWithWatch(const std::string& categories,
                             const std::string& category_name,
                             const std::string& event_name,
                             int num_occurrences) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(num_occurrences > 0);
    watch_notification_count_ = num_occurrences;
    return BeginTracing(categories) &&
           content::TraceController::GetInstance()->SetWatchEvent(
               this, category_name, event_name);
  }

  bool WaitForWatchEvent(base::TimeDelta timeout) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (watch_notification_count_ == 0)
      return true;

    if (timeout != base::TimeDelta()) {
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
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    using namespace base::debug;

    TraceResultBuffer::SimpleOutput output;
    trace_buffer_.SetOutputCallback(output.GetCallback());

    trace_buffer_.Start();
    if (!content::TraceController::GetInstance()->EndTracingAsync(this))
      return false;
    // Wait for OnEndTracingComplete() to quit the message loop.
    // OnTraceDataCollected may be called multiple times while blocking here.
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    trace_buffer_.Finish();
    trace_buffer_.SetOutputCallback(TraceResultBuffer::OutputCallback());

    *json_trace_output = output.json_output;

    // Watch notifications can occur during this method's message loop run, but
    // not after, so clear them here.
    watch_notification_count_ = 0;
    return true;
  }

 private:
  friend struct DefaultSingletonTraits<InProcessTraceController>;

  // TraceSubscriber implementation
  virtual void OnEndTracingComplete() OVERRIDE {
    message_loop_runner_->Quit();
  }

  // TraceSubscriber implementation
  virtual void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& trace_fragment) OVERRIDE {
    trace_buffer_.AddFragment(trace_fragment->data());
  }

  // TraceSubscriber implementation
  virtual void OnEventWatchNotification() OVERRIDE {
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

  // For collecting trace data asynchronously.
  base::debug::TraceResultBuffer trace_buffer_;

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  base::OneShotTimer<InProcessTraceController> timer_;

  bool is_waiting_on_watch_;
  int watch_notification_count_;

  DISALLOW_COPY_AND_ASSIGN(InProcessTraceController);
};

}  // namespace

namespace tracing {

bool BeginTracing(const std::string& categories) {
  return InProcessTraceController::GetInstance()->BeginTracing(categories);
}

bool BeginTracingWithWatch(const std::string& categories,
                           const std::string& category_name,
                           const std::string& event_name,
                           int num_occurrences) {
  return InProcessTraceController::GetInstance()->BeginTracingWithWatch(
      categories, category_name, event_name, num_occurrences);
}

bool WaitForWatchEvent(base::TimeDelta timeout) {
  return InProcessTraceController::GetInstance()->WaitForWatchEvent(timeout);
}

bool EndTracing(std::string* json_trace_output) {
  return InProcessTraceController::GetInstance()->EndTracing(json_trace_output);
}

}  // namespace tracing

