// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_

#include <map>
#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "gpu/command_buffer/service/gl_utils.h"

namespace gpu {
namespace gles2 {

// This class keeps track of the renderbuffers and whether or not they have
// been cleared.
class RenderbufferManager {
 public:
  // Info about Renderbuffers currently in the system.
  class RenderbufferInfo : public base::RefCounted<RenderbufferInfo> {
   public:
    typedef scoped_refptr<RenderbufferInfo> Ref;

    explicit RenderbufferInfo(GLuint service_id)
        : service_id_(service_id),
          cleared_(false),
          internal_format_(GL_RGBA4) {
    }

    GLuint service_id() const {
      return service_id_;
    }

    bool cleared() const {
      return cleared_;
    }

    void set_cleared() {
      cleared_ = true;
    }

    GLenum internal_format() const {
      return internal_format_;
    }

    void set_internal_format(GLenum internalformat) {
      internal_format_ = internalformat;
      cleared_ = false;
    }

    bool IsDeleted() {
      return service_id_ == 0;
    }

   private:
    friend class RenderbufferManager;
    friend class base::RefCounted<RenderbufferInfo>;

    ~RenderbufferInfo() { }

    void MarkAsDeleted() {
      service_id_ = 0;
    }

    // Service side renderbuffer id.
    GLuint service_id_;

    // Whether this renderbuffer has been cleared
    bool cleared_;

    // Renderbuffer internalformat set through RenderbufferStorage().
    GLenum internal_format_;
  };

  explicit RenderbufferManager(GLint max_renderbuffer_size);
  ~RenderbufferManager();

  GLint max_renderbuffer_size() const {
    return max_renderbuffer_size_;
  }

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a RenderbufferInfo for the given renderbuffer.
  void CreateRenderbufferInfo(GLuint client_id, GLuint service_id);

  // Gets the renderbuffer info for the given renderbuffer.
  RenderbufferInfo* GetRenderbufferInfo(GLuint client_id);

  // Removes a renderbuffer info for the given renderbuffer.
  void RemoveRenderbufferInfo(GLuint client_id);

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

 private:
  GLint max_renderbuffer_size_;

  // Info for each renderbuffer in the system.
  // TODO(gman): Choose a faster container.
  typedef std::map<GLuint, RenderbufferInfo::Ref> RenderbufferInfoMap;
  RenderbufferInfoMap renderbuffer_infos_;

  DISALLOW_COPY_AND_ASSIGN(RenderbufferManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_
