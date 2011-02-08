// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H_

#include <GLES2/gl2.h>

#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "../common/gles2_cmd_utils.h"
#include "../common/scoped_ptr.h"
#include "../client/gles2_cmd_helper.h"
#include "../client/ring_buffer.h"

// TODO(gman): replace with logging code expansion.
#define GPU_CLIENT_LOG(args)

namespace gpu {

class MappedMemoryManager;

namespace gles2 {

class ClientSideBufferHelper;

// Base class for IdHandlers
class IdHandlerInterface {
 public:
  IdHandlerInterface() { }
  virtual ~IdHandlerInterface() { }

  // Makes some ids at or above id_offset.
  virtual void MakeIds(GLuint id_offset, GLsizei n, GLuint* ids) = 0;

  // Frees some ids.
  virtual void FreeIds(GLsizei n, const GLuint* ids) = 0;

  // Marks an id as used for glBind functions. id = 0 does nothing.
  virtual bool MarkAsUsedForBind(GLuint id) = 0;
};

// This class emulates GLES2 over command buffers. It can be used by a client
// program so that the program does not need deal with shared memory and command
// buffer management. See gl2_lib.h.  Note that there is a performance gain to
// be had by changing your code to use command buffers directly by using the
// GLES2CmdHelper but that entails changing your code to use and deal with
// shared memory and synchronization issues.
class GLES2Implementation {
 public:
  // The maxiumum result size from simple GL get commands.
  static const size_t kMaxSizeOfSimpleResult = 16 * sizeof(uint32);  // NOLINT.

  // used for testing only. If more things are reseved add them here.
  static const unsigned int kStartingOffset = kMaxSizeOfSimpleResult;

  // The bucket used for results. Public for testing only.
  static const uint32 kResultBucketId = 1;

  // Alignment of allocations.
  static const unsigned int kAlignment = 4;

  // GL names for the buffers used to emulate client side buffers.
  static const GLuint kClientSideArrayId = 0xFEDCBA98u;
  static const GLuint kClientSideElementArrayId = 0xFEDCBA99u;

  // Number of swap buffers allowed before waiting.
  static const size_t kMaxSwapBuffers = 1;

  GLES2Implementation(
      GLES2CmdHelper* helper,
      size_t transfer_buffer_size,
      void* transfer_buffer,
      int32 transfer_buffer_id,
      bool share_resources);

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

  void BindBuffer(GLenum target, GLuint buffer);
  void DeleteBuffers(GLsizei n, const GLuint* buffers);
  void DisableVertexAttribArray(GLuint index);
  void DrawArrays(GLenum mode, GLint first, GLsizei count);
  void EnableVertexAttribArray(GLuint index);
  void GetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params);
  void GetVertexAttribiv(GLuint index, GLenum pname, GLint* params);

  GLuint MakeTextureId() {
    GLuint id;
    texture_id_handler_->MakeIds(0, 1, &id);
    return id;
  }

  void FreeTextureId(GLuint id) {
    texture_id_handler_->FreeIds(1, &id);
  }

 private:
  // Wraps RingBufferWrapper to provide aligned allocations.
  class AlignedRingBuffer : public RingBufferWrapper {
   public:
    AlignedRingBuffer(RingBuffer::Offset base_offset,
                      unsigned int size,
                      CommandBufferHelper *helper,
                      void *base)
        : RingBufferWrapper(base_offset, size, helper, base) {
    }

    static unsigned int RoundToAlignment(unsigned int size) {
      return (size + kAlignment - 1) & ~(kAlignment - 1);
    }

    // Overrriden from RingBufferWrapper
    void *Alloc(unsigned int size) {
      return RingBufferWrapper::Alloc(RoundToAlignment(size));
    }

    // Overrriden from RingBufferWrapper
    template <typename T> T *AllocTyped(unsigned int count) {
      return static_cast<T *>(Alloc(count * sizeof(T)));
    }
  };

  // Base class for mapped resources.
  struct MappedResource {
    MappedResource(GLenum _access, int _shm_id, void* mem, unsigned int offset)
        : access(_access),
          shm_id(_shm_id),
          shm_memory(mem),
          shm_offset(offset) {
    }

    // access mode. Currently only GL_WRITE_ONLY is valid
    GLenum access;

    // Shared memory ID for buffer.
    int shm_id;

    // Address of shared memory
    void* shm_memory;

    // Offset of shared memory
    unsigned int shm_offset;
  };

  // Used to track mapped textures.
  struct MappedTexture : public MappedResource {
    MappedTexture(
        GLenum access,
        int shm_id,
        void* shm_mem,
        unsigned int shm_offset,
        GLenum _target,
        GLint _level,
        GLint _xoffset,
        GLint _yoffset,
        GLsizei _width,
        GLsizei _height,
        GLenum _format,
        GLenum _type)
        : MappedResource(access, shm_id, shm_mem, shm_offset),
          target(_target),
          level(_level),
          xoffset(_xoffset),
          yoffset(_yoffset),
          width(_width),
          height(_height),
          format(_format),
          type(_type) {
    }

