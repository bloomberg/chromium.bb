// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
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
          has_been_bound_(false),
          samples_(0),
          internal_format_(GL_RGBA4),
          width_(0),
          height_(0) {
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

    GLsizei samples() const {
      return samples_;
    }

    GLsizei width() const {
      return width_;
    }

    GLsizei height() const {
      return height_;
    }

    void SetInfo(
        GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
      samples_ = samples;
      internal_format_ = internalformat;
      width_ = width;
      height_ = height;
      cleared_ = false;
    }

    bool IsDeleted() const {
      return service_id_ == 0;
    }

    void MarkAsValid() {
      has_been_bound_ = true;
    }

    bool IsValid() const {
      return has_been_bound_ && !IsDeleted();
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

    // Whether this renderbuffer has ever been bound.
    bool has_been_bound_;

    // Number of samples (for multi-sampled renderbuffers)
    GLsizei samples_;

    // Renderbuffer internalformat set through RenderbufferStorage().
    GLenum internal_format_;

    // Dimensions of renderbuffer.
    GLsizei width_;
    GLsizei height_;
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
  typedef base::hash_map<GLuint, RenderbufferInfo::Ref> RenderbufferInfoMap;
  RenderbufferInfoMap renderbuffer_infos_;

  DISALLOW_COPY_AND_ASSIGN(RenderbufferManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_
