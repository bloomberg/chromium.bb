// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_WEB_VIEW_SCHEDULER_IMPL_H_
#define COMPONENTS_SCHEDULER_RENDERER_WEB_VIEW_SCHEDULER_IMPL_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/scheduler/base/task_queue.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/platform/WebViewScheduler.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {
class WebView;
}  // namespace blink

namespace scheduler {

class AutoAdvancingVirtualTimeDomain;
class RendererSchedulerImpl;
class WebFrameSchedulerImpl;

class SCHEDULER_EXPORT WebViewSchedulerImpl : public blink::WebViewScheduler {
 public:
  WebViewSchedulerImpl(blink::WebView* web_view,
                       RendererSchedulerImpl* renderer_scheduler,
                       bool disable_background_timer_throttling);

  ~WebViewSchedulerImpl() override;

  // blink::WebViewScheduler implementation:
  void setPageVisible(bool page_visible) override;
  blink::WebPassOwnPtr<blink::WebFrameScheduler> createFrameScheduler()
      override;

  // TODO(alexclarke): Expose in blink::WebViewScheduler.
  void enableVirtualTime();
  void setAllowVirtualTimeToAdvance(
      bool allow_virtual_time_to_advance);

  // Virtual for testing.
  virtual void AddConsoleWarning(const std::string& message);

  scoped_ptr<WebFrameSchedulerImpl> createWebFrameSchedulerImpl();

 private:
  friend class WebFrameSchedulerImpl;

  void Unregister(WebFrameSchedulerImpl* frame_scheduler);

  AutoAdvancingVirtualTimeDomain* virtual_time_domain() const {
    return virtual_time_domain_.get();
  }

  TaskQueue::PumpPolicy GetVirtualTimePumpPolicy() const;

  std::set<WebFrameSchedulerImpl*> frame_schedulers_;
  scoped_ptr<AutoAdvancingVirtualTimeDomain> virtual_time_domain_;
  TaskQueue::PumpPolicy virtual_time_pump_policy_;
  blink::WebView* web_view_;
  RendererSchedulerImpl* renderer_scheduler_;
  bool page_visible_;
  bool disable_background_timer_throttling_;
  bool allow_virtual_time_to_advance_;

  DISALLOW_COPY_AND_ASSIGN(WebViewSchedulerImpl);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_WEB_VIEW_SCHEDULER_IMPL_H_