    // These match the arguments to TexSubImage2D.
    GLenum target;
    GLint level;
    GLint xoffset;
    GLint yoffset;
    GLsizei width;
    GLsizei height;
    GLenum format;
    GLenum type;
  };

  // Used to track mapped buffers.
  struct MappedBuffer : public MappedResource {
    MappedBuffer(
        GLenum access,
        int shm_id,
        void* shm_mem,
        unsigned int shm_offset,
        GLenum _target,
        GLintptr _offset,
        GLsizeiptr _size)
        : MappedResource(access, shm_id, shm_mem, shm_offset),
          target(_target),
          offset(_offset),
          size(_size) {
    }

    // These match the arguments to BufferSubData.
    GLenum target;
    GLintptr offset;
    GLsizeiptr size;
  };

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
  void SetGLError(GLenum error, const char* msg);

  // Returns the last error and clears it. Useful for debugging.
  const std::string& GetLastError() {
    return last_error_;
  }

  // Waits for all commands to execute.
  void WaitForCmd();

  // TODO(gman): These bucket functions really seem like they belong in
  // CommandBufferHelper (or maybe BucketHelper?). Unfortunately they need
  // a transfer buffer to function which is currently managed by this class.

  // Gets the contents of a bucket.
  void GetBucketContents(uint32 bucket_id, std::vector<int8>* data);

  // Sets the contents of a bucket.
  void SetBucketContents(uint32 bucket_id, const void* data, size_t size);

  // Sets the contents of a bucket as a string.
  void SetBucketAsCString(uint32 bucket_id, const char* str);

  // Gets the contents of a bucket as a string. Returns false if there is no
  // string available which is a separate case from the empty string.
  bool GetBucketAsString(uint32 bucket_id, std::string* str);

  // Sets the contents of a bucket as a string.
  void SetBucketAsString(uint32 bucket_id, const std::string& str);

  // Returns true if id is reserved.
  bool IsBufferReservedId(GLuint id);
  bool IsFramebufferReservedId(GLuint id) { return false;  }
  bool IsRenderbufferReservedId(GLuint id) { return false; }
  bool IsTextureReservedId(GLuint id) { return false; }

  // Helper for GetVertexAttrib
  bool GetVertexAttribHelper(GLuint index, GLenum pname, uint32* param);

  // Asks the service for the max index in an element array buffer.
  GLsizei GetMaxIndexInElementArrayBuffer(
      GLuint buffer_id, GLsizei count, GLenum type, GLuint offset);

  GLES2Util util_;
  GLES2CmdHelper* helper_;
  scoped_ptr<IdHandlerInterface> buffer_id_handler_;
  scoped_ptr<IdHandlerInterface> framebuffer_id_handler_;
  scoped_ptr<IdHandlerInterface> renderbuffer_id_handler_;
  scoped_ptr<IdHandlerInterface> program_and_shader_id_handler_;
  scoped_ptr<IdHandlerInterface> texture_id_handler_;
  AlignedRingBuffer transfer_buffer_;
  int transfer_buffer_id_;
  void* result_buffer_;
  uint32 result_shm_offset_;
  std::string last_error_;

  std::queue<int32> swap_buffers_tokens_;

  // pack alignment as last set by glPixelStorei
  GLint pack_alignment_;

  // unpack alignment as last set by glPixelStorei
  GLint unpack_alignment_;

  // The currently bound array buffer.
  GLuint bound_array_buffer_id_;

  // The currently bound element array buffer.
  GLuint bound_element_array_buffer_id_;

  // GL names for the buffers used to emulate client side buffers.
  GLuint client_side_array_id_;
  GLuint client_side_element_array_id_;

  // Info for each vertex attribute saved so we can simulate client side
  // buffers.
  scoped_ptr<ClientSideBufferHelper> client_side_buffer_helper_;

  GLuint reserved_ids_[2];

  // Current GL error bits.
  uint32 error_bits_;

  // Map of GLenum to Strings for glGetString.  We need to cache these because
  // the pointer passed back to the client has to remain valid for eternity.
  typedef std::map<uint32, std::set<std::string> > GLStringMap;
  GLStringMap gl_strings_;

  // Similar cache for glGetRequestableExtensionsCHROMIUM. We don't
  // have an enum for this so handle it separately.
  std::set<std::string> requestable_extensions_set_;

  typedef std::map<const void*, MappedBuffer> MappedBufferMap;
  MappedBufferMap mapped_buffers_;

  typedef std::map<const void*, MappedTexture> MappedTextureMap;
  MappedTextureMap mapped_textures_;

  scoped_ptr<MappedMemoryManager> mapped_memory_;

  DISALLOW_COPY_AND_ASSIGN(GLES2Implementation);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H_

