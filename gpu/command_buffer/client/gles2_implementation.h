// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/client/buffer_tracker.h"
#include "gpu/command_buffer/client/client_context_state.h"
#include "gpu/command_buffer/client/client_transfer_cache.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_impl_export.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/client/ref_counted.h"
#include "gpu/command_buffer/client/share_group.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"

#if DCHECK_IS_ON() && !defined(__native_client__) && \
    !defined(GLES2_CONFORMANCE_TESTS)
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

#if defined(GPU_CLIENT_DEBUG)
  // Set to 1 to have the client fail when a GL error is generated.
  // This helps find bugs in the renderer since the debugger stops on the error.
#  if 0
#    define GL_CLIENT_FAIL_GL_ERRORS
#  endif
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

#define GPU_CLIENT_VALIDATE_DESTINATION_OPTIONAL_INITALIZATION(type, ptr) \
    GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION_ASSERT(!ptr || \
        (ptr[0] == static_cast<type>(0) || ptr[0] == static_cast<type>(-1)));

namespace gpu {

class GpuControl;
class IdAllocator;
struct SharedMemoryLimits;

namespace gles2 {

class GLES2CmdHelper;
class VertexArrayObjectManager;
class QueryTracker;

// This class emulates GLES2 over command buffers. It can be used by a client
// program so that the program does not need deal with shared memory and command
// buffer management. See gl2_lib.h.  Note that there is a performance gain to
// be had by changing your code to use command buffers directly by using the
// GLES2CmdHelper but that entails changing your code to use and deal with
// shared memory and synchronization issues.
class GLES2_IMPL_EXPORT GLES2Implementation
    : public GLES2Interface,
      public ContextSupport,
      public GpuControlClient,
      public base::trace_event::MemoryDumpProvider {
 public:
  // Stores GL state that never changes.
  struct GLES2_IMPL_EXPORT GLStaticState {
    GLStaticState();
    ~GLStaticState();

    typedef std::pair<GLenum, GLenum> ShaderPrecisionKey;
    typedef std::map<ShaderPrecisionKey,
                     cmds::GetShaderPrecisionFormat::Result>
        ShaderPrecisionMap;
    ShaderPrecisionMap shader_precisions;
  };

  // The maximum result size from simple GL get commands.
  static const size_t kMaxSizeOfSimpleResult =
      16 * sizeof(uint32_t);  // NOLINT.

  // used for testing only. If more things are reseved add them here.
  static const unsigned int kStartingOffset = kMaxSizeOfSimpleResult;

  // Size in bytes to issue async flush for transfer buffer.
  static const unsigned int kSizeToFlush = 256 * 1024;

  // The bucket used for results. Public for testing only.
  static const uint32_t kResultBucketId = 1;

  // Alignment of allocations.
  static const unsigned int kAlignment = 16;

  // GL names for the buffers used to emulate client side buffers.
  static const GLuint kClientSideArrayId = 0xFEDCBA98u;
  static const GLuint kClientSideElementArrayId = 0xFEDCBA99u;

  // Number of swap buffers allowed before waiting.
  static const size_t kMaxSwapBuffers = 2;

  GLES2Implementation(GLES2CmdHelper* helper,
                      scoped_refptr<ShareGroup> share_group,
                      TransferBufferInterface* transfer_buffer,
                      bool bind_generates_resource,
                      bool lose_context_when_out_of_memory,
                      bool support_client_side_arrays,
                      GpuControl* gpu_control);

  ~GLES2Implementation() override;

  gpu::ContextResult Initialize(const SharedMemoryLimits& limits);

  // The GLES2CmdHelper being used by this GLES2Implementation. You can use
  // this to issue cmds at a lower level for certain kinds of optimization.
  GLES2CmdHelper* helper() const;

  // Gets client side generated errors.
  GLenum GetClientSideGLError();

  // GLES2Interface implementation
  void FreeSharedMemory(void*) override;

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gpu/command_buffer/client/gles2_implementation_autogen.h"

  // ContextSupport implementation.
  void FlushPendingWork() override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       base::OnceClosure callback) override;
  bool IsSyncTokenSignaled(const gpu::SyncToken& sync_token) override;
  void SignalQuery(uint32_t query, base::OnceClosure callback) override;
  void GetGpuFence(uint32_t gpu_fence_id,
                   base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                       callback) override;
  void SetAggressivelyFreeResources(bool aggressively_free_resources) override;
  void Swap() override;
  void SwapWithBounds(const std::vector<gfx::Rect>& rects) override;
  void PartialSwapBuffers(const gfx::Rect& sub_buffer) override;
  void CommitOverlayPlanes() override;
  void ScheduleOverlayPlane(int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            unsigned overlay_texture_id,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& uv_rect) override;
  uint64_t ShareGroupTracingGUID() const override;
  void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t)> callback) override;
  void SetSnapshotRequested() override;
  bool ThreadSafeShallowLockDiscardableTexture(uint32_t texture_id) override;
  void CompleteLockDiscardableTexureOnContextThread(
      uint32_t texture_id) override;
  bool ThreadsafeDiscardableTextureIsDeletedForTracing(
      uint32_t texture_id) override;
  void CreateTransferCacheEntry(
      const cc::ClientTransferCacheEntry& entry) override;
  bool ThreadsafeLockTransferCacheEntry(cc::TransferCacheEntryType type,
                                        uint32_t id) override;
  void UnlockTransferCacheEntries(
      const std::vector<std::pair<cc::TransferCacheEntryType, uint32_t>>&
          entries) override;
  void DeleteTransferCacheEntry(cc::TransferCacheEntryType type,
                                uint32_t id) override;
  unsigned int GetTransferBufferFreeSize() const override;

  // TODO(danakj): Move to ContextSupport once ContextProvider doesn't need to
  // intercept it.
  void SetLostContextCallback(const base::Closure& callback);

  void GetProgramInfoCHROMIUMHelper(GLuint program,
                                    std::vector<int8_t>* result);
  GLint GetAttribLocationHelper(GLuint program, const char* name);
  GLint GetUniformLocationHelper(GLuint program, const char* name);
  GLint GetFragDataIndexEXTHelper(GLuint program, const char* name);
  GLint GetFragDataLocationHelper(GLuint program, const char* name);
  bool GetActiveAttribHelper(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name);
  bool GetActiveUniformHelper(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name);
  void GetUniformBlocksCHROMIUMHelper(GLuint program,
                                      std::vector<int8_t>* result);
  void GetUniformsES3CHROMIUMHelper(GLuint program,
                                    std::vector<int8_t>* result);
  GLuint GetUniformBlockIndexHelper(GLuint program, const char* name);
  bool GetActiveUniformBlockNameHelper(
      GLuint program, GLuint index, GLsizei bufsize,
      GLsizei* length, char* name);
  bool GetActiveUniformBlockivHelper(
      GLuint program, GLuint index, GLenum pname, GLint* params);
  void GetTransformFeedbackVaryingsCHROMIUMHelper(GLuint program,
                                                  std::vector<int8_t>* result);
  bool GetTransformFeedbackVaryingHelper(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name);
  bool GetUniformIndicesHelper(
      GLuint program, GLsizei count, const char* const* names, GLuint* indices);
  bool GetActiveUniformsivHelper(
      GLuint program, GLsizei count, const GLuint* indices,
      GLenum pname, GLint* params);
  bool GetSyncivHelper(
      GLsync sync, GLenum pname, GLsizei bufsize, GLsizei* length,
      GLint* values);
  bool GetQueryObjectValueHelper(
      const char* function_name, GLuint id, GLenum pname, GLuint64* params);

  void FreeUnusedSharedMemory();
  void FreeEverything();

  // Helper to set verified bit on sync token if allowed by gpu control.
  bool GetVerifiedSyncTokenForIPC(const gpu::SyncToken& sync_token,
                                  gpu::SyncToken* verified_sync_token);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  const scoped_refptr<ShareGroup>& share_group() const { return share_group_; }

  const Capabilities& capabilities() const {
    return capabilities_;
  }

  GpuControl* gpu_control() {
    return gpu_control_;
  }

  ShareGroupContextData* share_group_context_data() {
    return &share_group_context_data_;
  }

 private:
  friend class GLES2ImplementationTest;
  friend class VertexArrayObjectManager;
  friend class QueryTracker;

  using IdNamespaces = id_namespaces::IdNamespaces;

  // Used to track whether an extension is available
  enum ExtensionStatus {
      kAvailableExtensionStatus,
      kUnavailableExtensionStatus,
      kUnknownExtensionStatus
  };

  enum Dimension {
      k2D,
      k3D,
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
          bound_texture_cube_map(0),
          bound_texture_external_oes(0) {}

    // texture currently bound to this unit's GL_TEXTURE_2D with glBindTexture
    GLuint bound_texture_2d;

    // texture currently bound to this unit's GL_TEXTURE_CUBE_MAP with
    // glBindTexture
    GLuint bound_texture_cube_map;

    // texture currently bound to this unit's GL_TEXTURE_EXTERNAL_OES with
    // glBindTexture
    GLuint bound_texture_external_oes;
  };

  // Checks for single threaded access.
  class SingleThreadChecker {
   public:
    explicit SingleThreadChecker(GLES2Implementation* gles2_implementation);
    ~SingleThreadChecker();

   private:
    GLES2Implementation* gles2_implementation_;
  };

  // Gets the value of the result.
  template <typename T>
  T GetResultAs() {
    return static_cast<T>(GetResultBuffer());
  }

  // GpuControlClient implementation.
  void OnGpuControlLostContext() final;
  void OnGpuControlLostContextMaybeReentrant() final;
  void OnGpuControlErrorMessage(const char* message, int32_t id) final;

  void* GetResultBuffer();
  int32_t GetResultShmId();
  uint32_t GetResultShmOffset();

  bool IsChromiumFramebufferMultisampleAvailable();

  bool IsExtensionAvailableHelper(
      const char* extension, ExtensionStatus* status);

  // Gets the GLError through our wrapper.
  GLenum GetGLError();

  // Sets our wrapper for the GLError.
  void SetGLError(GLenum error, const char* function_name, const char* msg);
  void SetGLErrorInvalidEnum(
      const char* function_name, GLenum value, const char* label);

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
  bool GetBucketContents(uint32_t bucket_id, std::vector<int8_t>* data);

  // Sets the contents of a bucket.
  void SetBucketContents(uint32_t bucket_id, const void* data, size_t size);

  // Sets the contents of a bucket as a string.
  void SetBucketAsCString(uint32_t bucket_id, const char* str);

  // Gets the contents of a bucket as a string. Returns false if there is no
  // string available which is a separate case from the empty string.
  bool GetBucketAsString(uint32_t bucket_id, std::string* str);

  // Sets the contents of a bucket as a string.
  void SetBucketAsString(uint32_t bucket_id, const std::string& str);

  // Returns true if id is reserved.
  bool IsBufferReservedId(GLuint id);
  bool IsFramebufferReservedId(GLuint id) { return false; }
  bool IsRenderbufferReservedId(GLuint id) { return false; }
  bool IsTextureReservedId(GLuint id) { return false; }
  bool IsVertexArrayReservedId(GLuint id) { return false; }
  bool IsProgramReservedId(GLuint id) { return false; }
  bool IsSamplerReservedId(GLuint id) { return false; }
  bool IsTransformFeedbackReservedId(GLuint id) { return false; }

  void BindBufferHelper(GLenum target, GLuint buffer);
  void BindBufferBaseHelper(GLenum target, GLuint index, GLuint buffer);
  void BindBufferRangeHelper(GLenum target, GLuint index, GLuint buffer,
                             GLintptr offset, GLsizeiptr size);
  void BindFramebufferHelper(GLenum target, GLuint framebuffer);
  void BindRenderbufferHelper(GLenum target, GLuint renderbuffer);
  void BindSamplerHelper(GLuint unit, GLuint sampler);
  void BindTextureHelper(GLenum target, GLuint texture);
  void BindTransformFeedbackHelper(GLenum target, GLuint transformfeedback);
  void BindVertexArrayOESHelper(GLuint array);
  void UseProgramHelper(GLuint program);

  void BindBufferStub(GLenum target, GLuint buffer);
  void BindBufferBaseStub(GLenum target, GLuint index, GLuint buffer);
  void BindBufferRangeStub(GLenum target, GLuint index, GLuint buffer,
                           GLintptr offset, GLsizeiptr size);
  void BindRenderbufferStub(GLenum target, GLuint renderbuffer);
  void BindTextureStub(GLenum target, GLuint texture);

  void GenBuffersHelper(GLsizei n, const GLuint* buffers);
  void GenFramebuffersHelper(GLsizei n, const GLuint* framebuffers);
  void GenRenderbuffersHelper(GLsizei n, const GLuint* renderbuffers);
  void GenTexturesHelper(GLsizei n, const GLuint* textures);
  void GenVertexArraysOESHelper(GLsizei n, const GLuint* arrays);
  void GenQueriesEXTHelper(GLsizei n, const GLuint* queries);
  void GenSamplersHelper(GLsizei n, const GLuint* samplers);
  void GenTransformFeedbacksHelper(GLsizei n, const GLuint* transformfeedbacks);

  void DeleteBuffersHelper(GLsizei n, const GLuint* buffers);
  void DeleteFramebuffersHelper(GLsizei n, const GLuint* framebuffers);
  void DeleteRenderbuffersHelper(GLsizei n, const GLuint* renderbuffers);
  void DeleteTexturesHelper(GLsizei n, const GLuint* textures);
  bool DeleteProgramHelper(GLuint program);
  bool DeleteShaderHelper(GLuint shader);
  void DeleteQueriesEXTHelper(GLsizei n, const GLuint* queries);
  void DeleteVertexArraysOESHelper(GLsizei n, const GLuint* arrays);
  void DeleteSamplersHelper(GLsizei n, const GLuint* samplers);
  void DeleteTransformFeedbacksHelper(
      GLsizei n, const GLuint* transformfeedbacks);
  void DeleteSyncHelper(GLsync sync);

  void DeleteBuffersStub(GLsizei n, const GLuint* buffers);
  void DeleteRenderbuffersStub(GLsizei n, const GLuint* renderbuffers);
  void DeleteTexturesStub(GLsizei n, const GLuint* textures);
  void DeletePathsCHROMIUMStub(GLuint first_client_id, GLsizei range);
  void DeleteProgramStub(GLsizei n, const GLuint* programs);
  void DeleteShaderStub(GLsizei n, const GLuint* shaders);
  void DeleteSamplersStub(GLsizei n, const GLuint* samplers);
  void DeleteSyncStub(GLsizei n, const GLuint* syncs);
  void DestroyGpuFenceCHROMIUMHelper(GLuint client_id);

  void BufferDataHelper(
      GLenum target, GLsizeiptr size, const void* data, GLenum usage);
  void BufferSubDataHelper(
      GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
  void BufferSubDataHelperImpl(
      GLenum target, GLintptr offset, GLsizeiptr size, const void* data,
      ScopedTransferBufferPtr* buffer);

  GLuint CreateImageCHROMIUMHelper(ClientBuffer buffer,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum internalformat);
  void DestroyImageCHROMIUMHelper(GLuint image_id);

  // Helper for GetVertexAttrib
  bool GetVertexAttribHelper(GLuint index, GLenum pname, uint32_t* param);

  GLuint GetMaxValueInBufferCHROMIUMHelper(
      GLuint buffer_id, GLsizei count, GLenum type, GLuint offset);

  void WaitAllAsyncTexImage2DCHROMIUMHelper();

  void RestoreElementAndArrayBuffers(bool restore);
  void RestoreArrayBuffer(bool restrore);

  // The pixels pointer should already account for unpack skip
  // images/rows/pixels.
  void TexSubImage2DImpl(GLenum target,
                         GLint level,
                         GLint xoffset,
                         GLint yoffset,
                         GLsizei width,
                         GLsizei height,
                         GLenum format,
                         GLenum type,
                         uint32_t unpadded_row_size,
                         const void* pixels,
                         uint32_t pixels_padded_row_size,
                         GLboolean internal,
                         ScopedTransferBufferPtr* buffer,
                         uint32_t buffer_padded_row_size);
  void TexSubImage3DImpl(GLenum target,
                         GLint level,
                         GLint xoffset,
                         GLint yoffset,
                         GLint zoffset,
                         GLsizei width,
                         GLsizei height,
                         GLsizei depth,
                         GLenum format,
                         GLenum type,
                         uint32_t unpadded_row_size,
                         const void* pixels,
                         uint32_t pixels_padded_row_size,
                         GLboolean internal,
                         ScopedTransferBufferPtr* buffer,
                         uint32_t buffer_padded_row_size);

  // Helpers for query functions.
  bool GetHelper(GLenum pname, GLint* params);
  GLuint GetBoundBufferHelper(GLenum target);
  bool GetBooleanvHelper(GLenum pname, GLboolean* params);
  bool GetBufferParameteri64vHelper(
      GLenum target, GLenum pname, GLint64* params);
  bool GetBufferParameterivHelper(GLenum target, GLenum pname, GLint* params);
  bool GetFloatvHelper(GLenum pname, GLfloat* params);
  bool GetFramebufferAttachmentParameterivHelper(
      GLenum target, GLenum attachment, GLenum pname, GLint* params);
  bool GetInteger64vHelper(GLenum pname, GLint64* params);
  bool GetIntegervHelper(GLenum pname, GLint* params);
  bool GetIntegeri_vHelper(GLenum pname, GLuint index, GLint* data);
  bool GetInteger64i_vHelper(GLenum pname, GLuint index, GLint64* data);
  bool GetInternalformativHelper(
      GLenum target, GLenum format, GLenum pname, GLsizei bufSize,
      GLint* params);
  bool GetProgramivHelper(GLuint program, GLenum pname, GLint* params);
  bool GetSamplerParameterfvHelper(
      GLuint sampler, GLenum pname, GLfloat* params);
  bool GetSamplerParameterivHelper(
      GLuint sampler, GLenum pname, GLint* params);
  bool GetRenderbufferParameterivHelper(
      GLenum target, GLenum pname, GLint* params);
  bool GetShaderivHelper(GLuint shader, GLenum pname, GLint* params);
  bool GetTexParameterfvHelper(GLenum target, GLenum pname, GLfloat* params);
  bool GetTexParameterivHelper(GLenum target, GLenum pname, GLint* params);
  const GLubyte* GetStringHelper(GLenum name);

  bool IsExtensionAvailable(const char* ext);

  // Caches certain capabilties state. Return true if cached.
  bool SetCapabilityState(GLenum cap, bool enabled);

  IdHandlerInterface* GetIdHandler(SharedIdNamespaces id_namespace) const;
  RangeIdHandlerInterface* GetRangeIdHandler(int id_namespace) const;
  // IdAllocators for objects that can't be shared among contexts.
  IdAllocator* GetIdAllocator(IdNamespaces id_namespace) const;

  void FinishHelper();
  void FlushHelper();

  void RunIfContextNotLost(base::OnceClosure callback);

  // Validate if an offset is valid, i.e., non-negative and fit into 32-bit.
  // If not, generate an approriate error, and return false.
  bool ValidateOffset(const char* func, GLintptr offset);

  // Validate if a size is valid, i.e., non-negative and fit into 32-bit.
  // If not, generate an approriate error, and return false.
  bool ValidateSize(const char* func, GLsizeiptr offset);

  // Remove the transfer buffer from the buffer tracker. For buffers used
  // asynchronously the memory is free:ed if the upload has completed. For
  // other buffers, the memory is either free:ed immediately or free:ed pending
  // a token.
  void RemoveTransferBuffer(BufferTracker::Buffer* buffer);

  bool GetBoundPixelTransferBuffer(
      GLenum target, const char* function_name, GLuint* buffer_id);
  BufferTracker::Buffer* GetBoundPixelTransferBufferIfValid(
      GLuint buffer_id,
      const char* function_name, GLuint offset, GLsizei size);

  // Pack 2D arrays of char into a bucket.
  // Helper function for ShaderSource(), TransformFeedbackVaryings(), etc.
  bool PackStringsToBucket(GLsizei count,
                           const char* const* str,
                           const GLint* length,
                           const char* func_name);

  const std::string& GetLogPrefix() const;

  bool PrepareInstancedPathCommand(const char* function_name,
                                   GLsizei num_paths,
                                   GLenum path_name_type,
                                   const void* paths,
                                   GLenum transform_type,
                                   const GLfloat* transform_values,
                                   ScopedTransferBufferPtr* buffer,
                                   uint32_t* out_paths_shm_id,
                                   size_t* out_paths_offset,
                                   uint32_t* out_transforms_shm_id,
                                   size_t* out_transforms_offset);
#if defined(GL_CLIENT_FAIL_GL_ERRORS)
  void CheckGLError();
  void FailGLError(GLenum error);
#else
  void CheckGLError() { }
  void FailGLError(GLenum /* error */) { }
#endif

  void RemoveMappedBufferRangeByTarget(GLenum target);
  void RemoveMappedBufferRangeById(GLuint buffer);
  void ClearMappedBufferRangeMap();

  void DrawElementsImpl(GLenum mode, GLsizei count, GLenum type,
                        const void* indices, const char* func_name);
  void UpdateCachedExtensionsIfNeeded();
  void InvalidateCachedExtensions();

  PixelStoreParams GetUnpackParameters(Dimension dimension);

  GLES2Util util_;
  GLES2CmdHelper* helper_;
  TransferBufferInterface* transfer_buffer_;
  std::string last_error_;
  DebugMarkerManager debug_marker_manager_;
  std::string this_in_hex_;

  base::queue<int32_t> swap_buffers_tokens_;
  base::queue<int32_t> rate_limit_tokens_;

  ExtensionStatus chromium_framebuffer_multisample_;

  GLStaticState static_state_;
  ClientContextState state_;

  // pack alignment as last set by glPixelStorei
  GLint pack_alignment_;

  // pack row length as last set by glPixelStorei
  GLint pack_row_length_;

  // pack skip pixels as last set by glPixelStorei
  GLint pack_skip_pixels_;

  // pack skip rows as last set by glPixelStorei
  GLint pack_skip_rows_;

  // unpack alignment as last set by glPixelStorei
  GLint unpack_alignment_;

  // unpack row length as last set by glPixelStorei
  GLint unpack_row_length_;

  // unpack image height as last set by glPixelStorei
  GLint unpack_image_height_;

  // unpack skip rows as last set by glPixelStorei
  GLint unpack_skip_rows_;

  // unpack skip pixels as last set by glPixelStorei
  GLint unpack_skip_pixels_;

  // unpack skip images as last set by glPixelStorei
  GLint unpack_skip_images_;

  std::unique_ptr<TextureUnit[]> texture_units_;

  // 0 to gl_state_.max_combined_texture_image_units.
  GLuint active_texture_unit_;

  GLuint bound_framebuffer_;
  GLuint bound_read_framebuffer_;
  GLuint bound_renderbuffer_;

  // The program in use by glUseProgram
  GLuint current_program_;

  GLuint bound_array_buffer_;
  GLuint bound_copy_read_buffer_;
  GLuint bound_copy_write_buffer_;
  GLuint bound_pixel_pack_buffer_;
  GLuint bound_pixel_unpack_buffer_;
  GLuint bound_transform_feedback_buffer_;
  GLuint bound_uniform_buffer_;

  // The currently bound pixel transfer buffers.
  GLuint bound_pixel_pack_transfer_buffer_id_;
  GLuint bound_pixel_unpack_transfer_buffer_id_;

  // Client side management for vertex array objects. Needed to correctly
  // track client side arrays.
  std::unique_ptr<VertexArrayObjectManager> vertex_array_object_manager_;

  GLuint reserved_ids_[2];

  // Current GL error bits.
  uint32_t error_bits_;

  // Whether or not to print debugging info.
  bool debug_;

  // When true, the context is lost when a GL_OUT_OF_MEMORY error occurs.
  const bool lose_context_when_out_of_memory_;

  // Whether or not to support client side arrays.
  const bool support_client_side_arrays_;

  // Used to check for single threaded access.
  int use_count_;

  // Changed every time a flush or finish occurs.
  uint32_t flush_id_;

  // Avoid recycling of client-allocated GpuFence IDs by saving the
  // last-allocated one and requesting the next one to be higher than that.
  // This will wrap around as needed, but the space should be plenty big enough
  // to avoid collisions.
  uint32_t last_gpu_fence_id_ = 0;

  // Maximum amount of extra memory from the mapped memory pool to use when
  // needing to transfer something exceeding the default transfer buffer.
  uint32_t max_extra_transfer_buffer_size_;

  // Set of strings returned from glGetString.  We need to cache these because
  // the pointer passed back to the client has to remain valid for eternity.
  std::set<std::string> gl_strings_;

  typedef std::map<const void*, MappedBuffer> MappedBufferMap;
  MappedBufferMap mapped_buffers_;

  // TODO(zmo): Consolidate |mapped_buffers_| and |mapped_buffer_range_map_|.
  typedef base::hash_map<GLuint, MappedBuffer> MappedBufferRangeMap;
  MappedBufferRangeMap mapped_buffer_range_map_;

  typedef std::map<const void*, MappedTexture> MappedTextureMap;
  MappedTextureMap mapped_textures_;

  std::unique_ptr<MappedMemoryManager> mapped_memory_;

  scoped_refptr<ShareGroup> share_group_;
  ShareGroupContextData share_group_context_data_;

  std::unique_ptr<QueryTracker> query_tracker_;
  std::unique_ptr<IdAllocator>
      id_allocators_[static_cast<int>(IdNamespaces::kNumIdNamespaces)];

  std::unique_ptr<BufferTracker> buffer_tracker_;
  ClientTransferCache transfer_cache_;

  base::Optional<ScopedTransferBufferPtr> raster_mapped_buffer_;

  base::Callback<void(const char*, int32_t)> error_message_callback_;
  base::Closure lost_context_callback_;
  bool lost_context_callback_run_ = false;

  int current_trace_stack_;

  GpuControl* const gpu_control_;

  Capabilities capabilities_;

  // Flag to indicate whether the implementation can retain resources, or
  // whether it should aggressively free them.
  bool aggressively_free_resources_;

  // Result of last GetString(GL_EXTENSIONS), used to keep
  // GetString(GL_EXTENSIONS), GetStringi(GL_EXTENSIONS, index) and
  // GetIntegerv(GL_NUM_EXTENSIONS) in sync. This points to gl_strings, valid
  // forever.
  const char* cached_extension_string_;

  // Populated if cached_extension_string_ != nullptr. These point to
  // gl_strings, valid forever.
  std::vector<const char*> cached_extensions_;

  base::WeakPtrFactory<GLES2Implementation> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GLES2Implementation);
};

inline bool GLES2Implementation::GetBufferParameteri64vHelper(
    GLenum /* target */, GLenum /* pname */, GLint64* /* params */) {
  return false;
}

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
