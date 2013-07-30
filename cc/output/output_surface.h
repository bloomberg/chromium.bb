// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OUTPUT_SURFACE_H_
#define CC_OUTPUT_OUTPUT_SURFACE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/output/context_provider.h"
#include "cc/output/software_output_device.h"
#include "cc/scheduler/frame_rate_controller.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"

namespace base { class SingleThreadTaskRunner; }

namespace ui { struct LatencyInfo; }

namespace gfx {
class Rect;
class Size;
class Transform;
}

namespace cc {

class CompositorFrame;
class CompositorFrameAck;
struct ManagedMemoryPolicy;
class OutputSurfaceClient;
class OutputSurfaceCallbacks;

// Represents the output surface for a compositor. The compositor owns
// and manages its destruction. Its lifetime is:
//   1. Created on the main thread by the LayerTreeHost through its client.
//   2. Passed to the compositor thread and bound to a client via BindToClient.
//      From here on, it will only be used on the compositor thread.
//   3. If the 3D context is lost, then the compositor will delete the output
//      surface (on the compositor thread) and go back to step 1.
class CC_EXPORT OutputSurface : public FrameRateControllerClient {
 public:
  enum {
    DEFAULT_MAX_FRAMES_PENDING = 2
  };

  explicit OutputSurface(scoped_ptr<WebKit::WebGraphicsContext3D> context3d);

  explicit OutputSurface(scoped_ptr<cc::SoftwareOutputDevice> software_device);

  OutputSurface(scoped_ptr<WebKit::WebGraphicsContext3D> context3d,
                scoped_ptr<cc::SoftwareOutputDevice> software_device);

  virtual ~OutputSurface();

  struct Capabilities {
    Capabilities()
        : delegated_rendering(false),
          max_frames_pending(0),
          deferred_gl_initialization(false),
          draw_and_swap_full_viewport_every_frame(false),
          adjust_deadline_for_parent(true) {}
    bool delegated_rendering;
    int max_frames_pending;
    bool deferred_gl_initialization;
    bool draw_and_swap_full_viewport_every_frame;
    // This doesn't handle the <webview> case, but once BeginFrame is
    // supported natively, we shouldn't need adjust_deadline_for_parent.
    bool adjust_deadline_for_parent;
  };

  const Capabilities& capabilities() const {
    return capabilities_;
  }

  // Obtain the 3d context or the software device associated with this output
  // surface. Either of these may return a null pointer, but not both.
  // In the event of a lost context, the entire output surface should be
  // recreated.
  WebKit::WebGraphicsContext3D* context3d() const {
    return context3d_.get();
  }

  SoftwareOutputDevice* software_device() const {
    return software_device_.get();
  }

  // In the case where both the context3d and software_device are present
  // (namely Android WebView), this is called to determine whether the software
  // device should be used on the current frame.
  virtual bool ForcedDrawToSoftwareDevice() const;

  // Called by the compositor on the compositor thread. This is a place where
  // thread-specific data for the output surface can be initialized, since from
  // this point on the output surface will only be used on the compositor
  // thread.
  virtual bool BindToClient(OutputSurfaceClient* client);

  void InitializeBeginFrameEmulation(
      base::SingleThreadTaskRunner* task_runner,
      bool throttle_frame_production,
      base::TimeDelta interval);

  void SetMaxFramesPending(int max_frames_pending);

  virtual void EnsureBackbuffer();
  virtual void DiscardBackbuffer();

  virtual void Reshape(gfx::Size size, float scale_factor);
  virtual gfx::Size SurfaceSize() const;

  virtual void BindFramebuffer();

  // The implementation may destroy or steal the contents of the CompositorFrame
  // passed in (though it will not take ownership of the CompositorFrame
  // itself).
  virtual void SwapBuffers(CompositorFrame* frame);

  // Notifies frame-rate smoothness preference. If true, all non-critical
  // processing should be stopped, or lowered in priority.
  virtual void UpdateSmoothnessTakesPriority(bool prefer_smoothness) {}

  // Requests a BeginFrame notification from the output surface. The
  // notification will be delivered by calling
  // OutputSurfaceClient::BeginFrame until the callback is disabled.
  virtual void SetNeedsBeginFrame(bool enable);

 protected:
  // Synchronously initialize context3d and enter hardware mode.
  // This can only supported in threaded compositing mode.
  // |offscreen_context_provider| should match what is returned by
  // LayerTreeClient::OffscreenContextProviderForCompositorThread.
  bool InitializeAndSetContext3D(
      scoped_ptr<WebKit::WebGraphicsContext3D> context3d,
      scoped_refptr<ContextProvider> offscreen_context_provider);
  void ReleaseGL();

  void PostSwapBuffersComplete();

  struct cc::OutputSurface::Capabilities capabilities_;
  scoped_ptr<OutputSurfaceCallbacks> callbacks_;
  scoped_ptr<WebKit::WebGraphicsContext3D> context3d_;
  scoped_ptr<cc::SoftwareOutputDevice> software_device_;
  bool has_gl_discard_backbuffer_;
  bool has_swap_buffers_complete_callback_;
  gfx::Size surface_size_;
  float device_scale_factor_;
  base::WeakPtrFactory<OutputSurface> weak_ptr_factory_;

  // The FrameRateController is deprecated.
  // Platforms should move to native BeginFrames instead.
  void OnVSyncParametersChanged(base::TimeTicks timebase,
                                base::TimeDelta interval);
  virtual void FrameRateControllerTick(bool throttled,
                                       const BeginFrameArgs& args) OVERRIDE;
  scoped_ptr<FrameRateController> frame_rate_controller_;
  int max_frames_pending_;
  int pending_swap_buffers_;
  bool needs_begin_frame_;
  bool begin_frame_pending_;

  // Forwarded to OutputSurfaceClient but threaded through OutputSurface
  // first so OutputSurface has a chance to update the FrameRateController
  bool HasClient() { return !!client_; }
  void SetNeedsRedrawRect(gfx::Rect damage_rect);
  void BeginFrame(const BeginFrameArgs& args);
  void DidSwapBuffers();
  void OnSwapBuffersComplete(const CompositorFrameAck* ack);
  void DidLoseOutputSurface();
  void SetExternalStencilTest(bool enabled);
  void SetExternalDrawConstraints(const gfx::Transform& transform,
                                  gfx::Rect viewport);

  // virtual for testing.
  virtual base::TimeDelta RetroactiveBeginFramePeriod();
  virtual void PostCheckForRetroactiveBeginFrame();
  void CheckForRetroactiveBeginFrame();

 private:
  OutputSurfaceClient* client_;
  friend class OutputSurfaceCallbacks;

  void SetContext3D(scoped_ptr<WebKit::WebGraphicsContext3D> context3d);
  void ResetContext3D();
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy,
                       bool discard_backbuffer_when_not_visible);

  // This stores a BeginFrame that we couldn't process immediately, but might
  // process retroactively in the near future.
  BeginFrameArgs skipped_begin_frame_args_;

  // check_for_retroactive_begin_frame_pending_ is used to avoid posting
  // redundant checks for a retroactive BeginFrame.
  bool check_for_retroactive_begin_frame_pending_;

  DISALLOW_COPY_AND_ASSIGN(OutputSurface);
};

}  // namespace cc

#endif  // CC_OUTPUT_OUTPUT_SURFACE_H_
