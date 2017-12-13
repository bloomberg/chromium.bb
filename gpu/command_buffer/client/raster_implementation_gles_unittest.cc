// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/raster_implementation_gles.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>

#include "cc/paint/display_item_list.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/color_space.h"

using testing::_;
using testing::Return;
using testing::SetArgPointee;

namespace gpu {
namespace raster {

class RasterMockGLES2Interface : public gles2::GLES2InterfaceStub {
 public:
  // Command buffer Flush / Finish.
  MOCK_METHOD0(Finish, void());
  MOCK_METHOD0(ShallowFlushCHROMIUM, void());
  MOCK_METHOD0(OrderingBarrierCHROMIUM, void());

  // SyncTokens.
  MOCK_METHOD0(InsertFenceSyncCHROMIUM, GLuint64());
  MOCK_METHOD2(GenSyncTokenCHROMIUM,
               void(GLuint64 fence_sync, GLbyte* sync_token));
  MOCK_METHOD2(GenUnverifiedSyncTokenCHROMIUM,
               void(GLuint64 fence_sync, GLbyte* sync_token));
  MOCK_METHOD2(VerifySyncTokensCHROMIUM,
               void(GLbyte** sync_tokens, GLsizei count));
  MOCK_METHOD1(WaitSyncTokenCHROMIUM, void(const GLbyte* sync_token));

  // Command buffer state.
  MOCK_METHOD0(GetError, GLenum());
  MOCK_METHOD0(GetGraphicsResetStatusKHR, GLenum());
  MOCK_METHOD2(GetIntegerv, void(GLenum pname, GLint* params));
  MOCK_METHOD2(LoseContextCHROMIUM, void(GLenum current, GLenum other));

  // Queries: GL_COMMANDS_ISSUED_CHROMIUM / GL_COMMANDS_COMPLETED_CHROMIUM.
  MOCK_METHOD2(GenQueriesEXT, void(GLsizei n, GLuint* queries));
  MOCK_METHOD2(DeleteQueriesEXT, void(GLsizei n, const GLuint* queries));
  MOCK_METHOD2(BeginQueryEXT, void(GLenum target, GLuint id));
  MOCK_METHOD1(EndQueryEXT, void(GLenum target));
  MOCK_METHOD3(GetQueryObjectuivEXT,
               void(GLuint id, GLenum pname, GLuint* params));

  // Texture objects.
  MOCK_METHOD2(GenTextures, void(GLsizei n, GLuint* textures));
  MOCK_METHOD2(DeleteTextures, void(GLsizei n, const GLuint* textures));
  MOCK_METHOD2(BindTexture, void(GLenum target, GLuint texture));
  MOCK_METHOD1(ActiveTexture, void(GLenum texture));
  MOCK_METHOD1(GenerateMipmap, void(GLenum target));
  MOCK_METHOD2(SetColorSpaceMetadataCHROMIUM,
               void(GLuint texture_id, GLColorSpace color_space));
  MOCK_METHOD3(TexParameteri, void(GLenum target, GLenum pname, GLint param));

  // Mailboxes.
  MOCK_METHOD1(GenMailboxCHROMIUM, void(GLbyte*));
  MOCK_METHOD2(ProduceTextureDirectCHROMIUM,
               void(GLuint texture, const GLbyte* mailbox));
  MOCK_METHOD1(CreateAndConsumeTextureCHROMIUM, GLuint(const GLbyte* mailbox));

  // Image objects.
  MOCK_METHOD4(CreateImageCHROMIUM,
               GLuint(ClientBuffer buffer,
                      GLsizei width,
                      GLsizei height,
                      GLenum internalformat));
  MOCK_METHOD2(BindTexImage2DCHROMIUM, void(GLenum target, GLint imageId));
  MOCK_METHOD2(ReleaseTexImage2DCHROMIUM, void(GLenum target, GLint imageId));
  MOCK_METHOD1(DestroyImageCHROMIUM, void(GLuint image_id));

