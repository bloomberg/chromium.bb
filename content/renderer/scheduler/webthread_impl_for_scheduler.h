// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_WEBTHREAD_IMPL_FOR_SCHEDULER_H_
#define CONTENT_RENDERER_SCHEDULER_WEBTHREAD_IMPL_FOR_SCHEDULER_H_

#include "content/child/webthread_impl.h"

#include "base/containers/scoped_ptr_hash_map.h"

namespace content {

class RendererScheduler;

class CONTENT_EXPORT WebThreadImplForScheduler : public WebThreadBase {
 public:
  explicit WebThreadImplForScheduler(RendererScheduler* scheduler);
  virtual ~WebThreadImplForScheduler();

  // blink::WebThread implementation.
  blink::PlatformThreadId threadId() const override;

  // WebThreadBase implementation.
  base::SingleThreadTaskRunner* TaskRunner() const override;

 private:
  base::MessageLoop* MessageLoop() const override;
  void AddTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;
  void RemoveTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  RendererScheduler* scheduler_;  // Not owned.
  blink::PlatformThreadId thread_id_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_WEBTHREAD_IMPL_FOR_SCHEDULER_H_
