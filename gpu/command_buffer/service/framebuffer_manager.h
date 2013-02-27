// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "gpu/gpu_export.h"

namespace gpu {
namespace gles2 {

class FramebufferManager;

// Info about a particular Framebuffer.
class GPU_EXPORT Framebuffer : public base::RefCounted<Framebuffer> {
 public:
  class Attachment : public base::RefCounted<Attachment> {
   public:
    virtual GLsizei width() const = 0;
    virtual GLsizei height() const = 0;
    virtual GLenum internal_format() const = 0;
    virtual GLsizei samples() const = 0;
    virtual bool cleared() const = 0;
    virtual void SetCleared(
        RenderbufferManager* renderbuffer_manager,
        TextureManager* texture_manager,
        bool cleared) = 0;
    virtual bool IsTexture(Texture* texture) const = 0;
    virtual bool IsRenderbuffer(
        Renderbuffer* renderbuffer) const = 0;
    virtual bool CanRenderTo() const = 0;
    virtual void DetachFromFramebuffer() const = 0;
    virtual bool ValidForAttachmentType(GLenum attachment_type) = 0;
    virtual void AddToSignature(
        TextureManager* texture_manager, std::string* signature) const = 0;

   protected:
    friend class base::RefCounted<Attachment>;
    virtual ~Attachment() {}
  };

  Framebuffer(FramebufferManager* manager, GLuint service_id);

  GLuint service_id() const {
    return service_id_;
  }

  bool HasUnclearedAttachment(GLenum attachment) const;

  void MarkAttachmentAsCleared(
    RenderbufferManager* renderbuffer_manager,
    TextureManager* texture_manager,
    GLenum attachment,
    bool cleared);

  // Attaches a renderbuffer to a particlar attachment.
  // Pass null to detach.
  void AttachRenderbuffer(
      GLenum attachment, Renderbuffer* renderbuffer);

  // Attaches a texture to a particlar attachment. Pass null to detach.
  void AttachTexture(
      GLenum attachment, Texture* texture, GLenum target,
      GLint level);

  // Unbinds the given renderbuffer if it is bound.
  void UnbindRenderbuffer(
      GLenum target, Renderbuffer* renderbuffer);

  // Unbinds the given texture if it is bound.
  void UnbindTexture(
      GLenum target, Texture* texture);

  const Attachment* GetAttachment(GLenum attachment) const;

  bool IsDeleted() const {
    return deleted_;
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

  // Implements optimized glGetFramebufferStatus.
  GLenum GetStatus(TextureManager* texture_manager, GLenum target) const;

  // Check all attachments are cleared
  bool IsCleared() const;

  static void ClearFramebufferCompleteComboMap();

 private:
  friend class FramebufferManager;
  friend class base::RefCounted<Framebuffer>;

  ~Framebuffer();

  void MarkAsDeleted();

  void MarkAttachmentsAsCleared(
    RenderbufferManager* renderbuffer_manager,
    TextureManager* texture_manager,
    bool cleared);

  void MarkAsComplete(unsigned state_id) {
    framebuffer_complete_state_count_id_ = state_id;
  }

  unsigned framebuffer_complete_state_count_id() const {
    return framebuffer_complete_state_count_id_;
  }

  // The managers that owns this.
  FramebufferManager* manager_;

  bool deleted_;

  // Service side framebuffer id.
  GLuint service_id_;

  // Whether this framebuffer has ever been bound.
  bool has_been_bound_;

  // state count when this framebuffer was last checked for completeness.
  unsigned framebuffer_complete_state_count_id_;

  // A map of attachments.
  typedef base::hash_map<GLenum, scoped_refptr<Attachment> > AttachmentMap;
  AttachmentMap attachments_;

  // A map of successful frame buffer combos. If it's in the map
  // it should be FRAMEBUFFER_COMPLETE.
  typedef base::hash_map<std::string, bool> FramebufferComboCompleteMap;
  static FramebufferComboCompleteMap* framebuffer_combo_complete_map_;

  DISALLOW_COPY_AND_ASSIGN(Framebuffer);
};

// This class keeps track of the frambebuffers and their attached renderbuffers
// so we can correctly clear them.
class GPU_EXPORT FramebufferManager {
 public:
  FramebufferManager();
  ~FramebufferManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a Framebuffer for the given framebuffer.
  void CreateFramebuffer(GLuint client_id, GLuint service_id);

  // Gets the framebuffer info for the given framebuffer.
  Framebuffer* GetFramebuffer(GLuint client_id);

  // Removes a framebuffer info for the given framebuffer.
  void RemoveFramebuffer(GLuint client_id);

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

  void MarkAttachmentsAsCleared(
    Framebuffer* framebuffer,
    RenderbufferManager* renderbuffer_manager,
    TextureManager* texture_manager);

  void MarkAsComplete(Framebuffer* framebuffer);

  bool IsComplete(Framebuffer* framebuffer);

  void IncFramebufferStateChangeCount() {
    // make sure this is never 0.
    framebuffer_state_change_count_ =
        (framebuffer_state_change_count_ + 1) | 0x80000000U;
  }

 private:
  friend class Framebuffer;

  void StartTracking(Framebuffer* info);
  void StopTracking(Framebuffer* info);

  // Info for each framebuffer in the system.
  typedef base::hash_map<GLuint, scoped_refptr<Framebuffer> >
      FramebufferInfoMap;
  FramebufferInfoMap framebuffer_infos_;

  // Incremented anytime anything changes that might effect framebuffer
  // state.
  unsigned framebuffer_state_change_count_;

  // Counts the number of Framebuffer allocated with 'this' as its manager.
  // Allows to check no Framebuffer will outlive this.
  unsigned int framebuffer_info_count_;

  bool have_context_;

  DISALLOW_COPY_AND_ASSIGN(FramebufferManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_FRAMEBUFFER_MANAGER_H_
