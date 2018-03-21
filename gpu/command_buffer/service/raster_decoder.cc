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
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
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
  Capabilities GetCapabilities() override;
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

  // ErrorClientState implementation.
  void OnContextLostError() override;
  void OnOutOfMemoryError() override;

  Logger* GetLogger() override;

  void SetIgnoreCachedStateForTest(bool ignore) override;

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

  const gl::GLVersionInfo& gl_version_info() {
    return feature_info_->gl_version_info();
  }

  const TextureManager* texture_manager() const {
    return group_->texture_manager();
  }

  TextureManager* texture_manager() { return group_->texture_manager(); }

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
  void DoBindTexImage2DCHROMIUM(GLuint texture_id, GLint image_id) {
    NOTIMPLEMENTED();
  }
  void DoProduceTextureDirect(GLuint texture, const volatile GLbyte* key);
  void DoReleaseTexImage2DCHROMIUM(GLuint texture_id, GLint image_id) {
    NOTIMPLEMENTED();
  }
  void DoTexStorage2D(GLuint texture_id,
                      GLint levels,
                      GLsizei width,
                      GLsizei height) {
    NOTIMPLEMENTED();
  }
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
      commands_to_process_(0),
      current_decoder_error_(error::kNoError),
      client_(client),
      logger_(&debug_marker_manager_, client),
      group_(group),
      validators_(new Validators),
      feature_info_(group_->feature_info()),
      state_(group_->feature_info(), this, &logger_),
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

  return caps;
}

void RasterDecoderImpl::RestoreState(const ContextState* prev_state) {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreActiveTexture() const {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreAllTextureUnitAndSamplerBindings(
    const ContextState* prev_state) const {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreActiveTextureUnitBinding(
    unsigned int target) const {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreBufferBinding(unsigned int target) {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreBufferBindings() const {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreFramebufferBindings() const {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreRenderbufferBindings() {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreProgramBindings() const {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreTextureState(unsigned service_id) const {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreTextureUnitBindings(unsigned unit) const {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreVertexAttribArray(unsigned index) {
  NOTIMPLEMENTED();
}

void RasterDecoderImpl::RestoreAllExternalTextureBindingsIfNeeded() {
  NOTIMPLEMENTED();
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

void RasterDecoderImpl::DoTexParameteri(GLuint texture_id,
                                        GLenum pname,
                                        GLint param) {
  TextureRef* texture = texture_manager()->GetTexture(texture_id);
  if (!texture) {
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

  texture_manager()->SetParameteri("glTexParameteri", GetErrorState(), texture,
                                   pname, param);
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

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "base/macros.h"
#include "gpu/command_buffer/service/raster_decoder_autogen.h"

}  // namespace raster
}  // namespace gpu
