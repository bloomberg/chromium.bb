// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "../client/ref_counted.h"
#include "../client/gles2_cmd_helper.h"
#include "../client/query_tracker.h"
#include "../client/ring_buffer.h"
#include "../client/share_group.h"
#include "gles2_impl_export.h"

#if !defined(NDEBUG) && !defined(__native_client__) && !defined(GLES2_CONFORMANCE_TESTS)  // NOLINT
  #if defined(GLES2_INLINE_OPTIMIZATION)
    // TODO(gman): Replace with macros that work with inline optmization.
    #define GPU_CLIENT_SINGLE_THREAD_CHECK()
    #define GPU_CLIENT_LOG(args)
    #define GPU_CLIENT_LOG_CODE_BLOCK(code)
    #define GPU_CLIENT_DCHECK_CODE_BLOCK(code)
  #else
    #include "base/logging.h"
    #define GPU_CLIENT_SINGLE_THREAD_CHECK() SingleThreadChecker checker(this);
    #define GPU_CLIENT_LOG(args)  DLOG_IF(INFO, debug_) << args;
    #define GPU_CLIENT_LOG_CODE_BLOCK(code) code
    #define GPU_CLIENT_DCHECK_CODE_BLOCK(code) code
    #define GPU_CLIENT_DEBUG
  #endif
#else
  #define GPU_CLIENT_SINGLE_THREAD_CHECK()
  #define GPU_CLIENT_LOG(args)
  #define GPU_CLIENT_LOG_CODE_BLOCK(code)
  #define GPU_CLIENT_DCHECK_CODE_BLOCK(code)
#endif

// Check that destination pointers point to initialized memory.
// When the context is lost, calling GL function has no effect so if destination
// pointers point to initialized memory it can often lead to crash bugs. eg.
//
// GLsizei len;
// glGetShaderSource(shader, max_size, &len, buffer);
// std::string src(buffer, buffer + len);  // len can be uninitialized here!!!
//
// Because this check is not official GL this check happens only on Chrome code,
// not Pepper.
//
// If it was up to us we'd just always write to the destination but the OpenGL
// spec defines the behavior of OpenGL functions, not us. :-(
#if defined(__native_client__) || defined(GLES2_CONFORMANCE_TESTS)
  #define GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION_ASSERT(v)
  #define GPU_CLIENT_DCHECK(v)
#elif defined(GPU_DCHECK)
  #define GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION_ASSERT(v) GPU_DCHECK(v)
  #define GPU_CLIENT_DCHECK(v) GPU_DCHECK(v)
#elif defined(DCHECK)
  #define GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION_ASSERT(v) DCHECK(v)
  #define GPU_CLIENT_DCHECK(v) DCHECK(v)
#else
  #define GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION_ASSERT(v) ASSERT(v)
  #define GPU_CLIENT_DCHECK(v) ASSERT(v)
#endif

#define GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(type, ptr) \
    GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION_ASSERT(ptr && \
        (ptr[0] == static_cast<type>(0) || ptr[0] == static_cast<type>(-1)));

namespace gpu {

class MappedMemoryManager;
class ScopedTransferBufferPtr;
class TransferBufferInterface;

namespace gles2 {

class ClientSideBufferHelper;

// This class emulates GLES2 over command buffers. It can be used by a client
// program so that the program does not need deal with shared memory and command
// buffer management. See gl2_lib.h.  Note that there is a performance gain to
// be had by changing your code to use command buffers directly by using the
// GLES2CmdHelper but that entails changing your code to use and deal with
// shared memory and synchronization issues.
class GLES2_IMPL_EXPORT GLES2Implementation {
 public:
  class ErrorMessageCallback {
   public:
    virtual ~ErrorMessageCallback() { }
    virtual void OnErrorMessage(const char* msg, int id) = 0;
  };

