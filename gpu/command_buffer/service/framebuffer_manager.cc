// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
namespace gles2 {

Framebuffer::FramebufferComboCompleteMap*
    Framebuffer::framebuffer_combo_complete_map_;

void Framebuffer::ClearFramebufferCompleteComboMap() {
  if (framebuffer_combo_complete_map_) {
    framebuffer_combo_complete_map_->clear();
  }
}

class RenderbufferAttachment
    : public Framebuffer::Attachment {
 public:
  explicit RenderbufferAttachment(
      Renderbuffer* renderbuffer)
      : renderbuffer_(renderbuffer) {
  }

  virtual GLsizei width() const OVERRIDE {
    return renderbuffer_->width();
  }

  virtual GLsizei height() const OVERRIDE {
    return renderbuffer_->height();
  }

  virtual GLenum internal_format() const OVERRIDE {
    return renderbuffer_->internal_format();
  }

  virtual GLsizei samples() const OVERRIDE {
    return renderbuffer_->samples();
  }

  virtual bool cleared() const OVERRIDE {
    return renderbuffer_->cleared();
  }

  virtual void SetCleared(
      RenderbufferManager* renderbuffer_manager,
      TextureManager* /* texture_manager */,
      bool cleared) OVERRIDE {
    renderbuffer_manager->SetCleared(renderbuffer_, cleared);
  }

  virtual bool IsTexture(
      Texture* /* texture */) const OVERRIDE {
    return false;
  }

  virtual bool IsRenderbuffer(
       Renderbuffer* renderbuffer) const OVERRIDE {
     return renderbuffer_ == renderbuffer;
  }

  virtual bool CanRenderTo() const OVERRIDE {
    return true;
  }

  virtual void DetachFromFramebuffer() const OVERRIDE {
    // Nothing to do for renderbuffers.
  }

  virtual bool ValidForAttachmentType(
      GLenum attachment_type, uint32 max_color_attachments) OVERRIDE {
    uint32 need = GLES2Util::GetChannelsNeededForAttachmentType(
        attachment_type, max_color_attachments);
    uint32 have = GLES2Util::GetChannelsForFormat(internal_format());
    return (need & have) != 0;
  }

  Renderbuffer* renderbuffer() const {
    return renderbuffer_.get();
  }

  virtual void AddToSignature(
      TextureManager* texture_manager, std::string* signature) const OVERRIDE {
    DCHECK(signature);
    renderbuffer_->AddToSignature(signature);
  }

 protected:
  virtual ~RenderbufferAttachment() { }

 private:
  scoped_refptr<Renderbuffer> renderbuffer_;

  DISALLOW_COPY_AND_ASSIGN(RenderbufferAttachment);
};

class TextureAttachment
    : public Framebuffer::Attachment {
 public:
  TextureAttachment(
      Texture* texture, GLenum target, GLint level)
      : texture_(texture),
        target_(target),
        level_(level) {
  }

  virtual GLsizei width() const OVERRIDE {
    GLsizei temp_width = 0;
    GLsizei temp_height = 0;
    texture_->GetLevelSize(target_, level_, &temp_width, &temp_height);
    return temp_width;
  }

  virtual GLsizei height() const OVERRIDE {
    GLsizei temp_width = 0;
    GLsizei temp_height = 0;
    texture_->GetLevelSize(target_, level_, &temp_width, &temp_height);
    return temp_height;
  }

  virtual GLenum internal_format() const OVERRIDE {
    GLenum temp_type = 0;
    GLenum temp_internal_format = 0;
    texture_->GetLevelType(target_, level_, &temp_type, &temp_internal_format);
    return temp_internal_format;
  }

  virtual GLsizei samples() const OVERRIDE {
    return 0;
  }

  virtual bool cleared() const OVERRIDE {
    return texture_->IsLevelCleared(target_, level_);
  }

  virtual void SetCleared(
      RenderbufferManager* /* renderbuffer_manager */,
      TextureManager* texture_manager,
      bool cleared) OVERRIDE {
    texture_manager->SetLevelCleared(texture_, target_, level_, cleared);
  }

  virtual bool IsTexture(Texture* texture) const OVERRIDE {
    return texture == texture_.get();
  }

  virtual bool IsRenderbuffer(
       Renderbuffer* /* renderbuffer */)
          const OVERRIDE {
    return false;
  }

  Texture* texture() const {
    return texture_.get();
  }

  virtual bool CanRenderTo() const OVERRIDE {
    return texture_->CanRenderTo();
  }

  virtual void DetachFromFramebuffer() const OVERRIDE {
    texture_->DetachFromFramebuffer();
  }

  virtual bool ValidForAttachmentType(
      GLenum attachment_type, uint32 max_color_attachments) OVERRIDE {
    GLenum type = 0;
    GLenum internal_format = 0;
    if (!texture_->GetLevelType(target_, level_, &type, &internal_format)) {
      return false;
    }
    uint32 need = GLES2Util::GetChannelsNeededForAttachmentType(
        attachment_type, max_color_attachments);
    uint32 have = GLES2Util::GetChannelsForFormat(internal_format);
    return (need & have) != 0;
  }

  virtual void AddToSignature(
      TextureManager* texture_manager, std::string* signature) const OVERRIDE {
    DCHECK(signature);
    texture_manager->AddToSignature(texture_, target_, level_, signature);
  }

 protected:
  virtual ~TextureAttachment() {}

 private:
  scoped_refptr<Texture> texture_;
  GLenum target_;
  GLint level_;

  DISALLOW_COPY_AND_ASSIGN(TextureAttachment);
};

FramebufferManager::FramebufferManager(
    uint32 max_draw_buffers, uint32 max_color_attachments)
    : framebuffer_state_change_count_(1),
      framebuffer_count_(0),
      have_context_(true),
      max_draw_buffers_(max_draw_buffers),
      max_color_attachments_(max_color_attachments) {
  DCHECK_GT(max_draw_buffers_, 0u);
  DCHECK_GT(max_color_attachments_, 0u);
}

FramebufferManager::~FramebufferManager() {
  DCHECK(framebuffers_.empty());
  // If this triggers, that means something is keeping a reference to a
  // Framebuffer belonging to this.
  CHECK_EQ(framebuffer_count_, 0u);
}

void Framebuffer::MarkAsDeleted() {
  deleted_ = true;
  while (!attachments_.empty()) {
    Attachment* attachment = attachments_.begin()->second.get();
    attachment->DetachFromFramebuffer();
    attachments_.erase(attachments_.begin());
  }
}

void FramebufferManager::Destroy(bool have_context) {
  have_context_ = have_context;
  framebuffers_.clear();
}

void FramebufferManager::StartTracking(
    Framebuffer* /* framebuffer */) {
  ++framebuffer_count_;
}

void FramebufferManager::StopTracking(
    Framebuffer* /* framebuffer */) {
  --framebuffer_count_;
}

void FramebufferManager::CreateFramebuffer(
    GLuint client_id, GLuint service_id) {
  std::pair<FramebufferMap::iterator, bool> result =
      framebuffers_.insert(
          std::make_pair(
              client_id,
              scoped_refptr<Framebuffer>(
                  new Framebuffer(this, service_id))));
  DCHECK(result.second);
}

Framebuffer::Framebuffer(
    FramebufferManager* manager, GLuint service_id)
    : manager_(manager),
      deleted_(false),
      service_id_(service_id),
      has_been_bound_(false),
      framebuffer_complete_state_count_id_(0) {
  manager->StartTracking(this);
  DCHECK_GT(manager->max_draw_buffers_, 0u);
  draw_buffers_.reset(new GLenum[manager->max_draw_buffers_]);
  draw_buffers_[0] = GL_COLOR_ATTACHMENT0;
  for (uint32 i = 1; i < manager->max_draw_buffers_; ++i)
    draw_buffers_[i] = GL_NONE;
}

Framebuffer::~Framebuffer() {
  if (manager_) {
    if (manager_->have_context_) {
      GLuint id = service_id();
      glDeleteFramebuffersEXT(1, &id);
    }
    manager_->StopTracking(this);
    manager_ = NULL;
  }
}

bool Framebuffer::HasUnclearedAttachment(
    GLenum attachment) const {
  AttachmentMap::const_iterator it =
      attachments_.find(attachment);
  if (it != attachments_.end()) {
    const Attachment* attachment = it->second;
    return !attachment->cleared();
  }
  return false;
}

void Framebuffer::MarkAttachmentAsCleared(
      RenderbufferManager* renderbuffer_manager,
      TextureManager* texture_manager,
      GLenum attachment,
      bool cleared) {
  AttachmentMap::iterator it = attachments_.find(attachment);
  if (it != attachments_.end()) {
    Attachment* a = it->second;
    if (a->cleared() != cleared) {
      a->SetCleared(renderbuffer_manager,
                    texture_manager,
                    cleared);
    }
  }
}

void Framebuffer::MarkAttachmentsAsCleared(
      RenderbufferManager* renderbuffer_manager,
      TextureManager* texture_manager,
      bool cleared) {
  for (AttachmentMap::iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    Attachment* attachment = it->second;
    if (attachment->cleared() != cleared) {
      attachment->SetCleared(renderbuffer_manager, texture_manager, cleared);
    }
  }
}

bool Framebuffer::HasDepthAttachment() const {
  return attachments_.find(GL_DEPTH_STENCIL_ATTACHMENT) != attachments_.end() ||
         attachments_.find(GL_DEPTH_ATTACHMENT) != attachments_.end();
}

bool Framebuffer::HasStencilAttachment() const {
  return attachments_.find(GL_DEPTH_STENCIL_ATTACHMENT) != attachments_.end() ||
         attachments_.find(GL_STENCIL_ATTACHMENT) != attachments_.end();
}

GLenum Framebuffer::GetColorAttachmentFormat() const {
  AttachmentMap::const_iterator it = attachments_.find(GL_COLOR_ATTACHMENT0);
  if (it == attachments_.end()) {
    return 0;
  }
  const Attachment* attachment = it->second;
  return attachment->internal_format();
}

GLenum Framebuffer::IsPossiblyComplete() const {
  if (attachments_.empty()) {
    return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
  }

  GLsizei width = -1;
  GLsizei height = -1;
  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    GLenum attachment_type = it->first;
    Attachment* attachment = it->second;
    if (!attachment->ValidForAttachmentType(
            attachment_type, manager_->max_color_attachments_)) {
      return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    }
    if (width < 0) {
      width = attachment->width();
      height = attachment->height();
      if (width == 0 || height == 0) {
        return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
      }
    } else {
      if (attachment->width() != width || attachment->height() != height) {
        return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT;
      }
    }

    if (!attachment->CanRenderTo()) {
      return GL_FRAMEBUFFER_UNSUPPORTED;
    }
  }

