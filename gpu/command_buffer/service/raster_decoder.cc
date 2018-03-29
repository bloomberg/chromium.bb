// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder.h"

#include <stdint.h>

#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/common/raster_cmd_ids.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/raster_cmd_validation.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"

// Local versions of the SET_GL_ERROR macros
#define LOCAL_SET_GL_ERROR(error, function_name, msg) \
  ERRORSTATE_SET_GL_ERROR(state_.GetErrorState(), error, function_name, msg)
#define LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, value, label)          \
  ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(state_.GetErrorState(), function_name, \
                                       static_cast<uint32_t>(value), label)
#define LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER(function_name)         \
  ERRORSTATE_COPY_REAL_GL_ERRORS_TO_WRAPPER(state_.GetErrorState(), \
                                            function_name)
#define LOCAL_PEEK_GL_ERROR(function_name) \
  ERRORSTATE_PEEK_GL_ERROR(state_.GetErrorState(), function_name)
#define LOCAL_CLEAR_REAL_GL_ERRORS(function_name) \
  ERRORSTATE_CLEAR_REAL_GL_ERRORS(state_.GetErrorState(), function_name)
#define LOCAL_PERFORMANCE_WARNING(msg) \
  PerformanceWarning(__FILE__, __LINE__, msg)
#define LOCAL_RENDER_WARNING(msg) RenderWarning(__FILE__, __LINE__, msg)

using namespace gpu::gles2;

namespace gpu {
namespace raster {

namespace {

class TextureMetadata {
 public:
  TextureMetadata(bool use_buffer,
                  gfx::BufferUsage buffer_usage,
                  viz::ResourceFormat format,
                  const Capabilities& caps)
      : use_buffer_(use_buffer),
        buffer_usage_(buffer_usage),
        format_(format),
        target_(CalcTarget(use_buffer, buffer_usage, format, caps)) {}
  TextureMetadata(const TextureMetadata& tmd) = default;

  bool use_buffer() const { return use_buffer_; }
  gfx::BufferUsage buffer_usage() const { return buffer_usage_; }
  viz::ResourceFormat format() const { return format_; }
  GLenum target() const { return target_; }

 private:
  static GLenum CalcTarget(bool use_buffer,
                           gfx::BufferUsage buffer_usage,
                           viz::ResourceFormat format,
                           const gpu::Capabilities& caps) {
    if (use_buffer) {
      gfx::BufferFormat buffer_format = viz::BufferFormat(format);
      return GetBufferTextureTarget(buffer_usage, buffer_format, caps);
    } else {
      return GL_TEXTURE_2D;
    }
  }

  const bool use_buffer_;
  const gfx::BufferUsage buffer_usage_;
  const viz::ResourceFormat format_;
  const GLenum target_;
};

// This class prevents any GL errors that occur when it is in scope from
// being reported to the client.
class ScopedGLErrorSuppressor {
 public:
  ScopedGLErrorSuppressor(const char* function_name, ErrorState* error_state);
  ~ScopedGLErrorSuppressor();

 private:
  const char* function_name_;
  ErrorState* error_state_;
  DISALLOW_COPY_AND_ASSIGN(ScopedGLErrorSuppressor);
};

ScopedGLErrorSuppressor::ScopedGLErrorSuppressor(const char* function_name,
                                                 ErrorState* error_state)
    : function_name_(function_name), error_state_(error_state) {
  ERRORSTATE_COPY_REAL_GL_ERRORS_TO_WRAPPER(error_state_, function_name_);
}

ScopedGLErrorSuppressor::~ScopedGLErrorSuppressor() {
  ERRORSTATE_CLEAR_REAL_GL_ERRORS(error_state_, function_name_);
}

void RestoreCurrentTextureBindings(ContextState* state,
                                   GLenum target,
                                   GLuint texture_unit) {
  DCHECK(!state->texture_units.empty());
  DCHECK_LT(texture_unit, state->texture_units.size());
  TextureUnit& info = state->texture_units[texture_unit];
  GLuint last_id;
  TextureRef* texture_ref = info.GetInfoForTarget(target);
  if (texture_ref) {
    last_id = texture_ref->service_id();
  } else {
    last_id = 0;
  }

  state->api()->glBindTextureFn(target, last_id);
}

// Temporarily changes a decoder's bound texture and restore it when this
// object goes out of scope. Also temporarily switches to using active texture
// unit zero in case the client has changed that to something invalid.
class ScopedTextureBinder {
 public:
  ScopedTextureBinder(ContextState* state,
                      TextureManager* texture_manager,
                      TextureRef* texture_ref,
                      GLenum target);
  ~ScopedTextureBinder();

 private:
  ContextState* state_;
  GLenum target_;
  TextureUnit old_unit_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTextureBinder);
};

ScopedTextureBinder::ScopedTextureBinder(ContextState* state,
                                         TextureManager* texture_manager,
                                         TextureRef* texture_ref,
                                         GLenum target)
    : state_(state), target_(target), old_unit_(state->texture_units[0]) {
  auto* api = state->api();
  api->glActiveTextureFn(GL_TEXTURE0);

  Texture* texture = texture_ref->texture();
  if (texture->target() == 0) {
    texture_manager->SetTarget(texture_ref, target);
  }
  DCHECK_EQ(texture->target(), target)
      << "Texture bound to more than 1 target.";

  api->glBindTextureFn(target, texture_ref->service_id());

  TextureUnit& unit = state_->texture_units[0];
  unit.bind_target = target;
  unit.SetInfoForTarget(target, texture_ref);
}

ScopedTextureBinder::~ScopedTextureBinder() {
  state_->texture_units[0] = old_unit_;

  RestoreCurrentTextureBindings(state_, target_, 0);
  state_->RestoreActiveTexture();
}

// Temporarily changes a decoder's PIXEL_UNPACK_BUFFER to 0 and set pixel unpack
// params to default, and restore them when this object goes out of scope.
class ScopedPixelUnpackState {
 public:
  explicit ScopedPixelUnpackState(ContextState* state);
  ~ScopedPixelUnpackState();

 private:
  ContextState* state_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPixelUnpackState);
};

ScopedPixelUnpackState::ScopedPixelUnpackState(ContextState* state)
    : state_(state) {
  DCHECK(state_);
  state_->PushTextureUnpackState();
}

ScopedPixelUnpackState::~ScopedPixelUnpackState() {
  state_->RestoreUnpackState();
}

}  // namespace

class RasterDecoderImpl : public RasterDecoder, public gles2::ErrorStateClient {
 public:
  RasterDecoderImpl(DecoderClient* client,
                    CommandBufferServiceBase* command_buffer_service,
                    gles2::Outputter* outputter,
                    gles2::ContextGroup* group);
  ~RasterDecoderImpl() override;

  GLES2Util* GetGLES2Util() override { return &util_; }

