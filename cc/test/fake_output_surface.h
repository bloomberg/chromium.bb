// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_OUTPUT_SURFACE_H_
#define CC_TEST_FAKE_OUTPUT_SURFACE_H_

#include "base/callback.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface.h"
#include "cc/output/software_output_device.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"

namespace cc {

class FakeOutputSurface : public OutputSurface {
 public:
  virtual ~FakeOutputSurface();

  static scoped_ptr<FakeOutputSurface> Create3d() {
    return make_scoped_ptr(new FakeOutputSurface(
        TestContextProvider::Create(), false));
  }

  static scoped_ptr<FakeOutputSurface> Create3d(
      scoped_refptr<ContextProvider> context_provider) {
    return make_scoped_ptr(new FakeOutputSurface(context_provider, false));
  }

  static scoped_ptr<FakeOutputSurface> Create3d(
      scoped_ptr<TestWebGraphicsContext3D> context) {
    return make_scoped_ptr(new FakeOutputSurface(
        TestContextProvider::Create(context.Pass()), false));
  }

  static scoped_ptr<FakeOutputSurface> CreateSoftware(
      scoped_ptr<SoftwareOutputDevice> software_device) {
    return make_scoped_ptr(new FakeOutputSurface(software_device.Pass(),
                                                 false));
  }

  static scoped_ptr<FakeOutputSurface> CreateDelegating3d() {
    return make_scoped_ptr(new FakeOutputSurface(
        TestContextProvider::Create(), true));
  }

  static scoped_ptr<FakeOutputSurface> CreateDelegating3d(
      scoped_refptr<TestContextProvider> context_provider) {
    return make_scoped_ptr(new FakeOutputSurface(context_provider, true));
  }

  static scoped_ptr<FakeOutputSurface> CreateDelegating3d(
      scoped_ptr<TestWebGraphicsContext3D> context) {
    return make_scoped_ptr(new FakeOutputSurface(
        TestContextProvider::Create(context.Pass()), true));
  }

  static scoped_ptr<FakeOutputSurface> CreateDelegatingSoftware(
      scoped_ptr<SoftwareOutputDevice> software_device) {
    return make_scoped_ptr(
        new FakeOutputSurface(software_device.Pass(), true));
  }

  static scoped_ptr<FakeOutputSurface> CreateDeferredGL(
      scoped_ptr<SoftwareOutputDevice> software_device,
      bool delegated_rendering) {
    scoped_ptr<FakeOutputSurface> result(
        new FakeOutputSurface(software_device.Pass(), delegated_rendering));
    result->capabilities_.deferred_gl_initialization = true;
    return result.Pass();
  }

  static scoped_ptr<FakeOutputSurface> CreateAlwaysDrawAndSwap3d() {
    scoped_ptr<FakeOutputSurface> surface(Create3d());
    surface->capabilities_.draw_and_swap_full_viewport_every_frame = true;
    return surface.Pass();
  }

  static scoped_ptr<FakeOutputSurface> CreateOffscreen(
      scoped_ptr<TestWebGraphicsContext3D> context) {
    scoped_ptr<FakeOutputSurface> surface(new FakeOutputSurface(
        TestContextProvider::Create(context.Pass()), false));
    surface->capabilities_.uses_default_gl_framebuffer = false;
    return surface.Pass();
  }

  CompositorFrame& last_sent_frame() { return last_sent_frame_; }
  size_t num_sent_frames() { return num_sent_frames_; }

  virtual void SwapBuffers(CompositorFrame* frame) OVERRIDE;

  virtual void SetNeedsBeginFrame(bool enable) OVERRIDE;
  bool needs_begin_frame() const { return needs_begin_frame_; }

  virtual bool BindToClient(OutputSurfaceClient* client) OVERRIDE;

  using OutputSurface::ReleaseGL;
  using OutputSurface::InitializeAndSetContext3d;

  void SetTreeActivationCallback(const base::Closure& callback);

  const TransferableResourceArray& resources_held_by_parent() {
    return resources_held_by_parent_;
  }

  void ReturnResource(unsigned id, CompositorFrameAck* ack);

  virtual bool HasExternalStencilTest() const OVERRIDE;

  void set_has_external_stencil_test(bool has_test) {
    has_external_stencil_test_ = has_test;
  }

  void SetMemoryPolicyToSetAtBind(
      scoped_ptr<ManagedMemoryPolicy> memory_policy_to_set_at_bind);

 protected:
  FakeOutputSurface(
      scoped_refptr<ContextProvider> context_provider,
      bool delegated_rendering);

  FakeOutputSurface(
      scoped_ptr<SoftwareOutputDevice> software_device,
      bool delegated_rendering);

  FakeOutputSurface(
      scoped_refptr<ContextProvider> context_provider,
      scoped_ptr<SoftwareOutputDevice> software_device,
      bool delegated_rendering);

  void OnBeginFrame();

  OutputSurfaceClient* client_;
  CompositorFrame last_sent_frame_;
  size_t num_sent_frames_;
  bool needs_begin_frame_;
  bool has_external_stencil_test_;
  TransferableResourceArray resources_held_by_parent_;
  scoped_ptr<ManagedMemoryPolicy> memory_policy_to_set_at_bind_;

  base::WeakPtrFactory<FakeOutputSurface> fake_weak_ptr_factory_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_OUTPUT_SURFACE_H_
