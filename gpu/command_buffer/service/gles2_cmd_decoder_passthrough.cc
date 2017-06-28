// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"

#include "base/strings/string_split.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {
namespace gles2 {

namespace {
template <typename ClientType, typename ServiceType, typename DeleteFunction>
void DeleteServiceObjects(ClientServiceMap<ClientType, ServiceType>* id_map,
                          bool have_context,
                          DeleteFunction delete_function) {
  if (have_context) {
    for (auto client_service_id_pair : *id_map) {
      delete_function(client_service_id_pair.second);
    }
  }

  id_map->Clear();
}

template <typename ClientType, typename ServiceType, typename ResultType>
bool GetClientID(const ClientServiceMap<ClientType, ServiceType>* map,
                 ResultType service_id,
                 ResultType* result) {
  ClientType client_id = 0;
  if (!map->GetClientID(static_cast<ServiceType>(service_id), &client_id)) {
    return false;
  }
  *result = static_cast<ResultType>(client_id);
  return true;
};

}  // anonymous namespace

PassthroughResources::PassthroughResources() {}

PassthroughResources::~PassthroughResources() {}

void PassthroughResources::Destroy(bool have_context) {
  DeleteServiceObjects(&texture_id_map, have_context,
                       [](GLuint texture) { glDeleteTextures(1, &texture); });
  DeleteServiceObjects(&buffer_id_map, have_context,
                       [](GLuint buffer) { glDeleteBuffersARB(1, &buffer); });
  DeleteServiceObjects(
      &renderbuffer_id_map, have_context,
      [](GLuint renderbuffer) { glDeleteRenderbuffersEXT(1, &renderbuffer); });
  DeleteServiceObjects(&sampler_id_map, have_context,
                       [](GLuint sampler) { glDeleteSamplers(1, &sampler); });
  DeleteServiceObjects(&program_id_map, have_context,
                       [](GLuint program) { glDeleteProgram(program); });
  DeleteServiceObjects(&shader_id_map, have_context,
                       [](GLuint shader) { glDeleteShader(shader); });
  DeleteServiceObjects(&sync_id_map, have_context, [](uintptr_t sync) {
    glDeleteSync(reinterpret_cast<GLsync>(sync));
  });

  if (!have_context) {
    for (auto passthrough_texture : texture_object_map) {
      passthrough_texture.second->MarkContextLost();
    }
  }
  texture_object_map.clear();
}

GLES2DecoderPassthroughImpl::GLES2DecoderPassthroughImpl(
    GLES2DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    ContextGroup* group)
    : GLES2Decoder(command_buffer_service),
      client_(client),
      commands_to_process_(0),
      debug_marker_manager_(),
      logger_(&debug_marker_manager_, client_),
      surface_(),
      context_(),
      offscreen_(false),
      group_(group),
      feature_info_(new FeatureInfo),
      gpu_decoder_category_(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("gpu_decoder"))),
      gpu_trace_level_(2),
      gpu_trace_commands_(false),
      gpu_debug_commands_(false),
      has_robustness_extension_(false),
      context_lost_(false),
      reset_by_robustness_extension_(false),
      weak_ptr_factory_(this) {
  DCHECK(client);
  DCHECK(group);
}

GLES2DecoderPassthroughImpl::~GLES2DecoderPassthroughImpl() {}

