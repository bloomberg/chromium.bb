// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder.h"

#include "base/command_line.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/raster_decoder_unittest_base.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::_;
using ::testing::Return;

using namespace gpu::raster::cmds;

namespace gpu {
namespace raster {

class RasterDecoderTest : public RasterDecoderTestBase {
 public:
  RasterDecoderTest() = default;
};

INSTANTIATE_TEST_CASE_P(Service, RasterDecoderTest, ::testing::Bool());

TEST_P(RasterDecoderTest, TexParameteriValidArgs) {
  EXPECT_CALL(*gl_, TexParameteri(_, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  cmds::TexParameteri cmd;
  cmd.Init(client_texture_id_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, TexParameteri(_, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  cmd.Init(client_texture_id_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, TexParameteri(_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  cmd.Init(client_texture_id_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, TexParameteri(_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  cmd.Init(client_texture_id_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(RasterDecoderTest, TexParameteriInvalidArgsMipMap) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  cmds::TexParameteri cmd;
  cmd.Init(client_texture_id_, GL_GENERATE_MIPMAP, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(RasterDecoderTest, TexParameteriInvalidArgsMagFilter) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  cmds::TexParameteri cmd;
  cmd.Init(client_texture_id_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(RasterDecoderTest, TexParameteriInvalidArgsMinFilter) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  cmds::TexParameteri cmd;
  cmd.Init(client_texture_id_, GL_TEXTURE_MIN_FILTER,
           GL_NEAREST_MIPMAP_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(RasterDecoderTest, TexParameteriInvalidArgsWrapS) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  cmds::TexParameteri cmd;
  cmd.Init(client_texture_id_, GL_TEXTURE_WRAP_S, GL_REPEAT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(RasterDecoderTest, TexParameteriInvalidArgsWrapT) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  cmds::TexParameteri cmd;
  cmd.Init(client_texture_id_, GL_TEXTURE_WRAP_T, GL_REPEAT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

const GLsync kGlSync = reinterpret_cast<GLsync>(0xdeadbeef);

TEST_P(RasterDecoderTest, BeginEndQueryEXTCommandsCompletedCHROMIUM) {
  GenHelper<GenQueriesEXTImmediate>(kNewClientId);

  BeginQueryEXT begin_cmd;
  begin_cmd.Init(GL_COMMANDS_COMPLETED_CHROMIUM, kNewClientId,
                 shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != nullptr);
  EXPECT_FALSE(query->IsPending());

  EXPECT_CALL(*gl_, Flush()).RetiresOnSaturation();
  EXPECT_CALL(*gl_, FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0))
      .WillOnce(Return(kGlSync))
      .RetiresOnSaturation();
#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif

  EndQueryEXT end_cmd;
  end_cmd.Init(GL_COMMANDS_COMPLETED_CHROMIUM, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(query->IsPending());

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif
  EXPECT_CALL(*gl_, ClientWaitSync(kGlSync, _, _))
      .WillOnce(Return(GL_TIMEOUT_EXPIRED))
      .RetiresOnSaturation();
  query_manager->ProcessPendingQueries(false);

  EXPECT_TRUE(query->IsPending());

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif
  EXPECT_CALL(*gl_, ClientWaitSync(kGlSync, _, _))
      .WillOnce(Return(GL_ALREADY_SIGNALED))
      .RetiresOnSaturation();
  query_manager->ProcessPendingQueries(false);

  EXPECT_FALSE(query->IsPending());

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif
  EXPECT_CALL(*gl_, DeleteSync(kGlSync)).Times(1).RetiresOnSaturation();
  ResetDecoder();
}

TEST_P(RasterDecoderTest, BeginEndQueryEXTCommandsIssuedCHROMIUM) {
  BeginQueryEXT begin_cmd;

  GenHelper<GenQueriesEXTImmediate>(kNewClientId);

  // Test valid parameters work.
  begin_cmd.Init(GL_COMMANDS_ISSUED_CHROMIUM, kNewClientId, shared_memory_id_,
                 kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != nullptr);
  EXPECT_FALSE(query->IsPending());

  // Test end succeeds
  EndQueryEXT end_cmd;
  end_cmd.Init(GL_COMMANDS_ISSUED_CHROMIUM, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_FALSE(query->IsPending());
}

TEST_P(RasterDecoderTest, ProduceAndConsumeTexture) {
  Mailbox mailbox = Mailbox::Generate();
  GLuint new_texture_id = kNewClientId;

  // TODO(backer): Use TexStorage2D to set some attributes like width and
  // height.

  gles2::TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  gles2::Texture* texture = texture_ref->texture();
  EXPECT_EQ(kServiceTextureId, texture->service_id());

  ProduceTextureDirectImmediate& produce_cmd =
      *GetImmediateAs<ProduceTextureDirectImmediate>();
  produce_cmd.Init(client_texture_id_, mailbox.name);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(produce_cmd, sizeof(mailbox.name)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // TODO(backer): Check that ProduceTextureDirect did not change attributes
  // like width and height.

  // Service ID has not changed.
  EXPECT_EQ(kServiceTextureId, texture->service_id());

  CreateAndConsumeTextureINTERNALImmediate& consume_cmd =
      *GetImmediateAs<CreateAndConsumeTextureINTERNALImmediate>();
  consume_cmd.Init(new_texture_id, false /* use_buffer */,
                   gfx::BufferUsage::GPU_READ, viz::ResourceFormat::RGBA_8888,
                   mailbox.name);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(consume_cmd, sizeof(mailbox.name)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // TODO(backer): Check that new_texture_id has appropriate attributes like
  // width and height.

  // Service ID is restored.
  texture_ref = group().texture_manager()->GetTexture(new_texture_id);
  ASSERT_NE(texture_ref, nullptr);
  EXPECT_EQ(kServiceTextureId, texture_ref->service_id());
}

}  // namespace raster
}  // namespace gpu
