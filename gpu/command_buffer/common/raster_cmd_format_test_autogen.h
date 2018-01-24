// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file contains unit tests for raster commmands
// It is included by raster_cmd_format_test.cc

#ifndef GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_TEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_TEST_AUTOGEN_H_

TEST_F(RasterFormatTest, ActiveTexture) {
  cmds::ActiveTexture& cmd = *GetBufferAs<cmds::ActiveTexture>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ActiveTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, BindTexture) {
  cmds::BindTexture& cmd = *GetBufferAs<cmds::BindTexture>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, CompressedTexImage2DBucket) {
  cmds::CompressedTexImage2DBucket& cmd =
      *GetBufferAs<cmds::CompressedTexImage2DBucket>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLenum>(13), static_cast<GLsizei>(14),
              static_cast<GLsizei>(15), static_cast<GLuint>(16));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CompressedTexImage2DBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, CompressedTexImage2D) {
  cmds::CompressedTexImage2D& cmd = *GetBufferAs<cmds::CompressedTexImage2D>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLenum>(13), static_cast<GLsizei>(14),
              static_cast<GLsizei>(15), static_cast<GLsizei>(16),
              static_cast<uint32_t>(17), static_cast<uint32_t>(18));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CompressedTexImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.imageSize);
  EXPECT_EQ(static_cast<uint32_t>(17), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(18), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, DeleteTexturesImmediate) {
  static GLuint ids[] = {
      12, 23, 34,
  };
  cmds::DeleteTexturesImmediate& cmd =
      *GetBufferAs<cmds::DeleteTexturesImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteTexturesImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(RasterFormatTest, Finish) {
  cmds::Finish& cmd = *GetBufferAs<cmds::Finish>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Finish::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, Flush) {
  cmds::Flush& cmd = *GetBufferAs<cmds::Flush>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Flush::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, GenTexturesImmediate) {
  static GLuint ids[] = {
      12, 23, 34,
  };
  cmds::GenTexturesImmediate& cmd = *GetBufferAs<cmds::GenTexturesImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenTexturesImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(RasterFormatTest, GetError) {
  cmds::GetError& cmd = *GetBufferAs<cmds::GetError>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<uint32_t>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetError::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32_t>(11), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, GetIntegerv) {
  cmds::GetIntegerv& cmd = *GetBufferAs<cmds::GetIntegerv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetIntegerv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, PixelStorei) {
  cmds::PixelStorei& cmd = *GetBufferAs<cmds::PixelStorei>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::PixelStorei::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<GLint>(12), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, TexImage2D) {
  cmds::TexImage2D& cmd = *GetBufferAs<cmds::TexImage2D>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
              static_cast<GLint>(13), static_cast<GLsizei>(14),
              static_cast<GLsizei>(15), static_cast<GLenum>(16),
              static_cast<GLenum>(17), static_cast<uint32_t>(18),
              static_cast<uint32_t>(19));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(16), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.type);
  EXPECT_EQ(static_cast<uint32_t>(18), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(19), cmd.pixels_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, TexParameteri) {
  cmds::TexParameteri& cmd = *GetBufferAs<cmds::TexParameteri>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLenum>(12), static_cast<GLint>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexParameteri::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<GLint>(13), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, TexSubImage2D) {
  cmds::TexSubImage2D& cmd = *GetBufferAs<cmds::TexSubImage2D>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLenum>(11), static_cast<GLint>(12),
      static_cast<GLint>(13), static_cast<GLint>(14), static_cast<GLsizei>(15),
      static_cast<GLsizei>(16), static_cast<GLenum>(17),
      static_cast<GLenum>(18), static_cast<uint32_t>(19),
      static_cast<uint32_t>(20), static_cast<GLboolean>(21));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexSubImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(18), cmd.type);
  EXPECT_EQ(static_cast<uint32_t>(19), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(20), cmd.pixels_shm_offset);
  EXPECT_EQ(static_cast<GLboolean>(21), cmd.internal);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, TexStorage2DEXT) {
  cmds::TexStorage2DEXT& cmd = *GetBufferAs<cmds::TexStorage2DEXT>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLenum>(11),
                           static_cast<GLsizei>(12), static_cast<GLenum>(13),
                           static_cast<GLsizei>(14), static_cast<GLsizei>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexStorage2DEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.levels);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalFormat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, GenQueriesEXTImmediate) {
  static GLuint ids[] = {
      12, 23, 34,
  };
  cmds::GenQueriesEXTImmediate& cmd =
      *GetBufferAs<cmds::GenQueriesEXTImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenQueriesEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(RasterFormatTest, DeleteQueriesEXTImmediate) {
  static GLuint ids[] = {
      12, 23, 34,
  };
  cmds::DeleteQueriesEXTImmediate& cmd =
      *GetBufferAs<cmds::DeleteQueriesEXTImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteQueriesEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(RasterFormatTest, BeginQueryEXT) {
  cmds::BeginQueryEXT& cmd = *GetBufferAs<cmds::BeginQueryEXT>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BeginQueryEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.sync_data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.sync_data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, EndQueryEXT) {
  cmds::EndQueryEXT& cmd = *GetBufferAs<cmds::EndQueryEXT>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::EndQueryEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.submit_count);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, CopySubTextureCHROMIUM) {
  cmds::CopySubTextureCHROMIUM& cmd =
      *GetBufferAs<cmds::CopySubTextureCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLuint>(11), static_cast<GLint>(12),
      static_cast<GLenum>(13), static_cast<GLuint>(14), static_cast<GLint>(15),
      static_cast<GLint>(16), static_cast<GLint>(17), static_cast<GLint>(18),
      static_cast<GLint>(19), static_cast<GLsizei>(20),
      static_cast<GLsizei>(21), static_cast<GLboolean>(22),
      static_cast<GLboolean>(23), static_cast<GLboolean>(24));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CopySubTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.source_id);
  EXPECT_EQ(static_cast<GLint>(12), cmd.source_level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.dest_target);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.dest_id);
  EXPECT_EQ(static_cast<GLint>(15), cmd.dest_level);
  EXPECT_EQ(static_cast<GLint>(16), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(17), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(18), cmd.x);
  EXPECT_EQ(static_cast<GLint>(19), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(20), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(21), cmd.height);
  EXPECT_EQ(static_cast<GLboolean>(22), cmd.unpack_flip_y);
  EXPECT_EQ(static_cast<GLboolean>(23), cmd.unpack_premultiply_alpha);
  EXPECT_EQ(static_cast<GLboolean>(24), cmd.unpack_unmultiply_alpha);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, CompressedCopyTextureCHROMIUM) {
  cmds::CompressedCopyTextureCHROMIUM& cmd =
      *GetBufferAs<cmds::CompressedCopyTextureCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CompressedCopyTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.source_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.dest_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, ProduceTextureDirectCHROMIUMImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
  };
  cmds::ProduceTextureDirectCHROMIUMImmediate& cmd =
      *GetBufferAs<cmds::ProduceTextureDirectCHROMIUMImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::ProduceTextureDirectCHROMIUMImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(RasterFormatTest, BindTexImage2DCHROMIUM) {
  cmds::BindTexImage2DCHROMIUM& cmd =
      *GetBufferAs<cmds::BindTexImage2DCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindTexImage2DCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.imageId);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, ReleaseTexImage2DCHROMIUM) {
  cmds::ReleaseTexImage2DCHROMIUM& cmd =
      *GetBufferAs<cmds::ReleaseTexImage2DCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ReleaseTexImage2DCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.imageId);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, TraceBeginCHROMIUM) {
  cmds::TraceBeginCHROMIUM& cmd = *GetBufferAs<cmds::TraceBeginCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TraceBeginCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.category_bucket_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.name_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, TraceEndCHROMIUM) {
  cmds::TraceEndCHROMIUM& cmd = *GetBufferAs<cmds::TraceEndCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::TraceEndCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, LoseContextCHROMIUM) {
  cmds::LoseContextCHROMIUM& cmd = *GetBufferAs<cmds::LoseContextCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::LoseContextCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.current);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.other);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, WaitSyncTokenCHROMIUM) {
  cmds::WaitSyncTokenCHROMIUM& cmd =
      *GetBufferAs<cmds::WaitSyncTokenCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLuint64>(12),
              static_cast<GLuint64>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::WaitSyncTokenCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.namespace_id);
  EXPECT_EQ(static_cast<GLuint64>(12), cmd.command_buffer_id());
  EXPECT_EQ(static_cast<GLuint64>(13), cmd.release_count());
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, InitializeDiscardableTextureCHROMIUM) {
  cmds::InitializeDiscardableTextureCHROMIUM& cmd =
      *GetBufferAs<cmds::InitializeDiscardableTextureCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::InitializeDiscardableTextureCHROMIUM::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, UnlockDiscardableTextureCHROMIUM) {
  cmds::UnlockDiscardableTextureCHROMIUM& cmd =
      *GetBufferAs<cmds::UnlockDiscardableTextureCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::UnlockDiscardableTextureCHROMIUM::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, LockDiscardableTextureCHROMIUM) {
  cmds::LockDiscardableTextureCHROMIUM& cmd =
      *GetBufferAs<cmds::LockDiscardableTextureCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::LockDiscardableTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, BeginRasterCHROMIUM) {
  cmds::BeginRasterCHROMIUM& cmd = *GetBufferAs<cmds::BeginRasterCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<GLuint>(13), static_cast<GLboolean>(14),
              static_cast<GLboolean>(15), static_cast<GLint>(16));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BeginRasterCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.sk_color);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.msaa_sample_count);
  EXPECT_EQ(static_cast<GLboolean>(14), cmd.can_use_lcd_text);
  EXPECT_EQ(static_cast<GLboolean>(15), cmd.use_distance_field_text);
  EXPECT_EQ(static_cast<GLint>(16), cmd.pixel_config);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, RasterCHROMIUM) {
  cmds::RasterCHROMIUM& cmd = *GetBufferAs<cmds::RasterCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLsizeiptr>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::RasterCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizeiptr>(11), cmd.size);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.list_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.list_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, EndRasterCHROMIUM) {
  cmds::EndRasterCHROMIUM& cmd = *GetBufferAs<cmds::EndRasterCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::EndRasterCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, CreateTransferCacheEntryINTERNAL) {
  cmds::CreateTransferCacheEntryINTERNAL& cmd =
      *GetBufferAs<cmds::CreateTransferCacheEntryINTERNAL>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLuint>(12), static_cast<GLuint>(13),
                           static_cast<GLuint>(14), static_cast<GLuint>(15),
                           static_cast<GLuint>(16), static_cast<GLuint>(17));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::CreateTransferCacheEntryINTERNAL::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.entry_type);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.entry_id);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.handle_shm_id);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.handle_shm_offset);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.data_shm_id);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.data_shm_offset);
  EXPECT_EQ(static_cast<GLuint>(17), cmd.data_size);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, DeleteTransferCacheEntryINTERNAL) {
  cmds::DeleteTransferCacheEntryINTERNAL& cmd =
      *GetBufferAs<cmds::DeleteTransferCacheEntryINTERNAL>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::DeleteTransferCacheEntryINTERNAL::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.entry_type);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.entry_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, UnlockTransferCacheEntryINTERNAL) {
  cmds::UnlockTransferCacheEntryINTERNAL& cmd =
      *GetBufferAs<cmds::UnlockTransferCacheEntryINTERNAL>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::UnlockTransferCacheEntryINTERNAL::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.entry_type);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.entry_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, TexStorage2DImageCHROMIUM) {
  cmds::TexStorage2DImageCHROMIUM& cmd =
      *GetBufferAs<cmds::TexStorage2DImageCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLsizei>(13), static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexStorage2DImageCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.internalFormat);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

#endif  // GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_TEST_AUTOGEN_H_