  // This does not mean the framebuffer is actually complete. It just means our
  // checks passed.
  return GL_FRAMEBUFFER_COMPLETE;
}

GLenum Framebuffer::GetStatus(
    TextureManager* texture_manager, GLenum target) const {
  // Check if we have this combo already.
  std::string signature(base::StringPrintf("|FBO|target=%04x", target));
  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    Attachment* attachment = it->second;
    signature += base::StringPrintf(
        "|Attachment|attachmentpoint=%04x", it->first);
    attachment->AddToSignature(texture_manager, &signature);
  }

  if (!framebuffer_combo_complete_map_) {
    framebuffer_combo_complete_map_ = new FramebufferComboCompleteMap();
  }

  FramebufferComboCompleteMap::const_iterator it =
      framebuffer_combo_complete_map_->find(signature);
  if (it != framebuffer_combo_complete_map_->end()) {
    return GL_FRAMEBUFFER_COMPLETE;
  }
  GLenum result = glCheckFramebufferStatusEXT(target);
  if (result == GL_FRAMEBUFFER_COMPLETE) {
    framebuffer_combo_complete_map_->insert(std::make_pair(signature, true));
  }
  return result;
}

bool Framebuffer::IsCleared() const {
  // are all the attachments cleaared?
  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    Attachment* attachment = it->second;
    if (!attachment->cleared()) {
      return false;
    }
  }
  return true;
}