  // Stores client side cached GL state.
  struct GLCachedState {
    struct IntState {
      IntState()
          : max_combined_texture_image_units(0),
            max_cube_map_texture_size(0),
            max_fragment_uniform_vectors(0),
            max_renderbuffer_size(0),
            max_texture_image_units(0),
            max_texture_size(0),
            max_varying_vectors(0),
            max_vertex_attribs(0),
            max_vertex_texture_image_units(0),
            max_vertex_uniform_vectors(0),
            num_compressed_texture_formats(0),
            num_shader_binary_formats(0) {
      }

      GLint max_combined_texture_image_units;
      GLint max_cube_map_texture_size;
      GLint max_fragment_uniform_vectors;
      GLint max_renderbuffer_size;
      GLint max_texture_image_units;
      GLint max_texture_size;
      GLint max_varying_vectors;
      GLint max_vertex_attribs;
      GLint max_vertex_texture_image_units;
      GLint max_vertex_uniform_vectors;
      GLint num_compressed_texture_formats;
      GLint num_shader_binary_formats;
    };
    struct EnableState {
      EnableState()
          : blend(false),
            cull_face(false),
            depth_test(false),
            dither(false),
            polygon_offset_fill(false),
            sample_alpha_to_coverage(false),
            sample_coverage(false),
            scissor_test(false),
            stencil_test(false) {
      }

      bool blend;
      bool cull_face;
      bool depth_test;
      bool dither;
      bool polygon_offset_fill;
      bool sample_alpha_to_coverage;
      bool sample_coverage;
      bool scissor_test;
      bool stencil_test;
    };

    IntState int_state;
    EnableState enable_state;
  };

  // The maxiumum result size from simple GL get commands.
  static const size_t kMaxSizeOfSimpleResult = 16 * sizeof(uint32);  // NOLINT.

  // used for testing only. If more things are reseved add them here.
  static const unsigned int kStartingOffset = kMaxSizeOfSimpleResult;

  // Size in bytes to issue async flush for transfer buffer.
  static const unsigned int kSizeToFlush = 256 * 1024;

  // The bucket used for results. Public for testing only.
  static const uint32 kResultBucketId = 1;

  // Alignment of allocations.
  static const unsigned int kAlignment = 4;

  // GL names for the buffers used to emulate client side buffers.
  static const GLuint kClientSideArrayId = 0xFEDCBA98u;
  static const GLuint kClientSideElementArrayId = 0xFEDCBA99u;

  // Number of swap buffers allowed before waiting.
  static const size_t kMaxSwapBuffers = 2;

  GLES2Implementation(
      GLES2CmdHelper* helper,
      ShareGroup* share_group,
      TransferBufferInterface* transfer_buffer,
      bool share_resources,
      bool bind_generates_resource);

  ~GLES2Implementation();

  bool Initialize(
      unsigned int starting_transfer_buffer_size,
      unsigned int min_transfer_buffer_size,
      unsigned int max_transfer_buffer_size);

  // The GLES2CmdHelper being used by this GLES2Implementation. You can use
  // this to issue cmds at a lower level for certain kinds of optimization.
  GLES2CmdHelper* helper() const {
    return helper_;
  }

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "../client/gles2_implementation_autogen.h"

  void DisableVertexAttribArray(GLuint index);
  void EnableVertexAttribArray(GLuint index);
  void GetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params);
  void GetVertexAttribiv(GLuint index, GLenum pname, GLint* params);

  void GetProgramInfoCHROMIUMHelper(GLuint program, std::vector<int8>* result);
  GLint GetAttribLocationHelper(GLuint program, const char* name);
  GLint GetUniformLocationHelper(GLuint program, const char* name);
  bool GetActiveAttribHelper(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name);
  bool GetActiveUniformHelper(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name);

  GLuint MakeTextureId();
  void FreeTextureId(GLuint id);

  void SetSharedMemoryChunkSizeMultiple(unsigned int multiple);

  void FreeUnusedSharedMemory();
  void FreeEverything();

  void SetErrorMessageCallback(ErrorMessageCallback* callback) {
    error_message_callback_ = callback;
  }

