// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_WEBTHREAD_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_WEBTHREAD_IMPL_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "third_party/WebKit/public/platform/WebThread.h"

namespace html_viewer {

class WebThreadBase : public blink::WebThread {
 public:
  ~WebThreadBase() override;

  void addTaskObserver(TaskObserver* observer) override;
  void removeTaskObserver(TaskObserver* observer) override;

  bool isCurrentThread() const override = 0;

 protected:
  WebThreadBase();

 private:
  class TaskObserverAdapter;

  typedef std::map<TaskObserver*, TaskObserverAdapter*> TaskObserverMap;
  TaskObserverMap task_observer_map_;
};

class WebThreadImpl : public WebThreadBase {
 public:
  explicit WebThreadImpl(const char* name);
  ~WebThreadImpl() override;

  void postTask(const blink::WebTraceLocation& location, Task* task) override;
  void postDelayedTask(const blink::WebTraceLocation& location,
                       Task* task,
                       long long delay_ms) override;

  void enterRunLoop() override;
  void exitRunLoop() override;

  base::MessageLoop* message_loop() const { return thread_->message_loop(); }

  bool isCurrentThread() const override;
  blink::PlatformThreadId threadId() const override;

 private:
  scoped_ptr<base::Thread> thread_;
};

class WebThreadImplForMessageLoop : public WebThreadBase {
 public:
  explicit WebThreadImplForMessageLoop(
      base::MessageLoopProxy* message_loop);
  ~WebThreadImplForMessageLoop() override;

  void postTask(const blink::WebTraceLocation& location, Task* task) override;
  void postDelayedTask(const blink::WebTraceLocation& location,
                       Task* task,
                       long long delay_ms) override;

  void enterRunLoop() override;
  void exitRunLoop() override;

 private:
  bool isCurrentThread() const override;
  blink::PlatformThreadId threadId() const override;

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  blink::PlatformThreadId thread_id_;
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBTHREAD_IMPL_H_