GLenum Framebuffer::GetDrawBuffer(GLenum draw_buffer) const {
  GLsizei index = static_cast<GLsizei>(
      draw_buffer - GL_DRAW_BUFFER0_ARB);
  CHECK(index >= 0 &&
        index < static_cast<GLsizei>(manager_->max_draw_buffers_));
  return draw_buffers_[index];
}

void Framebuffer::SetDrawBuffers(GLsizei n, const GLenum* bufs) {
  DCHECK(n <= static_cast<GLsizei>(manager_->max_draw_buffers_));
  for (GLsizei i = 0; i < n; ++i)
    draw_buffers_[i] = bufs[i];
}

void Framebuffer::UnbindRenderbuffer(
    GLenum target, Renderbuffer* renderbuffer) {
  bool done;
  do {
    done = true;
    for (AttachmentMap::const_iterator it = attachments_.begin();
         it != attachments_.end(); ++it) {
      Attachment* attachment = it->second;
      if (attachment->IsRenderbuffer(renderbuffer)) {
        // TODO(gman): manually detach renderbuffer.
        // glFramebufferRenderbufferEXT(target, it->first, GL_RENDERBUFFER, 0);
        AttachRenderbuffer(it->first, NULL);
        done = false;
        break;
      }
    }
  } while (!done);
}

