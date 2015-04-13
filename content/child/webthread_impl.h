// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBTHREAD_IMPL_H_
#define CONTENT_CHILD_WEBTHREAD_IMPL_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebThread.h"

namespace blink {
class WebTraceLocation;
}

namespace content {

class CONTENT_EXPORT WebThreadBase : public blink::WebThread {
 public:
  virtual ~WebThreadBase();

  // blink::WebThread implementation.
  virtual bool isCurrentThread() const;
  virtual blink::PlatformThreadId threadId() const = 0;

  virtual void postTask(const blink::WebTraceLocation& location, Task* task);
  virtual void postDelayedTask(const blink::WebTraceLocation& location,
                               Task* task,
                               long long delay_ms);

  virtual void enterRunLoop();
  virtual void exitRunLoop();

  virtual void addTaskObserver(TaskObserver* observer);
  virtual void removeTaskObserver(TaskObserver* observer);

  // Returns the base::Bind-compatible task runner for posting tasks to this
  // thread. Can be called from any thread.
  virtual base::SingleThreadTaskRunner* TaskRunner() const = 0;

 protected:
  class TaskObserverAdapter;

  WebThreadBase();

  // Returns the underlying MessageLoop for this thread. Only used for entering
  // and exiting a nested run loop. Only called on the thread that the
  // WebThread belongs to.
  virtual base::MessageLoop* MessageLoop() const = 0;

  virtual void AddTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer);
  virtual void RemoveTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer);

  static void RunWebThreadTask(scoped_ptr<blink::WebThread::Task> task);

 private:
  typedef std::map<TaskObserver*, TaskObserverAdapter*> TaskObserverMap;
  TaskObserverMap task_observer_map_;
};

class CONTENT_EXPORT WebThreadImpl : public WebThreadBase {
 public:
  explicit WebThreadImpl(const char* name);
  virtual ~WebThreadImpl();

  // blink::WebThread implementation.
  blink::PlatformThreadId threadId() const override;

  // WebThreadBase implementation.
  base::SingleThreadTaskRunner* TaskRunner() const override;

 private:
  base::MessageLoop* MessageLoop() const override;

  scoped_ptr<base::Thread> thread_;
};

} // namespace content

#endif  // CONTENT_CHILD_WEBTHREAD_IMPL_H_
