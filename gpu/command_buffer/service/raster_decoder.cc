// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/gles2_cmd_ids.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shader_translator.h"
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

// TODO(backer): Use a different set of commands.
RasterDecoder::CommandInfo RasterDecoder::command_info[] = {
#define GLES2_CMD_OP(name)                                   \
  {                                                          \
      nullptr, cmds::name::kArgFlags, cmds::name::cmd_flags, \
      sizeof(cmds::name) / sizeof(CommandBufferEntry) - 1,   \
  }, /* NOLINT */
    GLES2_COMMAND_LIST(GLES2_CMD_OP)
#undef GLES2_CMD_OP
};

RasterDecoder::RasterDecoder(GLES2DecoderClient* client,
                             CommandBufferServiceBase* command_buffer_service,
                             Outputter* outputter,
                             ContextGroup* group)
    : GLES2Decoder(command_buffer_service, outputter),
      commands_to_process_(0),
      current_decoder_error_(error::kNoError),
      client_(client),
      logger_(&debug_marker_manager_, client),
      group_(group),
      validators_(group_->feature_info()->validators()),
      feature_info_(group_->feature_info()),
      state_(group_->feature_info(), this, &logger_),
      weak_ptr_factory_(this) {}

RasterDecoder::~RasterDecoder() {}

base::WeakPtr<GLES2Decoder> RasterDecoder::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

gpu::ContextResult RasterDecoder::Initialize(
    const scoped_refptr<gl::GLSurface>& surface,
    const scoped_refptr<gl::GLContext>& context,
    bool offscreen,
    const DisallowedFeatures& disallowed_features,
    const ContextCreationAttribHelper& attrib_helper) {
  TRACE_EVENT0("gpu", "RasterDecoder::Initialize");
  DCHECK(context->IsCurrent(surface.get()));
  DCHECK(!context_.get());

  state_.set_api(gl::g_current_gl_context);

  set_initialized();

  // TODO(backer): Remove temporary hack once we use a separate set of
  // commands. Thread safe because Initialize is always called from CrGpuMain
  // thread.
  static bool updated_command_info = false;
  if (!updated_command_info) {
    updated_command_info = true;
    command_info[cmds::GetString::kCmdId - kFirstGLES2Command].cmd_handler =
        &RasterDecoder::HandleGetString;
    command_info[cmds::TraceBeginCHROMIUM::kCmdId - kFirstGLES2Command]
        .cmd_handler = &RasterDecoder::HandleTraceBeginCHROMIUM;
    command_info[cmds::TraceEndCHROMIUM::kCmdId - kFirstGLES2Command]
        .cmd_handler = &RasterDecoder::HandleTraceEndCHROMIUM;
    command_info[cmds::InsertFenceSyncCHROMIUM::kCmdId - kFirstGLES2Command]
        .cmd_handler = &RasterDecoder::HandleInsertFenceSyncCHROMIUM;
    command_info[cmds::WaitSyncTokenCHROMIUM::kCmdId - kFirstGLES2Command]
        .cmd_handler = &RasterDecoder::HandleWaitSyncTokenCHROMIUM;
  }

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

void RasterDecoder::Destroy(bool have_context) {}

void RasterDecoder::SetSurface(const scoped_refptr<gl::GLSurface>& surface) {
  NOTIMPLEMENTED();
}
void RasterDecoder::ReleaseSurface() {
  NOTIMPLEMENTED();
}

void RasterDecoder::TakeFrontBuffer(const Mailbox& mailbox) {
  NOTIMPLEMENTED();
}
void RasterDecoder::ReturnFrontBuffer(const Mailbox& mailbox, bool is_lost) {
  NOTIMPLEMENTED();
}
bool RasterDecoder::ResizeOffscreenFramebuffer(const gfx::Size& size) {
  NOTIMPLEMENTED();
  return true;
}

// Make this decoder's GL context current.
bool RasterDecoder::MakeCurrent() {
  DCHECK(surface_);
  if (!context_.get())
    return false;

  if (WasContextLost()) {
    LOG(ERROR) << "  GLES2DecoderImpl: Trying to make lost context current.";
    return false;
  }

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "  GLES2DecoderImpl: Context lost during MakeCurrent.";
    MarkContextLost(error::kMakeCurrentFailed);
    group_->LoseContexts(error::kUnknown);
    return false;
  }
  return true;
}

