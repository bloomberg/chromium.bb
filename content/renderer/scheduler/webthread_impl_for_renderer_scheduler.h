// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_WEBTHREAD_IMPL_FOR_RENDERER_SCHEDULER_H_
#define CONTENT_RENDERER_SCHEDULER_WEBTHREAD_IMPL_FOR_RENDERER_SCHEDULER_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "content/child/webthread_base.h"

namespace blink {
class WebScheduler;
};

namespace content {

class RendererScheduler;
class WebSchedulerImpl;

class CONTENT_EXPORT WebThreadImplForRendererScheduler : public WebThreadBase {
 public:
  explicit WebThreadImplForRendererScheduler(RendererScheduler* scheduler);
  virtual ~WebThreadImplForRendererScheduler();

  // blink::WebThread implementation.
  blink::WebScheduler* scheduler() const;
  blink::PlatformThreadId threadId() const override;

  // WebThreadBase implementation.
  base::SingleThreadTaskRunner* TaskRunner() const override;
  SingleThreadIdleTaskRunner* IdleTaskRunner() const override;

 private:
  base::MessageLoop* MessageLoop() const override;
  void AddTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;
  void RemoveTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;

  scoped_ptr<WebSchedulerImpl> web_scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  RendererScheduler* scheduler_;  // Not owned.
  blink::PlatformThreadId thread_id_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_WEBTHREAD_IMPL_FOR_RENDERER_SCHEDULER_H_
