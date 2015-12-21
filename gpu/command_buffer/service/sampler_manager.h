// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SAMPLER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SAMPLER_MANAGER_H_

#include <vector>
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/gpu_export.h"

namespace gpu {

namespace gles2 {

class SamplerManager;

class GPU_EXPORT Sampler : public base::RefCounted<Sampler> {
 public:
  Sampler(SamplerManager* manager, GLuint service_id);

  // The service side OpenGL id of the texture.
  GLuint service_id() const {
    return service_id_;
  }

  // Sampler parameters
  GLenum min_filter() const {
    return min_filter_;
  }

  GLenum mag_filter() const {
    return mag_filter_;
  }

  GLenum wrap_r() const {
    return wrap_r_;
  }

  GLenum wrap_s() const {
    return wrap_s_;
  }

  GLenum wrap_t() const {
    return wrap_t_;
  }

  GLenum compare_func() const {
    return compare_func_;
  }

  GLenum compare_mode() const {
    return compare_mode_;
  }

  GLfloat max_lod() const {
    return max_lod_;
  }

  GLfloat min_lod() const {
    return min_lod_;
  }

  bool IsDeleted() const {
    return deleted_;
  }

 protected:
  virtual ~Sampler();

  SamplerManager* manager() const {
    return manager_;
  }

  void MarkAsDeleted() {
    deleted_ = true;
  }

 private:
  friend class SamplerManager;
  friend class base::RefCounted<Sampler>;

  // Sets a sampler parameter.
  // Returns GL_NO_ERROR on success. Otherwise the error to generate.
  GLenum SetParameteri(
      const FeatureInfo* feature_info, GLenum pname, GLint param);
  GLenum SetParameterf(
      const FeatureInfo* feature_info, GLenum pname, GLfloat param);

  // The manager that owns this Sampler.
  SamplerManager* manager_;

    // The id of the texure
  GLuint service_id_;

  // Texture parameters.
  GLenum min_filter_;
  GLenum mag_filter_;
  GLenum wrap_r_;
  GLenum wrap_s_;
  GLenum wrap_t_;
  GLenum compare_func_;
  GLenum compare_mode_;
  GLfloat max_lod_;
  GLfloat min_lod_;

  // True if deleted.
  bool deleted_;
};

// This class keeps track of the samplers and their state.
class GPU_EXPORT SamplerManager {
 public:
  SamplerManager(FeatureInfo* feature_info);
  ~SamplerManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a Sampler for the given sampler.
  Sampler* CreateSampler(GLuint client_id, GLuint service_id);

  // Gets the Sampler info for the given sampler.
  Sampler* GetSampler(GLuint client_id);

  // Removes a Sampler info for the given sampler.
  void RemoveSampler(GLuint client_id);

  // Sets a sampler parameter of a Sampler.
  // Returns GL_NO_ERROR on success. Otherwise the error to generate.
  void SetParameteri(
      const char* function_name, ErrorState* error_state,
      Sampler* sampler, GLenum pname, GLint param);
  void SetParameterf(
      const char* function_name, ErrorState* error_state,
      Sampler* sampler, GLenum pname, GLfloat param);

 private:
  friend class Sampler;

  scoped_refptr<FeatureInfo> feature_info_;

  // Info for each sampler in the system.
  typedef base::hash_map<GLuint, scoped_refptr<Sampler> > SamplerMap;
  SamplerMap samplers_;

  bool have_context_;

  DISALLOW_COPY_AND_ASSIGN(SamplerManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SAMPLER_MANAGER_H_
