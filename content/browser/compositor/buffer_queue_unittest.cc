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
class MockBufferQueue : public BufferQueue {
 public:
  MockBufferQueue(scoped_refptr<cc::ContextProvider> context_provider,
                  unsigned int internalformat)
      : BufferQueue(context_provider, internalformat) {}
  MOCK_METHOD4(CopyBufferDamage,
               void(int, int, const gfx::Rect&, const gfx::Rect&));
};

class BufferQueueTest : public ::testing::Test {
 public:
  BufferQueueTest() : doublebuffering_(true), first_frame_(true) {}

  virtual void SetUp() OVERRIDE {
    scoped_refptr<cc::TestContextProvider> context_provider =
        cc::TestContextProvider::Create(cc::TestWebGraphicsContext3D::Create());
    context_provider->BindToCurrentThread();
    output_surface_.reset(new MockBufferQueue(context_provider, GL_RGBA8_OES));
    output_surface_->Initialize();
  }

  unsigned current_surface() { return output_surface_->current_surface_.image; }
  const std::vector<BufferQueue::AllocatedSurface>& available_surfaces() {
    return output_surface_->available_surfaces_;
  }
  const std::deque<BufferQueue::AllocatedSurface>& in_flight_surfaces() {
    return output_surface_->in_flight_surfaces_;
  }

  const BufferQueue::AllocatedSurface& last_frame() {
    return output_surface_->in_flight_surfaces_.back();
  }
  const BufferQueue::AllocatedSurface& next_frame() {
    return output_surface_->available_surfaces_.back();
  }
  const gfx::Size size() { return output_surface_->size_; }

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
    for (std::deque<BufferQueue::AllocatedSurface>::const_iterator it =
             in_flight_surfaces().begin();
         it != in_flight_surfaces().end();
         ++it)
      EXPECT_TRUE(InsertUnique(&buffers, it->image));
  }

  void SwapBuffers() {
    output_surface_->SwapBuffers(gfx::Rect(output_surface_->size_));
  }

  void SendDamagedFrame(const gfx::Rect& damage) {
    // We don't care about the GL-level implementation here, just how it uses
    // damage rects.
    output_surface_->BindFramebuffer();
    output_surface_->SwapBuffers(damage);
    if (doublebuffering_ || !first_frame_)
      output_surface_->PageFlipComplete();
    first_frame_ = false;
  }

  void SendFullFrame() { SendDamagedFrame(gfx::Rect(output_surface_->size_)); }

 protected:
  bool InsertUnique(std::set<unsigned>* set, unsigned value) {
    if (!value)
      return true;
    if (set->find(value) != set->end())
      return false;
    set->insert(value);
    return true;
  }

  scoped_ptr<MockBufferQueue> output_surface_;
  bool doublebuffering_;
  bool first_frame_;
};

namespace {
const gfx::Size screen_size = gfx::Size(30, 30);
const gfx::Rect screen_rect = gfx::Rect(screen_size);
const gfx::Rect small_damage = gfx::Rect(gfx::Size(10, 10));
const gfx::Rect large_damage = gfx::Rect(gfx::Size(20, 20));
const gfx::Rect overlapping_damage = gfx::Rect(gfx::Size(5, 20));

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

TEST_F(BufferQueueTest, PartialSwapReuse) {
  // Check that
  output_surface_->Reshape(screen_size, 1.0f);
  ASSERT_TRUE(doublebuffering_);
  EXPECT_CALL(*output_surface_,
              CopyBufferDamage(_, _, small_damage, screen_rect)).Times(1);
  EXPECT_CALL(*output_surface_,
              CopyBufferDamage(_, _, small_damage, small_damage)).Times(1);
  EXPECT_CALL(*output_surface_,
              CopyBufferDamage(_, _, large_damage, small_damage)).Times(1);
  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendDamagedFrame(small_damage);
  SendDamagedFrame(large_damage);
  // Verify that the damage has propagated.
  EXPECT_EQ(next_frame().damage, large_damage);
}

TEST_F(BufferQueueTest, PartialSwapFullFrame) {
  output_surface_->Reshape(screen_size, 1.0f);
  ASSERT_TRUE(doublebuffering_);
  EXPECT_CALL(*output_surface_,
              CopyBufferDamage(_, _, small_damage, screen_rect)).Times(1);
  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendFullFrame();
  SendFullFrame();
  EXPECT_EQ(next_frame().damage, screen_rect);
}

TEST_F(BufferQueueTest, PartialSwapOverlapping) {
  output_surface_->Reshape(screen_size, 1.0f);
  ASSERT_TRUE(doublebuffering_);
  EXPECT_CALL(*output_surface_,
              CopyBufferDamage(_, _, small_damage, screen_rect)).Times(1);
  EXPECT_CALL(*output_surface_,
              CopyBufferDamage(_, _, overlapping_damage, small_damage))
      .Times(1);

  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendDamagedFrame(overlapping_damage);
  EXPECT_EQ(next_frame().damage, overlapping_damage);
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
  SwapBuffers();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  output_surface_->PageFlipComplete();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  output_surface_->BindFramebuffer();
  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_NE(0U, current_surface());
  EXPECT_EQ(1U, in_flight_surfaces().size());
  SwapBuffers();
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
  SwapBuffers();
  output_surface_->PageFlipComplete();
  output_surface_->BindFramebuffer();
  SwapBuffers();

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
