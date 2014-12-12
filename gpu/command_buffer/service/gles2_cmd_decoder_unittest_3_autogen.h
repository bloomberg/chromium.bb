// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// It is included by gles2_cmd_decoder_unittest_3.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_3_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_3_AUTOGEN_H_

TEST_P(GLES2DecoderTest3, EndTransformFeedbackValidArgs) {
  EXPECT_CALL(*gl_, EndTransformFeedback());
  SpecializedSetup<cmds::EndTransformFeedback, 0>(true);
  cmds::EndTransformFeedback cmd;
  cmd.Init();
  decoder_->set_unsafe_es3_apis_enabled(true);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  decoder_->set_unsafe_es3_apis_enabled(false);
  EXPECT_EQ(error::kUnknownCommand, ExecuteCmd(cmd));
}
// TODO(gman): InsertEventMarkerEXT

// TODO(gman): PushGroupMarkerEXT

TEST_P(GLES2DecoderTest3, PopGroupMarkerEXTValidArgs) {
  SpecializedSetup<cmds::PopGroupMarkerEXT, 0>(true);
  cmds::PopGroupMarkerEXT cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): GenVertexArraysOESImmediate
// TODO(gman): DeleteVertexArraysOESImmediate
// TODO(gman): IsVertexArrayOES
// TODO(gman): BindVertexArrayOES
// TODO(gman): SwapBuffers
// TODO(gman): GetMaxValueInBufferCHROMIUM
// TODO(gman): EnableFeatureCHROMIUM

// TODO(gman): ResizeCHROMIUM
// TODO(gman): GetRequestableExtensionsCHROMIUM

// TODO(gman): RequestExtensionCHROMIUM

// TODO(gman): GetProgramInfoCHROMIUM

// TODO(gman): GetTranslatedShaderSourceANGLE
// TODO(gman): PostSubBufferCHROMIUM
// TODO(gman): TexImageIOSurface2DCHROMIUM
// TODO(gman): CopyTextureCHROMIUM
// TODO(gman): DrawArraysInstancedANGLE
// TODO(gman): DrawElementsInstancedANGLE
// TODO(gman): VertexAttribDivisorANGLE
// TODO(gman): GenMailboxCHROMIUM

// TODO(gman): ProduceTextureCHROMIUMImmediate
// TODO(gman): ProduceTextureDirectCHROMIUMImmediate
// TODO(gman): ConsumeTextureCHROMIUMImmediate
// TODO(gman): CreateAndConsumeTextureCHROMIUMImmediate
// TODO(gman): BindUniformLocationCHROMIUMBucket
// TODO(gman): GenValuebuffersCHROMIUMImmediate
// TODO(gman): DeleteValuebuffersCHROMIUMImmediate

TEST_P(GLES2DecoderTest3, IsValuebufferCHROMIUMValidArgs) {
  SpecializedSetup<cmds::IsValuebufferCHROMIUM, 0>(true);
  cmds::IsValuebufferCHROMIUM cmd;
  cmd.Init(client_valuebuffer_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, IsValuebufferCHROMIUMInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsValuebufferCHROMIUM, 0>(false);
  cmds::IsValuebufferCHROMIUM cmd;
  cmd.Init(client_valuebuffer_id_, kInvalidSharedMemoryId,
           shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_valuebuffer_id_, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}
// TODO(gman): BindValuebufferCHROMIUM
// TODO(gman): SubscribeValueCHROMIUM
// TODO(gman): PopulateSubscribedValuesCHROMIUM
// TODO(gman): UniformValuebufferCHROMIUM
// TODO(gman): BindTexImage2DCHROMIUM
// TODO(gman): ReleaseTexImage2DCHROMIUM
// TODO(gman): TraceBeginCHROMIUM

// TODO(gman): TraceEndCHROMIUM
// TODO(gman): AsyncTexSubImage2DCHROMIUM

// TODO(gman): AsyncTexImage2DCHROMIUM

// TODO(gman): WaitAsyncTexImage2DCHROMIUM

// TODO(gman): WaitAllAsyncTexImage2DCHROMIUM

// TODO(gman): LoseContextCHROMIUM
// TODO(gman): InsertSyncPointCHROMIUM

// TODO(gman): WaitSyncPointCHROMIUM

// TODO(gman): DrawBuffersEXTImmediate
// TODO(gman): DiscardBackbufferCHROMIUM

// TODO(gman): ScheduleOverlayPlaneCHROMIUM
// TODO(gman): SwapInterval
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_3_AUTOGEN_H_
