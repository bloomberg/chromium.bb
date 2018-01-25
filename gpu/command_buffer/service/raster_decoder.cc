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
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/common/raster_cmd_ids.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/command_buffer/service/logger.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

// Local versions of the SET_GL_ERROR macros
#define LOCAL_SET_GL_ERROR(error, function_name, msg) \
  ERRORSTATE_SET_GL_ERROR(state_.GetErrorState(), error, function_name, msg)
#define LOCAL_SET_GL_ERROR_INVALID_ENUM(function_name, value, label)          \
  ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(state_.GetErrorState(), function_name, \
                                       value, label)
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

class RasterDecoderImpl : public RasterDecoder, public gles2::ErrorStateClient {
 public:
  RasterDecoderImpl(DecoderClient* client,
                    CommandBufferServiceBase* command_buffer_service,
                    gles2::Outputter* outputter,
                    gles2::ContextGroup* group);
  ~RasterDecoderImpl() override;

  // DecoderContext implementation.
  base::WeakPtr<DecoderContext> AsWeakPtr() override;
  gpu::ContextResult Initialize(
      const scoped_refptr<gl::GLSurface>& surface,
      const scoped_refptr<gl::GLContext>& context,
      bool offscreen,
      const gles2::DisallowedFeatures& disallowed_features,
      const ContextCreationAttribs& attrib_helper) override;
  bool initialized() const override;
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
  gles2::QueryManager* GetQueryManager() override;
  gles2::GpuFenceManager* GetGpuFenceManager() override;
  bool HasPendingQueries() const override;
  void ProcessPendingQueries(bool did_finish) override;
  bool HasMoreIdleWork() const override;
  void PerformIdleWork() override;
  bool HasPollingWork() const override;
  void PerformPollingWork() override;
  TextureBase* GetTextureBase(uint32_t client_id) override;
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

 private:
  gl::GLApi* api() const { return state_.api(); }

  const FeatureInfo::FeatureFlags& features() const {
    return feature_info_->feature_flags();
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

  // Gets the number of values that will be returned by glGetXXX. Returns
  // false if pname is unknown.
  bool GetNumValuesReturnedForGLGet(GLenum pname, GLsizei* num_values) {
    NOTIMPLEMENTED();
    return false;
  }

  void DoActiveTexture(GLenum texture_unit) { NOTIMPLEMENTED(); }
  void DoBindTexture(GLenum target, GLuint texture) { NOTIMPLEMENTED(); }
  void DeleteTexturesHelper(GLsizei n, const volatile GLuint* client_ids) {
    NOTIMPLEMENTED();
  }
  bool GenTexturesHelper(GLsizei n, const GLuint* client_ids) {
    NOTIMPLEMENTED();
    return true;
  }
  bool GenQueriesEXTHelper(GLsizei n, const GLuint* client_ids) {
    NOTIMPLEMENTED();
    return true;
  }
  void DeleteQueriesEXTHelper(GLsizei n, const volatile GLuint* client_ids) {
    NOTIMPLEMENTED();
  }
  void DoFinish() { NOTIMPLEMENTED(); }
  void DoFlush() { NOTIMPLEMENTED(); }
  void DoGetIntegerv(GLenum pname, GLint* params, GLsizei params_size) {
    NOTIMPLEMENTED();
  }
  void DoTexParameteri(GLenum target, GLenum pname, GLint param) {
    NOTIMPLEMENTED();
  }
  void DoTexStorage2DEXT(GLenum target,
                         GLsizei levels,
                         GLenum internal_format,
                         GLsizei width,
                         GLsizei height) {
    NOTIMPLEMENTED();
  }
  void DoTexStorage2DImageCHROMIUM(GLenum target,
                                   GLenum internal_format,
                                   GLenum buffer_usage,
                                   GLsizei width,
                                   GLsizei height) {
    NOTIMPLEMENTED();
  }
  void DoCopySubTextureCHROMIUM(GLuint source_id,
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
                                GLboolean unpack_unmultiply_alpha) {
    NOTIMPLEMENTED();
  }
  void DoCompressedCopyTextureCHROMIUM(GLuint source_id, GLuint dest_id) {
    NOTIMPLEMENTED();
  }
  void DoProduceTextureDirectCHROMIUM(GLuint texture,
                                      const volatile GLbyte* key) {
    NOTIMPLEMENTED();
  }
  void DoBindTexImage2DCHROMIUM(GLenum target, GLint image_id) {
    NOTIMPLEMENTED();
  }
  void DoReleaseTexImage2DCHROMIUM(GLenum target, GLint image_id) {
    NOTIMPLEMENTED();
  }
  void DoTraceEndCHROMIUM();
  void DoLoseContextCHROMIUM(GLenum current, GLenum other) { NOTIMPLEMENTED(); }
  void DoBeginRasterCHROMIUM(GLuint texture_id,
                             GLuint sk_color,
                             GLuint msaa_sample_count,
                             GLboolean can_use_lcd_text,
                             GLboolean use_distance_field_text,
                             GLint pixel_config) {
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

  bool initialized_;

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
  const gles2::Validators* validators_;
  scoped_refptr<gles2::FeatureInfo> feature_info_;

  // All the state for this context.
  gles2::ContextState state_;

  bool gpu_debug_commands_;

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
      debug_(false),
      log_commands_(false) {}

RasterDecoder::~RasterDecoder() {}

RasterDecoderImpl::RasterDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    Outputter* outputter,
    ContextGroup* group)
    : RasterDecoder(command_buffer_service),
      initialized_(false),
      commands_to_process_(0),
      current_decoder_error_(error::kNoError),
      client_(client),
      logger_(&debug_marker_manager_, client),
      group_(group),
      validators_(group_->feature_info()->validators()),
      feature_info_(group_->feature_info()),
      state_(group_->feature_info(), this, &logger_),
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

  initialized_ = true;

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

  return gpu::ContextResult::kSuccess;
}

bool RasterDecoderImpl::initialized() const {
  return initialized_;
}

const gles2::ContextState* RasterDecoderImpl::GetContextState() {
  NOTIMPLEMENTED();
  return nullptr;
}

void RasterDecoderImpl::Destroy(bool have_context) {}

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
  NOTIMPLEMENTED();
  return nullptr;
}