void Framebuffer::UnbindTexture(
    GLenum target, Texture* texture) {
  bool done;
  do {
    done = true;
    for (AttachmentMap::const_iterator it = attachments_.begin();
         it != attachments_.end(); ++it) {
      Attachment* attachment = it->second;
      if (attachment->IsTexture(texture)) {
        // TODO(gman): manually detach texture.
        // glFramebufferTexture2DEXT(target, it->first, GL_TEXTURE_2D, 0, 0);
        AttachTexture(it->first, NULL, GL_TEXTURE_2D, 0);
        done = false;
        break;
      }
    }
  } while (!done);
}

Framebuffer* FramebufferManager::GetFramebuffer(
    GLuint client_id) {
  FramebufferMap::iterator it = framebuffers_.find(client_id);
  return it != framebuffers_.end() ? it->second : NULL;
}

void FramebufferManager::RemoveFramebuffer(GLuint client_id) {
  FramebufferMap::iterator it = framebuffers_.find(client_id);
  if (it != framebuffers_.end()) {
    it->second->MarkAsDeleted();
    framebuffers_.erase(it);
  }
}

void Framebuffer::AttachRenderbuffer(
    GLenum attachment, Renderbuffer* renderbuffer) {
  const Attachment* a = GetAttachment(attachment);
  if (a)
    a->DetachFromFramebuffer();
  if (renderbuffer) {
    attachments_[attachment] = scoped_refptr<Attachment>(
        new RenderbufferAttachment(renderbuffer));
  } else {
    attachments_.erase(attachment);
  }
  framebuffer_complete_state_count_id_ = 0;
}

void Framebuffer::AttachTexture(
    GLenum attachment, Texture* texture, GLenum target,
    GLint level) {
  const Attachment* a = GetAttachment(attachment);
  if (a)
    a->DetachFromFramebuffer();
  if (texture) {
    attachments_[attachment] = scoped_refptr<Attachment>(
        new TextureAttachment(texture, target, level));
    texture->AttachToFramebuffer();
  } else {
    attachments_.erase(attachment);
  }
  framebuffer_complete_state_count_id_ = 0;
}

const Framebuffer::Attachment*
    Framebuffer::GetAttachment(
        GLenum attachment) const {
  AttachmentMap::const_iterator it = attachments_.find(attachment);
  if (it != attachments_.end()) {
    return it->second;
  }
  return NULL;
}

bool FramebufferManager::GetClientId(
    GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (FramebufferMap::const_iterator it = framebuffers_.begin();
       it != framebuffers_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

void FramebufferManager::MarkAttachmentsAsCleared(
    Framebuffer* framebuffer,
    RenderbufferManager* renderbuffer_manager,
    TextureManager* texture_manager) {
  DCHECK(framebuffer);
  framebuffer->MarkAttachmentsAsCleared(renderbuffer_manager,
                                        texture_manager,
                                        true);
  MarkAsComplete(framebuffer);
}

void FramebufferManager::MarkAsComplete(
    Framebuffer* framebuffer) {
  DCHECK(framebuffer);
  framebuffer->MarkAsComplete(framebuffer_state_change_count_);
}

bool FramebufferManager::IsComplete(
    Framebuffer* framebuffer) {
  DCHECK(framebuffer);
  return framebuffer->framebuffer_complete_state_count_id() ==
      framebuffer_state_change_count_;
}

}  // namespace gles2
}  // namespace gpu