  ShareGroup* share_group() const {
    return share_group_.get();
  }

 private:
  friend class ClientSideBufferHelper;
  friend class GLES2ImplementationTest;

  // Used to track whether an extension is available
  enum ExtensionStatus {
      kAvailableExtensionStatus,
      kUnavailableExtensionStatus,
      kUnknownExtensionStatus
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

  struct TextureUnit {
    TextureUnit()
        : bound_texture_2d(0),
          bound_texture_cube_map(0) {
    }

    // texture currently bound to this unit's GL_TEXTURE_2D with glBindTexture
    GLuint bound_texture_2d;

    // texture currently bound to this unit's GL_TEXTURE_CUBE_MAP with
    // glBindTexture
    GLuint bound_texture_cube_map;
  };

  // Checks for single threaded access.
  class SingleThreadChecker {
   public:
    SingleThreadChecker(GLES2Implementation* gles2_implementation);
    ~SingleThreadChecker();

   private:
    GLES2Implementation* gles2_implementation_;
  };

  // Gets the value of the result.
  template <typename T>
  T GetResultAs() {
    return static_cast<T>(GetResultBuffer());
  }

  void* GetResultBuffer();
  int32 GetResultShmId();
  uint32 GetResultShmOffset();

  // Lazily determines if GL_ANGLE_pack_reverse_row_order is available
  bool IsAnglePackReverseRowOrderAvailable();

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
  bool GetBucketContents(uint32 bucket_id, std::vector<int8>* data);

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

  void BindBufferHelper(GLenum target, GLuint texture);
  void BindFramebufferHelper(GLenum target, GLuint texture);
  void BindRenderbufferHelper(GLenum target, GLuint texture);
  void BindTextureHelper(GLenum target, GLuint texture);

  void DeleteBuffersHelper(GLsizei n, const GLuint* buffers);
  void DeleteFramebuffersHelper(GLsizei n, const GLuint* framebuffers);
  void DeleteRenderbuffersHelper(GLsizei n, const GLuint* renderbuffers);
  void DeleteTexturesHelper(GLsizei n, const GLuint* textures);
  bool DeleteProgramHelper(GLuint program);
  bool DeleteShaderHelper(GLuint shader);
  void DeleteQueriesEXTHelper(GLsizei n, const GLuint* textures);

  void DeleteBuffersStub(GLsizei n, const GLuint* buffers);
  void DeleteFramebuffersStub(GLsizei n, const GLuint* framebuffers);
  void DeleteRenderbuffersStub(GLsizei n, const GLuint* renderbuffers);
  void DeleteTexturesStub(GLsizei n, const GLuint* textures);
  void DeleteProgramStub(GLsizei n, const GLuint* programs);
  void DeleteShaderStub(GLsizei n, const GLuint* shaders);
  // TODO(gman): Remove this as queries are not shared.
  void DeleteQueriesStub(GLsizei n, const GLuint* queries);