GLES2Decoder::Error GLES2DecoderPassthroughImpl::DoCommands(
    unsigned int num_commands,
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

template <bool DebugImpl>
GLES2Decoder::Error GLES2DecoderPassthroughImpl::DoCommandsImpl(
    unsigned int num_commands,
    const volatile void* buffer,
    int num_entries,
    int* entries_processed) {
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

    // size can't overflow because it is 21 bits.
    if (static_cast<int>(size) + process_pos > num_entries) {
      result = error::kOutOfBounds;
      break;
    }

    if (DebugImpl && log_commands()) {
      LOG(ERROR) << "[" << logger_.GetLogPrefix() << "]"
                 << "cmd: " << GetCommandName(command);
    }

    const unsigned int arg_count = size - 1;
    unsigned int command_index = command - kFirstGLES2Command;
    if (command_index < arraysize(command_info)) {
      const CommandInfo& info = command_info[command_index];
      unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
      if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
          (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
        bool doing_gpu_trace = false;
        if (DebugImpl && gpu_trace_commands_) {
          if (CMD_FLAG_GET_TRACE_LEVEL(info.cmd_flags) <= gpu_trace_level_) {
            doing_gpu_trace = true;
            gpu_tracer_->Begin(TRACE_DISABLED_BY_DEFAULT("gpu_decoder"),
                               GetCommandName(command), kTraceDecoder);
          }
        }

        uint32_t immediate_data_size = (arg_count - info_arg_count) *
                                       sizeof(CommandBufferEntry);  // NOLINT
        if (info.cmd_handler) {
          result = (this->*info.cmd_handler)(immediate_data_size, cmd_data);
        } else {
          result = error::kUnknownCommand;
        }

        if (DebugImpl && doing_gpu_trace) {
          gpu_tracer_->End(kTraceDecoder);
        }
      } else {
        result = error::kInvalidArguments;
      }
    } else {
      result = DoCommonCommand(command, arg_count, cmd_data);
    }

    if (result == error::kNoError && context_lost_) {
      result = error::kLostContext;
    }

    if (result != error::kDeferCommandUntilLater) {
      process_pos += size;
      cmd_data += size;
    }
  }

  if (entries_processed)
    *entries_processed = process_pos;

  return result;
}

base::WeakPtr<GLES2Decoder> GLES2DecoderPassthroughImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool GLES2DecoderPassthroughImpl::Initialize(
    const scoped_refptr<gl::GLSurface>& surface,
    const scoped_refptr<gl::GLContext>& context,
    bool offscreen,
    const DisallowedFeatures& disallowed_features,
    const ContextCreationAttribHelper& attrib_helper) {
  // Take ownership of the context and surface. The surface can be replaced
  // with SetSurface.
  context_ = context;
  surface_ = surface;
  offscreen_ = offscreen;

  // Create GPU Tracer for timing values.
  gpu_tracer_.reset(new GPUTracer(this));

  if (!group_->Initialize(this, attrib_helper.context_type,
                          disallowed_features)) {
    group_ = NULL;  // Must not destroy ContextGroup if it is not initialized.
    Destroy(true);
    return false;
  }

  // Each context initializes its own feature info because some extensions may
  // be enabled dynamically.  Don't disallow any features, leave it up to ANGLE
  // to dynamically enable extensions.
  if (!feature_info_->Initialize(attrib_helper.context_type,
                                 DisallowedFeatures())) {
    Destroy(true);
    return false;
  }

  // Check for required extensions
  if (!feature_info_->feature_flags().angle_robust_client_memory ||
      !feature_info_->feature_flags().chromium_bind_generates_resource ||
      !feature_info_->feature_flags().chromium_copy_texture ||
      !feature_info_->feature_flags().angle_client_arrays ||
      glIsEnabled(GL_CLIENT_ARRAYS_ANGLE) != GL_FALSE ||
      feature_info_->feature_flags().angle_webgl_compatibility !=
          IsWebGLContextType(attrib_helper.context_type) ||
      !feature_info_->feature_flags().angle_request_extension) {
    Destroy(true);
    return false;
  }

  bind_generates_resource_ = group_->bind_generates_resource();

  resources_ = group_->passthrough_resources();

  mailbox_manager_ = group_->mailbox_manager();

  // Query information about the texture units
  GLint num_texture_units = 0;
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &num_texture_units);

  active_texture_unit_ = 0;
  bound_textures_[GL_TEXTURE_2D].resize(num_texture_units, 0);
  bound_textures_[GL_TEXTURE_CUBE_MAP].resize(num_texture_units, 0);
  if (feature_info_->gl_version_info().IsAtLeastGLES(3, 0)) {
    bound_textures_[GL_TEXTURE_2D_ARRAY].resize(num_texture_units, 0);
    bound_textures_[GL_TEXTURE_3D].resize(num_texture_units, 0);
  }
  if (feature_info_->gl_version_info().IsAtLeastGLES(3, 1)) {
    bound_textures_[GL_TEXTURE_2D_MULTISAMPLE].resize(num_texture_units, 0);
  }
  if (feature_info_->feature_flags().oes_egl_image_external ||
      feature_info_->feature_flags().nv_egl_stream_consumer_external) {
    bound_textures_[GL_TEXTURE_EXTERNAL_OES].resize(num_texture_units, 0);
  }

  if (group_->gpu_preferences().enable_gpu_driver_debug_logging &&
      feature_info_->feature_flags().khr_debug) {
    InitializeGLDebugLogging();
  }

  has_robustness_extension_ = feature_info_->feature_flags().khr_robustness ||
                              feature_info_->feature_flags().ext_robustness;

  set_initialized();
  return true;
}

