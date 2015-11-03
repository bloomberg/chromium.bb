// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_COMPOSITOR_BLIMP_COMPOSITOR_H_
#define BLIMP_CLIENT_COMPOSITOR_BLIMP_COMPOSITOR_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}

namespace cc {
class LayerTreeHost;
}

namespace blimp {

// BlimpCompositor provides the basic framework and setup to host a
// LayerTreeHost (compositor).  The rendering surface this compositor draws to
// is defined by the gfx::AcceleratedWidget returned by
// BlimpCompositor::GetWindow().
class BlimpCompositor : public cc::LayerTreeHostClient {
 public:
  ~BlimpCompositor() override;

  void SetVisible(bool visible);
  void SetSize(const gfx::Size& size);

  // LayerTreeHostClient implementation.
  void WillBeginMainFrame() override;
  void DidBeginMainFrame() override;
  void BeginMainFrame(const cc::BeginFrameArgs& args) override;
  void BeginMainFrameNotExpectedSoon() override;
  void UpdateLayerTreeHost() override;
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override;
  void RequestNewOutputSurface() override;
  void DidInitializeOutputSurface() override;
  void DidFailToInitializeOutputSurface() override;
  void WillCommit() override;
  void DidCommit() override;
  void DidCommitAndDrawFrame() override;
  void DidCompleteSwapBuffers() override;
  void DidCompletePageScaleAnimation() override;
  void RecordFrameTimingEvents(
      scoped_ptr<cc::FrameTimingTracker::CompositeTimingSet> composite_events,
      scoped_ptr<cc::FrameTimingTracker::MainFrameTimingSet> main_frame_events)
      override;

 protected:
  // |dp_to_px| is the scale factor required to move from dp (device pixels) to
  // px.
  explicit BlimpCompositor(float dp_to_px);

  // Returns a widget for use in constructing this compositor's OutputSurface.
  // Will only ever be called while this compositor is visible.
  virtual gfx::AcceleratedWidget GetWindow() = 0;

  // Populates the cc::LayerTreeSettings used by the cc::LayerTreeHost.  Can be
  // overridden to provide custom settings parameters.
  virtual void GenerateLayerTreeSettings(cc::LayerTreeSettings* settings);

 private:
  // Creates (if necessary) and returns a TaskRunner for a thread meant to run
  // compositor rendering.
  scoped_refptr<base::SingleThreadTaskRunner> GetCompositorTaskRunner();

  gfx::Size viewport_size_;

  // The scale factor used to convert dp units (device independent pixels) to
  // pixels.
  float device_scale_factor_;
  scoped_ptr<cc::LayerTreeHost> host_;
  scoped_ptr<cc::LayerTreeSettings> settings_;

  // Lazily created thread that will run the compositor rendering tasks.
  scoped_ptr<base::Thread> compositor_thread_;

  DISALLOW_COPY_AND_ASSIGN(BlimpCompositor);
};

}  // namespace blimp

#endif  // BLIMP_CLIENT_COMPOSITOR_BLIMP_COMPOSITOR_H_