GpuFenceManager* RasterDecoderImpl::GetGpuFenceManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool RasterDecoderImpl::HasPendingQueries() const {
  NOTIMPLEMENTED();
  return false;
}

void RasterDecoderImpl::ProcessPendingQueries(bool did_finish) {
  NOTIMPLEMENTED();
}

bool RasterDecoderImpl::HasMoreIdleWork() const {
  NOTIMPLEMENTED();
  return false;
}

void RasterDecoderImpl::PerformIdleWork() {
  NOTIMPLEMENTED();
}

bool RasterDecoderImpl::HasPollingWork() const {
  NOTIMPLEMENTED();
  return false;
}

void RasterDecoderImpl::PerformPollingWork() {
  NOTIMPLEMENTED();
}

TextureBase* RasterDecoderImpl::GetTextureBase(uint32_t client_id) {
  NOTIMPLEMENTED();
  return nullptr;
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

void RasterDecoderImpl::BeginDecoding() {
  // TODO(backer): Add support the tracing commands.
  gpu_debug_commands_ = log_commands() || debug();
}

void RasterDecoderImpl::EndDecoding() {
  NOTIMPLEMENTED();
}

const char* RasterDecoderImpl::GetCommandName(unsigned int command_id) const {
  if (command_id >= kFirstRasterCommand && command_id < kNumCommands) {
    return GetCommandName(static_cast<CommandId>(command_id));
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

error::Error RasterDecoderImpl::HandleTraceBeginCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TraceBeginCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::TraceBeginCHROMIUM*>(cmd_data);
  Bucket* category_bucket = GetBucket(c.category_bucket_id);
  Bucket* name_bucket = GetBucket(c.name_bucket_id);
  if (!category_bucket || category_bucket->size() == 0 || !name_bucket ||
      name_bucket->size() == 0) {
    return error::kInvalidArguments;
  }

  std::string category_name;
  std::string trace_name;
  if (!category_bucket->GetAsString(&category_name) ||
      !name_bucket->GetAsString(&trace_name)) {
    return error::kInvalidArguments;
  }

  debug_marker_manager_.PushGroup(trace_name);
  return error::kNoError;
}

void RasterDecoderImpl::DoTraceEndCHROMIUM() {
  debug_marker_manager_.PopGroup();
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

error::Error RasterDecoderImpl::HandleCompressedTexImage2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleCompressedTexImage2DBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandlePixelStorei(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleTexImage2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleTexSubImage2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleBeginQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleEndQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  NOTIMPLEMENTED();
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

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "base/macros.h"
#include "gpu/command_buffer/service/raster_cmd_decoder_autogen.h"

}  // namespace raster
}  // namespace gpu
