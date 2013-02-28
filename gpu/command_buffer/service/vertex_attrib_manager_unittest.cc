// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/vertex_attrib_manager.h"

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::Pointee;
using ::testing::_;

namespace gpu {
namespace gles2 {

class VertexAttribManagerTest : public testing::Test {
 public:
  static const uint32 kNumVertexAttribs = 8;

  VertexAttribManagerTest() {
  }

  virtual ~VertexAttribManagerTest() {
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());

    for (uint32 ii = 0; ii < kNumVertexAttribs; ++ii) {
      EXPECT_CALL(*gl_, VertexAttrib4f(ii, 0.0f, 0.0f, 0.0f, 1.0f))
          .Times(1)
          .RetiresOnSaturation();
    }

    manager_ = new VertexAttribManager();
    manager_->Initialize(kNumVertexAttribs);
  }

  virtual void TearDown() {
    manager_ = NULL;
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  scoped_refptr<VertexAttribManager> manager_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const uint32 VertexAttribManagerTest::kNumVertexAttribs;
#endif

TEST_F(VertexAttribManagerTest, Basic) {
  EXPECT_TRUE(manager_->GetVertexAttrib(kNumVertexAttribs) == NULL);
  EXPECT_FALSE(manager_->HaveFixedAttribs());

  const VertexAttribManager::VertexAttribInfoList& infos =
      manager_->GetEnabledVertexAttribInfos();
  EXPECT_EQ(0u, infos.size());

  for (uint32 ii = 0; ii < kNumVertexAttribs; ii += kNumVertexAttribs - 1) {
    VertexAttrib* info =
        manager_->GetVertexAttrib(ii);
    ASSERT_TRUE(info != NULL);
    EXPECT_EQ(ii, info->index());
    EXPECT_TRUE(info->buffer() == NULL);
    EXPECT_EQ(0, info->offset());
    EXPECT_EQ(4, info->size());
    EXPECT_EQ(static_cast<GLenum>(GL_FLOAT), info->type());
    EXPECT_EQ(GL_FALSE, info->normalized());
    EXPECT_EQ(0, info->gl_stride());
    EXPECT_FALSE(info->enabled());
    manager_->Enable(ii, true);
    EXPECT_TRUE(info->enabled());
  }
}

TEST_F(VertexAttribManagerTest, Enable) {
  const VertexAttribManager::VertexAttribInfoList& infos =
      manager_->GetEnabledVertexAttribInfos();

  VertexAttrib* info1 =
      manager_->GetVertexAttrib(1);
  VertexAttrib* info2 =
      manager_->GetVertexAttrib(3);

  manager_->Enable(1, true);
  ASSERT_EQ(1u, infos.size());
  EXPECT_TRUE(info1->enabled());
  manager_->Enable(3, true);
  ASSERT_EQ(2u, infos.size());
  EXPECT_TRUE(info2->enabled());

  manager_->Enable(1, false);
  ASSERT_EQ(1u, infos.size());
  EXPECT_FALSE(info1->enabled());

  manager_->Enable(3, false);
  ASSERT_EQ(0u, infos.size());
  EXPECT_FALSE(info2->enabled());
}

TEST_F(VertexAttribManagerTest, SetAttribInfo) {
  BufferManager buffer_manager(NULL);
  buffer_manager.CreateBuffer(1, 2);
  Buffer* buffer = buffer_manager.GetBuffer(1);
  ASSERT_TRUE(buffer != NULL);

  VertexAttrib* info =
      manager_->GetVertexAttrib(1);

  manager_->SetAttribInfo(1, buffer, 3, GL_SHORT, GL_TRUE, 32, 32, 4);

  EXPECT_EQ(buffer, info->buffer());
  EXPECT_EQ(4, info->offset());
  EXPECT_EQ(3, info->size());
  EXPECT_EQ(static_cast<GLenum>(GL_SHORT), info->type());
  EXPECT_EQ(GL_TRUE, info->normalized());
  EXPECT_EQ(32, info->gl_stride());

  // The VertexAttribManager must be destroyed before the BufferManager
  // so it releases its buffers.
  manager_ = NULL;
  buffer_manager.Destroy(false);
}

TEST_F(VertexAttribManagerTest, HaveFixedAttribs) {
  EXPECT_FALSE(manager_->HaveFixedAttribs());
  manager_->SetAttribInfo(1, NULL, 4, GL_FIXED, GL_FALSE, 0, 16, 0);
  EXPECT_TRUE(manager_->HaveFixedAttribs());
  manager_->SetAttribInfo(3, NULL, 4, GL_FIXED, GL_FALSE, 0, 16, 0);
  EXPECT_TRUE(manager_->HaveFixedAttribs());
  manager_->SetAttribInfo(1, NULL, 4, GL_FLOAT, GL_FALSE, 0, 16, 0);
  EXPECT_TRUE(manager_->HaveFixedAttribs());
  manager_->SetAttribInfo(3, NULL, 4, GL_FLOAT, GL_FALSE, 0, 16, 0);
  EXPECT_FALSE(manager_->HaveFixedAttribs());
}

TEST_F(VertexAttribManagerTest, CanAccess) {
  BufferManager buffer_manager(NULL);
  buffer_manager.CreateBuffer(1, 2);
  Buffer* buffer = buffer_manager.GetBuffer(1);
  ASSERT_TRUE(buffer != NULL);

  VertexAttrib* info =
      manager_->GetVertexAttrib(1);

  EXPECT_TRUE(info->CanAccess(0));
  manager_->Enable(1, true);
  EXPECT_FALSE(info->CanAccess(0));

  manager_->SetAttribInfo(1, buffer, 4, GL_FLOAT, GL_FALSE, 0, 16, 0);
  EXPECT_FALSE(info->CanAccess(0));

  EXPECT_TRUE(buffer_manager.SetTarget(buffer, GL_ARRAY_BUFFER));
  buffer_manager.SetInfo(buffer, 15, GL_STATIC_DRAW);

  EXPECT_FALSE(info->CanAccess(0));
  buffer_manager.SetInfo(buffer, 16, GL_STATIC_DRAW);
  EXPECT_TRUE(info->CanAccess(0));
  EXPECT_FALSE(info->CanAccess(1));

  manager_->SetAttribInfo(1, buffer, 4, GL_FLOAT, GL_FALSE, 0, 16, 1);
  EXPECT_FALSE(info->CanAccess(0));

  buffer_manager.SetInfo(buffer, 32, GL_STATIC_DRAW);
  EXPECT_TRUE(info->CanAccess(0));
  EXPECT_FALSE(info->CanAccess(1));
  manager_->SetAttribInfo(1, buffer, 4, GL_FLOAT, GL_FALSE, 0, 16, 0);
  EXPECT_TRUE(info->CanAccess(1));
  manager_->SetAttribInfo(1, buffer, 4, GL_FLOAT, GL_FALSE, 0, 20, 0);
  EXPECT_TRUE(info->CanAccess(0));
  EXPECT_FALSE(info->CanAccess(1));

  // The VertexAttribManager must be destroyed before the BufferManager
  // so it releases its buffers.
  manager_ = NULL;
  buffer_manager.Destroy(false);
}

TEST_F(VertexAttribManagerTest, Unbind) {
  BufferManager buffer_manager(NULL);
  buffer_manager.CreateBuffer(1, 2);
  buffer_manager.CreateBuffer(3, 4);
  Buffer* buffer1 = buffer_manager.GetBuffer(1);
  Buffer* buffer2 = buffer_manager.GetBuffer(3);
  ASSERT_TRUE(buffer1 != NULL);
  ASSERT_TRUE(buffer2 != NULL);

  VertexAttrib* info1 =
      manager_->GetVertexAttrib(1);
  VertexAttrib* info3 =
      manager_->GetVertexAttrib(3);

  // Attach to 2 buffers.
  manager_->SetAttribInfo(1, buffer1, 3, GL_SHORT, GL_TRUE, 32, 32, 4);
  manager_->SetAttribInfo(3, buffer1, 3, GL_SHORT, GL_TRUE, 32, 32, 4);
  // Check they were attached.
  EXPECT_EQ(buffer1, info1->buffer());
  EXPECT_EQ(buffer1, info3->buffer());
  // Unbind unattached buffer.
  manager_->Unbind(buffer2);
  // Should be no-op.
  EXPECT_EQ(buffer1, info1->buffer());
  EXPECT_EQ(buffer1, info3->buffer());
  // Unbind buffer.
  manager_->Unbind(buffer1);
  // Check they were detached
  EXPECT_TRUE(NULL == info1->buffer());
  EXPECT_TRUE(NULL == info3->buffer());

  // The VertexAttribManager must be destroyed before the BufferManager
  // so it releases its buffers.
  manager_ = NULL;
  buffer_manager.Destroy(false);
}

}  // namespace gles2
}  // namespace gpu


