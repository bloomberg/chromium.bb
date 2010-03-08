// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H_

#include <map>
#include <string>
#include <vector>
#include "../common/gles2_cmd_utils.h"
#include "../client/gles2_cmd_helper.h"
#include "../client/id_allocator.h"
#include "../client/fenced_allocator.h"

namespace gpu {
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

  ~GLES2Implementation();

  // The GLES2CmdHelper being used by this GLES2Implementation. You can use
  // this to issue cmds at a lower level for certain kinds of optimization.
  GLES2CmdHelper* helper() const {
    return helper_;
  }

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "../client/gles2_implementation_autogen.h"

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
    return static_cast<T>(result_buffer_);
  }

  // Gets the GLError through our wrapper.
  GLenum GetGLError();

  // Sets our wrapper for the GLError.
  void SetGLError(GLenum error);

  // Waits for all commands to execute.
  void WaitForCmd();

  // TODO(gman): These bucket functions really seem like they belong in
  // CommandBufferHelper (or maybe BucketHelper?). Unfortunately they need
  // a transfer buffer to function which is currently managed by this class.

  // Gets the contents of a bucket.
  void GetBucketContents(uint32 bucket_id, std::vector<int8>* data);

  // Sets the contents of a bucket.
  void SetBucketContents(uint32 bucket_id, const void* data, size_t size);

  // Gets the contents of a bucket as a string. Returns false if there is no
  // string available which is a separate case from the empty string.
  bool GetBucketAsString(uint32 bucket_id, std::string* str);

  // Sets the contents of a bucket as a string.
  void SetBucketAsString(uint32 bucket_id, const std::string& str);

  // The maxiumum result size from simple GL get commands.
  static const size_t kMaxSizeOfSimpleResult = 16 * sizeof(uint32);  // NOLINT.
  static const uint32 kResultBucketId = 1;

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

  // Current GL error bits.
  uint32 error_bits_;

  // Map of GLenum to Strings for glGetString.  We need to cache these because
  // the pointer passed back to the client has to remain valid for eternity.
  typedef std::map<uint32, std::string> GLStringMap;
  GLStringMap gl_strings_;

  DISALLOW_COPY_AND_ASSIGN(GLES2Implementation);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H_

