// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_COMPOSITOR_FRAME_SINK_H_
#define CC_TEST_FAKE_COMPOSITOR_FRAME_SINK_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/output/software_output_device.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_gles2_interface.h"
#include "cc/test/test_web_graphics_context_3d.h"

namespace cc {

class FakeCompositorFrameSink : public CompositorFrameSink {
 public:
  ~FakeCompositorFrameSink() override;

  static std::unique_ptr<FakeCompositorFrameSink> Create3d() {
    return base::WrapUnique(
        new FakeCompositorFrameSink(TestContextProvider::Create(),
                                    TestContextProvider::CreateWorker(), true));
  }

  static std::unique_ptr<FakeCompositorFrameSink> Create3d(
      scoped_refptr<TestContextProvider> context_provider) {
    return base::WrapUnique(new FakeCompositorFrameSink(
        context_provider, TestContextProvider::CreateWorker(), true));
  }

  static std::unique_ptr<FakeCompositorFrameSink> Create3d(
      std::unique_ptr<TestWebGraphicsContext3D> context) {
    return base::WrapUnique(new FakeCompositorFrameSink(
        TestContextProvider::Create(std::move(context)),
        TestContextProvider::CreateWorker(), true));
  }

  static std::unique_ptr<FakeCompositorFrameSink> CreateSoftware() {
    return base::WrapUnique(
        new FakeCompositorFrameSink(nullptr, nullptr, nullptr, true));
  }

  void set_max_frames_pending(int max) {
    capabilities_.max_frames_pending = max;
  }

  CompositorFrame* last_sent_frame() { return last_sent_frame_.get(); }
  size_t num_sent_frames() { return num_sent_frames_; }

  void SwapBuffers(CompositorFrame frame) override;

  CompositorFrameSinkClient* client() { return client_; }
  bool BindToClient(CompositorFrameSinkClient* client) override;
  void DetachFromClient() override;

  void set_framebuffer(GLint framebuffer, GLenum format) {
    framebuffer_ = framebuffer;
    framebuffer_format_ = format;
  }
  void BindFramebuffer() override;
  uint32_t GetFramebufferCopyTextureFormat() override;

  const TransferableResourceArray& resources_held_by_parent() {
    return resources_held_by_parent_;
  }

  bool HasExternalStencilTest() const override;

  bool SurfaceIsSuspendForRecycle() const override;

  OverlayCandidateValidator* GetOverlayCandidateValidator() const override;
  void SetOverlayCandidateValidator(OverlayCandidateValidator* validator) {
    overlay_candidate_validator_ = validator;
  }

  void set_has_external_stencil_test(bool has_test) {
    has_external_stencil_test_ = has_test;
  }

  void set_suspended_for_recycle(bool suspended) {
    suspended_for_recycle_ = suspended;
  }

  gfx::Rect last_swap_rect() const {
    DCHECK(last_swap_rect_valid_);
    return last_swap_rect_;
  }

  void ReturnResourcesHeldByParent();

 protected:
  FakeCompositorFrameSink(
      scoped_refptr<ContextProvider> context_provider,
      scoped_refptr<ContextProvider> worker_context_provider,
      bool delegated_rendering);

  FakeCompositorFrameSink(
      scoped_refptr<ContextProvider> context_provider,
      scoped_refptr<ContextProvider> worker_context_provider,
      std::unique_ptr<SoftwareOutputDevice> software_device,
      bool delegated_rendering);

  CompositorFrameSinkClient* client_ = nullptr;
  std::unique_ptr<CompositorFrame> last_sent_frame_;
  size_t num_sent_frames_ = 0;
  bool has_external_stencil_test_ = false;
  bool suspended_for_recycle_ = false;
  GLint framebuffer_ = 0;
  GLenum framebuffer_format_ = 0;
  TransferableResourceArray resources_held_by_parent_;
  OverlayCandidateValidator* overlay_candidate_validator_ = nullptr;
  bool last_swap_rect_valid_ = false;
  gfx::Rect last_swap_rect_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_COMPOSITOR_FRAME_SINK_H_
