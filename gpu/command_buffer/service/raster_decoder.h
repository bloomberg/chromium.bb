// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_H_
#define GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/gpu_export.h"

namespace gpu {
namespace raster {

// This class implements the AsyncAPIInterface interface, decoding
// RasterInterface commands and calling GL.
class GPU_EXPORT RasterDecoder : public gles2::GLES2Decoder,
                                 public gles2::ErrorStateClient {
 public:
  RasterDecoder(gles2::GLES2DecoderClient* client,
                CommandBufferServiceBase* command_buffer_service,
                gles2::Outputter* outputter,
                gles2::ContextGroup* group);

  ~RasterDecoder() override;

  // GLES2Decoder implementation.
  base::WeakPtr<GLES2Decoder> AsWeakPtr() override;
  gpu::ContextResult Initialize(
      const scoped_refptr<gl::GLSurface>& surface,
      const scoped_refptr<gl::GLContext>& context,
      bool offscreen,
      const gles2::DisallowedFeatures& disallowed_features,
      const gles2::ContextCreationAttribHelper& attrib_helper) override;
  void Destroy(bool have_context) override;
  void SetSurface(const scoped_refptr<gl::GLSurface>& surface) override;
  void ReleaseSurface() override;
  void TakeFrontBuffer(const Mailbox& mailbox) override;
  void ReturnFrontBuffer(const Mailbox& mailbox, bool is_lost) override;
  bool ResizeOffscreenFramebuffer(const gfx::Size& size) override;
  bool MakeCurrent() override;
  gles2::GLES2Util* GetGLES2Util() override;
  gl::GLContext* GetGLContext() override;
  gles2::ContextGroup* GetContextGroup() override;
  const gles2::FeatureInfo* GetFeatureInfo() const override;
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
  void RestoreGlobalState() const override;
  void RestoreProgramBindings() const override;
  void RestoreTextureState(unsigned service_id) const override;
  void RestoreTextureUnitBindings(unsigned unit) const override;
  void RestoreVertexAttribArray(unsigned index) override;
  void RestoreAllExternalTextureBindingsIfNeeded() override;
  void RestoreDeviceWindowRectangles() const override;
  void ClearAllAttributes() const override;
  void RestoreAllAttributes() const override;
  void SetIgnoreCachedStateForTest(bool ignore) override;
  void SetForceShaderNameHashingForTest(bool force) override;
  uint32_t GetAndClearBackbufferClearBitsForTest() override;
  size_t GetSavedBackTextureCountForTest() override;
  size_t GetCreatedBackTextureCountForTest() override;
  gles2::QueryManager* GetQueryManager() override;
  gles2::GpuFenceManager* GetGpuFenceManager() override;
  gles2::FramebufferManager* GetFramebufferManager() override;
  gles2::TransformFeedbackManager* GetTransformFeedbackManager() override;
  gles2::VertexArrayManager* GetVertexArrayManager() override;
  gles2::ImageManager* GetImageManagerForTest() override;
  bool HasPendingQueries() const override;
  void ProcessPendingQueries(bool did_finish) override;
  bool HasMoreIdleWork() const override;
  void PerformIdleWork() override;
  bool HasPollingWork() const override;
  void PerformPollingWork() override;
  bool GetServiceTextureId(uint32_t client_texture_id,
                           uint32_t* service_texture_id) override;
  gles2::TextureBase* GetTextureBase(uint32_t client_id) override;
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
  bool IsCompressedTextureFormat(unsigned format) override;
  bool ClearLevel3D(gles2::Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int width,
                    int height,
                    int depth) override;
  gles2::ErrorState* GetErrorState() override;
  void WaitForReadPixels(base::Closure callback) override;
  bool WasContextLost() const override;
  bool WasContextLostByRobustnessExtension() const override;
  void MarkContextLost(error::ContextLostReason reason) override;
  bool CheckResetStatus() override;
  gles2::Logger* GetLogger() override;
  void BeginDecoding() override;
  void EndDecoding() override;
  const char* GetCommandName(unsigned int command_id) const;
  error::Error DoCommands(unsigned int num_commands,
                          const volatile void* buffer,
                          int num_entries,
                          int* entries_processed) override;
  const gles2::ContextState* GetContextState() override;
  scoped_refptr<gles2::ShaderTranslatorInterface> GetTranslator(
      unsigned int type) override;
  void BindImage(uint32_t client_texture_id,
                 uint32_t texture_target,
                 gl::GLImage* image,
                 bool can_bind_to_sampler) override;

  // ErrorClientState implementation.
  void OnContextLostError() override;
  void OnOutOfMemoryError() override;

 private:
  gl::GLApi* api() const { return state_.api(); }

  // Set remaining commands to process to 0 to force DoCommands to return
  // and allow context preemption and GPU watchdog checks in CommandExecutor().
  void ExitCommandProcessingEarly() { commands_to_process_ = 0; }

  error::Error HandleGetString(uint32_t immediate_data_size,
                               const volatile void* cmd_data);
  error::Error HandleTraceBeginCHROMIUM(uint32_t immediate_data_size,
                                        const volatile void* cmd_data);
  error::Error HandleTraceEndCHROMIUM(uint32_t immediate_data_size,
                                      const volatile void* cmd_data);
  error::Error HandleInsertFenceSyncCHROMIUM(uint32_t immediate_data_size,
                                             const volatile void* cmd_data);
  error::Error HandleWaitSyncTokenCHROMIUM(uint32_t immediate_data_size,
                                           const volatile void* cmd_data);

  typedef gpu::gles2::GLES2Decoder::Error (RasterDecoder::*CmdHandler)(
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
  static CommandInfo
      command_info[gles2::kNumCommands - gles2::kFirstGLES2Command];

  // Number of commands remaining to be processed in DoCommands().
  int commands_to_process_;

  // The current decoder error communicates the decoder error through command
  // processing functions that do not return the error value. Should be set
  // only if not returning an error.
  error::Error current_decoder_error_;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;

  gles2::GLES2DecoderClient* client_;

  gles2::DebugMarkerManager debug_marker_manager_;
  gles2::Logger logger_;

  // The ContextGroup for this decoder uses to track resources.
  scoped_refptr<gles2::ContextGroup> group_;
  const gles2::Validators* validators_;
  scoped_refptr<gles2::FeatureInfo> feature_info_;

  // All the state for this context.
  gles2::ContextState state_;

  base::WeakPtrFactory<gles2::GLES2Decoder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RasterDecoder);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_H_