void GLES2DecoderPassthroughImpl::Destroy(bool have_context) {
  if (have_context) {
    FlushErrors();
  }

  DeleteServiceObjects(
      &framebuffer_id_map_, have_context,
      [](GLuint framebuffer) { glDeleteFramebuffersEXT(1, &framebuffer); });
  DeleteServiceObjects(&transform_feedback_id_map_, have_context,
                       [](GLuint transform_feedback) {
                         glDeleteTransformFeedbacks(1, &transform_feedback);
                       });
  DeleteServiceObjects(&query_id_map_, have_context,
                       [](GLuint query) { glDeleteQueries(1, &query); });
  DeleteServiceObjects(
      &vertex_array_id_map_, have_context,
      [](GLuint vertex_array) { glDeleteVertexArraysOES(1, &vertex_array); });

  // Destroy the GPU Tracer which may own some in process GPU Timings.
  if (gpu_tracer_) {
    gpu_tracer_->Destroy(have_context);
    gpu_tracer_.reset();
  }

  // Destroy the surface before the context, some surface destructors make GL
  // calls.
  surface_ = nullptr;

  if (group_) {
    group_->Destroy(this, have_context);
    group_ = nullptr;
  }

  if (context_.get()) {
    context_->ReleaseCurrent(nullptr);
    context_ = nullptr;
  }
}

void GLES2DecoderPassthroughImpl::SetSurface(
    const scoped_refptr<gl::GLSurface>& surface) {
  DCHECK(context_->IsCurrent(nullptr));
  DCHECK(surface_.get());
  surface_ = surface;
}

void GLES2DecoderPassthroughImpl::ReleaseSurface() {
  if (!context_.get())
    return;
  if (WasContextLost()) {
    DLOG(ERROR) << "  GLES2DecoderImpl: Trying to release lost context.";
    return;
  }
  context_->ReleaseCurrent(surface_.get());
  surface_ = nullptr;
}

void GLES2DecoderPassthroughImpl::TakeFrontBuffer(const Mailbox& mailbox) {}

void GLES2DecoderPassthroughImpl::ReturnFrontBuffer(const Mailbox& mailbox,
                                                    bool is_lost) {}

bool GLES2DecoderPassthroughImpl::ResizeOffscreenFramebuffer(
    const gfx::Size& size) {
  return true;
}

bool GLES2DecoderPassthroughImpl::MakeCurrent() {
  if (!context_.get())
    return false;

  if (WasContextLost()) {
    LOG(ERROR) << "  GLES2DecoderPassthroughImpl: Trying to make lost context "
                  "current.";
    return false;
  }

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR)
        << "  GLES2DecoderPassthroughImpl: Context lost during MakeCurrent.";
    MarkContextLost(error::kMakeCurrentFailed);
    group_->LoseContexts(error::kUnknown);
    return false;
  }

  if (CheckResetStatus()) {
    LOG(ERROR) << "  GLES2DecoderPassthroughImpl: Context reset detected after "
                  "MakeCurrent.";
    group_->LoseContexts(error::kUnknown);
    return false;
  }

  return true;
}

gpu::gles2::GLES2Util* GLES2DecoderPassthroughImpl::GetGLES2Util() {
  return nullptr;
}

gl::GLContext* GLES2DecoderPassthroughImpl::GetGLContext() {
  return context_.get();
}

gpu::gles2::ContextGroup* GLES2DecoderPassthroughImpl::GetContextGroup() {
  return group_.get();
}

const FeatureInfo* GLES2DecoderPassthroughImpl::GetFeatureInfo() const {
  return group_->feature_info();
}

