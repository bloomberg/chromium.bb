// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "cc/test/test_context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "content/browser/compositor/buffer_queue.h"
#include "content/browser/compositor/gpu_surfaceless_browser_compositor_output_surface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using ::testing::_;
using ::testing::Expectation;
using ::testing::Ne;
using ::testing::Return;

namespace content {

class BufferQueueTest : public ::testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    scoped_refptr<cc::TestContextProvider> context_provider =
        cc::TestContextProvider::Create(cc::TestWebGraphicsContext3D::Create());
    context_provider->BindToCurrentThread();
    output_surface_.reset(new BufferQueue(context_provider, GL_RGBA8_OES));
  }

  unsigned current_surface() { return output_surface_->current_surface_.image; }
  const std::vector<BufferQueue::AllocatedSurface>& available_surfaces() {
    return output_surface_->available_surfaces_;
  }
  const std::queue<BufferQueue::AllocatedSurface>& in_flight_surfaces() {
    return output_surface_->in_flight_surfaces_;
  }

  int CountBuffers() {
    int n = available_surfaces().size() + in_flight_surfaces().size();
    if (current_surface())
      n++;
    return n;
  }

  // Check that each buffer is unique if present.
  void CheckUnique() {
    std::set<unsigned> buffers;
    EXPECT_TRUE(InsertUnique(&buffers, current_surface()));
    for (size_t i = 0; i < available_surfaces().size(); i++)
      EXPECT_TRUE(InsertUnique(&buffers, available_surfaces()[i].image));
    std::queue<BufferQueue::AllocatedSurface> copy = in_flight_surfaces();
    while (!copy.empty()) {
      EXPECT_TRUE(InsertUnique(&buffers, copy.front().image));
      copy.pop();
    }
  }

 protected:
  bool InsertUnique(std::set<unsigned>* set, unsigned value) {
    if (!value)
      return true;
    if (set->find(value) != set->end())
      return false;
    set->insert(value);
    return true;
  }

  scoped_ptr<BufferQueue> output_surface_;
};

namespace {

class MockedContext : public cc::TestWebGraphicsContext3D {
 public:
  MOCK_METHOD2(bindFramebuffer, void(GLenum, GLuint));
  MOCK_METHOD2(bindTexture, void(GLenum, GLuint));
  MOCK_METHOD2(bindTexImage2DCHROMIUM, void(GLenum, GLint));
  MOCK_METHOD4(createImageCHROMIUM, GLuint(GLsizei, GLsizei, GLenum, GLenum));
  MOCK_METHOD1(destroyImageCHROMIUM, void(GLuint));
  MOCK_METHOD5(framebufferTexture2D,
               void(GLenum, GLenum, GLenum, GLuint, GLint));
};

scoped_ptr<BufferQueue> CreateOutputSurfaceWithMock(MockedContext** context) {
  *context = new MockedContext();
  scoped_refptr<cc::TestContextProvider> context_provider =
      cc::TestContextProvider::Create(
          scoped_ptr<cc::TestWebGraphicsContext3D>(*context));
  context_provider->BindToCurrentThread();
  scoped_ptr<BufferQueue> buffer_queue(
      new BufferQueue(context_provider, GL_RGBA8_OES));
  buffer_queue->Initialize();
  return buffer_queue.Pass();
}

TEST(BufferQueueStandaloneTest, FboInitialization) {
  MockedContext* context;
  scoped_ptr<BufferQueue> output_surface =
      CreateOutputSurfaceWithMock(&context);

  EXPECT_CALL(*context, bindFramebuffer(GL_FRAMEBUFFER, Ne(0U)));
  ON_CALL(*context, framebufferTexture2D(_, _, _, _, _))
      .WillByDefault(Return());

  output_surface->Reshape(gfx::Size(10, 20), 1.0f);
}

TEST(BufferQueueStandaloneTest, FboBinding) {
  MockedContext* context;
  scoped_ptr<BufferQueue> output_surface =
      CreateOutputSurfaceWithMock(&context);
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, Ne(0U)));
  EXPECT_CALL(*context, destroyImageCHROMIUM(1));
  Expectation image =
      EXPECT_CALL(
          *context,
          createImageCHROMIUM(0, 0, GL_RGBA8_OES, GL_IMAGE_SCANOUT_CHROMIUM))
          .WillOnce(Return(1));
  Expectation fb =
      EXPECT_CALL(*context, bindFramebuffer(GL_FRAMEBUFFER, Ne(0U)));
  Expectation tex = EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, Ne(0U)));
  Expectation bind_tex =
      EXPECT_CALL(*context, bindTexImage2DCHROMIUM(GL_TEXTURE_2D, 1))
          .After(tex, image);
  EXPECT_CALL(
      *context,
      framebufferTexture2D(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Ne(0U), _))
      .After(fb, bind_tex);

  output_surface->BindFramebuffer();
}

TEST_F(BufferQueueTest, MultipleBindCalls) {
  // Check that multiple bind calls do not create or change surfaces.
  output_surface_->BindFramebuffer();
  EXPECT_EQ(1, CountBuffers());
  unsigned int fb = current_surface();
  output_surface_->BindFramebuffer();
  EXPECT_EQ(1, CountBuffers());
  EXPECT_EQ(fb, current_surface());
}

TEST_F(BufferQueueTest, CheckDoubleBuffering) {
  // Check buffer flow through double buffering path.
  EXPECT_EQ(0, CountBuffers());
  output_surface_->BindFramebuffer();
  EXPECT_EQ(1, CountBuffers());
  EXPECT_NE(0U, current_surface());
  output_surface_->SwapBuffers();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  output_surface_->PageFlipComplete();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  output_surface_->BindFramebuffer();
  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_NE(0U, current_surface());
  EXPECT_EQ(1U, in_flight_surfaces().size());
  output_surface_->SwapBuffers();
  CheckUnique();
  EXPECT_EQ(2U, in_flight_surfaces().size());
  output_surface_->PageFlipComplete();
  CheckUnique();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  EXPECT_EQ(1U, available_surfaces().size());
  output_surface_->BindFramebuffer();
  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_TRUE(available_surfaces().empty());
}

TEST_F(BufferQueueTest, CheckTripleBuffering) {
  // Check buffer flow through triple buffering path.

  // This bit is the same sequence tested in the doublebuffering case.
  output_surface_->BindFramebuffer();
  output_surface_->SwapBuffers();
  output_surface_->PageFlipComplete();
  output_surface_->BindFramebuffer();
  output_surface_->SwapBuffers();

  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_EQ(2U, in_flight_surfaces().size());
  output_surface_->BindFramebuffer();
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_NE(0U, current_surface());
  EXPECT_EQ(2U, in_flight_surfaces().size());
  output_surface_->PageFlipComplete();
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_NE(0U, current_surface());
  EXPECT_EQ(1U, in_flight_surfaces().size());
  EXPECT_EQ(1U, available_surfaces().size());
}

}  // namespace
}  // namespace content
