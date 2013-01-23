// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_

#include <string>
#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/gpu_export.h"

namespace gpu {
namespace gles2 {

// This class keeps track of the renderbuffers and whether or not they have
// been cleared.
class GPU_EXPORT RenderbufferManager {
 public:
  // Info about Renderbuffers currently in the system.
  class GPU_EXPORT RenderbufferInfo
      : public base::RefCounted<RenderbufferInfo> {
   public:
    typedef scoped_refptr<RenderbufferInfo> Ref;

    RenderbufferInfo(RenderbufferManager* manager, GLuint service_id)
        : manager_(manager),
          deleted_(false),
          service_id_(service_id),
          cleared_(true),
          has_been_bound_(false),
          samples_(0),
          internal_format_(GL_RGBA4),
          width_(0),
          height_(0) {
      manager_->StartTracking(this);
    }

    GLuint service_id() const {
      return service_id_;
    }

    bool cleared() const {
      return cleared_;
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

    bool IsDeleted() const {
      return deleted_;
    }

    void MarkAsValid() {
      has_been_bound_ = true;
    }

    bool IsValid() const {
      return has_been_bound_ && !IsDeleted();
    }

    size_t EstimatedSize();

    void AddToSignature(std::string* signature) const;

   private:
    friend class RenderbufferManager;
    friend class base::RefCounted<RenderbufferInfo>;

    ~RenderbufferInfo();

    void set_cleared(bool cleared) {
      cleared_ = cleared;
    }

    void SetInfo(
        GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
      samples_ = samples;
      internal_format_ = internalformat;
      width_ = width;
      height_ = height;
      cleared_ = false;
    }

    void MarkAsDeleted() {
      deleted_ = true;
    }

    // RenderbufferManager that owns this RenderbufferInfo.
    RenderbufferManager* manager_;

    bool deleted_;

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

  RenderbufferManager(MemoryTracker* memory_tracker,
                      GLint max_renderbuffer_size,
                      GLint max_samples);
  ~RenderbufferManager();

  GLint max_renderbuffer_size() const {
    return max_renderbuffer_size_;
  }

  GLint max_samples() const {
    return max_samples_;
  }

  bool HaveUnclearedRenderbuffers() const {
    return num_uncleared_renderbuffers_ != 0;
  }

  void SetInfo(
      RenderbufferInfo* renderbuffer,
      GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);

  void SetCleared(RenderbufferInfo* renderbuffer, bool cleared);

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

  size_t mem_represented() const {
    return memory_tracker_->GetMemRepresented();
  }

  static bool ComputeEstimatedRenderbufferSize(
      int width, int height, int samples, int internal_format, uint32* size);
  static GLenum InternalRenderbufferFormatToImplFormat(GLenum impl_format);

 private:
  void StartTracking(RenderbufferInfo* renderbuffer);
  void StopTracking(RenderbufferInfo* renderbuffer);

  scoped_ptr<MemoryTypeTracker> memory_tracker_;

  GLint max_renderbuffer_size_;
  GLint max_samples_;

  int num_uncleared_renderbuffers_;

  // Counts the number of RenderbufferInfo allocated with 'this' as its manager.
  // Allows to check no RenderbufferInfo will outlive this.
  unsigned renderbuffer_info_count_;

  bool have_context_;

  // Info for each renderbuffer in the system.
  typedef base::hash_map<GLuint, RenderbufferInfo::Ref> RenderbufferInfoMap;
  RenderbufferInfoMap renderbuffer_infos_;

  DISALLOW_COPY_AND_ASSIGN(RenderbufferManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_
