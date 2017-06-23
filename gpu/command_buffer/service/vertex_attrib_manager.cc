// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/vertex_attrib_manager.h"

#include <stdint.h>

#include <list>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/program_manager.h"
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
      integer_(GL_FALSE),
      is_client_side_array_(false),
      list_(NULL) {
}

VertexAttrib::VertexAttrib(const VertexAttrib& other) = default;

VertexAttrib::~VertexAttrib() {
}

void VertexAttrib::SetInfo(
    Buffer* buffer,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei gl_stride,
    GLsizei real_stride,
    GLsizei offset,
    GLboolean integer) {
  DCHECK_GT(real_stride, 0);
  buffer_ = buffer;
  size_ = size;
  type_ = type;
  normalized_ = normalized;
  gl_stride_ = gl_stride;
  real_stride_ = real_stride;
  offset_ = offset;
  integer_ = integer;
}

void VertexAttrib::Unbind(Buffer* buffer) {
  if (buffer_.get() == buffer) {
    buffer_ = NULL;
  }
}

bool VertexAttrib::CanAccess(GLuint index) const {
  if (!enabled_) {
    return true;
  }

  DCHECK(buffer_.get() && !buffer_->IsDeleted());
  // The number of elements that can be accessed.
  GLsizeiptr buffer_size = buffer_->size();
  if (offset_ > buffer_size || real_stride_ == 0) {
    return false;
  }

  uint32_t usable_size = buffer_size - offset_;
  GLuint num_elements = usable_size / real_stride_ +
      ((usable_size % real_stride_) >=
       (GLES2Util::GetGroupSizeForBufferType(size_, type_)) ? 1 : 0);
  return index < num_elements;
}

VertexAttribManager::VertexAttribManager()
    : num_fixed_attribs_(0),
      element_array_buffer_(NULL),
      manager_(NULL),
      deleted_(false),
      service_id_(0) {
}

VertexAttribManager::VertexAttribManager(VertexArrayManager* manager,
                                         GLuint service_id,
                                         uint32_t num_vertex_attribs)
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

void VertexAttribManager::Initialize(uint32_t max_vertex_attribs,
                                     bool init_attribs) {
  vertex_attribs_.resize(max_vertex_attribs);
  uint32_t packed_size = (max_vertex_attribs + 15) / 16;
  attrib_base_type_mask_.resize(packed_size);
  attrib_enabled_mask_.resize(packed_size);

  for (uint32_t ii = 0; ii < packed_size; ++ii) {
    attrib_enabled_mask_[ii] = 0u;
    attrib_base_type_mask_[ii] = 0u;
  }

  for (uint32_t vv = 0; vv < vertex_attribs_.size(); ++vv) {
    vertex_attribs_[vv].set_index(vv);
    vertex_attribs_[vv].SetList(&disabled_vertex_attribs_);

    if (init_attribs) {
      glVertexAttrib4f(vv, 0.0f, 0.0f, 0.0f, 1.0f);
    }
  }
}

void VertexAttribManager::SetElementArrayBuffer(Buffer* buffer) {
  element_array_buffer_ = buffer;
}

bool VertexAttribManager::Enable(GLuint index, bool enable) {
  if (index >= vertex_attribs_.size()) {
    return false;
  }

  VertexAttrib& info = vertex_attribs_[index];
  if (info.enabled() != enable) {
    info.set_enabled(enable);
    info.SetList(enable ? &enabled_vertex_attribs_ : &disabled_vertex_attribs_);
    GLuint shift_bits = (index % 16) * 2;
    if (enable) {
      attrib_enabled_mask_[index / 16] |= (0x3 << shift_bits);
    } else {
      attrib_enabled_mask_[index / 16] &= ~(0x3 << shift_bits);
    }
  }
  return true;
}

void VertexAttribManager::Unbind(Buffer* buffer) {
  if (element_array_buffer_.get() == buffer) {
    element_array_buffer_ = NULL;
  }
  for (uint32_t vv = 0; vv < vertex_attribs_.size(); ++vv) {
    vertex_attribs_[vv].Unbind(buffer);
  }
}