gpu::Capabilities GLES2DecoderPassthroughImpl::GetCapabilities() {
  DCHECK(initialized());
  Capabilities caps;

  PopulateNumericCapabilities(&caps, feature_info_.get());

  glGetIntegerv(GL_BIND_GENERATES_RESOURCE_CHROMIUM,
                &caps.bind_generates_resource_chromium);
  DCHECK_EQ(caps.bind_generates_resource_chromium != GL_FALSE,
            group_->bind_generates_resource());

  caps.egl_image_external =
      feature_info_->feature_flags().oes_egl_image_external;
  caps.texture_format_astc =
      feature_info_->feature_flags().ext_texture_format_astc;
  caps.texture_format_atc =
      feature_info_->feature_flags().ext_texture_format_atc;
  caps.texture_format_bgra8888 =
      feature_info_->feature_flags().ext_texture_format_bgra8888;
  caps.texture_format_dxt1 =
      feature_info_->feature_flags().ext_texture_format_dxt1;
  caps.texture_format_dxt5 =
      feature_info_->feature_flags().ext_texture_format_dxt5;
  caps.texture_format_etc1 =
      feature_info_->feature_flags().oes_compressed_etc1_rgb8_texture;
  caps.texture_format_etc1_npot = caps.texture_format_etc1;
  caps.texture_rectangle = feature_info_->feature_flags().arb_texture_rectangle;
  caps.texture_usage = feature_info_->feature_flags().angle_texture_usage;
  caps.texture_storage = feature_info_->feature_flags().ext_texture_storage;
  caps.discard_framebuffer =
      feature_info_->feature_flags().ext_discard_framebuffer;
  caps.sync_query = feature_info_->feature_flags().chromium_sync_query;
#if defined(OS_MACOSX)
  // This is unconditionally true on mac, no need to test for it at runtime.
  caps.iosurface = true;
#endif
  caps.flips_vertically = surface_->FlipsVertically();
  caps.blend_equation_advanced =
      feature_info_->feature_flags().blend_equation_advanced;
  caps.blend_equation_advanced_coherent =
      feature_info_->feature_flags().blend_equation_advanced_coherent;
  caps.texture_rg = feature_info_->feature_flags().ext_texture_rg;
  caps.texture_norm16 = feature_info_->feature_flags().ext_texture_norm16;
  caps.texture_half_float_linear =
      feature_info_->feature_flags().enable_texture_half_float_linear;
  caps.image_ycbcr_422 =
      feature_info_->feature_flags().chromium_image_ycbcr_422;
  caps.image_ycbcr_420v =
      feature_info_->feature_flags().chromium_image_ycbcr_420v;
  caps.max_copy_texture_chromium_size =
      feature_info_->workarounds().max_copy_texture_chromium_size;
  caps.render_buffer_format_bgra8888 =
      feature_info_->feature_flags().ext_render_buffer_format_bgra8888;
  caps.occlusion_query_boolean =
      feature_info_->feature_flags().occlusion_query_boolean;
  caps.timer_queries = feature_info_->feature_flags().ext_disjoint_timer_query;
  caps.post_sub_buffer = surface_->SupportsPostSubBuffer();
  caps.surfaceless = !offscreen_ && surface_->IsSurfaceless();
  caps.flips_vertically = !offscreen_ && surface_->FlipsVertically();
  caps.multisample_compatibility =
      feature_info_->feature_flags().ext_multisample_compatibility;

  // TODO:
  // caps.commit_overlay_planes

  return caps;
}

void GLES2DecoderPassthroughImpl::RestoreState(const ContextState* prev_state) {

}

void GLES2DecoderPassthroughImpl::RestoreActiveTexture() const {}

void GLES2DecoderPassthroughImpl::RestoreAllTextureUnitAndSamplerBindings(
    const ContextState* prev_state) const {}

void GLES2DecoderPassthroughImpl::RestoreActiveTextureUnitBinding(
    unsigned int target) const {}

void GLES2DecoderPassthroughImpl::RestoreBufferBinding(unsigned int target) {}

void GLES2DecoderPassthroughImpl::RestoreBufferBindings() const {}

void GLES2DecoderPassthroughImpl::RestoreFramebufferBindings() const {}

void GLES2DecoderPassthroughImpl::RestoreRenderbufferBindings() {}

void GLES2DecoderPassthroughImpl::RestoreGlobalState() const {}

void GLES2DecoderPassthroughImpl::RestoreProgramBindings() const {}

void GLES2DecoderPassthroughImpl::RestoreTextureState(
    unsigned service_id) const {}