  void BufferDataHelper(
      GLenum target, GLsizeiptr size, const void* data, GLenum usage);
  void BufferSubDataHelper(
      GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
  void BufferSubDataHelperImpl(
      GLenum target, GLintptr offset, GLsizeiptr size, const void* data,
      ScopedTransferBufferPtr* buffer);

  // Helper for GetVertexAttrib
  bool GetVertexAttribHelper(GLuint index, GLenum pname, uint32* param);

  GLuint GetMaxValueInBufferCHROMIUMHelper(
      GLuint buffer_id, GLsizei count, GLenum type, GLuint offset);

  // The pixels pointer should already account for unpack skip rows and skip
  // pixels.
  void TexSubImage2DImpl(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLenum type, uint32 unpadded_row_size,
      const void* pixels, uint32 pixels_padded_row_size, GLboolean internal,
      ScopedTransferBufferPtr* buffer, uint32 buffer_padded_row_size);

  // Helpers for query functions.
  bool GetHelper(GLenum pname, GLint* params);
  bool GetBooleanvHelper(GLenum pname, GLboolean* params);
  bool GetBufferParameterivHelper(GLenum target, GLenum pname, GLint* params);
  bool GetFloatvHelper(GLenum pname, GLfloat* params);
  bool GetFramebufferAttachmentParameterivHelper(
      GLenum target, GLenum attachment, GLenum pname, GLint* params);
  bool GetIntegervHelper(GLenum pname, GLint* params);
  bool GetProgramivHelper(GLuint program, GLenum pname, GLint* params);
  bool GetRenderbufferParameterivHelper(
      GLenum target, GLenum pname, GLint* params);
  bool GetShaderivHelper(GLuint shader, GLenum pname, GLint* params);
  bool GetTexParameterfvHelper(GLenum target, GLenum pname, GLfloat* params);
  bool GetTexParameterivHelper(GLenum target, GLenum pname, GLint* params);
  const GLubyte* GetStringHelper(GLenum name);

  bool IsExtensionAvailable(const char* ext);

  // Caches certain capabilties state. Return true if cached.
  bool SetCapabilityState(GLenum cap, bool enabled);

  IdHandlerInterface* GetIdHandler(int id_namespace) const;

  GLES2Util util_;
  GLES2CmdHelper* helper_;
  TransferBufferInterface* transfer_buffer_;
  std::string last_error_;

  std::queue<int32> swap_buffers_tokens_;
  std::queue<int32> rate_limit_tokens_;

  ExtensionStatus angle_pack_reverse_row_order_status;

  GLCachedState gl_state_;

  // pack alignment as last set by glPixelStorei
  GLint pack_alignment_;

  // unpack alignment as last set by glPixelStorei
  GLint unpack_alignment_;

  // unpack yflip as last set by glPixelstorei
  bool unpack_flip_y_;

  // unpack row length as last set by glPixelStorei
  GLint unpack_row_length_;

  // unpack skip rows as last set by glPixelStorei
  GLint unpack_skip_rows_;

  // unpack skip pixels as last set by glPixelStorei
  GLint unpack_skip_pixels_;

  // pack reverse row order as last set by glPixelstorei
  bool pack_reverse_row_order_;

  scoped_array<TextureUnit> texture_units_;

  // 0 to gl_state_.max_combined_texture_image_units.
  GLuint active_texture_unit_;

  GLuint bound_framebuffer_;
  GLuint bound_renderbuffer_;

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

  // Whether or not to print debugging info.
  bool debug_;

  // Used to check for single threaded access.
  int use_count_;

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

  scoped_refptr<ShareGroup> share_group_;

  scoped_ptr<QueryTracker> query_tracker_;
  QueryTracker::Query* current_query_;

  ErrorMessageCallback* error_message_callback_;

  DISALLOW_COPY_AND_ASSIGN(GLES2Implementation);
};

inline bool GLES2Implementation::GetBufferParameterivHelper(
    GLenum /* target */, GLenum /* pname */, GLint* /* params */) {
  return false;
}

inline bool GLES2Implementation::GetFramebufferAttachmentParameterivHelper(
    GLenum /* target */,
    GLenum /* attachment */,
    GLenum /* pname */,
    GLint* /* params */) {
  return false;
}

inline bool GLES2Implementation::GetRenderbufferParameterivHelper(
    GLenum /* target */, GLenum /* pname */, GLint* /* params */) {
  return false;
}

inline bool GLES2Implementation::GetShaderivHelper(
    GLuint /* shader */, GLenum /* pname */, GLint* /* params */) {
  return false;
}

inline bool GLES2Implementation::GetTexParameterfvHelper(
    GLenum /* target */, GLenum /* pname */, GLfloat* /* params */) {
  return false;
}

inline bool GLES2Implementation::GetTexParameterivHelper(
    GLenum /* target */, GLenum /* pname */, GLint* /* params */) {
  return false;
}

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H_
