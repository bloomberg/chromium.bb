// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/client/client_discardable_texture_manager.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/implementation_base.h"
#include "gpu/command_buffer/client/logging.h"
#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/raster_export.h"

namespace gpu {

class GpuControl;
struct SharedMemoryLimits;

namespace raster {

class RasterCmdHelper;

// This class emulates Raster over command buffers. It can be used by a client
// program so that the program does not need deal with shared memory and command
// buffer management.
class RASTER_EXPORT RasterImplementation : public RasterInterface,
                                           public ImplementationBase,
                                           public gles2::QueryTrackerClient {
 public:
  RasterImplementation(RasterCmdHelper* helper,
                       TransferBufferInterface* transfer_buffer,
                       bool bind_generates_resource,
                       bool lose_context_when_out_of_memory,
                       GpuControl* gpu_control);

  ~RasterImplementation() override;

  gpu::ContextResult Initialize(const SharedMemoryLimits& limits);

  // The RasterCmdHelper being used by this RasterImplementation. You can use
  // this to issue cmds at a lower level for certain kinds of optimization.
  RasterCmdHelper* helper() const;

  // QueryTrackerClient implementation.
  void IssueBeginQuery(GLenum target,
                       GLuint id,
                       uint32_t sync_data_shm_id,
                       uint32_t sync_data_shm_offset) override;
  void IssueEndQuery(GLenum target, GLuint submit_count) override;
  void IssueQueryCounter(GLuint id,
                         GLenum target,
                         uint32_t sync_data_shm_id,
                         uint32_t sync_data_shm_offset,
                         GLuint submit_count) override;
  void IssueSetDisjointValueSync(uint32_t sync_data_shm_id,
                                 uint32_t sync_data_shm_offset) override;
  GLenum GetClientSideGLError() override;
  CommandBufferHelper* cmd_buffer_helper() override;
  void SetGLError(GLenum error,
                  const char* function_name,
                  const char* msg) override;

  // ClientTransferCache::Client implementation.
  void IssueCreateTransferCacheEntry(GLuint entry_type,
                                     GLuint entry_id,
                                     GLuint handle_shm_id,
                                     GLuint handle_shm_offset,
                                     GLuint data_shm_id,
                                     GLuint data_shm_offset,
                                     GLuint data_size) override;
  void IssueDeleteTransferCacheEntry(GLuint entry_type,
                                     GLuint entry_id) override;
  void IssueUnlockTransferCacheEntry(GLuint entry_type,
                                     GLuint entry_id) override;
  CommandBuffer* command_buffer() const override;

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/raster_implementation_autogen.h"

  // RasterInterface implementation.
  void GenTextures(GLsizei n, GLuint* textures) override;
  void BindTexture(GLenum target, GLuint texture) override;
  void ActiveTexture(GLenum texture) override;
  void GenerateMipmap(GLenum target) override;
  void SetColorSpaceMetadataCHROMIUM(GLuint texture_id,
                                     GLColorSpace color_space) override;
  void GenMailboxCHROMIUM(GLbyte* mailbox) override;
  void ProduceTextureDirectCHROMIUM(GLuint texture,
                                    const GLbyte* mailbox) override;
  GLuint CreateAndConsumeTextureCHROMIUM(const GLbyte* mailbox) override;
  void BindTexImage2DCHROMIUM(GLenum target, GLint imageId) override;
  void ReleaseTexImage2DCHROMIUM(GLenum target, GLint imageId) override;
  void TexImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const void* pixels) override;
  void TexSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     const void* pixels) override;
  void CompressedTexImage2D(GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height,
                            GLint border,
                            GLsizei imageSize,
                            const void* data) override;
  void TexStorageForRaster(GLenum target,
                           viz::ResourceFormat format,
                           GLsizei width,
                           GLsizei height,
                           RasterTexStorageFlags flags) override;
  void CopySubTextureCHROMIUM(GLuint source_id,
                              GLint source_level,
                              GLenum dest_target,
                              GLuint dest_id,
                              GLint dest_level,
                              GLint xoffset,
                              GLint yoffset,
                              GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height,
                              GLboolean unpack_flip_y,
                              GLboolean unpack_premultiply_alpha,
                              GLboolean unpack_unmultiply_alpha) override;
  void BeginRasterCHROMIUM(
      GLuint texture_id,
      GLuint sk_color,
      GLuint msaa_sample_count,
      GLboolean can_use_lcd_text,
      GLboolean use_distance_field_text,
      GLint pixel_config,
      const cc::RasterColorSpace& raster_color_space) override;
  void RasterCHROMIUM(const cc::DisplayItemList* list,
                      cc::ImageProvider* provider,
                      const gfx::Size& content_size,
                      const gfx::Rect& full_raster_rect,
                      const gfx::Rect& playback_rect,
                      const gfx::Vector2dF& post_translate,
                      GLfloat post_scale,
                      bool requires_clear) override;
  void BeginGpuRaster() override;
  void EndGpuRaster() override;