void GLES2DecoderPassthroughImpl::RestoreTextureUnitBindings(
    unsigned unit) const {}

void GLES2DecoderPassthroughImpl::RestoreVertexAttribArray(unsigned index) {}

void GLES2DecoderPassthroughImpl::RestoreAllExternalTextureBindingsIfNeeded() {}

void GLES2DecoderPassthroughImpl::ClearAllAttributes() const {}

void GLES2DecoderPassthroughImpl::RestoreAllAttributes() const {}

void GLES2DecoderPassthroughImpl::SetIgnoreCachedStateForTest(bool ignore) {}

void GLES2DecoderPassthroughImpl::SetForceShaderNameHashingForTest(bool force) {

}

size_t GLES2DecoderPassthroughImpl::GetSavedBackTextureCountForTest() {
  return 0;
}

size_t GLES2DecoderPassthroughImpl::GetCreatedBackTextureCountForTest() {
  return 0;
}

gpu::gles2::QueryManager* GLES2DecoderPassthroughImpl::GetQueryManager() {
  return nullptr;
}

gpu::gles2::FramebufferManager*
GLES2DecoderPassthroughImpl::GetFramebufferManager() {
  return nullptr;
}

gpu::gles2::TransformFeedbackManager*
GLES2DecoderPassthroughImpl::GetTransformFeedbackManager() {
  return nullptr;
}

gpu::gles2::VertexArrayManager*
GLES2DecoderPassthroughImpl::GetVertexArrayManager() {
  return nullptr;
}

gpu::gles2::ImageManager*
GLES2DecoderPassthroughImpl::GetImageManagerForTest() {
  return group_->image_manager();
}

bool GLES2DecoderPassthroughImpl::HasPendingQueries() const {
  return !pending_queries_.empty();
}

void GLES2DecoderPassthroughImpl::ProcessPendingQueries(bool did_finish) {
  // TODO(geofflang): If this returned an error, store it somewhere.
  ProcessQueries(did_finish);
}

bool GLES2DecoderPassthroughImpl::HasMoreIdleWork() const {
  return gpu_tracer_->HasTracesToProcess();
}

void GLES2DecoderPassthroughImpl::PerformIdleWork() {
  gpu_tracer_->ProcessTraces();
}

bool GLES2DecoderPassthroughImpl::HasPollingWork() const {
  return false;
}

void GLES2DecoderPassthroughImpl::PerformPollingWork() {}

bool GLES2DecoderPassthroughImpl::GetServiceTextureId(
    uint32_t client_texture_id,
    uint32_t* service_texture_id) {
  return resources_->texture_id_map.GetServiceID(client_texture_id,
                                                 service_texture_id);
}

bool GLES2DecoderPassthroughImpl::ClearLevel(Texture* texture,
                                             unsigned target,
                                             int level,
                                             unsigned format,
                                             unsigned type,
                                             int xoffset,
                                             int yoffset,
                                             int width,
                                             int height) {
  return true;
}

bool GLES2DecoderPassthroughImpl::ClearCompressedTextureLevel(Texture* texture,
                                                              unsigned target,
                                                              int level,
                                                              unsigned format,
                                                              int width,
                                                              int height) {
  return true;
}

bool GLES2DecoderPassthroughImpl::IsCompressedTextureFormat(unsigned format) {
  return false;
}

bool GLES2DecoderPassthroughImpl::ClearLevel3D(Texture* texture,
                                               unsigned target,
                                               int level,
                                               unsigned format,
                                               unsigned type,
                                               int width,
                                               int height,
                                               int depth) {
  return true;
}

gpu::gles2::ErrorState* GLES2DecoderPassthroughImpl::GetErrorState() {
  return nullptr;
}

void GLES2DecoderPassthroughImpl::WaitForReadPixels(base::Closure callback) {}

bool GLES2DecoderPassthroughImpl::WasContextLost() const {
  return context_lost_;
}

bool GLES2DecoderPassthroughImpl::WasContextLostByRobustnessExtension() const {
  return WasContextLost() && reset_by_robustness_extension_;
}

void GLES2DecoderPassthroughImpl::MarkContextLost(
    error::ContextLostReason reason) {
  // Only lose the context once.
  if (WasContextLost()) {
    return;
  }

  // Don't make GL calls in here, the context might not be current.
  command_buffer_service()->SetContextLostReason(reason);
  context_lost_ = true;
}