  // DecoderContext implementation.
  base::WeakPtr<DecoderContext> AsWeakPtr() override;
  gpu::ContextResult Initialize(
      const scoped_refptr<gl::GLSurface>& surface,
      const scoped_refptr<gl::GLContext>& context,
      bool offscreen,
      const gles2::DisallowedFeatures& disallowed_features,
      const ContextCreationAttribs& attrib_helper) override;
  const gles2::ContextState* GetContextState() override;
  void Destroy(bool have_context) override;
  bool MakeCurrent() override;
  gl::GLContext* GetGLContext() override;
  const FeatureInfo* GetFeatureInfo() const override {
    return feature_info_.get();
  }
  Capabilities GetCapabilities() override;
  void RestoreGlobalState() const override;
  void ClearAllAttributes() const override;
  void RestoreAllAttributes() const override;
  void RestoreState(const gles2::ContextState* prev_state) override;
  void RestoreActiveTexture() const override;
  void RestoreAllTextureUnitAndSamplerBindings(
      const gles2::ContextState* prev_state) const override;
  void RestoreActiveTextureUnitBinding(unsigned int target) const override;
  void RestoreBufferBinding(unsigned int target) override;
  void RestoreBufferBindings() const override;
  void RestoreFramebufferBindings() const override;
  void RestoreRenderbufferBindings() override;
  void RestoreProgramBindings() const override;
  void RestoreTextureState(unsigned service_id) const override;
  void RestoreTextureUnitBindings(unsigned unit) const override;
  void RestoreVertexAttribArray(unsigned index) override;
  void RestoreAllExternalTextureBindingsIfNeeded() override;
  QueryManager* GetQueryManager() override;
  gles2::GpuFenceManager* GetGpuFenceManager() override;
  bool HasPendingQueries() const override;
  void ProcessPendingQueries(bool did_finish) override;
  bool HasMoreIdleWork() const override;
  void PerformIdleWork() override;
  bool HasPollingWork() const override;
  void PerformPollingWork() override;
  TextureBase* GetTextureBase(uint32_t client_id) override;
  void SetLevelInfo(uint32_t client_id,
                    int level,
                    unsigned internal_format,
                    unsigned width,
                    unsigned height,
                    unsigned depth,
                    unsigned format,
                    unsigned type,
                    const gfx::Rect& cleared_rect) override;
  bool WasContextLost() const override;
  bool WasContextLostByRobustnessExtension() const override;
  void MarkContextLost(error::ContextLostReason reason) override;
  bool CheckResetStatus() override;
  void BeginDecoding() override;
  void EndDecoding() override;
  const char* GetCommandName(unsigned int command_id) const;
  error::Error DoCommands(unsigned int num_commands,
                          const volatile void* buffer,
                          int num_entries,
                          int* entries_processed) override;
  base::StringPiece GetLogPrefix() override;
  void BindImage(uint32_t client_texture_id,
                 uint32_t texture_target,
                 gl::GLImage* image,
                 bool can_bind_to_sampler) override;
  gles2::ContextGroup* GetContextGroup() override;
  gles2::ErrorState* GetErrorState() override;
  bool IsCompressedTextureFormat(unsigned format) override;
  bool ClearLevel(gles2::Texture* texture,
                  unsigned target,
                  int level,
                  unsigned format,
                  unsigned type,
                  int xoffset,
                  int yoffset,
                  int width,
                  int height) override;
  bool ClearCompressedTextureLevel(gles2::Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   int width,
                                   int height) override;
  bool ClearLevel3D(gles2::Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int width,
                    int height,
                    int depth) override {
    NOTIMPLEMENTED();
    return false;
  }

  // ErrorClientState implementation.
  void OnContextLostError() override;
  void OnOutOfMemoryError() override;

  Logger* GetLogger() override;

  void SetIgnoreCachedStateForTest(bool ignore) override;
  ImageManager* GetImageManagerForTest() override;

 private:
  std::unordered_map<GLuint, TextureMetadata> texture_metadata_;
  TextureMetadata* GetTextureMetadata(GLuint client_id) {
    auto it = texture_metadata_.find(client_id);
    DCHECK(it != texture_metadata_.end()) << "Undefined texture id";
    return &it->second;
  }

  gl::GLApi* api() const { return state_.api(); }

  const FeatureInfo::FeatureFlags& features() const {
    return feature_info_->feature_flags();
  }

  const GpuDriverBugWorkarounds& workarounds() const {
    return feature_info_->workarounds();
  }

  const gl::GLVersionInfo& gl_version_info() {
    return feature_info_->gl_version_info();
  }

  MemoryTracker* memory_tracker() { return group_->memory_tracker(); }

  BufferManager* buffer_manager() { return group_->buffer_manager(); }

  bool EnsureGPUMemoryAvailable(size_t estimated_size) {
    MemoryTracker* tracker = memory_tracker();
    if (tracker) {
      return tracker->EnsureGPUMemoryAvailable(estimated_size);
    }
    return true;
  }

  const TextureManager* texture_manager() const {
    return group_->texture_manager();
  }

  TextureManager* texture_manager() { return group_->texture_manager(); }

  ImageManager* image_manager() { return group_->image_manager(); }

  // Creates a Texture for the given texture.
  TextureRef* CreateTexture(GLuint client_id, GLuint service_id) {
    return texture_manager()->CreateTexture(client_id, service_id);
  }

  // Gets the texture info for the given texture. Returns nullptr if none
  // exists.
  TextureRef* GetTexture(GLuint client_id) const {
    return texture_manager()->GetTexture(client_id);
  }

  // Deletes the texture info for the given texture.
  void RemoveTexture(GLuint client_id) {
    texture_manager()->RemoveTexture(client_id);

    auto texture_iter = texture_metadata_.find(client_id);
    DCHECK(texture_iter != texture_metadata_.end());

    texture_metadata_.erase(texture_iter);
  }

  void UnbindTexture(TextureRef* texture_ref) {
    // Unbind texture_ref from texture_ref units.
    state_.UnbindTexture(texture_ref);
  }

  // Set remaining commands to process to 0 to force DoCommands to return
  // and allow context preemption and GPU watchdog checks in
  // CommandExecutor().
  void ExitCommandProcessingEarly() { commands_to_process_ = 0; }

  template <bool DebugImpl>
  error::Error DoCommandsImpl(unsigned int num_commands,
                              const volatile void* buffer,
                              int num_entries,
                              int* entries_processed);

  // Helper for glGetIntegerv.  Returns false if pname is unhandled.
  bool GetHelper(GLenum pname, GLint* params, GLsizei* num_written);

  // Gets the number of values that will be returned by glGetXXX. Returns
  // false if pname is unknown.
  bool GetNumValuesReturnedForGLGet(GLenum pname, GLsizei* num_values);

  GLuint DoCreateTexture(bool use_buffer,
                         gfx::BufferUsage /* buffer_usage */,
                         viz::ResourceFormat /* resource_format */);
  void CreateTexture(GLuint client_id,
                     GLuint service_id,
                     bool use_buffer,
                     gfx::BufferUsage buffer_usage,
                     viz::ResourceFormat resource_format);
  void DoCreateAndConsumeTextureINTERNAL(GLuint client_id,
                                         bool use_buffer,
                                         gfx::BufferUsage buffer_usage,
                                         viz::ResourceFormat resource_format,
                                         const volatile GLbyte* key);
  void DeleteTexturesHelper(GLsizei n, const volatile GLuint* client_ids);
  bool GenQueriesEXTHelper(GLsizei n, const GLuint* client_ids);
  void DeleteQueriesEXTHelper(GLsizei n, const volatile GLuint* client_ids);
  void DoFinish();
  void DoFlush();
  void DoGetIntegerv(GLenum pname, GLint* params, GLsizei params_size);
  void DoTexParameteri(GLuint texture_id, GLenum pname, GLint param);
  void DoBindTexImage2DCHROMIUM(GLuint texture_id, GLint image_id);
  void DoProduceTextureDirect(GLuint texture, const volatile GLbyte* key);
  void DoReleaseTexImage2DCHROMIUM(GLuint texture_id, GLint image_id);
  void TexStorage2DImage(TextureRef* texture_ref,
                         const TextureMetadata& texture_metadata,
                         GLsizei width,
                         GLsizei height) {
    NOTIMPLEMENTED();
  }
  void TexStorage2D(TextureRef* texture_ref,
                    const TextureMetadata& texture_metadata,
                    GLint levels,
                    GLsizei width,
                    GLsizei height);
  void TexImage2D(TextureRef* texture_ref,
                  const TextureMetadata& texture_metadata,
                  GLsizei width,
                  GLsizei height);
  void DoTexStorage2D(GLuint texture_id,
                      GLint levels,
                      GLsizei width,
                      GLsizei height);
  void DoCopySubTexture(GLuint source_id,
                        GLuint dest_id,
                        GLint xoffset,
                        GLint yoffset,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height) {
    NOTIMPLEMENTED();
  }
  void DoCompressedCopyTextureCHROMIUM(GLuint source_id, GLuint dest_id) {
    NOTIMPLEMENTED();
  }
  void DoProduceTextureDirectCHROMIUM(GLuint texture,
                                      const volatile GLbyte* key) {
    NOTIMPLEMENTED();
  }
  void DoLoseContextCHROMIUM(GLenum current, GLenum other) { NOTIMPLEMENTED(); }
  void DoBeginRasterCHROMIUM(GLuint texture_id,
                             GLuint sk_color,
                             GLuint msaa_sample_count,
                             GLboolean can_use_lcd_text,
                             GLint color_type) {
    NOTIMPLEMENTED();
  }
  void DoRasterCHROMIUM(GLsizeiptr size, const void* list) { NOTIMPLEMENTED(); }
  void DoEndRasterCHROMIUM() { NOTIMPLEMENTED(); }
  void DoCreateTransferCacheEntryINTERNAL(GLuint entry_type,
                                          GLuint entry_id,
                                          GLuint handle_shm_id,
                                          GLuint handle_shm_offset,
                                          GLuint data_shm_id,
                                          GLuint data_shm_offset,
                                          GLuint data_size) {
    NOTIMPLEMENTED();
  }
  void DoUnlockTransferCacheEntryINTERNAL(GLuint entry_type, GLuint entry_id) {
    NOTIMPLEMENTED();
  }
  void DoDeleteTransferCacheEntryINTERNAL(GLuint entry_type, GLuint entry_id) {
    NOTIMPLEMENTED();
  }
  void DoUnpremultiplyAndDitherCopyCHROMIUM(GLuint source_id,
                                            GLuint dest_id,
                                            GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height) {
    NOTIMPLEMENTED();
  }

#if defined(NDEBUG)
  void LogClientServiceMapping(const char* /* function_name */,
                               GLuint /* client_id */,
                               GLuint /* service_id */) {}
  template <typename T>
  void LogClientServiceForInfo(T* /* info */,
                               GLuint /* client_id */,
                               const char* /* function_name */) {}
#else
  void LogClientServiceMapping(const char* function_name,
                               GLuint client_id,
                               GLuint service_id) {
    if (service_logging_) {
      VLOG(1) << "[" << logger_.GetLogPrefix() << "] " << function_name
              << ": client_id = " << client_id
              << ", service_id = " << service_id;
    }
  }
  template <typename T>
  void LogClientServiceForInfo(T* info,
                               GLuint client_id,
                               const char* function_name) {
    if (info) {
      LogClientServiceMapping(function_name, client_id, info->service_id());
    }
  }
#endif

// Generate a member function prototype for each command in an automated and
// typesafe way.
#define RASTER_CMD_OP(name) \
  Error Handle##name(uint32_t immediate_data_size, const volatile void* data);

