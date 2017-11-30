// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_OUTPUT_SURFACE_H_
#define CC_TEST_FAKE_OUTPUT_SURFACE_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_gles2_interface.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/software_output_device.h"

namespace cc {

class FakeOutputSurface : public viz::OutputSurface {
 public:
  ~FakeOutputSurface() override;

  static std::unique_ptr<FakeOutputSurface> Create3d() {
    auto provider = TestContextProvider::Create();
    provider->BindToCurrentThread();
    return base::WrapUnique(new FakeOutputSurface(std::move(provider)));
  }

  static std::unique_ptr<FakeOutputSurface> Create3d(
      scoped_refptr<viz::ContextProvider> context_provider) {
    return base::WrapUnique(new FakeOutputSurface(context_provider));
  }

  static std::unique_ptr<FakeOutputSurface> CreateSoftware(
      std::unique_ptr<viz::SoftwareOutputDevice> software_device) {
    return base::WrapUnique(new FakeOutputSurface(std::move(software_device)));
  }

  static std::unique_ptr<FakeOutputSurface> CreateOffscreen(
      scoped_refptr<viz::ContextProvider> context_provider) {
    auto surface =
        base::WrapUnique(new FakeOutputSurface(std::move(context_provider)));
    surface->capabilities_.uses_default_gl_framebuffer = false;
    return surface;
  }

  void set_max_frames_pending(int max) {
    capabilities_.max_frames_pending = max;
  }

  viz::OutputSurfaceFrame* last_sent_frame() { return last_sent_frame_.get(); }
  size_t num_sent_frames() { return num_sent_frames_; }

  viz::OutputSurfaceClient* client() { return client_; }

  void BindToClient(viz::OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override;
  void SetDrawRectangle(const gfx::Rect& rect) override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SwapBuffers(viz::OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override {}
  bool SurfaceIsSuspendForRecycle() const override;
  viz::OverlayCandidateValidator* GetOverlayCandidateValidator() const override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;

  void set_framebuffer(GLint framebuffer, GLenum format) {
    framebuffer_ = framebuffer;
    framebuffer_format_ = format;
  }

  void SetOverlayCandidateValidator(viz::OverlayCandidateValidator* validator) {
    overlay_candidate_validator_ = validator;
  }

  void set_has_external_stencil_test(bool has_test) {
    has_external_stencil_test_ = has_test;
  }

  void set_suspended_for_recycle(bool suspended) {
    suspended_for_recycle_ = suspended;
  }

  const gfx::ColorSpace& last_reshape_color_space() {
    return last_reshape_color_space_;
  }

  const gfx::Rect& last_set_draw_rectangle() {
    return last_set_draw_rectangle_;
  }

 protected:
  explicit FakeOutputSurface(
      scoped_refptr<viz::ContextProvider> context_provider);
  explicit FakeOutputSurface(
      std::unique_ptr<viz::SoftwareOutputDevice> software_device);

  viz::OutputSurfaceClient* client_ = nullptr;
  std::unique_ptr<viz::OutputSurfaceFrame> last_sent_frame_;
  size_t num_sent_frames_ = 0;
  bool has_external_stencil_test_ = false;
  bool suspended_for_recycle_ = false;
  GLint framebuffer_ = 0;
  GLenum framebuffer_format_ = 0;
  viz::OverlayCandidateValidator* overlay_candidate_validator_ = nullptr;
  gfx::ColorSpace last_reshape_color_space_;
  gfx::Rect last_set_draw_rectangle_;

 private:
  void SwapBuffersAck(uint64_t swap_id);

  base::WeakPtrFactory<FakeOutputSurface> weak_ptr_factory_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_OUTPUT_SURFACE_H_
