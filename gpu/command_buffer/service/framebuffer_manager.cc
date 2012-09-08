// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "base/logging.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"

namespace gpu {
namespace gles2 {

class RenderbufferAttachment
    : public FramebufferManager::FramebufferInfo::Attachment {
 public:
  explicit RenderbufferAttachment(
      RenderbufferManager::RenderbufferInfo* renderbuffer)
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
      TextureManager* /* texture_manager */) OVERRIDE {
    renderbuffer_manager->SetCleared(renderbuffer_);
  }

  virtual bool IsTexture(
      TextureManager::TextureInfo* /* texture */) const OVERRIDE {
    return false;
  }

  virtual bool IsRenderbuffer(
       RenderbufferManager::RenderbufferInfo* renderbuffer) const OVERRIDE {
     return renderbuffer_ == renderbuffer;
  }

  virtual bool CanRenderTo() const OVERRIDE {
    return true;
  }

  virtual void DetachFromFramebuffer() OVERRIDE {
    // Nothing to do for renderbuffers.
  }

  virtual bool ValidForAttachmentType(GLenum attachment_type) OVERRIDE {
    uint32 need = GLES2Util::GetChannelsNeededForAttachmentType(
        attachment_type);
    uint32 have = GLES2Util::GetChannelsForFormat(internal_format());
    return (need & have) != 0;
  }

  RenderbufferManager::RenderbufferInfo* renderbuffer() const {
    return renderbuffer_.get();
  }

 protected:
  virtual ~RenderbufferAttachment() { }

 private:
  RenderbufferManager::RenderbufferInfo::Ref renderbuffer_;

  DISALLOW_COPY_AND_ASSIGN(RenderbufferAttachment);
};

class TextureAttachment
    : public FramebufferManager::FramebufferInfo::Attachment {
 public:
  TextureAttachment(
      TextureManager::TextureInfo* texture, GLenum target, GLint level)
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
      TextureManager* texture_manager) OVERRIDE {
    texture_manager->SetLevelCleared(texture_, target_, level_);
  }

  virtual bool IsTexture(TextureManager::TextureInfo* texture) const OVERRIDE {
    return texture == texture_.get();
  }

  virtual bool IsRenderbuffer(
       RenderbufferManager::RenderbufferInfo* /* renderbuffer */)
          const OVERRIDE {
    return false;
  }

  TextureManager::TextureInfo* texture() const {
    return texture_.get();
  }

  virtual bool CanRenderTo() const OVERRIDE {
    return texture_->CanRenderTo();
  }

  virtual void DetachFromFramebuffer() OVERRIDE {
    texture_->DetachFromFramebuffer();
  }

  virtual bool ValidForAttachmentType(GLenum attachment_type) OVERRIDE {
    GLenum type = 0;
    GLenum internal_format = 0;
    if (!texture_->GetLevelType(target_, level_, &type, &internal_format)) {
      return false;
    }
    uint32 need = GLES2Util::GetChannelsNeededForAttachmentType(
        attachment_type);
    uint32 have = GLES2Util::GetChannelsForFormat(internal_format);
    return (need & have) != 0;
  }

 protected:
  virtual ~TextureAttachment() {}

 private:
  TextureManager::TextureInfo::Ref texture_;
  GLenum target_;
  GLint level_;

  DISALLOW_COPY_AND_ASSIGN(TextureAttachment);
};

FramebufferManager::FramebufferManager()
    : framebuffer_state_change_count_(1),
      framebuffer_info_count_(0),
      have_context_(true) {
}

FramebufferManager::~FramebufferManager() {
  DCHECK(framebuffer_infos_.empty());
  // If this triggers, that means something is keeping a reference to a
  // FramebufferInfo belonging to this.
  CHECK_EQ(framebuffer_info_count_, 0u);
}

void FramebufferManager::FramebufferInfo::MarkAsDeleted() {
  deleted_ = true;
  while (!attachments_.empty()) {
    Attachment* attachment = attachments_.begin()->second.get();
    attachment->DetachFromFramebuffer();
    attachments_.erase(attachments_.begin());
  }
}

void FramebufferManager::Destroy(bool have_context) {
  have_context_ = have_context;
  framebuffer_infos_.clear();
}

void FramebufferManager::StartTracking(
    FramebufferManager::FramebufferInfo* /* framebuffer */) {
  ++framebuffer_info_count_;
}

void FramebufferManager::StopTracking(
    FramebufferManager::FramebufferInfo* /* framebuffer */) {
  --framebuffer_info_count_;
}

void FramebufferManager::CreateFramebufferInfo(
    GLuint client_id, GLuint service_id) {
  std::pair<FramebufferInfoMap::iterator, bool> result =
      framebuffer_infos_.insert(
          std::make_pair(
              client_id,
              FramebufferInfo::Ref(new FramebufferInfo(this, service_id))));
  DCHECK(result.second);
}

FramebufferManager::FramebufferInfo::FramebufferInfo(
    FramebufferManager* manager, GLuint service_id)
    : manager_(manager),
      deleted_(false),
      service_id_(service_id),
      has_been_bound_(false),
      framebuffer_complete_state_count_id_(0) {
  manager->StartTracking(this);
}

FramebufferManager::FramebufferInfo::~FramebufferInfo() {
  if (manager_) {
    if (manager_->have_context_) {
      GLuint id = service_id();
      glDeleteFramebuffersEXT(1, &id);
    }
    manager_->StopTracking(this);
    manager_ = NULL;
  }
}