  // Texture allocation and copying.
  MOCK_METHOD9(TexImage2D,
               void(GLenum target,
                    GLint level,
                    GLint internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const void* pixels));
  MOCK_METHOD9(TexSubImage2D,
               void(GLenum target,
                    GLint level,
                    GLint xoffset,
                    GLint yoffset,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLenum type,
                    const void* pixels));
  MOCK_METHOD8(CompressedTexImage2D,
               void(GLenum target,
                    GLint level,
                    GLenum internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLsizei imageSize,
                    const void* data));
  MOCK_METHOD2(CompressedCopyTextureCHROMIUM,
               void(GLuint source_id, GLuint dest_id));
  MOCK_METHOD5(TexStorage2DEXT,
               void(GLenum target,
                    GLsizei levels,
                    GLenum internalFormat,
                    GLsizei width,
                    GLsizei height));
  MOCK_METHOD5(TexStorage2DImageCHROMIUM,
               void(GLenum target,
                    GLenum internalFormat,
                    GLenum bufferUsage,
                    GLsizei width,
                    GLsizei height));

  // Discardable textures.
  MOCK_METHOD1(InitializeDiscardableTextureCHROMIUM, void(GLuint texture_id));
  MOCK_METHOD1(UnlockDiscardableTextureCHROMIUM, void(GLuint texture_id));
  MOCK_METHOD1(LockDiscardableTextureCHROMIUM, bool(GLuint texture_id));

  // OOP-Raster
  MOCK_METHOD6(BeginRasterCHROMIUM,
               void(GLuint texture_id,
                    GLuint sk_color,
                    GLuint msaa_sample_count,
                    GLboolean can_use_lcd_text,
                    GLboolean use_distance_field_text,
                    GLint pixel_config));
  MOCK_METHOD10(RasterCHROMIUM,
                void(const cc::DisplayItemList* list,
                     GLint translate_x,
                     GLint translate_y,
                     GLint clip_x,
                     GLint clip_y,
                     GLint clip_w,
                     GLint clip_h,
                     GLfloat post_translate_x,
                     GLfloat post_translate_y,
                     GLfloat post_scale));
  MOCK_METHOD0(EndRasterCHROMIUM, void());
};

class RasterImplementationGLESTest : public testing::Test {
 protected:
  RasterImplementationGLESTest() {}

  void SetUp() override {
    gl_.reset(new RasterMockGLES2Interface());

    ri_.reset(new RasterImplementationGLES(gl_.get(), gpu::Capabilities()));
  }

  void TearDown() override {}

  void SetUpWithCapabilities(const gpu::Capabilities& capabilities) {
    ri_.reset(new RasterImplementationGLES(gl_.get(), capabilities));
  }