gpu::gles2::Logger* GLES2DecoderPassthroughImpl::GetLogger() {
  return &logger_;
}

void GLES2DecoderPassthroughImpl::BeginDecoding() {
  gpu_tracer_->BeginDecoding();
  gpu_trace_commands_ = gpu_tracer_->IsTracing() && *gpu_decoder_category_;
  gpu_debug_commands_ = log_commands() || debug() || gpu_trace_commands_;
}

void GLES2DecoderPassthroughImpl::EndDecoding() {
  gpu_tracer_->EndDecoding();
}

const gpu::gles2::ContextState* GLES2DecoderPassthroughImpl::GetContextState() {
  return nullptr;
}

scoped_refptr<ShaderTranslatorInterface>
GLES2DecoderPassthroughImpl::GetTranslator(GLenum type) {
  return nullptr;
}

const char* GLES2DecoderPassthroughImpl::GetCommandName(
    unsigned int command_id) const {
  if (command_id >= kFirstGLES2Command && command_id < kNumCommands) {
    return gles2::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

void* GLES2DecoderPassthroughImpl::GetScratchMemory(size_t size) {
  if (scratch_memory_.size() < size) {
    scratch_memory_.resize(size, 0);
  }
  return scratch_memory_.data();
}

template <typename T>
error::Error GLES2DecoderPassthroughImpl::PatchGetNumericResults(GLenum pname,
                                                                 GLsizei length,
                                                                 T* params) {
  // Likely a gl error if no parameters were returned
  if (length < 1) {
    return error::kNoError;
  }

  switch (pname) {
    case GL_NUM_EXTENSIONS:
      // Currently handled on the client side.
      params[0] = 0;
      break;

    case GL_TEXTURE_BINDING_2D:
    case GL_TEXTURE_BINDING_CUBE_MAP:
    case GL_TEXTURE_BINDING_2D_ARRAY:
    case GL_TEXTURE_BINDING_3D:
      if (!GetClientID(&resources_->texture_id_map, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_ARRAY_BUFFER_BINDING:
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
    case GL_PIXEL_PACK_BUFFER_BINDING:
    case GL_PIXEL_UNPACK_BUFFER_BINDING:
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case GL_COPY_READ_BUFFER_BINDING:
    case GL_COPY_WRITE_BUFFER_BINDING:
    case GL_UNIFORM_BUFFER_BINDING:
      if (!GetClientID(&resources_->buffer_id_map, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_RENDERBUFFER_BINDING:
      if (!GetClientID(&resources_->renderbuffer_id_map, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_SAMPLER_BINDING:
      if (!GetClientID(&resources_->sampler_id_map, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_ACTIVE_PROGRAM:
      if (!GetClientID(&resources_->program_id_map, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_FRAMEBUFFER_BINDING:
    case GL_READ_FRAMEBUFFER_BINDING:
      if (!GetClientID(&framebuffer_id_map_, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_TRANSFORM_FEEDBACK_BINDING:
      if (!GetClientID(&transform_feedback_id_map_, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_VERTEX_ARRAY_BINDING:
      if (!GetClientID(&vertex_array_id_map_, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    default:
      break;
  }

  return error::kNoError;
}

// Instantiate templated functions
#define INSTANTIATE_PATCH_NUMERIC_RESULTS(type)                              \
  template error::Error GLES2DecoderPassthroughImpl::PatchGetNumericResults( \
      GLenum, GLsizei, type*)
INSTANTIATE_PATCH_NUMERIC_RESULTS(GLint);
INSTANTIATE_PATCH_NUMERIC_RESULTS(GLint64);
INSTANTIATE_PATCH_NUMERIC_RESULTS(GLfloat);
INSTANTIATE_PATCH_NUMERIC_RESULTS(GLboolean);
#undef INSTANTIATE_PATCH_NUMERIC_RESULTS

error::Error
GLES2DecoderPassthroughImpl::PatchGetFramebufferAttachmentParameter(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei length,
    GLint* params) {
  // Likely a gl error if no parameters were returned
  if (length < 1) {
    return error::kNoError;
  }

  switch (pname) {
    // If the attached object name was requested, it needs to be converted back
    // to a client id.
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: {
      GLint object_type = GL_NONE;
      glGetFramebufferAttachmentParameterivEXT(
          target, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
          &object_type);

      switch (object_type) {
        case GL_TEXTURE:
          if (!GetClientID(&resources_->texture_id_map, *params, params)) {
            return error::kInvalidArguments;
          }
          break;

        case GL_RENDERBUFFER:
          if (!GetClientID(&resources_->renderbuffer_id_map, *params, params)) {
            return error::kInvalidArguments;
          }
          break;

        case GL_NONE:
          // Default framebuffer, don't transform the result
          break;

        default:
          NOTREACHED();
          break;
      }
    } break;

    default:
      break;
  }

  return error::kNoError;
}

void GLES2DecoderPassthroughImpl::InsertError(GLenum error,
                                              const std::string&) {
  // Message ignored for now
  errors_.insert(error);
}

GLenum GLES2DecoderPassthroughImpl::PopError() {
  GLenum error = GL_NO_ERROR;
  if (!errors_.empty()) {
    error = *errors_.begin();
    errors_.erase(errors_.begin());
  }
  return error;
}

bool GLES2DecoderPassthroughImpl::FlushErrors() {
  bool had_error = false;
  GLenum error = glGetError();
  while (error != GL_NO_ERROR) {
    errors_.insert(error);
    had_error = true;

    // Check for context loss on out-of-memory errors
    if (error == GL_OUT_OF_MEMORY && !WasContextLost() && CheckResetStatus()) {
      MarkContextLost(error::kOutOfMemory);
      group_->LoseContexts(error::kUnknown);
      break;
    }

    error = glGetError();
  }
  return had_error;
}

bool GLES2DecoderPassthroughImpl::CheckResetStatus() {
  DCHECK(!WasContextLost());
  DCHECK(context_->IsCurrent(nullptr));

  if (IsRobustnessSupported()) {
    // If the reason for the call was a GL error, we can try to determine the
    // reset status more accurately.
    GLenum driver_status = glGetGraphicsResetStatusARB();
    if (driver_status == GL_NO_ERROR) {
      return false;
    }

    switch (driver_status) {
      case GL_GUILTY_CONTEXT_RESET_ARB:
        MarkContextLost(error::kGuilty);
        break;
      case GL_INNOCENT_CONTEXT_RESET_ARB:
        MarkContextLost(error::kInnocent);
        break;
      case GL_UNKNOWN_CONTEXT_RESET_ARB:
        MarkContextLost(error::kUnknown);
        break;
      default:
        NOTREACHED();
        return false;
    }
    reset_by_robustness_extension_ = true;
    return true;
  }
  return false;
}

bool GLES2DecoderPassthroughImpl::IsRobustnessSupported() {
  return has_robustness_extension_ &&
         context_->WasAllocatedUsingRobustnessExtension();
}

bool GLES2DecoderPassthroughImpl::IsEmulatedQueryTarget(GLenum target) const {
  // GL_COMMANDS_COMPLETED_CHROMIUM is implemented in ANGLE
  switch (target) {
    case GL_COMMANDS_ISSUED_CHROMIUM:
    case GL_LATENCY_QUERY_CHROMIUM:
    case GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM:
    case GL_GET_ERROR_QUERY_CHROMIUM:
      return true;

    default:
      return false;
  }
}

error::Error GLES2DecoderPassthroughImpl::ProcessQueries(bool did_finish) {
  while (!pending_queries_.empty()) {
    const PendingQuery& query = pending_queries_.front();
    GLuint result_available = GL_FALSE;
    GLuint64 result = 0;
    switch (query.target) {
      case GL_COMMANDS_ISSUED_CHROMIUM:
        result_available = GL_TRUE;
        result = GL_TRUE;
        break;

      case GL_LATENCY_QUERY_CHROMIUM:
        result_available = GL_TRUE;
        // TODO: time from when the query is ended?
        result = (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds();
        break;

      case GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM:
        // TODO: Use a fence and do a real async readback
        result_available = GL_TRUE;
        result = GL_TRUE;
        break;

      case GL_GET_ERROR_QUERY_CHROMIUM:
        result_available = GL_TRUE;
        FlushErrors();
        result = PopError();
        break;

      default:
        DCHECK(!IsEmulatedQueryTarget(query.target));
        if (did_finish) {
          result_available = GL_TRUE;
        } else {
          glGetQueryObjectuiv(query.service_id, GL_QUERY_RESULT_AVAILABLE,
                              &result_available);
        }
        if (result_available == GL_TRUE) {
          if (feature_info_->feature_flags().ext_disjoint_timer_query) {
            glGetQueryObjectui64v(query.service_id, GL_QUERY_RESULT, &result);
          } else {
            GLuint temp_result = 0;
            glGetQueryObjectuiv(query.service_id, GL_QUERY_RESULT,
                                &temp_result);
            result = temp_result;
          }
        }
        break;
    }

    if (!result_available) {
      break;
    }

    QuerySync* sync = GetSharedMemoryAs<QuerySync*>(
        query.shm_id, query.shm_offset, sizeof(QuerySync));
    if (sync == nullptr) {
      pending_queries_.pop_front();
      return error::kOutOfBounds;
    }

    // Mark the query as complete
    sync->result = result;
    base::subtle::Release_Store(&sync->process_count, query.submit_count);
    pending_queries_.pop_front();
  }

  // If glFinish() has been called, all of our queries should be completed.
  DCHECK(!did_finish || pending_queries_.empty());
  return error::kNoError;
}

void GLES2DecoderPassthroughImpl::RemovePendingQuery(GLuint service_id) {
  auto pending_iter =
      std::find_if(pending_queries_.begin(), pending_queries_.end(),
                   [service_id](const PendingQuery& pending_query) {
                     return pending_query.service_id == service_id;
                   });
  if (pending_iter != pending_queries_.end()) {
    QuerySync* sync = GetSharedMemoryAs<QuerySync*>(
        pending_iter->shm_id, pending_iter->shm_offset, sizeof(QuerySync));
    if (sync != nullptr) {
      sync->result = 0;
      base::subtle::Release_Store(&sync->process_count,
                                  pending_iter->submit_count);
    }

    pending_queries_.erase(pending_iter);
  }
}

void GLES2DecoderPassthroughImpl::UpdateTextureBinding(GLenum target,
                                                       GLuint client_id,
                                                       GLuint service_id) {
  size_t cur_texture_unit = active_texture_unit_;
  const auto& target_bound_textures = bound_textures_.at(target);
  for (size_t bound_texture_index = 0;
       bound_texture_index < target_bound_textures.size();
       bound_texture_index++) {
    GLuint bound_client_id = target_bound_textures[bound_texture_index];
    if (bound_client_id == client_id) {
      // Update the active texture unit if needed
      if (bound_texture_index != cur_texture_unit) {
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + bound_texture_index));
        cur_texture_unit = bound_texture_index;
      }

      // Update the texture binding
      glBindTexture(target, service_id);
    }
  }

  // Reset the active texture unit if it was changed
  if (cur_texture_unit != active_texture_unit_) {
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + active_texture_unit_));
  }
}

error::Error GLES2DecoderPassthroughImpl::BindTexImage2DCHROMIUMImpl(
    GLenum target,
    GLenum internalformat,
    GLint imageId) {
  if (target != GL_TEXTURE_2D) {
    InsertError(GL_INVALID_ENUM, "Invalid target");
    return error::kNoError;
  }

  gl::GLImage* image = group_->image_manager()->LookupImage(imageId);
  if (image == nullptr) {
    InsertError(GL_INVALID_OPERATION, "No image found with the given ID");
    return error::kNoError;
  }

  if (internalformat) {
    if (!image->BindTexImageWithInternalformat(target, internalformat)) {
      image->CopyTexImage(target);
    }
  } else {
    if (!image->BindTexImage(target)) {
      image->CopyTexImage(target);
    }
  }

  return error::kNoError;
}

#define GLES2_CMD_OP(name)                                               \
  {                                                                      \
      &GLES2DecoderPassthroughImpl::Handle##name, cmds::name::kArgFlags, \
      cmds::name::cmd_flags,                                             \
      sizeof(cmds::name) / sizeof(CommandBufferEntry) - 1,               \
  }, /* NOLINT */

const GLES2DecoderPassthroughImpl::CommandInfo
    GLES2DecoderPassthroughImpl::command_info[] = {
        GLES2_COMMAND_LIST(GLES2_CMD_OP)};

#undef GLES2_CMD_OP

}  // namespace gles2
}  // namespace gpu