  RASTER_COMMAND_LIST(RASTER_CMD_OP)
#undef RASTER_CMD_OP

  typedef error::Error (RasterDecoderImpl::*CmdHandler)(
      uint32_t immediate_data_size,
      const volatile void* data);

  // A struct to hold info about each command.
  struct CommandInfo {
    CmdHandler cmd_handler;
    uint8_t arg_flags;   // How to handle the arguments for this command
    uint8_t cmd_flags;   // How to handle this command
    uint16_t arg_count;  // How many arguments are expected for this command.
  };

  // A table of CommandInfo for all the commands.
  static const CommandInfo command_info[kNumCommands - kFirstRasterCommand];

  // Most recent generation of the TextureManager.  If this no longer matches
  // the current generation when our context becomes current, then we'll rebind
  // all the textures to stay up to date with Texture::service_id() changes.
  uint32_t texture_manager_service_id_generation_;

  // Number of commands remaining to be processed in DoCommands().
  int commands_to_process_;

  // The current decoder error communicates the decoder error through command
  // processing functions that do not return the error value. Should be set
  // only if not returning an error.
  error::Error current_decoder_error_;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;

  DecoderClient* client_;

  gles2::DebugMarkerManager debug_marker_manager_;
  gles2::Logger logger_;

  // The ContextGroup for this decoder uses to track resources.
  scoped_refptr<gles2::ContextGroup> group_;
  std::unique_ptr<Validators> validators_;
  scoped_refptr<gles2::FeatureInfo> feature_info_;

  std::unique_ptr<QueryManager> query_manager_;

  // All the state for this context.
  gles2::ContextState state_;

  GLES2Util util_;

  // States related to each manager.
  DecoderTextureState texture_state_;
  DecoderFramebufferState framebuffer_state_;

  bool gpu_debug_commands_;

  // Log extra info.
  bool service_logging_;

  base::WeakPtrFactory<DecoderContext> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RasterDecoderImpl);
};

constexpr RasterDecoderImpl::CommandInfo RasterDecoderImpl::command_info[] = {
#define RASTER_CMD_OP(name)                                    \
  {                                                            \
      &RasterDecoderImpl::Handle##name, cmds::name::kArgFlags, \
      cmds::name::cmd_flags,                                   \
      sizeof(cmds::name) / sizeof(CommandBufferEntry) - 1,     \
  }, /* NOLINT */
    RASTER_COMMAND_LIST(RASTER_CMD_OP)
#undef RASTER_CMD_OP
};

// static
RasterDecoder* RasterDecoder::Create(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    Outputter* outputter,
    ContextGroup* group) {
  return new RasterDecoderImpl(client, command_buffer_service, outputter,
                               group);
}

RasterDecoder::RasterDecoder(CommandBufferServiceBase* command_buffer_service)
    : CommonDecoder(command_buffer_service),
      initialized_(false),
      debug_(false),
      log_commands_(false) {}

RasterDecoder::~RasterDecoder() {}

bool RasterDecoder::initialized() const {
  return initialized_;
}

TextureBase* RasterDecoder::GetTextureBase(uint32_t client_id) {
  return nullptr;
}

void RasterDecoder::SetLevelInfo(uint32_t client_id,
                                 int level,
                                 unsigned internal_format,
                                 unsigned width,
                                 unsigned height,
                                 unsigned depth,
                                 unsigned format,
                                 unsigned type,
                                 const gfx::Rect& cleared_rect) {}

void RasterDecoder::BeginDecoding() {}

void RasterDecoder::EndDecoding() {}

base::StringPiece RasterDecoder::GetLogPrefix() {
  return GetLogger()->GetLogPrefix();
}

RasterDecoderImpl::RasterDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    Outputter* outputter,
    ContextGroup* group)
    : RasterDecoder(command_buffer_service),
      texture_manager_service_id_generation_(0),
      commands_to_process_(0),
      current_decoder_error_(error::kNoError),
      client_(client),
      logger_(&debug_marker_manager_, client),
      group_(group),
      validators_(new Validators),
      feature_info_(group_->feature_info()),
      state_(group_->feature_info(), this, &logger_),
      texture_state_(group_->feature_info()->workarounds()),
      service_logging_(
          group_->gpu_preferences().enable_gpu_service_logging_gpu),
      weak_ptr_factory_(this) {}

RasterDecoderImpl::~RasterDecoderImpl() {}

base::WeakPtr<DecoderContext> RasterDecoderImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

gpu::ContextResult RasterDecoderImpl::Initialize(
    const scoped_refptr<gl::GLSurface>& surface,
    const scoped_refptr<gl::GLContext>& context,
    bool offscreen,
    const DisallowedFeatures& disallowed_features,
    const ContextCreationAttribs& attrib_helper) {
  TRACE_EVENT0("gpu", "RasterDecoderImpl::Initialize");
  DCHECK(context->IsCurrent(surface.get()));
  DCHECK(!context_.get());

  state_.set_api(gl::g_current_gl_context);

  set_initialized();

  if (!offscreen) {
    return gpu::ContextResult::kFatalFailure;
  }

  if (group_->gpu_preferences().enable_gpu_debugging)
    set_debug(true);

  if (group_->gpu_preferences().enable_gpu_command_logging)
    set_log_commands(true);

  surface_ = surface;
  context_ = context;

  auto result =
      group_->Initialize(this, attrib_helper.context_type, disallowed_features);
  if (result != gpu::ContextResult::kSuccess) {
    group_ =
        nullptr;  // Must not destroy ContextGroup if it is not initialized.
    Destroy(true);
    return result;
  }
  CHECK_GL_ERROR();

  query_manager_.reset(new QueryManager());

  state_.texture_units.resize(group_->max_texture_units());
  state_.sampler_units.resize(group_->max_texture_units());
  for (uint32_t tt = 0; tt < state_.texture_units.size(); ++tt) {
    api()->glActiveTextureFn(GL_TEXTURE0 + tt);
    TextureRef* ref;
    ref = texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_2D);
    state_.texture_units[tt].bound_texture_2d = ref;
    api()->glBindTextureFn(GL_TEXTURE_2D, ref ? ref->service_id() : 0);
  }
  api()->glActiveTextureFn(GL_TEXTURE0);
  CHECK_GL_ERROR();

  return gpu::ContextResult::kSuccess;
}

