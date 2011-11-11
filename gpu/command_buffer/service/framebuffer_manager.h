// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_FRAMEBUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_FRAMEBUFFER_MANAGER_H_

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"

namespace gpu {
namespace gles2 {

// This class keeps track of the frambebuffers and their attached renderbuffers
// so we can correctly clear them.
class FramebufferManager {
 public:
  // Info about Framebuffers currently in the system.
  class FramebufferInfo : public base::RefCounted<FramebufferInfo> {
   public:
    typedef scoped_refptr<FramebufferInfo> Ref;

    class Attachment : public base::RefCounted<Attachment> {
     public:
      typedef scoped_refptr<Attachment> Ref;

      virtual ~Attachment() { }
      virtual GLsizei width() const = 0;
      virtual GLsizei height() const = 0;
      virtual GLenum internal_format() const = 0;
      virtual GLsizei samples() const = 0;
      virtual bool cleared() const = 0;
      virtual void SetCleared(
          RenderbufferManager* renderbuffer_manager,
          TextureManager* texture_manager) = 0;
      virtual bool IsTexture(TextureManager::TextureInfo* texture) const = 0;
      virtual bool IsRenderbuffer(
          RenderbufferManager::RenderbufferInfo* renderbuffer) const = 0;
      virtual bool CanRenderTo() const = 0;
      virtual void DetachFromFramebuffer() = 0;
      virtual bool ValidForAttachmentType(GLenum attachment_type) = 0;
    };

    explicit FramebufferInfo(GLuint service_id);

    GLuint service_id() const {
      return service_id_;
    }

    bool HasUnclearedAttachment(GLenum attachment) const;

    // Attaches a renderbuffer to a particlar attachment.
    // Pass null to detach.
    void AttachRenderbuffer(
        GLenum attachment, RenderbufferManager::RenderbufferInfo* renderbuffer);

    // Attaches a texture to a particlar attachment. Pass null to detach.
    void AttachTexture(
        GLenum attachment, TextureManager::TextureInfo* texture, GLenum target,
        GLint level);

    // Unbinds the given renderbuffer if it is bound.
    void UnbindRenderbuffer(
        GLenum target, RenderbufferManager::RenderbufferInfo* renderbuffer);

    // Unbinds the given texture if it is bound.
    void UnbindTexture(
        GLenum target, TextureManager::TextureInfo* texture);

    void MarkAttachmentsAsCleared(
      RenderbufferManager* renderbuffer_manager,
      TextureManager* texture_manager);

    const Attachment* GetAttachment(GLenum attachment) const;

    bool IsDeleted() const {
      return service_id_ == 0;
    }

    void MarkAsValid() {
      has_been_bound_ = true;
    }

    bool IsValid() const {
      return has_been_bound_ && !IsDeleted();
    }

    bool HasDepthAttachment() const;
    bool HasStencilAttachment() const;
    GLenum GetColorAttachmentFormat() const;

    // Verify all the rules in OpenGL ES 2.0.25 4.4.5 are followed.
    // Returns GL_FRAMEBUFFER_COMPLETE if there are no reasons we know we can't
    // use this combination of attachments. Otherwise returns the value
    // that glCheckFramebufferStatus should return for this set of attachments.
    // Note that receiving GL_FRAMEBUFFER_COMPLETE from this function does
    // not mean the real OpenGL will consider it framebuffer complete. It just
    // means it passed our tests.
    GLenum IsPossiblyComplete() const;

    // Check all attachments are cleared
    bool IsCleared() const;

   private:
    friend class FramebufferManager;
    friend class base::RefCounted<FramebufferInfo>;

    ~FramebufferInfo();

    void MarkAsDeleted();

    // Service side framebuffer id.
    GLuint service_id_;

    // Whether this framebuffer has ever been bound.
    bool has_been_bound_;

    // A map of attachments.
    typedef base::hash_map<GLenum, Attachment::Ref> AttachmentMap;
    AttachmentMap attachments_;

    DISALLOW_COPY_AND_ASSIGN(FramebufferInfo);
  };

  FramebufferManager();
  ~FramebufferManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a FramebufferInfo for the given framebuffer.
  void CreateFramebufferInfo(GLuint client_id, GLuint service_id);

  // Gets the framebuffer info for the given framebuffer.
  FramebufferInfo* GetFramebufferInfo(GLuint client_id);

  // Removes a framebuffer info for the given framebuffer.
  void RemoveFramebufferInfo(GLuint client_id);

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

 private:
  // Info for each framebuffer in the system.
  typedef base::hash_map<GLuint, FramebufferInfo::Ref> FramebufferInfoMap;
  FramebufferInfoMap framebuffer_infos_;

  DISALLOW_COPY_AND_ASSIGN(FramebufferManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_FRAMEBUFFER_MANAGER_H_
