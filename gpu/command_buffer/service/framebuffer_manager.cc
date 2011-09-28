// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
      RenderbufferManager::RenderbufferInfo* render_buffer)
      : render_buffer_(render_buffer) {
  }

  virtual ~RenderbufferAttachment() { }

  virtual GLsizei width() const {
    return render_buffer_->width();
  }

  virtual GLsizei height() const {
    return render_buffer_->height();
  }

  virtual GLenum internal_format() const {
    return render_buffer_->internal_format();
  }

  virtual GLsizei samples() const {
    return render_buffer_->samples();
  }

  virtual bool cleared() const {
    return render_buffer_->cleared();
  }

  virtual void set_cleared() {
    render_buffer_->set_cleared();
  }

  virtual bool IsTexture(TextureManager::TextureInfo* /* texture */) const {
    return false;
  }

  virtual bool CanRenderTo() const {
    return true;
  }

  RenderbufferManager::RenderbufferInfo* render_buffer() const {
    return render_buffer_.get();
  }

 private:
  RenderbufferManager::RenderbufferInfo::Ref render_buffer_;

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

  virtual ~TextureAttachment() { }

  virtual GLsizei width() const {
    GLsizei temp_width = 0;
    GLsizei temp_height = 0;
    texture_->GetLevelSize(target_, level_, &temp_width, &temp_height);
    return temp_width;
  }

  virtual GLsizei height() const {
    GLsizei temp_width = 0;
    GLsizei temp_height = 0;
    texture_->GetLevelSize(target_, level_, &temp_width, &temp_height);
    return temp_height;
  }

  virtual GLenum internal_format() const {
    GLenum temp_type = 0;
    GLenum temp_internal_format = 0;
    texture_->GetLevelType(target_, level_, &temp_type, &temp_internal_format);
    return temp_internal_format;
  }

  virtual GLsizei samples() const {
    return 0;
  }

  virtual bool cleared() const {
    // Textures are cleared on creation.
    return true;
  }

  virtual void set_cleared() {
    NOTREACHED();
  }

  virtual bool IsTexture(TextureManager::TextureInfo* texture) const {
    return texture == texture_.get();
  }

  TextureManager::TextureInfo* texture() const {
    return texture_.get();
  }

  virtual bool CanRenderTo() const {
    return texture_->CanRenderTo();
  }

 private:
  TextureManager::TextureInfo::Ref texture_;
  GLenum target_;
  GLint level_;

  DISALLOW_COPY_AND_ASSIGN(TextureAttachment);
};

FramebufferManager::FramebufferManager() {}

FramebufferManager::~FramebufferManager() {
  DCHECK(framebuffer_infos_.empty());
}

void FramebufferManager::Destroy(bool have_context) {
  while (!framebuffer_infos_.empty()) {
    if (have_context) {
      FramebufferInfo* info = framebuffer_infos_.begin()->second;
      if (!info->IsDeleted()) {
        GLuint service_id = info->service_id();
        glDeleteFramebuffersEXT(1, &service_id);
        info->MarkAsDeleted();
      }
    }
    framebuffer_infos_.erase(framebuffer_infos_.begin());
  }
}

void FramebufferManager::CreateFramebufferInfo(
    GLuint client_id, GLuint service_id) {
  std::pair<FramebufferInfoMap::iterator, bool> result =
      framebuffer_infos_.insert(
          std::make_pair(
              client_id,
              FramebufferInfo::Ref(new FramebufferInfo(service_id))));
  DCHECK(result.second);
}

FramebufferManager::FramebufferInfo::FramebufferInfo(GLuint service_id)
    : service_id_(service_id)
    , has_been_bound_(false) {
}

FramebufferManager::FramebufferInfo::~FramebufferInfo() {}

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

void FramebufferManager::FramebufferInfo::MarkAttachedRenderbuffersAsCleared() {
  for (AttachmentMap::iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    Attachment* attachment = it->second;
    if (!attachment->cleared()) {
      attachment->set_cleared();
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

bool FramebufferManager::FramebufferInfo::IsNotComplete() const {
  for (AttachmentMap::const_iterator it = attachments_.begin();
       it != attachments_.end(); ++it) {
    Attachment* attachment = it->second;
    if (attachment->width() == 0 || attachment->height() == 0) {
      return true;
    }
    if (!attachment->CanRenderTo()) {
      return true;
    }
  }
  return false;
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

}  // namespace gles2
}  // namespace gpu