const gles2::ContextState* RasterDecoderImpl::GetContextState() {
  NOTIMPLEMENTED();
  return nullptr;
}

void RasterDecoderImpl::Destroy(bool have_context) {
  if (query_manager_.get()) {
    query_manager_->Destroy(have_context);
    query_manager_.reset();
  }

  if (group_.get()) {
    group_->Destroy(this, have_context);
    group_ = NULL;
  }

  // Destroy the surface before the context, some surface destructors make GL
  // calls.
  surface_ = nullptr;

  if (context_.get()) {
    context_->ReleaseCurrent(NULL);
    context_ = NULL;
  }
}

// Make this decoder's GL context current.
bool RasterDecoderImpl::MakeCurrent() {
  DCHECK(surface_);
  if (!context_.get())
    return false;

  if (WasContextLost()) {
    LOG(ERROR) << "  RasterDecoderImpl: Trying to make lost context current.";
    return false;
  }

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "  RasterDecoderImpl: Context lost during MakeCurrent.";
    MarkContextLost(error::kMakeCurrentFailed);
    group_->LoseContexts(error::kUnknown);
    return false;
  }
  DCHECK_EQ(api(), gl::g_current_gl_context);

  // Rebind textures if the service ids may have changed.
  RestoreAllExternalTextureBindingsIfNeeded();

  return true;
}

gl::GLContext* RasterDecoderImpl::GetGLContext() {
  return context_.get();
}

Capabilities RasterDecoderImpl::GetCapabilities() {
  gpu::Capabilities caps;
  caps.gpu_rasterization = true;
  caps.supports_oop_raster = true;
  caps.texture_target_exception_list =
      group_->gpu_preferences().texture_target_exception_list;
  caps.texture_format_bgra8888 =
      feature_info_->feature_flags().ext_texture_format_bgra8888;
  caps.texture_storage_image =
      feature_info_->feature_flags().chromium_texture_storage_image;
  caps.texture_storage = feature_info_->feature_flags().ext_texture_storage;
  return caps;
}

void RasterDecoderImpl::RestoreGlobalState() const {
  state_.RestoreGlobalState(nullptr);
}

void RasterDecoderImpl::ClearAllAttributes() const {
  // Must use native VAO 0, as RestoreAllAttributes can't fully restore
  // other VAOs.
  if (feature_info_->feature_flags().native_vertex_array_object)
    api()->glBindVertexArrayOESFn(0);

  for (uint32_t i = 0; i < group_->max_vertex_attribs(); ++i) {
    if (i != 0)  // Never disable attribute 0
      state_.vertex_attrib_manager->SetDriverVertexAttribEnabled(i, false);
    if (features().angle_instanced_arrays)
      api()->glVertexAttribDivisorANGLEFn(i, 0);
  }
}

void RasterDecoderImpl::RestoreAllAttributes() const {
  state_.RestoreVertexAttribs();
}

void RasterDecoderImpl::RestoreState(const ContextState* prev_state) {
  TRACE_EVENT1("gpu", "RasterDecoderImpl::RestoreState", "context",
               logger_.GetLogPrefix());
  state_.RestoreState(prev_state);
}

void RasterDecoderImpl::RestoreActiveTexture() const {
  state_.RestoreActiveTexture();
}

void RasterDecoderImpl::RestoreAllTextureUnitAndSamplerBindings(
    const ContextState* prev_state) const {
  state_.RestoreAllTextureUnitAndSamplerBindings(prev_state);
}

void RasterDecoderImpl::RestoreActiveTextureUnitBinding(
    unsigned int target) const {
  state_.RestoreActiveTextureUnitBinding(target);
}

void RasterDecoderImpl::RestoreBufferBinding(unsigned int target) {
  if (target == GL_PIXEL_PACK_BUFFER) {
    state_.UpdatePackParameters();
  } else if (target == GL_PIXEL_UNPACK_BUFFER) {
    state_.UpdateUnpackParameters();
  }
  gles2::Buffer* bound_buffer =
      buffer_manager()->GetBufferInfoForTarget(&state_, target);
  api()->glBindBufferFn(target, bound_buffer ? bound_buffer->service_id() : 0);
}

void RasterDecoderImpl::RestoreBufferBindings() const {
  state_.RestoreBufferBindings();
}

void RasterDecoderImpl::RestoreFramebufferBindings() const {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreRenderbufferBindings() {
  state_.RestoreRenderbufferBindings();
}

void RasterDecoderImpl::RestoreProgramBindings() const {
  state_.RestoreProgramSettings(nullptr, false);
}

void RasterDecoderImpl::RestoreTextureState(unsigned service_id) const {
  Texture* texture = texture_manager()->GetTextureForServiceId(service_id);
  if (texture) {
    GLenum target = texture->target();
    api()->glBindTextureFn(target, service_id);
    api()->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, texture->wrap_s());
    api()->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, texture->wrap_t());
    api()->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER,
                             texture->min_filter());
    api()->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER,
                             texture->mag_filter());
    if (feature_info_->IsWebGL2OrES3Context()) {
      api()->glTexParameteriFn(target, GL_TEXTURE_BASE_LEVEL,
                               texture->base_level());
    }
    RestoreTextureUnitBindings(state_.active_texture_unit);
  }
}

void RasterDecoderImpl::RestoreTextureUnitBindings(unsigned unit) const {
  state_.RestoreTextureUnitBindings(unit, nullptr);
}

