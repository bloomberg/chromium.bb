// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_SYNCHRONOUS_COMPOSITOR_ANDROID_H_
#define CONTENT_PUBLIC_TEST_TEST_SYNCHRONOUS_COMPOSITOR_ANDROID_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/android/synchronous_compositor_client.h"

namespace content {

class CONTENT_EXPORT TestSynchronousCompositor : public SynchronousCompositor {
 public:
  TestSynchronousCompositor();
  ~TestSynchronousCompositor() override;

  void SetClient(SynchronousCompositorClient* client);

  // SynchronousCompositor overrides.
  scoped_ptr<cc::CompositorFrame> DemandDrawHw(
      const gfx::Size& surface_size,
      const gfx::Transform& transform,
      const gfx::Rect& viewport,
      const gfx::Rect& clip,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) override;
  void ReturnResources(const cc::CompositorFrameAck& frame_ack) override {}
  bool DemandDrawSw(SkCanvas* canvas) override;
  void SetMemoryPolicy(size_t bytes_limit) override {}
  void DidChangeRootLayerScrollOffset(
      const gfx::ScrollOffset& root_offset) override {}
  void SetIsActive(bool is_active) override {}
  void OnComputeScroll(base::TimeTicks animate_time) override {}

  void SetHardwareFrame(scoped_ptr<cc::CompositorFrame> frame);

 private:
  SynchronousCompositorClient* client_;
  scoped_ptr<cc::CompositorFrame> hardware_frame_;

  DISALLOW_COPY_AND_ASSIGN(TestSynchronousCompositor);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_SYNCHRONOUS_COMPOSITOR_ANDROID_H_
