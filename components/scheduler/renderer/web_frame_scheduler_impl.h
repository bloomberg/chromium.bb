// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_
#define COMPONENTS_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/platform/WebFrameScheduler.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace scheduler {

class RendererScheduler;
class TaskQueue;
class WebFrameHostSchedulerImpl;
class WebTaskRunnerImpl;

class SCHEDULER_EXPORT WebFrameSchedulerImpl : public blink::WebFrameScheduler {
 public:
  WebFrameSchedulerImpl(RendererScheduler* render_scheduler,
                        WebFrameHostSchedulerImpl* parent_frame_host_scheduler);

  ~WebFrameSchedulerImpl() override;

  // blink::WebFrameScheduler implementation:
  virtual void setFrameVisible(bool visible);
  virtual blink::WebTaskRunner* loadingTaskRunner();
  virtual blink::WebTaskRunner* timerTaskRunner();
  virtual void setFrameOrigin(const blink::WebSecurityOrigin* origin);

 private:
  scoped_refptr<TaskQueue> loading_task_queue_;
  scoped_refptr<TaskQueue> timer_task_queue_;
  scoped_ptr<WebTaskRunnerImpl> loading_web_task_runner_;
  scoped_ptr<WebTaskRunnerImpl> timer_web_task_runner_;
  RendererScheduler* render_scheduler_;                     // NOT OWNED
  WebFrameHostSchedulerImpl* parent_frame_host_scheduler_;  // NOT OWNED
  blink::WebSecurityOrigin origin_;
  bool visible_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameSchedulerImpl);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_
