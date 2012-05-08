// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_TESTS_GL_MANAGER_H_
#define GPU_COMMAND_BUFFER_TESTS_GL_MANAGER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/size.h"

namespace gfx {

class GLContext;
class GLShareGroup;
class GLSurface;

}

namespace gpu {

class CommandBufferService;
class TransferBuffer;
class GpuScheduler;

namespace gles2 {

class ContextGroup;
class MailboxManager;
class GLES2Decoder;
class GLES2CmdHelper;
class GLES2Implementation;
class ShareGroup;

};

class GLManager {
 public:
  GLManager();
  ~GLManager();

  void Initialize(const gfx::Size& size);
  void InitializeShared(const gfx::Size& size, GLManager* gl_manager);
  void InitializeSharedMailbox(const gfx::Size& size, GLManager* gl_manager);
  void Destroy();

  void MakeCurrent();

  gles2::MailboxManager* mailbox_manager() const {
    return mailbox_manager_.get();
  }

  gfx::GLShareGroup* share_group() const {
    return share_group_.get();
  }

  gles2::GLES2Implementation* gles2_implementation() const {
    return gles2_implementation_.get();
  }

 private:
  void Setup(
      const gfx::Size& size,
      gles2::MailboxManager* mailbox_manager,
      gfx::GLShareGroup* share_group,
      gles2::ContextGroup* context_group,
      gles2::ShareGroup* client_share_group);
  void PumpCommands();
  bool GetBufferChanged(int32 transfer_buffer_id);

  scoped_refptr<gles2::MailboxManager> mailbox_manager_;
  scoped_refptr<gfx::GLShareGroup> share_group_;
  scoped_ptr<CommandBufferService> command_buffer_;
  scoped_ptr<gles2::GLES2Decoder> decoder_;
  scoped_ptr<GpuScheduler> gpu_scheduler_;
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_refptr<gfx::GLContext> context_;
  scoped_ptr<gles2::GLES2CmdHelper> gles2_helper_;
  scoped_ptr<TransferBuffer> transfer_buffer_;
  scoped_ptr<gles2::GLES2Implementation> gles2_implementation_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_TESTS_GL_MANAGER_H_
