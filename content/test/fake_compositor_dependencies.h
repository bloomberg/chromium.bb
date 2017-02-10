// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_
#define CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_

#include "base/macros.h"
#include "cc/output/buffer_to_texture_target_map.h"
#include "cc/test/test_task_graph_runner.h"
#include "content/renderer/gpu/compositor_dependencies.h"
#include "third_party/WebKit/public/platform/scheduler/test/fake_renderer_scheduler.h"

namespace content {

class FakeCompositorDependencies : public CompositorDependencies {
 public:
  FakeCompositorDependencies();
  ~FakeCompositorDependencies() override;

  // CompositorDependencies implementation.
  bool IsGpuRasterizationForced() override;
  bool IsAsyncWorkerContextEnabled() override;
  int GetGpuRasterizationMSAASampleCount() override;
  bool IsLcdTextEnabled() override;
  bool IsDistanceFieldTextEnabled() override;
  bool IsZeroCopyEnabled() override;
  bool IsPartialRasterEnabled() override;
  bool IsGpuMemoryBufferCompositorResourcesEnabled() override;
  bool IsElasticOverscrollEnabled() override;
  const cc::BufferToTextureTargetMap& GetBufferToTextureTargetMap() override;
  scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorMainThreadTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorImplThreadTaskRunner() override;
  blink::scheduler::RendererScheduler* GetRendererScheduler() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  bool AreImageDecodeTasksEnabled() override;
  bool IsThreadedAnimationEnabled() override;
  bool IsScrollAnimatorEnabled() override;

 private:
  cc::TestTaskGraphRunner task_graph_runner_;
  blink::scheduler::FakeRendererScheduler renderer_scheduler_;
  cc::BufferToTextureTargetMap buffer_to_texture_target_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeCompositorDependencies);
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_