bool FramebufferManager::FramebufferInfo::HasUnclearedAttachment(
    GLenum attachment) const {
  AttachmentMap::const_iterator it =
      attachments_.find(attachment);
  if (it != attachments_.end()) {
    const Attachment* attachment = it->second;
    return !attachment->cleared();
  }
  return false;
}

void FramebufferManager::FramebufferInfo::MarkAttachmentsAsCleared(
      RenderbufferManager* renderbuffer_manager,
      TextureManager* texture_manager) {
  for (AttachmentMap::iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    Attachment* attachment = it->second;
    if (!attachment->cleared()) {
      attachment->SetCleared(renderbuffer_manager, texture_manager);
    }
  }
}

bool FramebufferManager::FramebufferInfo::HasDepthAttachment() const {
  return attachments_.find(GL_DEPTH_STENCIL_ATTACHMENT) != attachments_.end() ||
         attachments_.find(GL_DEPTH_ATTACHMENT) != attachments_.end();
}

bool FramebufferManager::FramebufferInfo::HasStencilAttachment() const {
  return attachments_.find(GL_DEPTH_STENCIL_ATTACHMENT) != attachments_.end() ||
         attachments_.find(GL_STENCIL_ATTACHMENT) != attachments_.end();
}

GLenum FramebufferManager::FramebufferInfo::GetColorAttachmentFormat() const {
  AttachmentMap::const_iterator it = attachments_.find(GL_COLOR_ATTACHMENT0);
  if (it == attachments_.end()) {
    return 0;
  }
  const Attachment* attachment = it->second;
  return attachment->internal_format();
}

GLenum FramebufferManager::FramebufferInfo::IsPossiblyComplete() const {
  if (attachments_.empty()) {
    return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
  }

  GLsizei width = -1;
  GLsizei height = -1;
  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    GLenum attachment_type = it->first;
    Attachment* attachment = it->second;
    if (!attachment->ValidForAttachmentType(attachment_type)) {
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

bool FramebufferManager::FramebufferInfo::IsCleared() const {
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

void FramebufferManager::FramebufferInfo::UnbindRenderbuffer(
    GLenum target, RenderbufferManager::RenderbufferInfo* renderbuffer) {
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

void FramebufferManager::FramebufferInfo::UnbindTexture(
    GLenum target, TextureManager::TextureInfo* texture) {
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

FramebufferManager::FramebufferInfo* FramebufferManager::GetFramebufferInfo(
    GLuint client_id) {
  FramebufferInfoMap::iterator it = framebuffer_infos_.find(client_id);
  return it != framebuffer_infos_.end() ? it->second : NULL;
}

void FramebufferManager::RemoveFramebufferInfo(GLuint client_id) {
  FramebufferInfoMap::iterator it = framebuffer_infos_.find(client_id);
  if (it != framebuffer_infos_.end()) {
    it->second->MarkAsDeleted();
    framebuffer_infos_.erase(it);
  }
}

void FramebufferManager::FramebufferInfo::AttachRenderbuffer(
    GLenum attachment, RenderbufferManager::RenderbufferInfo* renderbuffer) {
  DCHECK(attachment == GL_COLOR_ATTACHMENT0 ||
         attachment == GL_DEPTH_ATTACHMENT ||
         attachment == GL_STENCIL_ATTACHMENT ||
         attachment == GL_DEPTH_STENCIL_ATTACHMENT);
  if (renderbuffer) {
    attachments_[attachment] = Attachment::Ref(
        new RenderbufferAttachment(renderbuffer));
  } else {
    attachments_.erase(attachment);
  }
  framebuffer_complete_state_count_id_ = 0;
}

void FramebufferManager::FramebufferInfo::AttachTexture(
    GLenum attachment, TextureManager::TextureInfo* texture, GLenum target,
    GLint level) {
  DCHECK(attachment == GL_COLOR_ATTACHMENT0 ||
         attachment == GL_DEPTH_ATTACHMENT ||
         attachment == GL_STENCIL_ATTACHMENT ||
         attachment == GL_DEPTH_STENCIL_ATTACHMENT);
  const Attachment* a = GetAttachment(attachment);
  if (a && a->IsTexture(texture)) {
    texture->DetachFromFramebuffer();
  }
  if (texture) {
    attachments_[attachment] = Attachment::Ref(
        new TextureAttachment(texture, target, level));
    texture->AttachToFramebuffer();
  } else {
    attachments_.erase(attachment);
  }
  framebuffer_complete_state_count_id_ = 0;
}

const FramebufferManager::FramebufferInfo::Attachment*
    FramebufferManager::FramebufferInfo::GetAttachment(
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
  for (FramebufferInfoMap::const_iterator it = framebuffer_infos_.begin();
       it != framebuffer_infos_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

void FramebufferManager::MarkAttachmentsAsCleared(
    FramebufferManager::FramebufferInfo* framebuffer,
    RenderbufferManager* renderbuffer_manager,
    TextureManager* texture_manager) {
  DCHECK(framebuffer);
  framebuffer->MarkAttachmentsAsCleared(renderbuffer_manager, texture_manager);
  MarkAsComplete(framebuffer);
}

void FramebufferManager::MarkAsComplete(
    FramebufferManager::FramebufferInfo* framebuffer) {
  DCHECK(framebuffer);
  framebuffer->MarkAsComplete(framebuffer_state_change_count_);
}

bool FramebufferManager::IsComplete(
    FramebufferManager::FramebufferInfo* framebuffer) {
  DCHECK(framebuffer);
  return framebuffer->framebuffer_complete_state_count_id() ==
      framebuffer_state_change_count_;
}

}  // namespace gles2
}  // namespace gpu