void RasterDecoderImpl::RestoreVertexAttribArray(unsigned index) {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreAllExternalTextureBindingsIfNeeded() {
  if (texture_manager()->GetServiceIdGeneration() ==
      texture_manager_service_id_generation_)
    return;

  // Texture manager's version has changed, so rebind all external textures
  // in case their service ids have changed.
  for (unsigned texture_unit_index = 0;
       texture_unit_index < state_.texture_units.size(); texture_unit_index++) {
    const TextureUnit& texture_unit = state_.texture_units[texture_unit_index];
    if (texture_unit.bind_target != GL_TEXTURE_EXTERNAL_OES)
      continue;

    if (TextureRef* texture_ref =
            texture_unit.bound_texture_external_oes.get()) {
      api()->glActiveTextureFn(GL_TEXTURE0 + texture_unit_index);
      api()->glBindTextureFn(GL_TEXTURE_EXTERNAL_OES,
                             texture_ref->service_id());
    }
  }

  api()->glActiveTextureFn(GL_TEXTURE0 + state_.active_texture_unit);

  texture_manager_service_id_generation_ =
      texture_manager()->GetServiceIdGeneration();
}

QueryManager* RasterDecoderImpl::GetQueryManager() {
  return query_manager_.get();
}

GpuFenceManager* RasterDecoderImpl::GetGpuFenceManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool RasterDecoderImpl::HasPendingQueries() const {
  return query_manager_.get() && query_manager_->HavePendingQueries();
}

void RasterDecoderImpl::ProcessPendingQueries(bool did_finish) {
  if (!query_manager_.get())
    return;
  query_manager_->ProcessPendingQueries(did_finish);
}

bool RasterDecoderImpl::HasMoreIdleWork() const {
  return false;
}

void RasterDecoderImpl::PerformIdleWork() {
}

bool RasterDecoderImpl::HasPollingWork() const {
  return false;
}

void RasterDecoderImpl::PerformPollingWork() {}

TextureBase* RasterDecoderImpl::GetTextureBase(uint32_t client_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

void RasterDecoderImpl::SetLevelInfo(uint32_t client_id,
                                     int level,
                                     unsigned internal_format,
                                     unsigned width,
                                     unsigned height,
                                     unsigned depth,
                                     unsigned format,
                                     unsigned type,
                                     const gfx::Rect& cleared_rect) {
  NOTIMPLEMENTED();
}

bool RasterDecoderImpl::WasContextLost() const {
  return false;
}

bool RasterDecoderImpl::WasContextLostByRobustnessExtension() const {
  NOTIMPLEMENTED();
  return false;
}

void RasterDecoderImpl::MarkContextLost(error::ContextLostReason reason) {
  NOTIMPLEMENTED();
}

bool RasterDecoderImpl::CheckResetStatus() {
  NOTIMPLEMENTED();
  return false;
}

Logger* RasterDecoderImpl::GetLogger() {
  return &logger_;
}

void RasterDecoderImpl::SetIgnoreCachedStateForTest(bool ignore) {
  state_.SetIgnoreCachedStateForTest(ignore);
}

ImageManager* RasterDecoderImpl::GetImageManagerForTest() {
  return group_->image_manager();
}

void RasterDecoderImpl::BeginDecoding() {
  gpu_debug_commands_ = log_commands() || debug();
}

void RasterDecoderImpl::EndDecoding() {}

const char* RasterDecoderImpl::GetCommandName(unsigned int command_id) const {
  if (command_id >= kFirstRasterCommand && command_id < kNumCommands) {
    return raster::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

template <bool DebugImpl>
error::Error RasterDecoderImpl::DoCommandsImpl(unsigned int num_commands,
                                               const volatile void* buffer,
                                               int num_entries,
                                               int* entries_processed) {
  DCHECK(entries_processed);
  commands_to_process_ = num_commands;
  error::Error result = error::kNoError;
  const volatile CommandBufferEntry* cmd_data =
      static_cast<const volatile CommandBufferEntry*>(buffer);
  int process_pos = 0;
  unsigned int command = 0;

  while (process_pos < num_entries && result == error::kNoError &&
         commands_to_process_--) {
    const unsigned int size = cmd_data->value_header.size;
    command = cmd_data->value_header.command;

    if (size == 0) {
      result = error::kInvalidSize;
      break;
    }

    if (static_cast<int>(size) + process_pos > num_entries) {
      result = error::kOutOfBounds;
      break;
    }

    if (DebugImpl && log_commands()) {
      LOG(ERROR) << "[" << logger_.GetLogPrefix() << "]"
                 << "cmd: " << GetCommandName(command);
    }

    const unsigned int arg_count = size - 1;
    unsigned int command_index = command - kFirstRasterCommand;
    if (command_index < arraysize(command_info)) {
      const CommandInfo& info = command_info[command_index];
      unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
      if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
          (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
        uint32_t immediate_data_size = (arg_count - info_arg_count) *
                                       sizeof(CommandBufferEntry);  // NOLINT
        result = (this->*info.cmd_handler)(immediate_data_size, cmd_data);
        if (DebugImpl && debug() && !WasContextLost()) {
          GLenum error;
          while ((error = api()->glGetErrorFn()) != GL_NO_ERROR) {
            LOG(ERROR) << "[" << logger_.GetLogPrefix() << "] "
                       << "GL ERROR: " << GLES2Util::GetStringEnum(error)
                       << " : " << GetCommandName(command);
            LOCAL_SET_GL_ERROR(error, "DoCommand", "GL error from driver");
          }
        }
      } else {
        result = error::kInvalidArguments;
      }
    } else {
      result = DoCommonCommand(command, arg_count, cmd_data);
    }

    if (result == error::kNoError &&
        current_decoder_error_ != error::kNoError) {
      result = current_decoder_error_;
      current_decoder_error_ = error::kNoError;
    }

    if (result != error::kDeferCommandUntilLater) {
      process_pos += size;
      cmd_data += size;
    }
  }

  *entries_processed = process_pos;

  if (error::IsError(result)) {
    LOG(ERROR) << "Error: " << result << " for Command "
               << GetCommandName(command);
  }

  return result;
}

error::Error RasterDecoderImpl::DoCommands(unsigned int num_commands,
                                           const volatile void* buffer,
                                           int num_entries,
                                           int* entries_processed) {
  if (gpu_debug_commands_) {
    return DoCommandsImpl<true>(num_commands, buffer, num_entries,
                                entries_processed);
  } else {
    return DoCommandsImpl<false>(num_commands, buffer, num_entries,
                                 entries_processed);
  }
}

bool RasterDecoderImpl::GetHelper(GLenum pname,
                                  GLint* params,
                                  GLsizei* num_written) {
  DCHECK(num_written);
  switch (pname) {
    case GL_MAX_TEXTURE_SIZE:
      *num_written = 1;
      if (params) {
        params[0] = texture_manager()->MaxSizeForTarget(GL_TEXTURE_2D);
      }
      return true;
    default:
      *num_written = util_.GLGetNumValuesReturned(pname);
      if (*num_written)
        break;

      return false;
  }

  // TODO(backer): Only GL_ACTIVE_TEXTURE supported?
  if (pname != GL_ACTIVE_TEXTURE) {
    return false;
  }

  if (params) {
    api()->glGetIntegervFn(pname, params);
  }
  return true;
}

bool RasterDecoderImpl::GetNumValuesReturnedForGLGet(GLenum pname,
                                                     GLsizei* num_values) {
  *num_values = 0;
  if (state_.GetStateAsGLint(pname, NULL, num_values)) {
    return true;
  }
  return GetHelper(pname, NULL, num_values);
}

base::StringPiece RasterDecoderImpl::GetLogPrefix() {
  return logger_.GetLogPrefix();
}

void RasterDecoderImpl::BindImage(uint32_t client_texture_id,
                                  uint32_t texture_target,
                                  gl::GLImage* image,
                                  bool can_bind_to_sampler) {
  NOTIMPLEMENTED();
}

gles2::ContextGroup* RasterDecoderImpl::GetContextGroup() {
  return group_.get();
}

gles2::ErrorState* RasterDecoderImpl::GetErrorState() {
  return state_.GetErrorState();
}

bool RasterDecoderImpl::IsCompressedTextureFormat(unsigned format) {
  return feature_info_->validators()->compressed_texture_format.IsValid(format);
}

bool RasterDecoderImpl::ClearLevel(gles2::Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   unsigned type,
                                   int xoffset,
                                   int yoffset,
                                   int width,
                                   int height) {
  DCHECK(target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY &&
         target != GL_TEXTURE_EXTERNAL_OES);
  uint32_t channels = GLES2Util::GetChannelsForFormat(format);
  if (channels & GLES2Util::kDepth) {
    DCHECK(false) << "depth not supported";
    return false;
  }

  const uint32_t kMaxZeroSize = 1024 * 1024 * 4;

  uint32_t size;
  uint32_t padded_row_size;
  if (!GLES2Util::ComputeImageDataSizes(width, height, 1, format, type,
                                        state_.unpack_alignment, &size, nullptr,
                                        &padded_row_size)) {
    return false;
  }

  TRACE_EVENT1("gpu", "RasterDecoderImpl::ClearLevel", "size", size);

  int tile_height;

  if (size > kMaxZeroSize) {
    if (kMaxZeroSize < padded_row_size) {
      // That'd be an awfully large texture.
      return false;
    }
    // We should never have a large total size with a zero row size.
    DCHECK_GT(padded_row_size, 0U);
    tile_height = kMaxZeroSize / padded_row_size;
    if (!GLES2Util::ComputeImageDataSizes(width, tile_height, 1, format, type,
                                          state_.unpack_alignment, &size,
                                          nullptr, nullptr)) {
      return false;
    }
  } else {
    tile_height = height;
  }

  api()->glBindTextureFn(texture->target(), texture->service_id());
  {
    // Add extra scope to destroy zero and the object it owns right
    // after its usage.
    // Assumes the size has already been checked.
    std::unique_ptr<char[]> zero(new char[size]);
    memset(zero.get(), 0, size);

    ScopedPixelUnpackState reset_restore(&state_);
    GLint y = 0;
    while (y < height) {
      GLint h = y + tile_height > height ? height - y : tile_height;
      api()->glTexSubImage2DFn(
          target, level, xoffset, yoffset + y, width, h,
          TextureManager::AdjustTexFormat(feature_info_.get(), format), type,
          zero.get());
      y += tile_height;
    }
  }

  TextureRef* bound_texture =
      texture_manager()->GetTextureInfoForTarget(&state_, texture->target());
  api()->glBindTextureFn(texture->target(),
                         bound_texture ? bound_texture->service_id() : 0);
  DCHECK(glGetError() == GL_NO_ERROR);
  return true;
}

bool RasterDecoderImpl::ClearCompressedTextureLevel(gles2::Texture* texture,
                                                    unsigned target,
                                                    int level,
                                                    unsigned format,
                                                    int width,
                                                    int height) {
  DCHECK(target != GL_TEXTURE_3D && target != GL_TEXTURE_2D_ARRAY);
  // This code path can only be called if the texture was originally
  // allocated via TexStorage2D. Note that TexStorage2D is exposed
  // internally for ES 2.0 contexts, but compressed texture support is
  // not part of that exposure.
  DCHECK(feature_info_->IsWebGL2OrES3Context());

  GLsizei bytes_required = 0;
  std::string error_str;
  if (!GetCompressedTexSizeInBytes("ClearCompressedTextureLevel", width, height,
                                   1, format, &bytes_required,
                                   state_.GetErrorState())) {
    return false;
  }

  TRACE_EVENT1("gpu", "RasterDecoderImpl::ClearCompressedTextureLevel",
               "bytes_required", bytes_required);

  api()->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, 0);
  {
    // Add extra scope to destroy zero and the object it owns right
    // after its usage.
    std::unique_ptr<char[]> zero(new char[bytes_required]);
    memset(zero.get(), 0, bytes_required);
    api()->glBindTextureFn(texture->target(), texture->service_id());
    api()->glCompressedTexSubImage2DFn(target, level, 0, 0, width, height,
                                       format, bytes_required, zero.get());
  }
  TextureRef* bound_texture =
      texture_manager()->GetTextureInfoForTarget(&state_, texture->target());
  api()->glBindTextureFn(texture->target(),
                         bound_texture ? bound_texture->service_id() : 0);
  gles2::Buffer* bound_buffer =
      buffer_manager()->GetBufferInfoForTarget(&state_, GL_PIXEL_UNPACK_BUFFER);
  if (bound_buffer) {
    api()->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, bound_buffer->service_id());
  }
  return true;
}

void RasterDecoderImpl::OnContextLostError() {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::OnOutOfMemoryError() {
  NOTIMPLEMENTED();
}

error::Error RasterDecoderImpl::HandleWaitSyncTokenCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::WaitSyncTokenCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::WaitSyncTokenCHROMIUM*>(
          cmd_data);

  const gpu::CommandBufferNamespace kMinNamespaceId =
      gpu::CommandBufferNamespace::INVALID;
  const gpu::CommandBufferNamespace kMaxNamespaceId =
      gpu::CommandBufferNamespace::NUM_COMMAND_BUFFER_NAMESPACES;

  gpu::CommandBufferNamespace namespace_id =
      static_cast<gpu::CommandBufferNamespace>(c.namespace_id);
  if ((namespace_id < static_cast<int32_t>(kMinNamespaceId)) ||
      (namespace_id >= static_cast<int32_t>(kMaxNamespaceId))) {
    namespace_id = gpu::CommandBufferNamespace::INVALID;
  }
  const CommandBufferId command_buffer_id =
      CommandBufferId::FromUnsafeValue(c.command_buffer_id());
  const uint64_t release = c.release_count();

  gpu::SyncToken sync_token;
  sync_token.Set(namespace_id, command_buffer_id, release);
  return client_->OnWaitSyncToken(sync_token) ? error::kDeferCommandUntilLater
                                              : error::kNoError;
}

error::Error RasterDecoderImpl::HandleSetColorSpaceMetadata(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleBeginQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::BeginQueryEXT& c =
      *static_cast<const volatile raster::cmds::BeginQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint client_id = static_cast<GLuint>(c.id);
  int32_t sync_shm_id = static_cast<int32_t>(c.sync_data_shm_id);
  uint32_t sync_shm_offset = static_cast<uint32_t>(c.sync_data_shm_offset);

  switch (target) {
    case GL_COMMANDS_ISSUED_CHROMIUM:
    case GL_COMMANDS_COMPLETED_CHROMIUM:
      break;
    default:
      LOCAL_SET_GL_ERROR(GL_INVALID_ENUM, "glBeginQueryEXT",
                         "unknown query target");
      return error::kNoError;
  }

  if (query_manager_->GetActiveQuery(target)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT",
                       "query already in progress");
    return error::kNoError;
  }

  if (client_id == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT", "id is 0");
    return error::kNoError;
  }

  scoped_refptr<gpu::Buffer> buffer = GetSharedMemoryBuffer(sync_shm_id);
  if (!buffer)
    return error::kInvalidArguments;
  QuerySync* sync = static_cast<QuerySync*>(
      buffer->GetDataAddress(sync_shm_offset, sizeof(QuerySync)));
  if (!sync)
    return error::kOutOfBounds;

  QueryManager::Query* query = query_manager_->GetQuery(client_id);
  if (!query) {
    if (!query_manager_->IsValidQuery(client_id)) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT",
                         "id not made by glGenQueriesEXT");
      return error::kNoError;
    }

    query =
        query_manager_->CreateQuery(target, client_id, std::move(buffer), sync);
  } else {
    if (query->target() != target) {
      LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glBeginQueryEXT",
                         "target does not match");
      return error::kNoError;
    } else if (query->sync() != sync) {
      DLOG(ERROR) << "Shared memory used by query not the same as before";
      return error::kInvalidArguments;
    }
  }

  query_manager_->BeginQuery(query);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleEndQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::EndQueryEXT& c =
      *static_cast<const volatile raster::cmds::EndQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  uint32_t submit_count = static_cast<GLuint>(c.submit_count);

  QueryManager::Query* query = query_manager_->GetActiveQuery(target);
  if (!query) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glEndQueryEXT",
                       "No active query");
    return error::kNoError;
  }

  query_manager_->EndQuery(query, submit_count);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleInitializeDiscardableTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleUnlockDiscardableTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleLockDiscardableTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleInsertFenceSyncCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::InsertFenceSyncCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::InsertFenceSyncCHROMIUM*>(
          cmd_data);

  const uint64_t release_count = c.release_count();
  client_->OnFenceSyncRelease(release_count);
  // Exit inner command processing loop so that we check the scheduling state
  // and yield if necessary as we may have unblocked a higher priority context.
  ExitCommandProcessingEarly();
  return error::kNoError;
}

