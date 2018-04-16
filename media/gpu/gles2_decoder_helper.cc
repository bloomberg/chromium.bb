// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/gles2_decoder_helper.h"

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_context.h"

namespace media {

class GLES2DecoderHelperImpl : public GLES2DecoderHelper {
 public:
  explicit GLES2DecoderHelperImpl(gpu::DecoderContext* decoder)
      : decoder_(decoder) {
    DCHECK(decoder_);
    gpu::gles2::ContextGroup* group = decoder_->GetContextGroup();
    texture_manager_ = group->texture_manager();
    mailbox_manager_ = group->mailbox_manager();
    // TODO(sandersd): Support GLES2DecoderPassthroughImpl.
    DCHECK(texture_manager_);
    DCHECK(mailbox_manager_);
  }

  bool MakeContextCurrent() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return decoder_->MakeCurrent();
  }

  scoped_refptr<gpu::gles2::TextureRef> CreateTexture(GLenum target,
                                                      GLenum internal_format,
                                                      GLsizei width,
                                                      GLsizei height,
                                                      GLenum format,
                                                      GLenum type) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(decoder_->GetGLContext()->IsCurrent(nullptr));

    // We can't use texture_manager->CreateTexture(), since it requires a unique
    // |client_id|. Instead we create the texture directly, and create our own
    // TextureRef for it.
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(target, texture_id);

    scoped_refptr<gpu::gles2::TextureRef> texture_ref =
        gpu::gles2::TextureRef::Create(texture_manager_, 0, texture_id);
    texture_manager_->SetTarget(texture_ref.get(), target);
    texture_manager_->SetLevelInfo(texture_ref.get(),  // ref
                                   target,             // target
                                   0,                  // level
                                   internal_format,    // internal_format
                                   width,              // width
                                   height,             // height
                                   1,                  // depth
                                   0,                  // border
                                   format,             // format
                                   type,               // type
                                   gfx::Rect());       // cleared_rect

    texture_manager_->SetParameteri(__func__, decoder_->GetErrorState(),
                                    texture_ref.get(), GL_TEXTURE_MAG_FILTER,
                                    GL_LINEAR);
    texture_manager_->SetParameteri(__func__, decoder_->GetErrorState(),
                                    texture_ref.get(), GL_TEXTURE_MIN_FILTER,
                                    GL_LINEAR);

    texture_manager_->SetParameteri(__func__, decoder_->GetErrorState(),
                                    texture_ref.get(), GL_TEXTURE_WRAP_S,
                                    GL_CLAMP_TO_EDGE);
    texture_manager_->SetParameteri(__func__, decoder_->GetErrorState(),
                                    texture_ref.get(), GL_TEXTURE_WRAP_T,
                                    GL_CLAMP_TO_EDGE);

    texture_manager_->SetParameteri(__func__, decoder_->GetErrorState(),
                                    texture_ref.get(), GL_TEXTURE_BASE_LEVEL,
                                    0);
    texture_manager_->SetParameteri(__func__, decoder_->GetErrorState(),
                                    texture_ref.get(), GL_TEXTURE_MAX_LEVEL, 0);

    // TODO(sandersd): Do we always want to allocate for GL_TEXTURE_2D?
    if (target == GL_TEXTURE_2D) {
      glTexImage2D(target,           // target
                   0,                // level
                   internal_format,  // internal_format
                   width,            // width
                   height,           // height
                   0,                // border
                   format,           // format
                   type,             // type
                   nullptr);         // data
    }

    decoder_->RestoreActiveTextureUnitBinding(target);
    return texture_ref;
  }

  void SetCleared(gpu::gles2::TextureRef* texture_ref) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    texture_manager_->SetLevelCleared(
        texture_ref, texture_ref->texture()->target(), 0, true);
  }

  gpu::Mailbox CreateMailbox(gpu::gles2::TextureRef* texture_ref) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    gpu::Mailbox mailbox = gpu::Mailbox::Generate();
    mailbox_manager_->ProduceTexture(mailbox, texture_ref->texture());
    return mailbox;
  }

 private:
  gpu::DecoderContext* decoder_;
  gpu::gles2::TextureManager* texture_manager_;
  gpu::MailboxManager* mailbox_manager_;
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(GLES2DecoderHelperImpl);
};

// static
std::unique_ptr<GLES2DecoderHelper> GLES2DecoderHelper::Create(
    gpu::DecoderContext* decoder) {
  return std::make_unique<GLES2DecoderHelperImpl>(decoder);
}

}  // namespace media
