// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_H_

#include "../../gpu_export.h"
#include "../client/cmd_buffer_helper.h"
#include "../common/gles2_cmd_format.h"

namespace gpu {
namespace gles2 {

// A class that helps write GL command buffers.
class GPU_EXPORT GLES2CmdHelper : public CommandBufferHelper {
 public:
  explicit GLES2CmdHelper(CommandBuffer* command_buffer);
  virtual ~GLES2CmdHelper();

  // Include the auto-generated part of this class. We split this because it
  // means we can easily edit the non-auto generated parts right here in this
  // file instead of having to edit some template or the code generator.
  #include "../client/gles2_cmd_helper_autogen.h"

  // Helpers that could not be auto-generated.
  // TODO(gman): Auto generate these.

  void GetAttribLocation(
      GLuint program, uint32 name_shm_id, uint32 name_shm_offset,
      uint32 location_shm_id, uint32 location_shm_offset, uint32 data_size) {
    gles2::cmds::GetAttribLocation* c =
        GetCmdSpace<gles2::cmds::GetAttribLocation>();
    if (c) {
      c->Init(
          program, name_shm_id, name_shm_offset, location_shm_id,
          location_shm_offset, data_size);
    }
  }

  void GetAttribLocationImmediate(
      GLuint program, const char* name,
      uint32 location_shm_id, uint32 location_shm_offset) {
    const uint32 size = gles2::cmds::GetAttribLocationImmediate::ComputeSize(
        name);
    gles2::cmds::GetAttribLocationImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::GetAttribLocationImmediate>(
            size);
    if (c) {
      c->Init(program, name, location_shm_id, location_shm_offset);
    }
  }

  void GetAttribLocationBucket(
      GLuint program, uint32 name_bucket_id,
      uint32 location_shm_id, uint32 location_shm_offset) {
    gles2::cmds::GetAttribLocationBucket* c =
        GetCmdSpace<gles2::cmds::GetAttribLocationBucket>();
    if (c) {
      c->Init(program, name_bucket_id, location_shm_id, location_shm_offset);
    }
  }

  void GetUniformLocation(
      GLuint program, uint32 name_shm_id, uint32 name_shm_offset,
      uint32 location_shm_id, uint32 location_shm_offset, uint32 data_size) {
    gles2::cmds::GetUniformLocation* c =
        GetCmdSpace<gles2::cmds::GetUniformLocation>();
    if (c) {
      c->Init(
          program, name_shm_id, name_shm_offset, location_shm_id,
          location_shm_offset, data_size);
    }
  }

  void GetUniformLocationImmediate(
      GLuint program, const char* name,
      uint32 location_shm_id, uint32 location_shm_offset) {
    const uint32 size = gles2::cmds::GetUniformLocationImmediate::ComputeSize(
        name);
    gles2::cmds::GetUniformLocationImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::GetUniformLocationImmediate>(
            size);
    if (c) {
      c->Init(program, name, location_shm_id, location_shm_offset);
    }
  }

  void GetUniformLocationBucket(
      GLuint program, uint32 name_bucket_id,
      uint32 location_shm_id, uint32 location_shm_offset) {
    gles2::cmds::GetUniformLocationBucket* c =
        GetCmdSpace<gles2::cmds::GetUniformLocationBucket>();
    if (c) {
      c->Init(program, name_bucket_id, location_shm_id, location_shm_offset);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLES2CmdHelper);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_H_

