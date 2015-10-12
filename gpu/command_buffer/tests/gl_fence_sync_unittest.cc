// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define SHADER(Src) #Src

namespace gpu {

class GLFenceSyncTest : public testing::Test {
 protected:
  void SetUp() override {
    sync_point_manager_.reset(new SyncPointManager(false));

    GLManager::Options options;
    options.sync_point_manager = sync_point_manager_.get();
    gl1_.Initialize(options);
    gl2_.Initialize(options);
  }

  void TearDown() override {
    gl2_.Destroy();
    gl1_.Destroy();

    sync_point_manager_.reset();
  }

  scoped_ptr<SyncPointManager> sync_point_manager_;
  GLManager gl1_;
  GLManager gl2_;
};

TEST_F(GLFenceSyncTest, SimpleReleaseWait) {
  gl1_.MakeCurrent();

  const GLuint64 fence_sync = glInsertFenceSyncCHROMIUM();
  SyncToken sync_token;
  glFlush();
  glGenSyncTokenCHROMIUM(fence_sync, sync_token.GetData());
  ASSERT_TRUE(GL_NO_ERROR == glGetError());

  // Make sure it is actually released.
  scoped_refptr<SyncPointClientState> gl1_client_state =
      sync_point_manager_->GetSyncPointClientState(gl1_.GetNamespaceID(),
                                                   gl1_.GetCommandBufferID());
  EXPECT_TRUE(gl1_client_state->IsFenceSyncReleased(fence_sync));

  gl2_.MakeCurrent();
  glWaitSyncTokenCHROMIUM(sync_token.GetConstData());
  glFinish();

  // gl2 should not have released anything.
  scoped_refptr<SyncPointClientState> gl2_client_state =
      sync_point_manager_->GetSyncPointClientState(gl2_.GetNamespaceID(),
                                                   gl2_.GetCommandBufferID());
  EXPECT_EQ(0u, gl2_client_state->fence_sync_release());
}

}  // namespace gpu
