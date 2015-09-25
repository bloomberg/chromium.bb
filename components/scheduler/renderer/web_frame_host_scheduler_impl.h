// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_WEB_FRAME_HOST_SCHEDULER_IMPL_H_
#define COMPONENTS_SCHEDULER_RENDERER_WEB_FRAME_HOST_SCHEDULER_IMPL_H_

#include <set>

#include "base/macros.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/platform/WebFrameHostScheduler.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace scheduler {

class RendererScheduler;
class WebFrameSchedulerImpl;

class SCHEDULER_EXPORT WebFrameHostSchedulerImpl
    : public blink::WebFrameHostScheduler {
 public:
  explicit WebFrameHostSchedulerImpl(RendererScheduler* render_scheduler);

  ~WebFrameHostSchedulerImpl() override;

  // blink::WebFrameHostScheduler implementation:
  virtual void setPageInBackground(bool background);
  virtual blink::WebFrameScheduler* createFrameScheduler();

  void Unregister(WebFrameSchedulerImpl* frame_scheduler);

 private:
  std::set<WebFrameSchedulerImpl*> frame_schedulers_;
  RendererScheduler* render_scheduler_;
  bool background_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameHostSchedulerImpl);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_WEB_FRAME_HOST_SCHEDULER_IMPL_H_
