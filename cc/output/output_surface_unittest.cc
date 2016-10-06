// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "cc/output/output_surface_frame.h"
#include "cc/output/software_output_device.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TestOutputSurface : public OutputSurface {
 public:
  explicit TestOutputSurface(
      scoped_refptr<TestContextProvider> context_provider)
      : OutputSurface(std::move(context_provider)) {}

  explicit TestOutputSurface(
      std::unique_ptr<SoftwareOutputDevice> software_device)
      : OutputSurface(std::move(software_device)) {}

  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override {}
  void SwapBuffers(OutputSurfaceFrame frame) override {
    client_->DidSwapBuffersComplete();
  }
  uint32_t GetFramebufferCopyTextureFormat() override {
    // TestContextProvider has no real framebuffer, just use RGB.
    return GL_RGB;
  }
  OverlayCandidateValidator* GetOverlayCandidateValidator() const override {
    return nullptr;
  }
  bool IsDisplayedAsOverlayPlane() const override { return false; }
  unsigned GetOverlayTextureId() const override { return 0; }
  bool SurfaceIsSuspendForRecycle() const override { return false; }
  bool HasExternalStencilTest() const override { return false; }
  void ApplyExternalStencil() override {}

  void OnSwapBuffersCompleteForTesting() { client_->DidSwapBuffersComplete(); }

 protected:
};

class TestSoftwareOutputDevice : public SoftwareOutputDevice {
 public:
  TestSoftwareOutputDevice();
  ~TestSoftwareOutputDevice() override;

  // Overriden from cc:SoftwareOutputDevice
  void DiscardBackbuffer() override;
  void EnsureBackbuffer() override;

  int discard_backbuffer_count() { return discard_backbuffer_count_; }
  int ensure_backbuffer_count() { return ensure_backbuffer_count_; }

 private:
  int discard_backbuffer_count_;
  int ensure_backbuffer_count_;
};

TestSoftwareOutputDevice::TestSoftwareOutputDevice()
    : discard_backbuffer_count_(0), ensure_backbuffer_count_(0) {}

TestSoftwareOutputDevice::~TestSoftwareOutputDevice() {}

void TestSoftwareOutputDevice::DiscardBackbuffer() {
  SoftwareOutputDevice::DiscardBackbuffer();
  discard_backbuffer_count_++;
}

void TestSoftwareOutputDevice::EnsureBackbuffer() {
  SoftwareOutputDevice::EnsureBackbuffer();
  ensure_backbuffer_count_++;
}

TEST(OutputSurfaceTest, ContextLossInformsClient) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  TestOutputSurface output_surface(provider);

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));

  // Verify DidLoseOutputSurface callback is hooked up correctly.
  EXPECT_FALSE(client.did_lose_output_surface_called());
  output_surface.context_provider()->ContextGL()->LoseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  output_surface.context_provider()->ContextGL()->Flush();
  EXPECT_TRUE(client.did_lose_output_surface_called());
}

// TODO(danakj): Add a test for worker context failure as well when
// OutputSurface creates/binds it.
TEST(OutputSurfaceTest, ContextLossFailsBind) {
  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create();

  // Lose the context so BindToClient fails.
  context_provider->UnboundTestContext3d()->set_context_lost(true);

  TestOutputSurface output_surface(context_provider);

  FakeOutputSurfaceClient client;
  EXPECT_FALSE(output_surface.BindToClient(&client));
}

}  // namespace
}  // namespace cc