void RasterDecoderImpl::DoFinish() {
  api()->glFinishFn();
  ProcessPendingQueries(true);
}

void RasterDecoderImpl::DoFlush() {
  api()->glFlushFn();
  ProcessPendingQueries(false);
}

void RasterDecoderImpl::DoGetIntegerv(GLenum pname,
                                      GLint* params,
                                      GLsizei params_size) {
  DCHECK(params);
  GLsizei num_written = 0;
  if (state_.GetStateAsGLint(pname, params, &num_written) ||
      GetHelper(pname, params, &num_written)) {
    DCHECK_EQ(num_written, params_size);
    return;
  }
  NOTREACHED() << "Unhandled enum " << pname;
}

GLuint RasterDecoderImpl::DoCreateTexture(
    bool use_buffer,
    gfx::BufferUsage /* buffer_usage */,
    viz::ResourceFormat /* resource_format */) {
  GLuint service_id = 0;
  api()->glGenTexturesFn(1, &service_id);
  DCHECK(service_id);
  return service_id;
}

void RasterDecoderImpl::CreateTexture(GLuint client_id,
                                      GLuint service_id,
                                      bool use_buffer,
                                      gfx::BufferUsage buffer_usage,
                                      viz::ResourceFormat resource_format) {
  texture_metadata_.emplace(std::make_pair(
      client_id, TextureMetadata(use_buffer, buffer_usage, resource_format,
                                 GetCapabilities())));
  texture_manager()->CreateTexture(client_id, service_id);
}

