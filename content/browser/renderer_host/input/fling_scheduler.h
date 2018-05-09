// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_H_

#include "content/browser/renderer_host/input/fling_controller.h"

namespace ui {
class Compositor;
}

namespace content {

class RenderWidgetHostImpl;

class CONTENT_EXPORT FlingScheduler : public FlingControllerSchedulerClient {
 public:
  FlingScheduler(RenderWidgetHostImpl* host);
  ~FlingScheduler() override;

  // FlingControllerSchedulerClient
  void ScheduleFlingProgress(
      base::WeakPtr<FlingController> fling_controller) override;
  void DidStopFlingingOnBrowser(
      base::WeakPtr<FlingController> fling_controller) override;

  void ProgressFlingOnBeginFrameIfneeded(base::TimeTicks current_time);

 protected:
  virtual ui::Compositor* GetCompositor();
  RenderWidgetHostImpl* host_;
  base::WeakPtr<FlingController> fling_controller_;

 private:
  bool is_observer_added_ = false;

  DISALLOW_COPY_AND_ASSIGN(FlingScheduler);
};
}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_SCHEDULER_H_