  std::unique_ptr<RasterMockGLES2Interface> gl_;
  std::unique_ptr<RasterImplementationGLES> ri_;
};

TEST_F(RasterImplementationGLESTest, Finish) {
  EXPECT_CALL(*gl_, Finish()).Times(1);
  ri_->Finish();
}

TEST_F(RasterImplementationGLESTest, ShallowFlushCHROMIUM) {
  EXPECT_CALL(*gl_, ShallowFlushCHROMIUM()).Times(1);
  ri_->ShallowFlushCHROMIUM();
}

TEST_F(RasterImplementationGLESTest, OrderingBarrierCHROMIUM) {
  EXPECT_CALL(*gl_, OrderingBarrierCHROMIUM()).Times(1);
  ri_->OrderingBarrierCHROMIUM();
}

TEST_F(RasterImplementationGLESTest, InsertFenceSyncCHROMIUM) {
  const GLuint64 kFenceSync = 0x123u;
  GLuint64 fence_sync = 0;

  EXPECT_CALL(*gl_, InsertFenceSyncCHROMIUM()).WillOnce(Return(kFenceSync));
  fence_sync = ri_->InsertFenceSyncCHROMIUM();
  EXPECT_EQ(kFenceSync, fence_sync);
}

TEST_F(RasterImplementationGLESTest, GenSyncTokenCHROMIUM) {
  const GLuint64 kFenceSync = 0x123u;
  GLbyte sync_token_data[GL_SYNC_TOKEN_SIZE_CHROMIUM] = {};

  EXPECT_CALL(*gl_, GenSyncTokenCHROMIUM(kFenceSync, sync_token_data)).Times(1);
  ri_->GenSyncTokenCHROMIUM(kFenceSync, sync_token_data);
}

TEST_F(RasterImplementationGLESTest, GenUnverifiedSyncTokenCHROMIUM) {
  const GLuint64 kFenceSync = 0x123u;
  GLbyte sync_token_data[GL_SYNC_TOKEN_SIZE_CHROMIUM] = {};

  EXPECT_CALL(*gl_, GenUnverifiedSyncTokenCHROMIUM(kFenceSync, sync_token_data))
      .Times(1);
  ri_->GenUnverifiedSyncTokenCHROMIUM(kFenceSync, sync_token_data);
}

TEST_F(RasterImplementationGLESTest, VerifySyncTokensCHROMIUM) {
  const GLsizei kNumSyncTokens = 2;
  GLbyte sync_token_data[GL_SYNC_TOKEN_SIZE_CHROMIUM][kNumSyncTokens] = {};
  GLbyte* sync_tokens[2] = {sync_token_data[0], sync_token_data[1]};

  EXPECT_CALL(*gl_, VerifySyncTokensCHROMIUM(sync_tokens, kNumSyncTokens))
      .Times(1);
  ri_->VerifySyncTokensCHROMIUM(sync_tokens, kNumSyncTokens);
}

TEST_F(RasterImplementationGLESTest, WaitSyncTokenCHROMIUM) {
  GLbyte sync_token_data[GL_SYNC_TOKEN_SIZE_CHROMIUM] = {};

  EXPECT_CALL(*gl_, WaitSyncTokenCHROMIUM(sync_token_data)).Times(1);
  ri_->WaitSyncTokenCHROMIUM(sync_token_data);
}

TEST_F(RasterImplementationGLESTest, GetError) {
  const GLuint kGLInvalidOperation = GL_INVALID_OPERATION;

  EXPECT_CALL(*gl_, GetError()).WillOnce(Return(kGLInvalidOperation));
  GLenum error = ri_->GetError();
  EXPECT_EQ(kGLInvalidOperation, error);
}

TEST_F(RasterImplementationGLESTest, GetGraphicsResetStatusKHR) {
  const GLuint kGraphicsResetStatus = GL_UNKNOWN_CONTEXT_RESET_KHR;

  EXPECT_CALL(*gl_, GetGraphicsResetStatusKHR())
      .WillOnce(Return(kGraphicsResetStatus));
  GLenum status = ri_->GetGraphicsResetStatusKHR();
  EXPECT_EQ(kGraphicsResetStatus, status);
}

TEST_F(RasterImplementationGLESTest, GetIntegerv) {
  const GLint kActiveTexture = 3;
  GLint active_texture = 0;

  EXPECT_CALL(*gl_, GetIntegerv(GL_ACTIVE_TEXTURE, &active_texture))
      .WillOnce(SetArgPointee<1>(kActiveTexture));
  ri_->GetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
  EXPECT_EQ(kActiveTexture, active_texture);
}

TEST_F(RasterImplementationGLESTest, LoseContextCHROMIUM) {
  const GLenum kCurrent = GL_GUILTY_CONTEXT_RESET_ARB;
  const GLenum kOther = GL_INNOCENT_CONTEXT_RESET_ARB;

  EXPECT_CALL(*gl_, LoseContextCHROMIUM(kCurrent, kOther)).Times(1);
  ri_->LoseContextCHROMIUM(kCurrent, kOther);
}

TEST_F(RasterImplementationGLESTest, GenQueriesEXT) {
  const GLsizei kNumQueries = 2;
  GLuint queries[kNumQueries] = {};

  EXPECT_CALL(*gl_, GenQueriesEXT(kNumQueries, queries)).Times(1);
  ri_->GenQueriesEXT(kNumQueries, queries);
}

TEST_F(RasterImplementationGLESTest, DeleteQueriesEXT) {
  const GLsizei kNumQueries = 2;
  GLuint queries[kNumQueries] = {2, 3};

  EXPECT_CALL(*gl_, DeleteQueriesEXT(kNumQueries, queries)).Times(1);
  ri_->DeleteQueriesEXT(kNumQueries, queries);
}

TEST_F(RasterImplementationGLESTest, BeginQueryEXT) {
  const GLsizei kQueryTarget = GL_COMMANDS_ISSUED_CHROMIUM;
  const GLuint kQueryId = 23;

  EXPECT_CALL(*gl_, BeginQueryEXT(kQueryTarget, kQueryId)).Times(1);
  ri_->BeginQueryEXT(kQueryTarget, kQueryId);
}

TEST_F(RasterImplementationGLESTest, EndQueryEXT) {
  const GLsizei kQueryTarget = GL_COMMANDS_ISSUED_CHROMIUM;

  EXPECT_CALL(*gl_, EndQueryEXT(kQueryTarget)).Times(1);
  ri_->EndQueryEXT(kQueryTarget);
}

TEST_F(RasterImplementationGLESTest, GetQueryObjectuivEXT) {
  const GLuint kQueryId = 23;
  const GLsizei kQueryParam = GL_QUERY_RESULT_AVAILABLE_EXT;
  GLuint result = 0;

  EXPECT_CALL(*gl_, GetQueryObjectuivEXT(kQueryId, kQueryParam, &result))
      .Times(1);
  ri_->GetQueryObjectuivEXT(kQueryId, kQueryParam, &result);
}

// MOCK_METHOD2(DeleteTextures, void(GLsizei n, const GLuint* textures));
TEST_F(RasterImplementationGLESTest, GenTextures) {
  const GLsizei kNumTextures = 2;
  GLuint texture_ids[kNumTextures] = {};

  EXPECT_CALL(*gl_, GenTextures(kNumTextures, texture_ids)).Times(1);
  ri_->GenTextures(kNumTextures, texture_ids);
}

TEST_F(RasterImplementationGLESTest, DeleteTextures) {
  const GLsizei kNumTextures = 2;
  GLuint texture_ids[kNumTextures] = {2, 3};

  EXPECT_CALL(*gl_, DeleteTextures(kNumTextures, texture_ids)).Times(1);
  ri_->DeleteTextures(kNumTextures, texture_ids);
}

TEST_F(RasterImplementationGLESTest, BindTexture) {
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLuint kTextureId = 23;

  EXPECT_CALL(*gl_, BindTexture(kTarget, kTextureId)).Times(1);
  ri_->BindTexture(kTarget, kTextureId);
}

TEST_F(RasterImplementationGLESTest, ActiveTexture) {
  const GLenum kTextureUnit = GL_TEXTURE0;

  EXPECT_CALL(*gl_, ActiveTexture(kTextureUnit)).Times(1);
  ri_->ActiveTexture(kTextureUnit);
}

TEST_F(RasterImplementationGLESTest, GenerateMipmap) {
  const GLenum kTarget = GL_TEXTURE_2D;

  EXPECT_CALL(*gl_, GenerateMipmap(kTarget)).Times(1);
  ri_->GenerateMipmap(kTarget);
}

TEST_F(RasterImplementationGLESTest, SetColorSpaceMetadataCHROMIUM) {
  const GLuint kTextureId = 23;
  gfx::ColorSpace color_space;

  EXPECT_CALL(*gl_,
              SetColorSpaceMetadataCHROMIUM(
                  kTextureId, reinterpret_cast<GLColorSpace>(&color_space)))
      .Times(1);
  ri_->SetColorSpaceMetadataCHROMIUM(
      kTextureId, reinterpret_cast<GLColorSpace>(&color_space));
}

TEST_F(RasterImplementationGLESTest, TexParameteri) {
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLenum kPname = GL_TEXTURE_MIN_FILTER;
  const GLint kParam = GL_NEAREST;

  EXPECT_CALL(*gl_, TexParameteri(kTarget, kPname, kParam)).Times(1);
  ri_->TexParameteri(kTarget, kPname, kParam);
}

TEST_F(RasterImplementationGLESTest, GenMailboxCHROMIUM) {
  gpu::Mailbox mailbox;
  EXPECT_CALL(*gl_, GenMailboxCHROMIUM(mailbox.name)).Times(1);
  ri_->GenMailboxCHROMIUM(mailbox.name);
}

TEST_F(RasterImplementationGLESTest, ProduceTextureDirectCHROMIUM) {
  const GLuint kTextureId = 23;
  GLuint texture_id = 0;
  gpu::Mailbox mailbox;

  EXPECT_CALL(*gl_, GenTextures(1, &texture_id))
      .WillOnce(SetArgPointee<1>(kTextureId));
  EXPECT_CALL(*gl_, GenMailboxCHROMIUM(mailbox.name)).Times(1);
  EXPECT_CALL(*gl_, ProduceTextureDirectCHROMIUM(kTextureId, mailbox.name))
      .Times(1);

  ri_->GenTextures(1, &texture_id);
  ri_->GenMailboxCHROMIUM(mailbox.name);
  ri_->ProduceTextureDirectCHROMIUM(texture_id, mailbox.name);
}

TEST_F(RasterImplementationGLESTest, CreateAndConsumeTextureCHROMIUM) {
  const GLuint kTextureId = 23;
  GLuint texture_id = 0;
  gpu::Mailbox mailbox;

  EXPECT_CALL(*gl_, CreateAndConsumeTextureCHROMIUM(mailbox.name))
      .WillOnce(Return(kTextureId));
  texture_id = ri_->CreateAndConsumeTextureCHROMIUM(mailbox.name);
  EXPECT_EQ(kTextureId, texture_id);
}

TEST_F(RasterImplementationGLESTest, CreateImageCHROMIUM) {
  const GLsizei kWidth = 256;
  const GLsizei kHeight = 128;
  const GLenum kInternalFormat = GL_RGBA;
  const GLint kImageId = 23;
  const ClientBuffer client_buffer = 0;
  GLint image_id = 0;

  EXPECT_CALL(*gl_, CreateImageCHROMIUM(client_buffer, kWidth, kHeight,
                                        kInternalFormat))
      .WillOnce(Return(kImageId));
  image_id =
      ri_->CreateImageCHROMIUM(client_buffer, kWidth, kHeight, kInternalFormat);
  EXPECT_EQ(kImageId, image_id);
}

TEST_F(RasterImplementationGLESTest, BindTexImage2DCHROMIUM) {
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kImageId = 23;

  EXPECT_CALL(*gl_, BindTexImage2DCHROMIUM(kTarget, kImageId)).Times(1);
  ri_->BindTexImage2DCHROMIUM(kTarget, kImageId);
}

TEST_F(RasterImplementationGLESTest, ReleaseTexImage2DCHROMIUM) {
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kImageId = 23;

  EXPECT_CALL(*gl_, ReleaseTexImage2DCHROMIUM(kTarget, kImageId)).Times(1);
  ri_->ReleaseTexImage2DCHROMIUM(kTarget, kImageId);
}

TEST_F(RasterImplementationGLESTest, DestroyImageCHROMIUM) {
  const GLint kImageId = 23;

  EXPECT_CALL(*gl_, DestroyImageCHROMIUM(kImageId)).Times(1);
  ri_->DestroyImageCHROMIUM(kImageId);
}

TEST_F(RasterImplementationGLESTest, TexImage2D) {
  const GLenum target = GL_TEXTURE_2D;
  const GLint level = 1;
  const GLint internalformat = GL_RGBA;
  const GLsizei width = 2;
  const GLsizei height = 8;
  const GLint border = 0;
  const GLenum format = GL_RGBA;
  const GLenum type = GL_UNSIGNED_BYTE;
  const unsigned char pixels[64] = {};

  EXPECT_CALL(*gl_, TexImage2D(target, level, internalformat, width, height,
                               border, format, type, pixels))
      .Times(1);
  ri_->TexImage2D(target, level, internalformat, width, height, border, format,
                  type, pixels);
}

TEST_F(RasterImplementationGLESTest, TexSubImage2D) {
  const GLenum target = GL_TEXTURE_2D;
  const GLint level = 1;
  const GLint xoffset = 10;
  const GLint yoffset = 11;
  const GLsizei width = 2;
  const GLsizei height = 8;
  const GLenum format = GL_RGBA;
  const GLenum type = GL_UNSIGNED_BYTE;
  const unsigned char pixels[64] = {};

  EXPECT_CALL(*gl_, TexSubImage2D(target, level, xoffset, yoffset, width,
                                  height, format, type, pixels))
      .Times(1);
  ri_->TexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                     type, pixels);
}

TEST_F(RasterImplementationGLESTest, CompressedTexImage2D) {
  const GLenum target = GL_TEXTURE_2D;
  const GLint level = 1;
  const GLint internalformat = viz::GLInternalFormat(viz::ETC1);
  const GLsizei width = 2;
  const GLsizei height = 8;
  const GLint border = 0;
  const GLsizei image_size = 64;
  const unsigned char data[64] = {};

  EXPECT_CALL(*gl_, CompressedTexImage2D(target, level, internalformat, width,
                                         height, border, image_size, data))
      .Times(1);
  ri_->CompressedTexImage2D(target, level, internalformat, width, height,
                            border, image_size, data);
}

TEST_F(RasterImplementationGLESTest, CompressedCopyTextureCHROMIUM) {
  const GLuint source_id = 23;
  const GLuint dest_id = 24;

  EXPECT_CALL(*gl_, CompressedCopyTextureCHROMIUM(source_id, dest_id)).Times(1);
  ri_->CompressedCopyTextureCHROMIUM(source_id, dest_id);
}

TEST_F(RasterImplementationGLESTest, TexStorageForRasterTexImage2D) {
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLsizei kWidth = 2;
  const GLsizei kHeight = 8;
  const int kNumTestFormats = 11;
  viz::ResourceFormat test_formats[kNumTestFormats] = {
      viz::RGBA_8888,     viz::RGBA_4444, viz::BGRA_8888, viz::ALPHA_8,
      viz::LUMINANCE_8,   viz::RGB_565,   viz::ETC1,      viz::RED_8,
      viz::LUMINANCE_F16, viz::RGBA_F16,  viz::R16_EXT};

  for (int i = 0; i < kNumTestFormats; i++) {
    viz::ResourceFormat format = test_formats[i];
    EXPECT_CALL(*gl_, TexImage2D(kTarget, 0, viz::GLInternalFormat(format),
                                 kWidth, kHeight, 0, viz::GLDataFormat(format),
                                 viz::GLDataType(format), nullptr))
        .Times(1);
    ri_->TexStorageForRaster(kTarget, format, kWidth, kHeight,
                             gpu::raster::kNone);
  }
}

TEST_F(RasterImplementationGLESTest, TexStorageForRasterTexStorage2DEXT) {
  gpu::Capabilities capabilities;
  capabilities.texture_storage = true;
  SetUpWithCapabilities(capabilities);

  const GLenum kTarget = GL_TEXTURE_2D;
  const GLsizei kWidth = 2;
  const GLsizei kHeight = 8;
  const int kNumTestFormats = 10;
  const int kLevels = 1;
  viz::ResourceFormat test_formats[kNumTestFormats] = {
      viz::RGBA_8888,   viz::RGBA_4444, viz::BGRA_8888, viz::ALPHA_8,
      viz::LUMINANCE_8, viz::RGB_565,   viz::RED_8,     viz::LUMINANCE_F16,
      viz::RGBA_F16,    viz::R16_EXT};

  for (int i = 0; i < kNumTestFormats; i++) {
    viz::ResourceFormat format = test_formats[i];
    EXPECT_CALL(*gl_, TexStorage2DEXT(kTarget, kLevels,
                                      viz::TextureStorageFormat(format), kWidth,
                                      kHeight))
        .Times(1);
    ri_->TexStorageForRaster(kTarget, format, kWidth, kHeight,
                             gpu::raster::kNone);
  }
}

TEST_F(RasterImplementationGLESTest, TexStorageForRasterOverlay) {
  gpu::Capabilities capabilities;
  capabilities.texture_storage_image = true;
  SetUpWithCapabilities(capabilities);

  const GLenum kTarget = GL_TEXTURE_2D;
  const GLsizei kWidth = 2;
  const GLsizei kHeight = 8;
  const int kNumTestFormats = 10;
  viz::ResourceFormat test_formats[kNumTestFormats] = {
      viz::RGBA_8888,   viz::RGBA_4444, viz::BGRA_8888, viz::ALPHA_8,
      viz::LUMINANCE_8, viz::RGB_565,   viz::RED_8,     viz::LUMINANCE_F16,
      viz::RGBA_F16,    viz::R16_EXT};

  for (int i = 0; i < kNumTestFormats; i++) {
    viz::ResourceFormat format = test_formats[i];
    EXPECT_CALL(*gl_, TexStorage2DImageCHROMIUM(
                          kTarget, viz::TextureStorageFormat(format),
                          GL_SCANOUT_CHROMIUM, kWidth, kHeight))
        .Times(1);
    ri_->TexStorageForRaster(kTarget, format, kWidth, kHeight,
                             gpu::raster::kOverlay);
  }
}

TEST_F(RasterImplementationGLESTest, InitializeDiscardableTextureCHROMIUM) {
  const GLuint kTextureId = 23;

  EXPECT_CALL(*gl_, InitializeDiscardableTextureCHROMIUM(kTextureId)).Times(1);
  ri_->InitializeDiscardableTextureCHROMIUM(kTextureId);
}

TEST_F(RasterImplementationGLESTest, UnlockDiscardableTextureCHROMIUM) {
  const GLuint kTextureId = 23;

  EXPECT_CALL(*gl_, UnlockDiscardableTextureCHROMIUM(kTextureId)).Times(1);
  ri_->UnlockDiscardableTextureCHROMIUM(kTextureId);
}

TEST_F(RasterImplementationGLESTest, LockDiscardableTextureCHROMIUM) {
  const GLuint kTextureId = 23;
  bool ret = false;

  EXPECT_CALL(*gl_, LockDiscardableTextureCHROMIUM(kTextureId))
      .WillOnce(Return(true));
  ret = ri_->LockDiscardableTextureCHROMIUM(kTextureId);
  EXPECT_EQ(true, ret);

  EXPECT_CALL(*gl_, LockDiscardableTextureCHROMIUM(kTextureId))
      .WillOnce(Return(false));
  ret = ri_->LockDiscardableTextureCHROMIUM(kTextureId);
  EXPECT_EQ(false, ret);
}

TEST_F(RasterImplementationGLESTest, BeginRasterCHROMIUM) {
  const GLuint texture_id = 23;
  const GLuint sk_color = 0x226688AAu;
  const GLuint msaa_sample_count = 4;
  const GLboolean can_use_lcd_text = GL_TRUE;
  const GLboolean use_distance_field_text = GL_FALSE;
  const GLint pixel_config = kRGBA_8888_GrPixelConfig;
  EXPECT_CALL(*gl_, BeginRasterCHROMIUM(texture_id, sk_color, msaa_sample_count,
                                        can_use_lcd_text,
                                        use_distance_field_text, pixel_config))
      .Times(1);
  ri_->BeginRasterCHROMIUM(texture_id, sk_color, msaa_sample_count,
                           can_use_lcd_text, use_distance_field_text,
                           pixel_config);
}

TEST_F(RasterImplementationGLESTest, RasterCHROMIUM) {
  scoped_refptr<cc::DisplayItemList> display_list = new cc::DisplayItemList;
  const GLint translate_x = 1;
  const GLint translate_y = 2;
  const GLint clip_x = 3;
  const GLint clip_y = 4;
  const GLint clip_w = 5;
  const GLint clip_h = 6;
  const GLfloat post_translate_x = 7.0f;
  const GLfloat post_translate_y = 8.0f;
  const GLfloat post_scale = 9.0f;

  EXPECT_CALL(
      *gl_, RasterCHROMIUM(display_list.get(), translate_x, translate_y, clip_x,
                           clip_y, clip_w, clip_h, post_translate_x,
                           post_translate_y, post_scale))
      .Times(1);
  ri_->RasterCHROMIUM(display_list.get(), translate_x, translate_y, clip_x,
                      clip_y, clip_w, clip_h, post_translate_x,
                      post_translate_y, post_scale);
}

TEST_F(RasterImplementationGLESTest, EndRasterCHROMIUM) {
  EXPECT_CALL(*gl_, EndRasterCHROMIUM()).Times(1);
  ri_->EndRasterCHROMIUM();
}

}  // namespace raster
}  // namespace gpu
