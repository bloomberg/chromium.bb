// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"

#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface.h"

namespace gpu {
namespace gles2 {

class GLES2DecoderPassthroughImpl : public GLES2Decoder {
 public:
  explicit GLES2DecoderPassthroughImpl(ContextGroup* group)
      : commands_to_process_(0),
        debug_marker_manager_(),
        logger_(&debug_marker_manager_),
        surface_(),
        context_(),
        group_(group),
        feature_info_(group->feature_info()) {
    DCHECK(group);
  }

  ~GLES2DecoderPassthroughImpl() override {}

  Error DoCommands(unsigned int num_commands,
                   const void* buffer,
                   int num_entries,
                   int* entries_processed) override {
    commands_to_process_ = num_commands;
    error::Error result = error::kNoError;
    const CommandBufferEntry* cmd_data =
        static_cast<const CommandBufferEntry*>(buffer);
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

      const unsigned int arg_count = size - 1;
      unsigned int command_index = command - kFirstGLES2Command;
      if (command_index < arraysize(command_info)) {
        const CommandInfo& info = command_info[command_index];
        unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
        if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
            (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
          uint32_t immediate_data_size = (arg_count - info_arg_count) *
                                         sizeof(CommandBufferEntry);  // NOLINT
          if (info.cmd_handler) {
            result = (this->*info.cmd_handler)(immediate_data_size, cmd_data);
          } else {
            result = error::kUnknownCommand;
          }
        } else {
          result = error::kInvalidArguments;
        }
      } else {
        result = DoCommonCommand(command, arg_count, cmd_data);
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

  const char* GetCommandName(unsigned int command_id) const override {
    if (command_id >= kFirstGLES2Command && command_id < kNumCommands) {
      return gles2::GetCommandName(static_cast<CommandId>(command_id));
    }
    return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
  }

  bool Initialize(const scoped_refptr<gl::GLSurface>& surface,
                  const scoped_refptr<gl::GLContext>& context,
                  bool offscreen,
                  const gfx::Size& offscreen_size,
                  const DisallowedFeatures& disallowed_features,
                  const ContextCreationAttribHelper& attrib_helper) override {
    // Take ownership of the context and surface. The surface can be replaced
    // with SetSurface.
    context_ = context;
    surface_ = surface;

    if (!group_->Initialize(this, attrib_helper.context_type,
                            disallowed_features)) {
      group_ = NULL;  // Must not destroy ContextGroup if it is not initialized.
      Destroy(true);
      return false;
    }

    image_manager_.reset(new ImageManager());

    set_initialized();
    return true;
  }

  // Destroys the graphics context.
  void Destroy(bool have_context) override {
    if (image_manager_.get()) {
      image_manager_->Destroy(have_context);
      image_manager_.reset();
    }
  }

  // Set the surface associated with the default FBO.
  void SetSurface(const scoped_refptr<gl::GLSurface>& surface) override {
    DCHECK(context_->IsCurrent(nullptr));
    DCHECK(surface_.get());
    surface_ = surface;
  }

  // Releases the surface associated with the GL context.
  // The decoder should not be used until a new surface is set.
  void ReleaseSurface() override {
    if (!context_.get())
      return;
    if (WasContextLost()) {
      DLOG(ERROR) << "  GLES2DecoderImpl: Trying to release lost context.";
      return;
    }
    context_->ReleaseCurrent(surface_.get());
    surface_ = nullptr;
  }

  void TakeFrontBuffer(const Mailbox& mailbox) override {}

  void ReturnFrontBuffer(const Mailbox& mailbox, bool is_lost) override {}

  // Resize an offscreen frame buffer.
  bool ResizeOffscreenFrameBuffer(const gfx::Size& size) override {
    return true;
  }

  // Make this decoder's GL context current.
  bool MakeCurrent() override {
    if (!context_.get())
      return false;

    if (!context_->MakeCurrent(surface_.get())) {
      LOG(ERROR) << "  GLES2DecoderImpl: Context lost during MakeCurrent.";
      MarkContextLost(error::kMakeCurrentFailed);
      group_->LoseContexts(error::kUnknown);
      return false;
    }

    return true;
  }

  // Gets the GLES2 Util which holds info.
  GLES2Util* GetGLES2Util() override { return nullptr; }

  // Gets the associated GLContext.
  gl::GLContext* GetGLContext() override { return nullptr; }

  // Gets the associated ContextGroup
  ContextGroup* GetContextGroup() override { return nullptr; }

  Capabilities GetCapabilities() override {
    DCHECK(initialized());
    Capabilities caps;
    return caps;
  }

  // Restores all of the decoder GL state.
  void RestoreState(const ContextState* prev_state) override {}

  // Restore States.
  void RestoreActiveTexture() const override {}
  void RestoreAllTextureUnitBindings(
      const ContextState* prev_state) const override {}
  void RestoreActiveTextureUnitBinding(unsigned int target) const override {}
  void RestoreBufferBindings() const override {}
  void RestoreFramebufferBindings() const override {}
  void RestoreRenderbufferBindings() override {}
  void RestoreGlobalState() const override {}
  void RestoreProgramBindings() const override {}
  void RestoreTextureState(unsigned service_id) const override {}
  void RestoreTextureUnitBindings(unsigned unit) const override {}
  void RestoreAllExternalTextureBindingsIfNeeded() override {}

  void ClearAllAttributes() const override {}
  void RestoreAllAttributes() const override {}

  void SetIgnoreCachedStateForTest(bool ignore) override {}
  void SetForceShaderNameHashingForTest(bool force) override {}
  size_t GetSavedBackTextureCountForTest() override { return 0; }
  size_t GetCreatedBackTextureCountForTest() override { return 0; }

  // Gets the QueryManager for this context.
  QueryManager* GetQueryManager() override { return nullptr; }

  // Gets the TransformFeedbackManager for this context.
  TransformFeedbackManager* GetTransformFeedbackManager() override {
    return nullptr;
  }

  // Gets the VertexArrayManager for this context.
  VertexArrayManager* GetVertexArrayManager() override { return nullptr; }

  // Gets the ImageManager for this context.
  ImageManager* GetImageManager() override { return image_manager_.get(); }

  // Returns false if there are no pending queries.
  bool HasPendingQueries() const override { return false; }

  // Process any pending queries.
  void ProcessPendingQueries(bool did_finish) override {}

  // Returns false if there is no idle work to be made.
  bool HasMoreIdleWork() const override { return false; }

  // Perform any idle work that needs to be made.
  void PerformIdleWork() override {}

  bool GetServiceTextureId(uint32_t client_texture_id,
                           uint32_t* service_texture_id) override {
    return false;
  }

  // Provides detail about a lost context if one occurred.
  error::ContextLostReason GetContextLostReason() override {
    return error::kUnknown;
  }

  // Clears a level sub area of a texture
  // Returns false if a GL error should be generated.
  bool ClearLevel(Texture* texture,
                  unsigned target,
                  int level,
                  unsigned format,
                  unsigned type,
                  int xoffset,
                  int yoffset,
                  int width,
                  int height) override {
    return true;
  }

  // Clears a level sub area of a compressed 2D texture.
  // Returns false if a GL error should be generated.
  bool ClearCompressedTextureLevel(Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   int width,
                                   int height) override {
    return true;
  }

  // Indicates whether a given internal format is one for a compressed
  // texture.
  bool IsCompressedTextureFormat(unsigned format) override { return false; };

  // Clears a level of a 3D texture.
  // Returns false if a GL error should be generated.
  bool ClearLevel3D(Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int width,
                    int height,
                    int depth) override {
    return true;
  }

  ErrorState* GetErrorState() override { return nullptr; }

  // A callback for messages from the decoder.
  void SetShaderCacheCallback(const ShaderCacheCallback& callback) override {}

  // Sets the callback for fence sync release and wait calls. The wait call
  // returns true if the channel is still scheduled.
  void SetFenceSyncReleaseCallback(
      const FenceSyncReleaseCallback& callback) override {}
  void SetWaitFenceSyncCallback(
      const WaitFenceSyncCallback& callback) override {}

  void WaitForReadPixels(base::Closure callback) override {}

  uint32_t GetTextureUploadCount() override { return 0; }

  base::TimeDelta GetTotalTextureUploadTime() override {
    return base::TimeDelta();
  }

  base::TimeDelta GetTotalProcessingCommandsTime() override {
    return base::TimeDelta();
  }

  void AddProcessingCommandsTime(base::TimeDelta) override {}

  // Returns true if the context was lost either by GL_ARB_robustness, forced
  // context loss or command buffer parse error.
  bool WasContextLost() const override { return false; }

  // Returns true if the context was lost specifically by GL_ARB_robustness.
  bool WasContextLostByRobustnessExtension() const override { return false; }

  // Lose this context.
  void MarkContextLost(error::ContextLostReason reason) override {}

  Logger* GetLogger() override { return &logger_; }

  const ContextState* GetContextState() override { return nullptr; }

 private:
  int commands_to_process_;

  DebugMarkerManager debug_marker_manager_;
  Logger logger_;

  using CmdHandler =
      Error (GLES2DecoderPassthroughImpl::*)(uint32_t immediate_data_size,
                                             const void* data);

  // A struct to hold info about each command.
  struct CommandInfo {
    CmdHandler cmd_handler;
    uint8_t arg_flags;   // How to handle the arguments for this scommand
    uint8_t cmd_flags;   // How to handle this command
    uint16_t arg_count;  // How many arguments are expected for this command.
  };

  // A table of CommandInfo for all the commands.
  static const CommandInfo command_info[kNumCommands - kFirstGLES2Command];

  // The GL context this decoder renders to on behalf of the client.
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;

  // Managers
  std::unique_ptr<ImageManager> image_manager_;

  // The ContextGroup for this decoder uses to track resources.
  scoped_refptr<ContextGroup> group_;
  scoped_refptr<FeatureInfo> feature_info_;
};

const GLES2DecoderPassthroughImpl::CommandInfo
    GLES2DecoderPassthroughImpl::command_info[] = {};

GLES2Decoder* CreateGLES2DecoderPassthroughImpl(ContextGroup* group) {
  return new GLES2DecoderPassthroughImpl(group);
}

}  // namespace gles2
}  // namespace gpu
