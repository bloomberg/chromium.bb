// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_SYNCHRONOUS_COMPOSITOR_ANDROID_H_
#define CONTENT_PUBLIC_TEST_TEST_SYNCHRONOUS_COMPOSITOR_ANDROID_H_

#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/android/synchronous_compositor_client.h"

namespace content {

class CONTENT_EXPORT TestSynchronousCompositor : public SynchronousCompositor {
 public:
  TestSynchronousCompositor();
  ~TestSynchronousCompositor() override;

  void SetClient(SynchronousCompositorClient* client);

  // SynchronousCompositor overrides.
  bool InitializeHwDraw() override;
  void ReleaseHwDraw() override;
  scoped_ptr<cc::CompositorFrame> DemandDrawHw(
      gfx::Size surface_size,
      const gfx::Transform& transform,
      gfx::Rect viewport,
      gfx::Rect clip,
      gfx::Rect viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) override;
  void ReturnResources(const cc::CompositorFrameAck& frame_ack) override;
  bool DemandDrawSw(SkCanvas* canvas) override;
  void SetMemoryPolicy(size_t bytes_limit) override;
  void DidChangeRootLayerScrollOffset() override {}

 private:
  SynchronousCompositorClient* client_;
  bool hardware_initialized_;

  DISALLOW_COPY_AND_ASSIGN(TestSynchronousCompositor);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_SYNCHRONOUS_COMPOSITOR_ANDROID_H_
