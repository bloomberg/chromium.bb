// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/renderbuffer_manager.h"

#include "gpu/command_buffer/common/gl_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class RenderbufferManagerTest : public testing::Test {
 public:
  static const GLint kMaxSize = 128;

  RenderbufferManagerTest()
      : manager_(kMaxSize) {
  }
  ~RenderbufferManagerTest() {
    manager_.Destroy(false);
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock<gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
  }

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  RenderbufferManager manager_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint RenderbufferManagerTest::kMaxSize;
#endif

TEST_F(RenderbufferManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  EXPECT_EQ(kMaxSize, manager_.max_renderbuffer_size());
  // Check we can create renderbuffer.
  manager_.CreateRenderbufferInfo(kClient1Id, kService1Id);
  // Check renderbuffer got created.
  RenderbufferManager::RenderbufferInfo* info1 =
      manager_.GetRenderbufferInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  GLuint client_id = 0;
  EXPECT_TRUE(manager_.GetClientId(info1->service_id(), &client_id));
  EXPECT_EQ(kClient1Id, client_id);
  // Check we get nothing for a non-existent renderbuffer.
  EXPECT_TRUE(manager_.GetRenderbufferInfo(kClient2Id) == NULL);
  // Check trying to a remove non-existent renderbuffers does not crash.
  manager_.RemoveRenderbufferInfo(kClient2Id);
  // Check we can't get the renderbuffer after we remove it.
  manager_.RemoveRenderbufferInfo(kClient1Id);
  EXPECT_TRUE(manager_.GetRenderbufferInfo(kClient1Id) == NULL);
}

TEST_F(RenderbufferManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create renderbuffer.
  manager_.CreateRenderbufferInfo(kClient1Id, kService1Id);
  // Check renderbuffer got created.
  RenderbufferManager::RenderbufferInfo* info1 =
      manager_.GetRenderbufferInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_CALL(*gl_, DeleteRenderbuffersEXT(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  manager_.Destroy(true);
  info1 = manager_.GetRenderbufferInfo(kClient1Id);
  ASSERT_TRUE(info1 == NULL);
}

TEST_F(RenderbufferManagerTest, RenderbufferInfo) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create renderbuffer.
  manager_.CreateRenderbufferInfo(kClient1Id, kService1Id);
  // Check renderbuffer got created.
  RenderbufferManager::RenderbufferInfo* info1 =
      manager_.GetRenderbufferInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_EQ(kService1Id, info1->service_id());
  EXPECT_EQ(0, info1->samples());
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA4), info1->internal_format());
  EXPECT_EQ(0, info1->width());
  EXPECT_EQ(0, info1->height());

  EXPECT_FALSE(info1->cleared());
  info1->set_cleared();
  EXPECT_TRUE(info1->cleared());

  // Check if we set the info it gets marked as not cleared.
  const GLsizei kSamples = 4;
  const GLenum kFormat = GL_RGBA;
  const GLsizei kWidth = 128;
  const GLsizei kHeight = 64;
  info1->SetInfo(kSamples, kFormat, kWidth, kHeight);
  EXPECT_EQ(kSamples, info1->samples());
  EXPECT_EQ(kFormat, info1->internal_format());
  EXPECT_EQ(kWidth, info1->width());
  EXPECT_EQ(kHeight, info1->height());
  EXPECT_FALSE(info1->cleared());
  EXPECT_FALSE(info1->IsDeleted());
}

}  // namespace gles2
}  // namespace gpu


