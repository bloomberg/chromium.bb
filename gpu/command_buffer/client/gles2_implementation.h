// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H

#include "base/shared_memory.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/id_allocator.h"
#include "gpu/command_buffer/client/fenced_allocator.h"

namespace command_buffer {
namespace gles2 {

// This class emulates GLES2 over command buffers. It can be used by a client
// program so that the program does not need deal with shared memory and command
// buffer management. See gl2_lib.h.  Note that there is a performance gain to
// be had by changing your code to use command buffers directly by using the
// GLES2CmdHelper but that entails changing your code to use and deal with
// shared memory and synchronization issues.
class GLES2Implementation {
 public:
  GLES2Implementation(
      GLES2CmdHelper* helper,
      size_t transfer_buffer_size,
      void* transfer_buffer,
      int32 transfer_buffer_id);

  // The GLES2CmdHelper being used by this GLES2Implementation. You can use
  // this to issue cmds at a lower level for certain kinds of optimization.
  GLES2CmdHelper* helper() const {
    return helper_;
  }

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gpu/command_buffer/client/gles2_implementation_autogen.h"

 private:
  // Makes a set of Ids for glGen___ functions.
  void MakeIds(GLsizei n, GLuint* ids);

  // Frees a set of Ids for glDelete___ functions.
  void FreeIds(GLsizei n, const GLuint* ids);

  // Gets the shared memory id for the result buffer.
  uint32 result_shm_id() const {
    return transfer_buffer_id_;
  }

  // Gets the shared memory offset for the result buffer.
  uint32 result_shm_offset() const {
    return result_shm_offset_;
  }

  // Gets the value of the result.
  template <typename T>
  T GetResultAs() const {
    return *static_cast<T*>(result_buffer_);
  }

  // Waits for all commands to execute.
  void WaitForCmd();

  // The maxiumum result size from simple GL get commands.
  static const size_t kMaxSizeOfSimpleResult = 4 * sizeof(uint32);  // NOLINT.

  GLES2Util util_;
  GLES2CmdHelper* helper_;
  IdAllocator id_allocator_;
  FencedAllocatorWrapper transfer_buffer_;
  int transfer_buffer_id_;
  void* result_buffer_;
  uint32 result_shm_offset_;

  // pack alignment as last set by glPixelStorei
  GLint pack_alignment_;

  // unpack alignment as last set by glPixelStorei
  GLint unpack_alignment_;

  DISALLOW_COPY_AND_ASSIGN(GLES2Implementation);
};


}  // namespace gles2
}  // namespace command_buffer

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H

