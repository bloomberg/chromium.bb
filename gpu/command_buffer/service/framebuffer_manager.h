// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_FRAMEBUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_FRAMEBUFFER_MANAGER_H_

#include <map>
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"

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

    explicit FramebufferInfo(GLuint service_id)
        : service_id_(service_id) {
    }

    GLuint service_id() const {
      return service_id_;
    }

    // Attaches a renderbuffer to a particlar attachment.
    // Pass null to detach.
    void AttachRenderbuffer(
        GLenum attachment, RenderbufferManager::RenderbufferInfo* renderbuffer);

    bool IsDeleted() {
      return service_id_ == 0;
    }

   private:
    friend class FramebufferManager;
    friend class base::RefCounted<FramebufferInfo>;

    ~FramebufferInfo() { }

    void MarkAsDeleted() {
      service_id_ = 0;
      renderbuffers_.clear();
    }

    // Service side framebuffer id.
    GLuint service_id_;

    // A map of attachments to renderbuffers.
    typedef std::map<GLenum, RenderbufferManager::RenderbufferInfo::Ref>
        AttachmentToRenderbufferMap;
    AttachmentToRenderbufferMap renderbuffers_;
  };

  FramebufferManager() { }
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
  // TODO(gman): Choose a faster container.
  typedef std::map<GLuint, FramebufferInfo::Ref> FramebufferInfoMap;
  FramebufferInfoMap framebuffer_infos_;

  DISALLOW_COPY_AND_ASSIGN(FramebufferManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_FRAMEBUFFER_MANAGER_H_