void RasterDecoderImpl::DoCreateAndConsumeTextureINTERNAL(
    GLuint client_id,
    bool use_buffer,
    gfx::BufferUsage buffer_usage,
    viz::ResourceFormat resource_format,
    const volatile GLbyte* key) {
  TRACE_EVENT2("gpu", "RasterDecoderImpl::DoCreateAndConsumeTextureINTERNAL",
               "context", logger_.GetLogPrefix(), "key[0]",
               static_cast<unsigned char>(key[0]));
  Mailbox mailbox =
      Mailbox::FromVolatile(*reinterpret_cast<const volatile Mailbox*>(key));
  DLOG_IF(ERROR, !mailbox.Verify()) << "CreateAndConsumeTexture was "
                                       "passed a mailbox that was not "
                                       "generated by GenMailboxCHROMIUM.";
  if (!client_id) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glCreateAndConsumeTextureCHROMIUM",
                       "invalid client id");
    return;
  }

  TextureRef* texture_ref = GetTexture(client_id);
  if (texture_ref) {
    // No need to create texture here, the client_id already has an associated
    // texture.
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glCreateAndConsumeTextureCHROMIUM",
                       "client id already in use");
    return;
  }

  texture_metadata_.emplace(std::make_pair(
      client_id, TextureMetadata(use_buffer, buffer_usage, resource_format,
                                 GetCapabilities())));

  Texture* texture =
      static_cast<Texture*>(group_->mailbox_manager()->ConsumeTexture(mailbox));
  if (!texture) {
    // Create texture to handle invalid mailbox (see http://crbug.com/472465).
    GLuint service_id = 0;
    api()->glGenTexturesFn(1, &service_id);
    DCHECK(service_id);
    texture_manager()->CreateTexture(client_id, service_id);

    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION,
                       "glCreateAndConsumeTextureCHROMIUM",
                       "invalid mailbox name");
    return;
  }

  texture_ref = texture_manager()->Consume(client_id, texture);

  // TODO(backer): Validate that the consumed texture is consistent with
  // TextureMetadata.
}

void RasterDecoderImpl::DeleteTexturesHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    TextureRef* texture_ref = GetTexture(client_id);
    if (texture_ref) {
      UnbindTexture(texture_ref);
      RemoveTexture(client_id);
    }
  }
}

bool RasterDecoderImpl::GenQueriesEXTHelper(GLsizei n,
                                            const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (query_manager_->IsValidQuery(client_ids[ii])) {
      return false;
    }
  }
  query_manager_->GenQueries(n, client_ids);
  return true;
}

void RasterDecoderImpl::DeleteQueriesEXTHelper(
    GLsizei n,
    const volatile GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = client_ids[ii];
    query_manager_->RemoveQuery(client_id);
  }
}

void RasterDecoderImpl::DoTexParameteri(GLuint client_id,
                                        GLenum pname,
                                        GLint param) {
  TextureRef* texture = texture_manager()->GetTexture(client_id);
  if (!texture) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexParameteri", "unknown texture");
    return;
  }

  TextureMetadata* texture_metadata = GetTextureMetadata(client_id);
  if (!texture_metadata) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexParameteri", "unknown texture");
    return;
  }

  // TextureManager uses validators from the share group, which may include
  // GLES2. Perform stronger validation here.
  bool valid_param = true;
  bool valid_pname = true;
  switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
      valid_param = validators_->texture_min_filter_mode.IsValid(param);
      break;
    case GL_TEXTURE_MAG_FILTER:
      valid_param = validators_->texture_mag_filter_mode.IsValid(param);
      break;
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
      valid_param = validators_->texture_wrap_mode.IsValid(param);
      break;
    default:
      valid_pname = false;
  }
  if (!valid_pname) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexParameteri", pname, "pname");
    return;
  }
  if (!valid_param) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexParameteri", param, "pname");
    return;
  }

  ScopedTextureBinder binder(&state_, texture_manager(), texture,
                             texture_metadata->target());

  texture_manager()->SetParameteri("glTexParameteri", GetErrorState(), texture,
                                   pname, param);
}

void RasterDecoderImpl::DoBindTexImage2DCHROMIUM(GLuint client_id,
                                                 GLint image_id) {
  TextureRef* texture_ref = GetTexture(client_id);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "BindTexImage2DCHROMIUM",
                       "unknown texture");
    return;
  }

  TextureMetadata* texture_metadata = GetTextureMetadata(client_id);
  if (!texture_metadata) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "BindTexImage2DCHROMIUM",
                       "unknown texture");
    return;
  }

  gl::GLImage* image = image_manager()->LookupImage(image_id);
  if (!image) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "BindTexImage2DCHROMIUM",
                       "no image found with the given ID");
    return;
  }

  Texture* texture = texture_ref->texture();
  if (texture->IsImmutable()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "BindTexImage2DCHROMIUM",
                       "texture is immutable");
    return;
  }

  Texture::ImageState image_state = Texture::UNBOUND;

  {
    ScopedTextureBinder binder(&state_, texture_manager(), texture_ref,
                               texture_metadata->target());

    if (image->BindTexImage(texture_metadata->target()))
      image_state = Texture::BOUND;
  }

  gfx::Size size = image->GetSize();
  texture_manager()->SetLevelInfo(
      texture_ref, texture_metadata->target(), 0, image->GetInternalFormat(),
      size.width(), size.height(), 1, 0, image->GetInternalFormat(),
      GL_UNSIGNED_BYTE, gfx::Rect(size));
  texture_manager()->SetLevelImage(texture_ref, texture_metadata->target(), 0,
                                   image, image_state);
}

void RasterDecoderImpl::DoReleaseTexImage2DCHROMIUM(GLuint client_id,
                                                    GLint image_id) {
  TRACE_EVENT0("gpu", "RasterDecoderImpl::DoReleaseTexImage2DCHROMIUM");

  TextureRef* texture_ref = GetTexture(client_id);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "ReleaseTexImage2DCHROMIUM",
                       "unknown texture");
    return;
  }

  TextureMetadata* texture_metadata = GetTextureMetadata(client_id);
  if (!texture_metadata) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "ReleaseTexImage2DCHROMIUM",
                       "unknown texture");
    return;
  }

  gl::GLImage* image = image_manager()->LookupImage(image_id);
  if (!image) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "ReleaseTexImage2DCHROMIUM",
                       "no image found with the given ID");
    return;
  }

  Texture::ImageState image_state;

  // Do nothing when image is not currently bound.
  if (texture_ref->texture()->GetLevelImage(texture_metadata->target(), 0,
                                            &image_state) != image)
    return;

  if (image_state == Texture::BOUND) {
    ScopedTextureBinder binder(&state_, texture_manager(), texture_ref,
                               texture_metadata->target());

    image->ReleaseTexImage(texture_metadata->target());
    texture_manager()->SetLevelInfo(texture_ref, texture_metadata->target(), 0,
                                    GL_RGBA, 0, 0, 1, 0, GL_RGBA,
                                    GL_UNSIGNED_BYTE, gfx::Rect());
  }

  texture_manager()->SetLevelImage(texture_ref, texture_metadata->target(), 0,
                                   nullptr, Texture::UNBOUND);
}

void RasterDecoderImpl::DoProduceTextureDirect(GLuint client_id,
                                               const volatile GLbyte* key) {
  TRACE_EVENT2("gpu", "RasterDecoderImpl::DoProduceTextureDirect", "context",
               logger_.GetLogPrefix(), "key[0]",
               static_cast<unsigned char>(key[0]));

  Mailbox mailbox =
      Mailbox::FromVolatile(*reinterpret_cast<const volatile Mailbox*>(key));
  DLOG_IF(ERROR, !mailbox.Verify()) << "ProduceTextureDirect was passed a "
                                       "mailbox that was not generated by "
                                       "GenMailboxCHROMIUM.";

  TextureRef* texture_ref = GetTexture(client_id);

  bool clear = !client_id;
  if (clear) {
    DCHECK(!texture_ref);

    group_->mailbox_manager()->ProduceTexture(mailbox, nullptr);
    return;
  }

  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "ProduceTextureDirect",
                       "unknown texture");
    return;
  }

  Texture* produced = texture_manager()->Produce(texture_ref);
  if (!produced) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "ProduceTextureDirect",
                       "invalid texture");
    return;
  }

  group_->mailbox_manager()->ProduceTexture(mailbox, produced);
}

