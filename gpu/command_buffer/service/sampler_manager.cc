// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/sampler_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gpu_timing.h"

namespace gpu {
namespace gles2 {

Sampler::Sampler(SamplerManager* manager, GLuint service_id)
    : manager_(manager),
      service_id_(service_id),
      min_filter_(GL_NEAREST_MIPMAP_LINEAR),
      mag_filter_(GL_LINEAR),
      wrap_r_(GL_REPEAT),
      wrap_s_(GL_REPEAT),
      wrap_t_(GL_REPEAT),
      compare_func_(GL_LEQUAL),
      compare_mode_(GL_NONE),
      max_lod_(1000.0f),
      min_lod_(-1000.0f),
      deleted_(false) {
  DCHECK(manager);
}

Sampler::~Sampler() {
  if (manager_->have_context_) {
    glDeleteSamplers(1, &service_id_);
  }
}

GLenum Sampler::SetParameteri(
    const FeatureInfo* feature_info, GLenum pname, GLint param) {
  DCHECK(feature_info);

  switch (pname) {
    case GL_TEXTURE_MIN_LOD:
    case GL_TEXTURE_MAX_LOD: {
      GLfloat fparam = static_cast<GLfloat>(param);
      return SetParameterf(feature_info, pname, fparam);
    }
    case GL_TEXTURE_MIN_FILTER:
      if (!feature_info->validators()->texture_min_filter_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      min_filter_ = param;
      break;
    case GL_TEXTURE_MAG_FILTER:
      if (!feature_info->validators()->texture_mag_filter_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      mag_filter_ = param;
      break;
    case GL_TEXTURE_WRAP_R:
      if (!feature_info->validators()->texture_wrap_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      wrap_r_ = param;
      break;
    case GL_TEXTURE_WRAP_S:
      if (!feature_info->validators()->texture_wrap_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      wrap_s_ = param;
      break;
    case GL_TEXTURE_WRAP_T:
      if (!feature_info->validators()->texture_wrap_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      wrap_t_ = param;
      break;
    case GL_TEXTURE_COMPARE_FUNC:
      if (!feature_info->validators()->texture_compare_func.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      compare_func_ = param;
      break;
    case GL_TEXTURE_COMPARE_MODE:
      if (!feature_info->validators()->texture_compare_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      compare_mode_ = param;
      break;
    default:
      return GL_INVALID_ENUM;
  }
  return GL_NO_ERROR;
}

GLenum Sampler::SetParameterf(
    const FeatureInfo* feature_info, GLenum pname, GLfloat param) {
  switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_MAG_FILTER:
    case GL_TEXTURE_WRAP_R:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
    case GL_TEXTURE_COMPARE_FUNC:
    case GL_TEXTURE_COMPARE_MODE: {
      GLint iparam = static_cast<GLint>(param);
      return SetParameteri(feature_info, pname, iparam);
    }
    case GL_TEXTURE_MIN_LOD:
      min_lod_ = param;
      break;
    case GL_TEXTURE_MAX_LOD:
      max_lod_ = param;
      break;
    default:
      return GL_INVALID_ENUM;
  }
  return GL_NO_ERROR;
}

SamplerManager::SamplerManager(FeatureInfo* feature_info)
    : feature_info_(feature_info),
      have_context_(true) {
}

SamplerManager::~SamplerManager() {
  DCHECK(samplers_.empty());
}

void SamplerManager::Destroy(bool have_context) {
  have_context_ = have_context;
  while (!samplers_.empty()) {
    Sampler* sampler = samplers_.begin()->second.get();
    sampler->MarkAsDeleted();
    samplers_.erase(samplers_.begin());
  }
}

Sampler* SamplerManager::CreateSampler(GLuint client_id, GLuint service_id) {
  DCHECK_NE(0u, service_id);
  auto result = samplers_.insert(std::make_pair(client_id,
      scoped_refptr<Sampler>(new Sampler(this, service_id))));
  DCHECK(result.second);
  return result.first->second.get();
}

Sampler* SamplerManager::GetSampler(GLuint client_id) {
  SamplerMap::iterator it = samplers_.find(client_id);
  return it != samplers_.end() ? it->second.get() : nullptr;
}

void SamplerManager::RemoveSampler(GLuint client_id) {
  SamplerMap::iterator it = samplers_.find(client_id);
  if (it != samplers_.end()) {
    Sampler* sampler = it->second.get();

    sampler->MarkAsDeleted();
    samplers_.erase(it);
  }
}

void SamplerManager::SetParameteri(
    const char* function_name, ErrorState* error_state,
    Sampler* sampler, GLenum pname, GLint param) {
  DCHECK(error_state);
  DCHECK(sampler);
  GLenum result = sampler->SetParameteri(feature_info_.get(), pname, param);
  if (result != GL_NO_ERROR) {
    if (result == GL_INVALID_ENUM) {
      ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(
          error_state, function_name, param, "param");
    } else {
      ERRORSTATE_SET_GL_ERROR_INVALID_PARAMI(
          error_state, result, function_name, pname, param);
    }
  } else {
    glSamplerParameteri(sampler->service_id(), pname, param);
  }
}

void SamplerManager::SetParameterf(
    const char* function_name, ErrorState* error_state,
    Sampler* sampler, GLenum pname, GLfloat param) {
  DCHECK(error_state);
  DCHECK(sampler);
  GLenum result = sampler->SetParameterf(feature_info_.get(), pname, param);
  if (result != GL_NO_ERROR) {
    if (result == GL_INVALID_ENUM) {
      ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(
          error_state, function_name, param, "param");
    } else {
      ERRORSTATE_SET_GL_ERROR_INVALID_PARAMI(
          error_state, result, function_name, pname, param);
    }
  } else {
    glSamplerParameterf(sampler->service_id(), pname, param);
  }
}

}  // namespace gles2
}  // namespace gpu