  // ContextSupport implementation.
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

  bool GetQueryObjectValueHelper(const char* function_name,
                                 GLuint id,
                                 GLenum pname,
                                 GLuint64* params);

 private:
  friend class RasterImplementationTest;

  using IdNamespaces = gles2::id_namespaces::IdNamespaces;

  struct TextureUnit {
    TextureUnit() : bound_texture_2d(0) {}
    // texture currently bound to this unit's GL_TEXTURE_2D with glBindTexture
    GLuint bound_texture_2d;
  };

  // Checks for single threaded access.
  class SingleThreadChecker {
   public:
    explicit SingleThreadChecker(RasterImplementation* raster_implementation);
    ~SingleThreadChecker();

   private:
    RasterImplementation* raster_implementation_;
  };

  // ImplementationBase implementation.
  void IssueShallowFlush() override;

  // GpuControlClient implementation.
  void OnGpuControlLostContext() final;
  void OnGpuControlLostContextMaybeReentrant() final;
  void OnGpuControlErrorMessage(const char* message, int32_t id) final;

  // Gets the GLError through our wrapper.
  GLenum GetGLError();

  // Sets our wrapper for the GLError.
  void SetGLErrorInvalidEnum(const char* function_name,
                             GLenum value,
                             const char* label);

  // Returns the last error and clears it. Useful for debugging.
  const std::string& GetLastError() { return last_error_; }

  void GenQueriesEXTHelper(GLsizei n, const GLuint* queries);

  void DeleteTexturesHelper(GLsizei n, const GLuint* textures);
  void UnbindTexturesHelper(GLsizei n, const GLuint* textures);
  void DeleteQueriesEXTHelper(GLsizei n, const GLuint* queries);

  GLuint CreateImageCHROMIUMHelper(ClientBuffer buffer,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum internalformat);
  void DestroyImageCHROMIUMHelper(GLuint image_id);

  // Helpers for query functions.
  bool GetIntegervHelper(GLenum pname, GLint* params);
  bool GetTexParameterivHelper(GLenum target, GLenum pname, GLint* params);

  // IdAllocators for objects that can't be shared among contexts.
  IdAllocator* GetIdAllocator(IdNamespaces id_namespace);

  void FinishHelper();
  void FlushHelper();

  void RunIfContextNotLost(base::OnceClosure callback);

  const std::string& GetLogPrefix() const;

// Set to 1 to have the client fail when a GL error is generated.
// This helps find bugs in the renderer since the debugger stops on the error.
#if DCHECK_IS_ON()
#if 0
#define RASTER_CLIENT_FAIL_GL_ERRORS
#endif
#endif

#if defined(RASTER_CLIENT_FAIL_GL_ERRORS)
  void CheckGLError();
  void FailGLError(GLenum error);
#else
  void CheckGLError() {}
  void FailGLError(GLenum /* error */) {}
#endif

  void* MapRasterCHROMIUM(GLsizeiptr size);
  void UnmapRasterCHROMIUM(GLsizeiptr written_size);

  RasterCmdHelper* helper_;
  std::string last_error_;
  gles2::DebugMarkerManager debug_marker_manager_;
  std::string this_in_hex_;

  std::unique_ptr<TextureUnit[]> texture_units_;

  // 0 to capabilities_.max_combined_texture_image_units.
  GLuint active_texture_unit_;

  // Current GL error bits.
  uint32_t error_bits_;

  LogSettings log_settings_;

  // When true, the context is lost when a GL_OUT_OF_MEMORY error occurs.
  const bool lose_context_when_out_of_memory_;

  // Used to check for single threaded access.
  int use_count_;

  base::Optional<ScopedTransferBufferPtr> raster_mapped_buffer_;

  base::RepeatingCallback<void(const char*, int32_t)> error_message_callback_;

  int current_trace_stack_;

  Capabilities capabilities_;

  // Flag to indicate whether the implementation can retain resources, or
  // whether it should aggressively free them.
  bool aggressively_free_resources_;

  IdAllocator texture_id_allocator_;
  IdAllocator query_id_allocator_;

  ClientDiscardableTextureManager discardable_texture_manager_;

  mutable base::Lock lost_lock_;
  bool lost_;

  DISALLOW_COPY_AND_ASSIGN(RasterImplementation);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_H_
