// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/vertex_attrib_manager.h"

#include <list>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#define GLES2_GPU_SERVICE 1
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/vertex_array_manager.h"

namespace gpu {
namespace gles2 {

VertexAttrib::VertexAttrib()
    : index_(0),
      enabled_(false),
      size_(4),
      type_(GL_FLOAT),
      offset_(0),
      normalized_(GL_FALSE),
      gl_stride_(0),
      real_stride_(16),
      divisor_(0),
      list_(NULL) {
}

VertexAttrib::~VertexAttrib() {
}

bool VertexAttrib::CanAccess(GLuint index) const {
  if (!enabled_) {
    return true;
  }

  if (!buffer_ || buffer_->IsDeleted()) {
    return false;
  }

  // The number of elements that can be accessed.
  GLsizeiptr buffer_size = buffer_->size();
  if (offset_ > buffer_size || real_stride_ == 0) {
    return false;
  }

  uint32 usable_size = buffer_size - offset_;
  GLuint num_elements = usable_size / real_stride_ +
      ((usable_size % real_stride_) >=
       (GLES2Util::GetGLTypeSizeForTexturesAndBuffers(type_) * size_) ? 1 : 0);
  return index < num_elements;
}

VertexAttribManager::VertexAttribManager()
    : num_fixed_attribs_(0),
      element_array_buffer_(NULL),
      manager_(NULL),
      deleted_(false),
      service_id_(0) {
}

VertexAttribManager::VertexAttribManager(
    VertexArrayManager* manager, GLuint service_id, uint32 num_vertex_attribs)
    : num_fixed_attribs_(0),
      element_array_buffer_(NULL),
      manager_(manager),
      deleted_(false),
      service_id_(service_id) {
  manager_->StartTracking(this);
  Initialize(num_vertex_attribs, false);
}

VertexAttribManager::~VertexAttribManager() {
  if (manager_) {
    if (manager_->have_context_) {
      if (service_id_ != 0)  // 0 indicates an emulated VAO
        glDeleteVertexArraysOES(1, &service_id_);
    }
    manager_->StopTracking(this);
    manager_ = NULL;
  }
}

void VertexAttribManager::Initialize(
    uint32 max_vertex_attribs, bool init_attribs) {
  vertex_attrib_infos_.resize(max_vertex_attribs);
  bool disable_workarounds = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGpuDriverBugWorkarounds);

  for (uint32 vv = 0; vv < vertex_attrib_infos_.size(); ++vv) {
    vertex_attrib_infos_[vv].set_index(vv);
    vertex_attrib_infos_[vv].SetList(&disabled_vertex_attribs_);

    if (!disable_workarounds && init_attribs) {
      glVertexAttrib4f(vv, 0.0f, 0.0f, 0.0f, 1.0f);
    }
  }
}

bool VertexAttribManager::Enable(GLuint index, bool enable) {
  if (index >= vertex_attrib_infos_.size()) {
    return false;
  }
  VertexAttrib& info = vertex_attrib_infos_[index];
  if (info.enabled() != enable) {
    info.set_enabled(enable);
    info.SetList(enable ? &enabled_vertex_attribs_ : &disabled_vertex_attribs_);
  }
  return true;
}

void VertexAttribManager::Unbind(Buffer* buffer) {
  if (element_array_buffer_ == buffer) {
    element_array_buffer_ = NULL;
  }
  for (uint32 vv = 0; vv < vertex_attrib_infos_.size(); ++vv) {
    vertex_attrib_infos_[vv].Unbind(buffer);
  }
}

}  // namespace gles2
}  // namespace gpu