void RasterDecoderImpl::TexStorage2D(TextureRef* texture_ref,
                                     const TextureMetadata& texture_metadata,
                                     GLint levels,
                                     GLsizei width,
                                     GLsizei height) {
  TRACE_EVENT2("gpu", "RasterDecoderImpl::TexStorage2D", "width", width,
               "height", height);

  if (!texture_manager()->ValidForTarget(texture_metadata.target(), 0, width,
                                         height, 1 /* depth */) ||
      TextureManager::ComputeMipMapCount(texture_metadata.target(), width,
                                         height, 1 /* depth */) < levels) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage2D",
                       "dimensions out of range");
    return;
  }

  ScopedTextureBinder binder(&state_, texture_manager(), texture_ref,
                             texture_metadata.target());

  unsigned int internal_format =
      viz::GLInternalFormat(texture_metadata.format());
  GLenum format =
      TextureManager::ExtractFormatFromStorageFormat(internal_format);
  GLenum type = TextureManager::ExtractTypeFromStorageFormat(internal_format);

  // First lookup compatibility format via texture manager for swizzling legacy
  // LUMINANCE/ALPHA formats.
  GLenum compatibility_internal_format =
      texture_manager()->AdjustTexStorageFormat(feature_info_.get(),
                                                internal_format);

  Texture* texture = texture_ref->texture();
  if (workarounds().reset_base_mipmap_level_before_texstorage &&
      texture->base_level() > 0)
    api()->glTexParameteriFn(texture_metadata.target(), GL_TEXTURE_BASE_LEVEL,
                             0);

  // TODO(zmo): We might need to emulate TexStorage using TexImage or
  // CompressedTexImage on Mac OSX where we expose ES3 APIs when the underlying
  // driver is lower than 4.2 and ARB_texture_storage extension doesn't exist.
  api()->glTexStorage2DEXTFn(texture_metadata.target(), levels,
                             compatibility_internal_format, width, height);
  if (workarounds().reset_base_mipmap_level_before_texstorage &&
      texture->base_level() > 0)
    api()->glTexParameteriFn(texture_metadata.target(), GL_TEXTURE_BASE_LEVEL,
                             texture->base_level());

  {
    GLsizei level_width = width;
    GLsizei level_height = height;

    GLenum adjusted_internal_format =
        feature_info_->IsWebGL1OrES2Context() ? format : internal_format;
    for (int ii = 0; ii < levels; ++ii) {
      texture_manager()->SetLevelInfo(texture_ref, texture_metadata.target(),
                                      ii, adjusted_internal_format, level_width,
                                      level_height, 1 /* level_depth */, 0,
                                      format, type, gfx::Rect());
      level_width = std::max(1, level_width >> 1);
      level_height = std::max(1, level_height >> 1);
    }
    texture->ApplyFormatWorkarounds(feature_info_.get());
  }
}

void RasterDecoderImpl::TexImage2D(TextureRef* texture_ref,
                                   const TextureMetadata& texture_metadata,
                                   GLsizei width,
                                   GLsizei height) {
  TRACE_EVENT2("gpu", "RasterDecoderImpl::TexImage2D", "width", width, "height",
               height);

  // Set as failed for now, but if it successed, this will be set to not failed.
  texture_state_.tex_image_failed = true;

  DCHECK(!state_.bound_pixel_unpack_buffer.get());

  ScopedTextureBinder binder(&state_, texture_manager(), texture_ref,
                             texture_metadata.target());

  TextureManager::DoTexImageArguments args = {
      texture_metadata.target(),
      0 /* level */,
      viz::GLInternalFormat(texture_metadata.format()),
      width,
      height,
      1 /* depth */,
      0 /* border */,
      viz::GLDataFormat(texture_metadata.format()),
      viz::GLDataType(texture_metadata.format()),
      nullptr /* pixels */,
      0 /* pixels_size */,
      0 /* padding */,
      TextureManager::DoTexImageArguments::kTexImage2D};
  texture_manager()->ValidateAndDoTexImage(
      &texture_state_, &state_, &framebuffer_state_, "glTexStorage2D", args);

  // This may be a slow command.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();
}

void RasterDecoderImpl::DoTexStorage2D(GLuint client_id,
                                       GLint levels,
                                       GLsizei width,
                                       GLsizei height) {
  TextureRef* texture_ref = GetTexture(client_id);
  if (!texture_ref) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage2D", "unknown texture");
    return;
  }

  TextureMetadata* texture_metadata = GetTextureMetadata(client_id);
  if (!texture_metadata) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage2D", "unknown texture");
    return;
  }

  Texture* texture = texture_ref->texture();
  if (texture->IsImmutable()) {
    LOCAL_SET_GL_ERROR(GL_INVALID_OPERATION, "glTexStorage2D",
                       "texture is immutable");
    return;
  }

  if (levels == 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage2D", "levels == 0");
    return;
  }

  if (width < 1 || height < 1) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage2D", "dimensions < 1");
    return;
  }

  // For testing only. Allows us to stress the ability to respond to OOM errors.
  if (workarounds().simulate_out_of_memory_on_large_textures &&
      (width * height >= 4096 * 4096)) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, "glTexStorage2D",
                       "synthetic out of memory");
    return;
  }

  // Check if we have enough memory.
  unsigned int internal_format =
      viz::GLInternalFormat(texture_metadata->format());
  bool is_compressed_format =
      viz::IsResourceFormatCompressed(texture_metadata->format());
  GLenum format =
      TextureManager::ExtractFormatFromStorageFormat(internal_format);
  GLenum type = TextureManager::ExtractTypeFromStorageFormat(internal_format);
  GLsizei level_width = width;
  GLsizei level_height = height;
  base::CheckedNumeric<uint32_t> estimated_size(0);
  PixelStoreParams params;
  params.alignment = 1;
  for (int ii = 0; ii < levels; ++ii) {
    uint32_t size;
    if (is_compressed_format) {
      GLsizei level_size;
      if (!GetCompressedTexSizeInBytes("glTexStorage2D", level_width,
                                       level_height, 1, internal_format,
                                       &level_size, state_.GetErrorState())) {
        return;
      }
      size = static_cast<uint32_t>(level_size);
    } else {
      if (!GLES2Util::ComputeImageDataSizesES3(
              level_width, level_height, 1 /* level_depth */, format, type,
              params, &size, nullptr, nullptr, nullptr, nullptr)) {
        LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, "glTexStorage2D",
                           "dimensions too large");
        return;
      }
    }
    estimated_size += size;
    level_width = std::max(1, level_width >> 1);
    level_height = std::max(1, level_height >> 1);
  }

  if (!estimated_size.IsValid() ||
      !EnsureGPUMemoryAvailable(estimated_size.ValueOrDefault(0))) {
    LOCAL_SET_GL_ERROR(GL_OUT_OF_MEMORY, "glTexStorage2D", "out of memory");
    return;
  }

  if (texture_metadata->use_buffer()) {
    if (!GetCapabilities().texture_storage_image) {
      LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage2DImage", "use_buffer");
      return;
    }
    DCHECK(levels == 1);
    TexStorage2DImage(texture_ref, *texture_metadata, width, height);
  } else if (GetCapabilities().texture_storage) {
    TexStorage2D(texture_ref, *texture_metadata, levels, width, height);
  } else {
    // TODO(backer): Support more than one texture level.
    DCHECK(levels == 1);
    TexImage2D(texture_ref, *texture_metadata, width, height);
  }

  texture->SetImmutable(true);
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "base/macros.h"
#include "gpu/command_buffer/service/raster_decoder_autogen.h"

}  // namespace raster
}  // namespace gpu
