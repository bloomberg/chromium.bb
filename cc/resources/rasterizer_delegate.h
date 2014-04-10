// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTERIZER_DELEGATE_H_
#define CC_RESOURCES_RASTERIZER_DELEGATE_H_

#include <vector>

#include "cc/resources/rasterizer.h"

namespace cc {

class RasterizerDelegate : public RasterizerClient {
 public:
  virtual ~RasterizerDelegate();

  static scoped_ptr<RasterizerDelegate> Create(RasterizerClient* client,
                                               Rasterizer** rasterizers,
                                               size_t num_rasterizers);

  void SetClient(RasterizerClient* client);
  void Shutdown();
  void ScheduleTasks(RasterTaskQueue* queue);
  void CheckForCompletedTasks();

  // Overriden from RasterizerClient:
  virtual bool ShouldForceTasksRequiredForActivationToComplete() const OVERRIDE;
  virtual void DidFinishRunningTasks() OVERRIDE;
  virtual void DidFinishRunningTasksRequiredForActivation() OVERRIDE;

 private:
  RasterizerDelegate(RasterizerClient* client,
                     Rasterizer** rasterizers,
                     size_t num_rasterizers);

  RasterizerClient* client_;
  typedef std::vector<Rasterizer*> RasterizerVector;
  RasterizerVector rasterizers_;
  size_t did_finish_running_tasks_pending_count_;
  size_t did_finish_running_tasks_required_for_activation_pending_count_;
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTERIZER_DELEGATE_H_