bool VertexAttribManager::ValidateBindings(
    const char* function_name,
    GLES2Decoder* decoder,
    FeatureInfo* feature_info,
    BufferManager* buffer_manager,
    Program* current_program,
    GLuint max_vertex_accessed,
    bool instanced,
    GLsizei primcount) {
  DCHECK(primcount);
  ErrorState* error_state = decoder->GetErrorState();
  // true if any enabled, used divisor is zero
  bool divisor0 = false;
  bool have_enabled_active_attribs = false;
  const GLuint kInitialBufferId = 0xFFFFFFFFU;
  GLuint current_buffer_id = kInitialBufferId;
  bool use_client_side_arrays_for_stream_buffers = feature_info->workarounds(
      ).use_client_side_arrays_for_stream_buffers;
  // Validate all attribs currently enabled. If they are used by the current
  // program then check that they have enough elements to handle the draw call.
  // If they are not used by the current program check that they have a buffer
  // assigned.
  for (VertexAttribList::iterator it = enabled_vertex_attribs_.begin();
       it != enabled_vertex_attribs_.end(); ++it) {
    VertexAttrib* attrib = *it;
    Buffer* buffer = attrib->buffer();
    if (!buffer_manager->RequestBufferAccess(
            error_state, buffer, function_name,
            "attached to enabled attrib %u",
            attrib->index())) {
      return false;
    }
    const Program::VertexAttrib* attrib_info =
        current_program->GetAttribInfoByLocation(attrib->index());
    if (attrib_info) {
      divisor0 |= (attrib->divisor() == 0);
      have_enabled_active_attribs = true;
      GLuint count = attrib->MaxVertexAccessed(primcount, max_vertex_accessed);
      // This attrib is used in the current program.
      if (!attrib->CanAccess(count)) {
        ERRORSTATE_SET_GL_ERROR(
            error_state, GL_INVALID_OPERATION, function_name,
            (std::string(
                "attempt to access out of range vertices in attribute ") +
             base::UintToString(attrib->index())).c_str());
        return false;
      }
      if (use_client_side_arrays_for_stream_buffers) {
        glEnableVertexAttribArray(attrib->index());
        if (buffer->IsClientSideArray()) {
          if (current_buffer_id != 0) {
            current_buffer_id = 0;
            glBindBuffer(GL_ARRAY_BUFFER, 0);
          }
          attrib->set_is_client_side_array(true);
          const void* ptr = buffer->GetRange(attrib->offset(), 0);
          DCHECK(ptr);
          glVertexAttribPointer(
              attrib->index(),
              attrib->size(),
              attrib->type(),
              attrib->normalized(),
              attrib->gl_stride(),
              ptr);
        } else if (attrib->is_client_side_array()) {
          attrib->set_is_client_side_array(false);
          GLuint new_buffer_id = buffer->service_id();
          if (new_buffer_id != current_buffer_id) {
            current_buffer_id = new_buffer_id;
            glBindBuffer(GL_ARRAY_BUFFER, current_buffer_id);
          }
          const void* ptr = reinterpret_cast<const void*>(attrib->offset());
          glVertexAttribPointer(
              attrib->index(),
              attrib->size(),
              attrib->type(),
              attrib->normalized(),
              attrib->gl_stride(),
              ptr);
        }
      }
    } else {
      // This attrib is not used in the current program.
      if (use_client_side_arrays_for_stream_buffers) {
        // Disable client side arrays for unused attributes else we'll
        // read bad memory
        if (buffer->IsClientSideArray()) {
          // Don't disable attrib 0 since it's special.
          if (attrib->index() > 0) {
            glDisableVertexAttribArray(attrib->index());
          }
        }
      }
    }
  }

  // Due to D3D9 limitation, in ES2/WebGL1, instanced drawing needs at least
  // one enabled attribute with divisor zero. This does not apply to D3D11,
  // therefore, it also does not apply to ES3/WebGL2.
  // Non-instanced drawing is fine with having no attributes at all, but if
  // there are attributes, at least one should have divisor zero.
  // (See ANGLE_instanced_arrays spec)
  if (feature_info->IsWebGL1OrES2Context() && !divisor0 &&
      (instanced || have_enabled_active_attribs)) {
    ERRORSTATE_SET_GL_ERROR(
        error_state, GL_INVALID_OPERATION, function_name,
        "attempt to draw with all attributes having non-zero divisors");
    return false;
  }

  if (current_buffer_id != kInitialBufferId) {
    // Restore the buffer binding.
    decoder->RestoreBufferBindings();
  }

  return true;
}

}  // namespace gles2
}  // namespace gpu
