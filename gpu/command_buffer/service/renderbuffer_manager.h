// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_

#include <map>
#include "base/basictypes.h"
#include "base/logging.h"
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

    explicit RenderbufferInfo(GLuint renderbuffer_id)
        : renderbuffer_id_(renderbuffer_id),
          cleared_(false) {
    }

    GLuint renderbuffer_id() const {
      return renderbuffer_id_;
    }

    bool cleared() const {
      return cleared_;
    }

    void set_cleared() {
      cleared_ = true;
    }

    bool IsDeleted() {
      return renderbuffer_id_ == 0;
    }

   private:
    friend class RenderbufferManager;
    friend class base::RefCounted<RenderbufferInfo>;

    ~RenderbufferInfo() { }

    void MarkAsDeleted() {
      renderbuffer_id_ = 0;
    }

    // Service side renderbuffer id.
    GLuint renderbuffer_id_;

    // Whether this renderbuffer has been cleared
    bool cleared_;
  };

  RenderbufferManager() { }

  // Creates a RenderbufferInfo for the given renderbuffer.
  void CreateRenderbufferInfo(GLuint renderbuffer_id);

  // Gets the renderbuffer info for the given renderbuffer.
  RenderbufferInfo* GetRenderbufferInfo(GLuint renderbuffer_id);

  // Removes a renderbuffer info for the given renderbuffer.
  void RemoveRenderbufferInfo(GLuint renderbuffer_id);

 private:
  // Info for each renderbuffer in the system.
  // TODO(gman): Choose a faster container.
  typedef std::map<GLuint, RenderbufferInfo::Ref> RenderbufferInfoMap;
  RenderbufferInfoMap renderbuffer_infos_;

  DISALLOW_COPY_AND_ASSIGN(RenderbufferManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_


