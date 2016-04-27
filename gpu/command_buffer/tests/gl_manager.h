// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_TESTS_GL_MANAGER_H_
#define GPU_COMMAND_BUFFER_TESTS_GL_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace base {
class CommandLine;
}

namespace gfx {

class GLContext;
class GLShareGroup;
class GLSurface;

}

namespace gpu {

class CommandBufferService;
class CommandExecutor;
class SyncPointClient;
class SyncPointOrderData;
class SyncPointManager;
class TransferBuffer;

namespace gles2 {

class ContextGroup;
class MailboxManager;
class GLES2Decoder;
class GLES2CmdHelper;
class GLES2Implementation;
class ImageManager;
class ShareGroup;

};

class GLManager : private GpuControl {
 public:
  struct Options {
    Options();
    // The size of the backbuffer.
    gfx::Size size;
    // If not null will have a corresponding sync point manager.
    SyncPointManager* sync_point_manager;
    // If not null will share resources with this context.
    GLManager* share_group_manager;
    // If not null will share a mailbox manager with this context.
    GLManager* share_mailbox_manager;
    // If not null will create a virtual manager based on this context.
    GLManager* virtual_manager;
    // Whether or not glBindXXX generates a resource.
    bool bind_generates_resource;
    // Whether or not the context is auto-lost when GL_OUT_OF_MEMORY occurs.
    bool lose_context_when_out_of_memory;
    // Whether or not it's ok to lose the context.
    bool context_lost_allowed;
    gles2::ContextType context_type;
    // Force shader name hashing for all context types.
    bool force_shader_name_hashing;
  };
  GLManager();
  ~GLManager() override;

  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format);

  void Initialize(const Options& options);
  void InitializeWithCommandLine(const Options& options,
                                 const base::CommandLine& command_line);
  void Destroy();

  bool IsInitialized() const { return gles2_implementation() != nullptr; }

  void MakeCurrent();

  void SetSurface(gfx::GLSurface* surface);

  void set_use_iosurface_memory_buffers(bool use_iosurface_memory_buffers) {
    use_iosurface_memory_buffers_ = use_iosurface_memory_buffers;
  }

  void SetCommandsPaused(bool paused) { pause_commands_ = paused; }

  gles2::GLES2Decoder* decoder() const {
    return decoder_.get();
  }

  gles2::MailboxManager* mailbox_manager() const {
    return mailbox_manager_.get();
  }

  gfx::GLShareGroup* share_group() const {
    return share_group_.get();
  }

  gles2::GLES2Implementation* gles2_implementation() const {
    return gles2_implementation_.get();
  }

  gfx::GLContext* context() {
    return context_.get();
  }

  const GpuDriverBugWorkarounds& workarounds() const;

  // GpuControl implementation.
  void SetGpuControlClient(GpuControlClient*) override;
  Capabilities GetCapabilities() override;
  int32_t CreateImage(ClientBuffer buffer,
                      size_t width,
                      size_t height,
                      unsigned internalformat) override;
  void DestroyImage(int32_t id) override;
  int32_t CreateGpuMemoryBufferImage(size_t width,
                                     size_t height,
                                     unsigned internalformat,
                                     unsigned usage) override;
  void SignalQuery(uint32_t query, const base::Closure& callback) override;
  void SetLock(base::Lock*) override;
  bool IsGpuChannelLost() override;
  void EnsureWorkVisible() override;
  gpu::CommandBufferNamespace GetNamespaceID() const override;
  CommandBufferId GetCommandBufferID() const override;
  int32_t GetExtraCommandBufferData() const override;
  uint64_t GenerateFenceSyncRelease() override;
  bool IsFenceSyncRelease(uint64_t release) override;
  bool IsFenceSyncFlushed(uint64_t release) override;
  bool IsFenceSyncFlushReceived(uint64_t release) override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       const base::Closure& callback) override;
  bool CanWaitUnverifiedSyncToken(const gpu::SyncToken* sync_token) override;

 private:
  void PumpCommands();
  bool GetBufferChanged(int32_t transfer_buffer_id);
  void SetupBaseContext();
  void OnFenceSyncRelease(uint64_t release);
  bool OnWaitFenceSync(gpu::CommandBufferNamespace namespace_id,
                       gpu::CommandBufferId command_buffer_id,
                       uint64_t release);

  gpu::GpuPreferences gpu_preferences_;

  SyncPointManager* sync_point_manager_;  // Non-owning.

  scoped_refptr<SyncPointOrderData> sync_point_order_data_;
  std::unique_ptr<SyncPointClient> sync_point_client_;
  scoped_refptr<gles2::MailboxManager> mailbox_manager_;
  scoped_refptr<gfx::GLShareGroup> share_group_;
  std::unique_ptr<CommandBufferService> command_buffer_;
  std::unique_ptr<gles2::GLES2Decoder> decoder_;
  std::unique_ptr<CommandExecutor> executor_;
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_refptr<gfx::GLContext> context_;
  std::unique_ptr<gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<TransferBuffer> transfer_buffer_;
  std::unique_ptr<gles2::GLES2Implementation> gles2_implementation_;
  bool context_lost_allowed_;
  bool pause_commands_;
  uint32_t paused_order_num_;

  const CommandBufferId command_buffer_id_;
  uint64_t next_fence_sync_release_;

  bool use_iosurface_memory_buffers_ = false;

  // Used on Android to virtualize GL for all contexts.
  static int use_count_;
  static scoped_refptr<gfx::GLShareGroup>* base_share_group_;
  static scoped_refptr<gfx::GLSurface>* base_surface_;
  static scoped_refptr<gfx::GLContext>* base_context_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_TESTS_GL_MANAGER_H_