GLES2Util* RasterDecoder::GetGLES2Util() {
  NOTIMPLEMENTED();
  return nullptr;
}

gl::GLContext* RasterDecoder::GetGLContext() {
  return context_.get();
}

ContextGroup* RasterDecoder::GetContextGroup() {
  NOTIMPLEMENTED();
  return nullptr;
}

const FeatureInfo* RasterDecoder::GetFeatureInfo() const {
  NOTIMPLEMENTED();
  return nullptr;
}

Capabilities RasterDecoder::GetCapabilities() {
  gpu::Capabilities caps;
  caps.gpu_rasterization = true;
  caps.supports_oop_raster = true;
  return caps;
}

void RasterDecoder::RestoreState(const ContextState* prev_state) {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreActiveTexture() const {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreAllTextureUnitAndSamplerBindings(
    const ContextState* prev_state) const {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreActiveTextureUnitBinding(unsigned int target) const {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreBufferBinding(unsigned int target) {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreBufferBindings() const {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreFramebufferBindings() const {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreRenderbufferBindings() {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreGlobalState() const {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreProgramBindings() const {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreTextureState(unsigned service_id) const {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreTextureUnitBindings(unsigned unit) const {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreVertexAttribArray(unsigned index) {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreAllExternalTextureBindingsIfNeeded() {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreDeviceWindowRectangles() const {
  NOTIMPLEMENTED();
}

void RasterDecoder::ClearAllAttributes() const {
  NOTIMPLEMENTED();
}

void RasterDecoder::RestoreAllAttributes() const {
  NOTIMPLEMENTED();
}

void RasterDecoder::SetIgnoreCachedStateForTest(bool ignore) {
  NOTIMPLEMENTED();
}

void RasterDecoder::SetForceShaderNameHashingForTest(bool force) {
  NOTIMPLEMENTED();
}

uint32_t RasterDecoder::GetAndClearBackbufferClearBitsForTest() {
  NOTIMPLEMENTED();
  return 0;
}

size_t RasterDecoder::GetSavedBackTextureCountForTest() {
  NOTIMPLEMENTED();
  return 0;
}

size_t RasterDecoder::GetCreatedBackTextureCountForTest() {
  NOTIMPLEMENTED();
  return 0;
}

QueryManager* RasterDecoder::GetQueryManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

GpuFenceManager* RasterDecoder::GetGpuFenceManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

FramebufferManager* RasterDecoder::GetFramebufferManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

TransformFeedbackManager* RasterDecoder::GetTransformFeedbackManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

VertexArrayManager* RasterDecoder::GetVertexArrayManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

ImageManager* RasterDecoder::GetImageManagerForTest() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool RasterDecoder::HasPendingQueries() const {
  NOTIMPLEMENTED();
  return false;
}

void RasterDecoder::ProcessPendingQueries(bool did_finish) {
  NOTIMPLEMENTED();
}

bool RasterDecoder::HasMoreIdleWork() const {
  NOTIMPLEMENTED();
  return false;
}

void RasterDecoder::PerformIdleWork() {
  NOTIMPLEMENTED();
}

bool RasterDecoder::HasPollingWork() const {
  NOTIMPLEMENTED();
  return false;
}

void RasterDecoder::PerformPollingWork() {
  NOTIMPLEMENTED();
}

bool RasterDecoder::GetServiceTextureId(uint32_t client_texture_id,
                                        uint32_t* service_texture_id) {
  NOTIMPLEMENTED();
  return false;
}

TextureBase* RasterDecoder::GetTextureBase(uint32_t client_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool RasterDecoder::ClearLevel(Texture* texture,
                               unsigned target,
                               int level,
                               unsigned format,
                               unsigned type,
                               int xoffset,
                               int yoffset,
                               int width,
                               int height) {
  NOTIMPLEMENTED();
  return false;
}

bool RasterDecoder::ClearCompressedTextureLevel(Texture* texture,
                                                unsigned target,
                                                int level,
                                                unsigned format,
                                                int width,
                                                int height) {
  NOTIMPLEMENTED();
  return false;
}

bool RasterDecoder::IsCompressedTextureFormat(unsigned format) {
  NOTIMPLEMENTED();
  return false;
}

bool RasterDecoder::ClearLevel3D(Texture* texture,
                                 unsigned target,
                                 int level,
                                 unsigned format,
                                 unsigned type,
                                 int width,
                                 int height,
                                 int depth) {
  NOTIMPLEMENTED();
  return false;
}

ErrorState* RasterDecoder::GetErrorState() {
  return state_.GetErrorState();
}

void RasterDecoder::WaitForReadPixels(base::Closure callback) {
  NOTIMPLEMENTED();
}

bool RasterDecoder::WasContextLost() const {
  return false;
}

bool RasterDecoder::WasContextLostByRobustnessExtension() const {
  NOTIMPLEMENTED();
  return false;
}

void RasterDecoder::MarkContextLost(error::ContextLostReason reason) {
  NOTIMPLEMENTED();
}

bool RasterDecoder::CheckResetStatus() {
  NOTIMPLEMENTED();
  return false;
}

Logger* RasterDecoder::GetLogger() {
  return &logger_;
}

void RasterDecoder::BeginDecoding() {
  NOTIMPLEMENTED();
}

void RasterDecoder::EndDecoding() {
  NOTIMPLEMENTED();
}

const char* RasterDecoder::GetCommandName(unsigned int command_id) const {
  if (command_id >= kFirstGLES2Command && command_id < kNumCommands) {
    return gles2::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

error::Error RasterDecoder::DoCommands(unsigned int num_commands,
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

    const unsigned int arg_count = size - 1;
    unsigned int command_index = command - kFirstGLES2Command;
    if (command_index < arraysize(command_info)) {
      const CommandInfo& info = command_info[command_index];
      unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
      if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
          (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
        uint32_t immediate_data_size = (arg_count - info_arg_count) *
                                       sizeof(CommandBufferEntry);  // NOLINT
        if (info.cmd_handler == nullptr) {
          LOG(ERROR) << "[" << logger_.GetLogPrefix() << "] "
                     << GetCommandName(command) << "(" << command << ", "
                     << command_index << ") is NOTIMPLEMENTED";
        } else {
          result = (this->*info.cmd_handler)(immediate_data_size, cmd_data);
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

const ContextState* RasterDecoder::GetContextState() {
  NOTIMPLEMENTED();
  return nullptr;
}

scoped_refptr<ShaderTranslatorInterface> RasterDecoder::GetTranslator(
    unsigned int type) {
  NOTIMPLEMENTED();
  return nullptr;
}

void RasterDecoder::BindImage(uint32_t client_texture_id,
                              uint32_t texture_target,
                              gl::GLImage* image,
                              bool can_bind_to_sampler) {
  NOTIMPLEMENTED();
}

void RasterDecoder::OnContextLostError() {
  NOTIMPLEMENTED();
}

void RasterDecoder::OnOutOfMemoryError() {
  NOTIMPLEMENTED();
}

error::Error RasterDecoder::HandleTraceBeginCHROMIUM(
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

error::Error RasterDecoder::HandleTraceEndCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  debug_marker_manager_.PopGroup();
  return error::kNoError;
}

error::Error RasterDecoder::HandleInsertFenceSyncCHROMIUM(
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

error::Error RasterDecoder::HandleWaitSyncTokenCHROMIUM(
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
  sync_token.Set(namespace_id, 0, command_buffer_id, release);
  return client_->OnWaitSyncToken(sync_token) ? error::kDeferCommandUntilLater
                                              : error::kNoError;
}

error::Error RasterDecoder::HandleGetString(uint32_t immediate_data_size,
                                            const volatile void* cmd_data) {
  const volatile gles2::cmds::GetString& c =
      *static_cast<const volatile gles2::cmds::GetString*>(cmd_data);
  GLenum name = static_cast<GLenum>(c.name);

  // TODO(backer): Passthrough decoder does not validate. It's possible that
  // we don't have a validator there.
  if (!validators_->string_type.IsValid(name)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetString", name, "name");
    return error::kNoError;
  }

  const char* str = nullptr;
  std::string extensions;
  switch (name) {
    case GL_VERSION:
      str = GetServiceVersionString(feature_info_.get());
      break;
    case GL_SHADING_LANGUAGE_VERSION:
      str = GetServiceShadingLanguageVersionString(feature_info_.get());
      break;
    case GL_EXTENSIONS: {
      str = "";
      NOTIMPLEMENTED();
      break;
    }
    default:
      str = reinterpret_cast<const char*>(api()->glGetStringFn(name));
      break;
  }
  Bucket* bucket = CreateBucket(c.bucket_id);
  bucket->SetFromString(str);
  return error::kNoError;
}

}  // namespace raster
}  // namespace gpu
