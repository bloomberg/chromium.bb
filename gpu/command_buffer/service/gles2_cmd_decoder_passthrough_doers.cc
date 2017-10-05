// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"

#include "base/strings/string_number_conversions.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/dc_renderer_layer_params.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {
namespace gles2 {

namespace {

template <typename ClientType, typename ServiceType, typename GenFunction>
error::Error GenHelper(GLsizei n,
                       const volatile ClientType* client_ids,
                       ClientServiceMap<ClientType, ServiceType>* id_map,
                       GenFunction gen_function) {
  DCHECK(n >= 0);
  std::vector<ClientType> client_ids_copy(client_ids, client_ids + n);
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (id_map->GetServiceID(client_ids_copy[ii], nullptr)) {
      return error::kInvalidArguments;
    }
  }
  if (!CheckUniqueAndNonNullIds(n, client_ids_copy.data())) {
    return error::kInvalidArguments;
  }

  std::vector<ServiceType> service_ids(n, 0);
  gen_function(n, service_ids.data());
  for (GLsizei ii = 0; ii < n; ++ii) {
    id_map->SetIDMapping(client_ids_copy[ii], service_ids[ii]);
  }

  return error::kNoError;
}

template <typename ClientType, typename ServiceType, typename GenFunction>
error::Error CreateHelper(ClientType client_id,
                          ClientServiceMap<ClientType, ServiceType>* id_map,
                          GenFunction create_function) {
  if (id_map->GetServiceID(client_id, nullptr)) {
    return error::kInvalidArguments;
  }
  ServiceType service_id = create_function();
  id_map->SetIDMapping(client_id, service_id);
  return error::kNoError;
}

template <typename ClientType, typename ServiceType, typename DeleteFunction>
error::Error DeleteHelper(GLsizei n,
                          const volatile ClientType* client_ids,
                          ClientServiceMap<ClientType, ServiceType>* id_map,
                          DeleteFunction delete_function) {
  DCHECK(n >= 0);
  std::vector<ServiceType> service_ids(n, 0);
  for (GLsizei ii = 0; ii < n; ++ii) {
    ClientType client_id = client_ids[ii];

    // Don't pass service IDs of objects with a client ID of 0.  They are
    // emulated and should not be deleteable
    if (client_id != 0) {
      service_ids[ii] = id_map->GetServiceIDOrInvalid(client_id);
      id_map->RemoveClientID(client_id);
    }
  }

  delete_function(n, service_ids.data());

  return error::kNoError;
}

template <typename ClientType, typename ServiceType, typename DeleteFunction>
error::Error DeleteHelper(ClientType client_id,
                          ClientServiceMap<ClientType, ServiceType>* id_map,
                          DeleteFunction delete_function) {
  delete_function(id_map->GetServiceIDOrInvalid(client_id));
  id_map->RemoveClientID(client_id);
  return error::kNoError;
}

template <typename ClientType, typename ServiceType, typename GenFunction>
ServiceType GetServiceID(ClientType client_id,
                         ClientServiceMap<ClientType, ServiceType>* id_map,
                         bool create_if_missing,
                         GenFunction gen_function) {
  ServiceType service_id = id_map->invalid_service_id();
  if (id_map->GetServiceID(client_id, &service_id)) {
    return service_id;
  }

  if (create_if_missing) {
    service_id = gen_function();
    id_map->SetIDMapping(client_id, service_id);
    return service_id;
  }

  return id_map->invalid_service_id();
}

GLuint GetTextureServiceID(GLuint client_id,
                           PassthroughResources* resources,
                           bool create_if_missing) {
  return GetServiceID(client_id, &resources->texture_id_map, create_if_missing,
                      []() {
                        GLuint service_id = 0;
                        glGenTextures(1, &service_id);
                        return service_id;
                      });
}

GLuint GetBufferServiceID(GLuint client_id,
                          PassthroughResources* resources,
                          bool create_if_missing) {
  return GetServiceID(client_id, &resources->buffer_id_map, create_if_missing,
                      []() {
                        GLuint service_id = 0;
                        glGenBuffersARB(1, &service_id);
                        return service_id;
                      });
}

GLuint GetRenderbufferServiceID(GLuint client_id,
                                PassthroughResources* resources,
                                bool create_if_missing) {
  return GetServiceID(client_id, &resources->renderbuffer_id_map,
                      create_if_missing, []() {
                        GLuint service_id = 0;
                        glGenRenderbuffersEXT(1, &service_id);
                        return service_id;
                      });
}

GLuint GetFramebufferServiceID(GLuint client_id,
                               ClientServiceMap<GLuint, GLuint>* id_map,
                               bool create_if_missing) {
  return GetServiceID(client_id, id_map, create_if_missing, []() {
    GLuint service_id = 0;
    glGenFramebuffersEXT(1, &service_id);
    return service_id;
  });
}

GLuint GetTransformFeedbackServiceID(GLuint client_id,
                                     ClientServiceMap<GLuint, GLuint>* id_map) {
  return id_map->GetServiceIDOrInvalid(client_id);
}

GLuint GetVertexArrayServiceID(GLuint client_id,
                               ClientServiceMap<GLuint, GLuint>* id_map) {
  return id_map->GetServiceIDOrInvalid(client_id);
}

GLuint GetProgramServiceID(GLuint client_id, PassthroughResources* resources) {
  return resources->program_id_map.GetServiceIDOrInvalid(client_id);
}

GLuint GetShaderServiceID(GLuint client_id, PassthroughResources* resources) {
  return resources->shader_id_map.GetServiceIDOrInvalid(client_id);
}

GLuint GetQueryServiceID(GLuint client_id,
                         ClientServiceMap<GLuint, GLuint>* id_map) {
  return id_map->GetServiceIDOrInvalid(client_id);
}

GLuint GetSamplerServiceID(GLuint client_id, PassthroughResources* resources) {
  return resources->sampler_id_map.GetServiceIDOrInvalid(client_id);
}

GLsync GetSyncServiceID(GLuint client_id, PassthroughResources* resources) {
  return reinterpret_cast<GLsync>(
      resources->sync_id_map.GetServiceIDOrInvalid(client_id));
}

template <typename T>
void InsertValueIntoBuffer(std::vector<uint8_t>* data,
                           const T& value,
                           size_t offset) {
  DCHECK_LE(offset + sizeof(T), data->size());
  memcpy(data->data() + offset, &value, sizeof(T));
}

template <typename T>
void AppendValueToBuffer(std::vector<uint8_t>* data, const T& value) {
  const base::CheckedNumeric<size_t> old_size = data->size();
  data->resize((old_size + sizeof(T)).ValueOrDie());
  memcpy(data->data() + old_size.ValueOrDie(), &value, sizeof(T));
}

void AppendStringToBuffer(std::vector<uint8_t>* data,
                          const char* str,
                          size_t len) {
  const base::CheckedNumeric<size_t> old_size = data->size();
  data->resize((old_size + len).ValueOrDie());
  memcpy(data->data() + old_size.ValueOrDie(), str, len);
}

// In order to minimize the amount of data copied, the command buffer client
// unpack pixels before sending the glTex[Sub]Image[2|3]D calls. The only
// parameter it doesn't handle is the alignment. Resetting the unpack state is
// not needed when uploading from a PBO and for compressed formats which the
// client sends untouched. This class handles resetting and restoring the unpack
// state.
// TODO(cwallez@chromium.org) it would be nicer to handle the resetting /
// restoring on the client side.
class ScopedUnpackStateButAlignmentReset {
 public:
  ScopedUnpackStateButAlignmentReset(bool enable, bool is_3d) {
    if (!enable) {
      return;
    }

    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skip_pixels_);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skip_rows_);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length_);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    if (is_3d) {
      glGetIntegerv(GL_UNPACK_SKIP_IMAGES, &skip_images_);
      glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
      glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &image_height_);
      glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
    }
  }

  ~ScopedUnpackStateButAlignmentReset() {
    if (skip_pixels_ != 0) {
      glPixelStorei(GL_UNPACK_SKIP_PIXELS, skip_pixels_);
    }
    if (skip_rows_ != 0) {
      glPixelStorei(GL_UNPACK_SKIP_ROWS, skip_rows_);
    }
    if (skip_images_ != 0) {
      glPixelStorei(GL_UNPACK_SKIP_IMAGES, skip_images_);
    }
    if (row_length_ != 0) {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length_);
    }
    if (image_height_ != 0) {
      glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, image_height_);
    }
  }

 private:
  GLint skip_pixels_ = 0;
  GLint skip_rows_ = 0;
  GLint skip_images_ = 0;
  GLint row_length_ = 0;
  GLint image_height_ = 0;
};

class ScopedPackStateRowLengthReset {
 public:
  ScopedPackStateRowLengthReset(bool enable) {
    if (!enable) {
      return;
    }

    glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length_);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  }

  ~ScopedPackStateRowLengthReset() {
    if (row_length_ != 0) {
      glPixelStorei(GL_PACK_ROW_LENGTH, row_length_);
    }
  }

 private:
  GLint row_length_ = 0;
};

bool ModifyAttachmentForEmulatedFramebuffer(GLenum* attachment) {
  switch (*attachment) {
    case GL_BACK:
      *attachment = GL_COLOR_ATTACHMENT0;
      return true;

    case GL_DEPTH:
      *attachment = GL_DEPTH_ATTACHMENT;
      return true;

    case GL_STENCIL:
      *attachment = GL_STENCIL_ATTACHMENT;
      return true;

    default:
      return false;
  }
}

bool ModifyAttachmentsForEmulatedFramebuffer(std::vector<GLenum>* attachments) {
  for (GLenum& attachment : *attachments) {
    if (!ModifyAttachmentForEmulatedFramebuffer(&attachment)) {
      return false;
    }
  }

  return true;
}

}  // anonymous namespace

// Implementations of commands
error::Error GLES2DecoderPassthroughImpl::DoActiveTexture(GLenum texture) {
  FlushErrors();
  glActiveTexture(texture);
  if (FlushErrors()) {
    return error::kNoError;
  }

  active_texture_unit_ = static_cast<size_t>(texture) - GL_TEXTURE0;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoAttachShader(GLuint program,
                                                         GLuint shader) {
  glAttachShader(GetProgramServiceID(program, resources_),
                 GetShaderServiceID(shader, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindAttribLocation(
    GLuint program,
    GLuint index,
    const char* name) {
  glBindAttribLocation(GetProgramServiceID(program, resources_), index, name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindBuffer(GLenum target,
                                                       GLuint buffer) {
  FlushErrors();
  glBindBuffer(
      target, GetBufferServiceID(buffer, resources_, bind_generates_resource_));
  if (FlushErrors()) {
    return error::kNoError;
  }

  DCHECK(bound_buffers_.find(target) != bound_buffers_.end());
  bound_buffers_[target] = buffer;

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindBufferBase(GLenum target,
                                                           GLuint index,
                                                           GLuint buffer) {
  FlushErrors();
  glBindBufferBase(
      target, index,
      GetBufferServiceID(buffer, resources_, bind_generates_resource_));
  if (FlushErrors()) {
    return error::kNoError;
  }

  DCHECK(bound_buffers_.find(target) != bound_buffers_.end());
  bound_buffers_[target] = buffer;

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindBufferRange(GLenum target,
                                                            GLuint index,
                                                            GLuint buffer,
                                                            GLintptr offset,
                                                            GLsizeiptr size) {
  FlushErrors();
  glBindBufferRange(
      target, index,
      GetBufferServiceID(buffer, resources_, bind_generates_resource_), offset,
      size);
  if (FlushErrors()) {
    return error::kNoError;
  }

  DCHECK(bound_buffers_.find(target) != bound_buffers_.end());
  bound_buffers_[target] = buffer;

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFramebuffer(
    GLenum target,
    GLuint framebuffer) {
  FlushErrors();
  glBindFramebufferEXT(
      target, GetFramebufferServiceID(framebuffer, &framebuffer_id_map_,
                                      bind_generates_resource_));
  if (FlushErrors()) {
    return error::kNoError;
  }

  // Update tracking of the bound framebuffer
  switch (target) {
    case GL_FRAMEBUFFER_EXT:
      bound_draw_framebuffer_ = framebuffer;
      bound_read_framebuffer_ = framebuffer;
      break;

    case GL_DRAW_FRAMEBUFFER:
      bound_draw_framebuffer_ = framebuffer;
      break;

    case GL_READ_FRAMEBUFFER:
      bound_read_framebuffer_ = framebuffer;
      break;

    default:
      NOTREACHED();
      break;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindRenderbuffer(
    GLenum target,
    GLuint renderbuffer) {
  glBindRenderbufferEXT(target,
                        GetRenderbufferServiceID(renderbuffer, resources_,
                                                 bind_generates_resource_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindSampler(GLuint unit,
                                                        GLuint sampler) {
  glBindSampler(unit, GetSamplerServiceID(sampler, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindTexture(GLenum target,
                                                        GLuint texture) {
  GLuint service_id =
      GetTextureServiceID(texture, resources_, bind_generates_resource_);

  FlushErrors();

  glBindTexture(target, service_id);

  // Only update tracking if no error was generated in the bind call
  if (FlushErrors()) {
    return error::kNoError;
  }

  // Track the currently bound textures
  DCHECK(bound_textures_.find(target) != bound_textures_.end());
  DCHECK(bound_textures_[target].size() > active_texture_unit_);
  scoped_refptr<TexturePassthrough> texture_passthrough = nullptr;

  if (service_id != 0) {
    // Create a new texture object to track this texture
    auto texture_object_iter = resources_->texture_object_map.find(texture);
    if (texture_object_iter == resources_->texture_object_map.end()) {
      texture_passthrough = new TexturePassthrough(service_id, target);
      resources_->texture_object_map.insert(
          std::make_pair(texture, texture_passthrough));
    } else {
      texture_passthrough = texture_object_iter->second.get();
      // Shouldn't be possible to get here if this texture has a different
      // target than the one it was just bound to
      DCHECK(texture_object_iter->second->target() == target);
    }
  }

  bound_textures_[target][active_texture_unit_].client_id = texture;
  bound_textures_[target][active_texture_unit_].texture =
      std::move(texture_passthrough);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindTransformFeedback(
    GLenum target,
    GLuint transformfeedback) {
  glBindTransformFeedback(
      target, GetTransformFeedbackServiceID(transformfeedback,
                                            &transform_feedback_id_map_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendColor(GLclampf red,
                                                       GLclampf green,
                                                       GLclampf blue,
                                                       GLclampf alpha) {
  glBlendColor(red, green, blue, alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendEquation(GLenum mode) {
  glBlendEquation(mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendEquationSeparate(
    GLenum modeRGB,
    GLenum modeAlpha) {
  glBlendEquationSeparate(modeRGB, modeAlpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendFunc(GLenum sfactor,
                                                      GLenum dfactor) {
  glBlendFunc(sfactor, dfactor);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendFuncSeparate(GLenum srcRGB,
                                                              GLenum dstRGB,
                                                              GLenum srcAlpha,
                                                              GLenum dstAlpha) {
  glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBufferData(GLenum target,
                                                       GLsizeiptr size,
                                                       const void* data,
                                                       GLenum usage) {
  FlushErrors();
  glBufferData(target, size, data, usage);
  if (FlushErrors()) {
    return error::kNoError;
  }

  // Calling buffer data on a mapped buffer will implicitly unmap it
  resources_->mapped_buffer_map.erase(bound_buffers_[target]);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBufferSubData(GLenum target,
                                                          GLintptr offset,
                                                          GLsizeiptr size,
                                                          const void* data) {
  glBufferSubData(target, offset, size, data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCheckFramebufferStatus(
    GLenum target,
    uint32_t* result) {
  *result = glCheckFramebufferStatusEXT(target);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClear(GLbitfield mask) {
  glClear(mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferfi(GLenum buffer,
                                                          GLint drawbuffers,
                                                          GLfloat depth,
                                                          GLint stencil) {
  glClearBufferfi(buffer, drawbuffers, depth, stencil);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferfv(
    GLenum buffer,
    GLint drawbuffers,
    const volatile GLfloat* value) {
  glClearBufferfv(buffer, drawbuffers, const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferiv(
    GLenum buffer,
    GLint drawbuffers,
    const volatile GLint* value) {
  glClearBufferiv(buffer, drawbuffers, const_cast<const GLint*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferuiv(
    GLenum buffer,
    GLint drawbuffers,
    const volatile GLuint* value) {
  glClearBufferuiv(buffer, drawbuffers, const_cast<const GLuint*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearColor(GLclampf red,
                                                       GLclampf green,
                                                       GLclampf blue,
                                                       GLclampf alpha) {
  glClearColor(red, green, blue, alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearDepthf(GLclampf depth) {
  glClearDepthf(depth);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearStencil(GLint s) {
  glClearStencil(s);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClientWaitSync(GLuint sync,
                                                           GLbitfield flags,
                                                           GLuint64 timeout,
                                                           GLenum* result) {
  // Force GL_SYNC_FLUSH_COMMANDS_BIT to avoid infinite wait.
  GLbitfield modified_flags = flags | GL_SYNC_FLUSH_COMMANDS_BIT;
  *result = glClientWaitSync(GetSyncServiceID(sync, resources_), modified_flags,
                             timeout);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoColorMask(GLboolean red,
                                                      GLboolean green,
                                                      GLboolean blue,
                                                      GLboolean alpha) {
  glColorMask(red, green, blue, alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompileShader(GLuint shader) {
  glCompileShader(GetShaderServiceID(shader, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedTexImage2D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLsizei image_size,
    GLsizei data_size,
    const void* data) {
  glCompressedTexImage2DRobustANGLE(target, level, internalformat, width,
                                    height, border, image_size, data_size,
                                    data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLsizei image_size,
    GLsizei data_size,
    const void* data) {
  glCompressedTexSubImage2DRobustANGLE(target, level, xoffset, yoffset, width,
                                       height, format, image_size, data_size,
                                       data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedTexImage3D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLsizei image_size,
    GLsizei data_size,
    const void* data) {
  glCompressedTexImage3DRobustANGLE(target, level, internalformat, width,
                                    height, depth, border, image_size,
                                    data_size, data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedTexSubImage3D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLsizei image_size,
    GLsizei data_size,
    const void* data) {
  glCompressedTexSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset,
                                       width, height, depth, format, image_size,
                                       data_size, data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyBufferSubData(
    GLenum readtarget,
    GLenum writetarget,
    GLintptr readoffset,
    GLintptr writeoffset,
    GLsizeiptr size) {
  glCopyBufferSubData(readtarget, writetarget, readoffset, writeoffset, size);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyTexImage2D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLint border) {
  glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyTexSubImage2D(GLenum target,
                                                              GLint level,
                                                              GLint xoffset,
                                                              GLint yoffset,
                                                              GLint x,
                                                              GLint y,
                                                              GLsizei width,
                                                              GLsizei height) {
  glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyTexSubImage3D(GLenum target,
                                                              GLint level,
                                                              GLint xoffset,
                                                              GLint yoffset,
                                                              GLint zoffset,
                                                              GLint x,
                                                              GLint y,
                                                              GLsizei width,
                                                              GLsizei height) {
  glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width,
                      height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCreateProgram(GLuint client_id) {
  return CreateHelper(client_id, &resources_->program_id_map,
                      []() { return glCreateProgram(); });
}

error::Error GLES2DecoderPassthroughImpl::DoCreateShader(GLenum type,
                                                         GLuint client_id) {
  return CreateHelper(client_id, &resources_->shader_id_map,
                      [type]() { return glCreateShader(type); });
}

error::Error GLES2DecoderPassthroughImpl::DoCullFace(GLenum mode) {
  glCullFace(mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteBuffers(
    GLsizei n,
    const volatile GLuint* buffers) {
  // DeleteHelper requires that n is non-negative because it allocates a copy of
  // the IDs
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "n cannot be negative.");
    return error::kNoError;
  }

  std::vector<GLuint> service_ids(n, 0);
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = buffers[ii];

    // Update the bound and mapped buffer state tracking
    for (auto& buffer_binding : bound_buffers_) {
      if (buffer_binding.second == client_id) {
        buffer_binding.second = 0;
      }
      resources_->mapped_buffer_map.erase(client_id);
    }

    service_ids[ii] =
        resources_->buffer_id_map.GetServiceIDOrInvalid(client_id);
    resources_->buffer_id_map.RemoveClientID(client_id);
  }
  glDeleteBuffersARB(n, service_ids.data());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteFramebuffers(
    GLsizei n,
    const volatile GLuint* framebuffers) {
  // DeleteHelper requires that n is non-negative because it allocates a copy of
  // the IDs
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "n cannot be negative.");
    return error::kNoError;
  }

  std::vector<GLuint> framebuffers_copy(framebuffers, framebuffers + n);

  // If a bound framebuffer is deleted, it's binding is reset to 0.  In the case
  // of an emulated default framebuffer, bind the emulated one.
  for (GLuint framebuffer : framebuffers_copy) {
    if (framebuffer == bound_draw_framebuffer_) {
      bound_draw_framebuffer_ = 0;
      if (emulated_back_buffer_) {
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER,
                             emulated_back_buffer_->framebuffer_service_id);
      }
    }
    if (framebuffer == bound_read_framebuffer_) {
      bound_read_framebuffer_ = 0;
      if (emulated_back_buffer_) {
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER,
                             emulated_back_buffer_->framebuffer_service_id);
      }
    }
  }

  return DeleteHelper(n, framebuffers_copy.data(), &framebuffer_id_map_,
                      [](GLsizei n, GLuint* framebuffers) {
                        glDeleteFramebuffersEXT(n, framebuffers);
                      });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteProgram(GLuint program) {
  return DeleteHelper(program, &resources_->program_id_map,
                      [](GLuint program) { glDeleteProgram(program); });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteRenderbuffers(
    GLsizei n,
    const volatile GLuint* renderbuffers) {
  // DeleteHelper requires that n is non-negative because it allocates a copy of
  // the IDs
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "n cannot be negative.");
    return error::kNoError;
  }
  return DeleteHelper(n, renderbuffers, &resources_->renderbuffer_id_map,
                      [](GLsizei n, GLuint* renderbuffers) {
                        glDeleteRenderbuffersEXT(n, renderbuffers);
                      });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteSamplers(
    GLsizei n,
    const volatile GLuint* samplers) {
  // DeleteHelper requires that n is non-negative because it allocates a copy of
  // the IDs
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "n cannot be negative.");
    return error::kNoError;
  }
  return DeleteHelper(
      n, samplers, &resources_->sampler_id_map,
      [](GLsizei n, GLuint* samplers) { glDeleteSamplers(n, samplers); });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteSync(GLuint sync) {
  return DeleteHelper(sync, &resources_->sync_id_map, [](uintptr_t sync) {
    glDeleteSync(reinterpret_cast<GLsync>(sync));
  });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteShader(GLuint shader) {
  return DeleteHelper(shader, &resources_->shader_id_map,
                      [](GLuint shader) { glDeleteShader(shader); });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteTextures(
    GLsizei n,
    const volatile GLuint* textures) {
  // DeleteHelper requires that n is non-negative because it allocates a copy of
  // the IDs
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "n cannot be negative.");
    return error::kNoError;
  }

  // Textures that are currently associated with a mailbox are stored in the
  // texture_object_map_ and are deleted automatically when they are
  // unreferenced.  Only delete textures that are not in this map.
  std::vector<GLuint> non_mailbox_client_ids;
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = textures[ii];
    auto texture_object_iter = resources_->texture_object_map.find(client_id);
    if (texture_object_iter == resources_->texture_object_map.end()) {
      // Delete with DeleteHelper
      non_mailbox_client_ids.push_back(client_id);
    } else {
      // Deleted when unreferenced
      scoped_refptr<TexturePassthrough> texture = texture_object_iter->second;
      resources_->texture_id_map.RemoveClientID(client_id);
      resources_->texture_object_map.erase(client_id);
      UpdateTextureBinding(texture->target(), client_id, nullptr);
    }
  }
  return DeleteHelper(
      non_mailbox_client_ids.size(), non_mailbox_client_ids.data(),
      &resources_->texture_id_map,
      [](GLsizei n, GLuint* textures) { glDeleteTextures(n, textures); });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteTransformFeedbacks(
    GLsizei n,
    const volatile GLuint* ids) {
  // DeleteHelper requires that n is non-negative because it allocates a copy of
  // the IDs
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "n cannot be negative.");
    return error::kNoError;
  }
  return DeleteHelper(n, ids, &transform_feedback_id_map_,
                      [](GLsizei n, GLuint* transform_feedbacks) {
                        glDeleteTransformFeedbacks(n, transform_feedbacks);
                      });
}

error::Error GLES2DecoderPassthroughImpl::DoDepthFunc(GLenum func) {
  glDepthFunc(func);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDepthMask(GLboolean flag) {
  glDepthMask(flag);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDepthRangef(GLclampf zNear,
                                                        GLclampf zFar) {
  glDepthRangef(zNear, zFar);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDetachShader(GLuint program,
                                                         GLuint shader) {
  glDetachShader(GetProgramServiceID(program, resources_),
                 GetShaderServiceID(shader, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDisable(GLenum cap) {
  glDisable(cap);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDisableVertexAttribArray(
    GLuint index) {
  glDisableVertexAttribArray(index);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawArrays(GLenum mode,
                                                       GLint first,
                                                       GLsizei count) {
  glDrawArrays(mode, first, count);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawElements(GLenum mode,
                                                         GLsizei count,
                                                         GLenum type,
                                                         const void* indices) {
  glDrawElements(mode, count, type, indices);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEnable(GLenum cap) {
  glEnable(cap);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEnableVertexAttribArray(
    GLuint index) {
  glEnableVertexAttribArray(index);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFenceSync(GLenum condition,
                                                      GLbitfield flags,
                                                      GLuint client_id) {
  if (resources_->sync_id_map.GetServiceID(client_id, nullptr)) {
    return error::kInvalidArguments;
  }

  FlushErrors();
  GLsync service_id = glFenceSync(condition, flags);
  if (FlushErrors()) {
    return error::kInvalidArguments;
  }

  resources_->sync_id_map.SetIDMapping(client_id,
                                       reinterpret_cast<uintptr_t>(service_id));

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFinish() {
  glFinish();

  error::Error error = ProcessReadPixels(true);
  if (error != error::kNoError) {
    return error;
  }
  return ProcessQueries(true);
}

error::Error GLES2DecoderPassthroughImpl::DoFlush() {
  glFlush();

  error::Error error = ProcessReadPixels(false);
  if (error != error::kNoError) {
    return error;
  }
  return ProcessQueries(false);
}

error::Error GLES2DecoderPassthroughImpl::DoFlushMappedBufferRange(
    GLenum target,
    GLintptr offset,
    GLsizeiptr size) {
  auto bound_buffers_iter = bound_buffers_.find(target);
  if (bound_buffers_iter == bound_buffers_.end() ||
      bound_buffers_iter->second == 0) {
    InsertError(GL_INVALID_OPERATION, "No buffer bound to this target.");
    return error::kNoError;
  }

  GLuint client_buffer = bound_buffers_iter->second;
  auto mapped_buffer_info_iter =
      resources_->mapped_buffer_map.find(client_buffer);
  if (mapped_buffer_info_iter == resources_->mapped_buffer_map.end()) {
    InsertError(GL_INVALID_OPERATION, "Buffer is not mapped.");
    return error::kNoError;
  }

  const MappedBuffer& map_info = mapped_buffer_info_iter->second;

  if (offset < 0) {
    InsertError(GL_INVALID_VALUE, "Offset cannot be negative.");
    return error::kNoError;
  }

  if (size < 0) {
    InsertError(GL_INVALID_VALUE, "Size cannot be negative.");
    return error::kNoError;
  }

  base::CheckedNumeric<size_t> range_start(offset);
  base::CheckedNumeric<size_t> range_end = offset + size;
  if (!range_end.IsValid() && range_end.ValueOrDefault(0) > map_info.size) {
    InsertError(GL_INVALID_OPERATION,
                "Flush range is not within the original mapping size.");
    return error::kNoError;
  }

  uint8_t* mem = GetSharedMemoryAs<uint8_t*>(
      map_info.data_shm_id, map_info.data_shm_offset, map_info.size);
  if (!mem) {
    return error::kOutOfBounds;
  }

  memcpy(map_info.map_ptr + offset, mem + offset, size);
  glFlushMappedBufferRange(target, offset, size);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferRenderbuffer(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    GLuint renderbuffer) {
  if (IsEmulatedFramebufferBound(target)) {
    InsertError(GL_INVALID_OPERATION,
                "Cannot change the attachments of the default framebuffer.");
    return error::kNoError;
  }
  glFramebufferRenderbufferEXT(
      target, attachment, renderbuffertarget,
      GetRenderbufferServiceID(renderbuffer, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferTexture2D(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level) {
  if (IsEmulatedFramebufferBound(target)) {
    InsertError(GL_INVALID_OPERATION,
                "Cannot change the attachments of the default framebuffer.");
    return error::kNoError;
  }
  glFramebufferTexture2DEXT(target, attachment, textarget,
                            GetTextureServiceID(texture, resources_, false),
                            level);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferTextureLayer(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint layer) {
  if (IsEmulatedFramebufferBound(target)) {
    InsertError(GL_INVALID_OPERATION,
                "Cannot change the attachments of the default framebuffer.");
    return error::kNoError;
  }
  glFramebufferTextureLayer(target, attachment,
                            GetTextureServiceID(texture, resources_, false),
                            level, layer);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFrontFace(GLenum mode) {
  glFrontFace(mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenBuffers(
    GLsizei n,
    volatile GLuint* buffers) {
  return GenHelper(
      n, buffers, &resources_->buffer_id_map,
      [](GLsizei n, GLuint* buffers) { glGenBuffersARB(n, buffers); });
}

error::Error GLES2DecoderPassthroughImpl::DoGenerateMipmap(GLenum target) {
  glGenerateMipmapEXT(target);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenFramebuffers(
    GLsizei n,
    volatile GLuint* framebuffers) {
  return GenHelper(n, framebuffers, &framebuffer_id_map_,
                   [](GLsizei n, GLuint* framebuffers) {
                     glGenFramebuffersEXT(n, framebuffers);
                   });
}

error::Error GLES2DecoderPassthroughImpl::DoGenRenderbuffers(
    GLsizei n,
    volatile GLuint* renderbuffers) {
  return GenHelper(n, renderbuffers, &resources_->renderbuffer_id_map,
                   [](GLsizei n, GLuint* renderbuffers) {
                     glGenRenderbuffersEXT(n, renderbuffers);
                   });
}

error::Error GLES2DecoderPassthroughImpl::DoGenSamplers(
    GLsizei n,
    volatile GLuint* samplers) {
  return GenHelper(
      n, samplers, &resources_->sampler_id_map,
      [](GLsizei n, GLuint* samplers) { glGenSamplers(n, samplers); });
}

error::Error GLES2DecoderPassthroughImpl::DoGenTextures(
    GLsizei n,
    volatile GLuint* textures) {
  return GenHelper(
      n, textures, &resources_->texture_id_map,
      [](GLsizei n, GLuint* textures) { glGenTextures(n, textures); });
}

error::Error GLES2DecoderPassthroughImpl::DoGenTransformFeedbacks(
    GLsizei n,
    volatile GLuint* ids) {
  return GenHelper(n, ids, &transform_feedback_id_map_,
                   [](GLsizei n, GLuint* transform_feedbacks) {
                     glGenTransformFeedbacks(n, transform_feedbacks);
                   });
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveAttrib(GLuint program,
                                                            GLuint index,
                                                            GLint* size,
                                                            GLenum* type,
                                                            std::string* name,
                                                            int32_t* success) {
  FlushErrors();

  GLuint service_id = GetProgramServiceID(program, resources_);
  GLint active_attribute_max_length = 0;
  glGetProgramiv(service_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                 &active_attribute_max_length);
  if (FlushErrors()) {
    *success = 0;
    return error::kNoError;
  }

  std::vector<char> name_buffer(active_attribute_max_length, 0);
  glGetActiveAttrib(service_id, index, name_buffer.size(), nullptr, size, type,
                    name_buffer.data());
  *name = std::string(name_buffer.data());
  *success = FlushErrors() ? 0 : 1;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniform(GLuint program,
                                                             GLuint index,
                                                             GLint* size,
                                                             GLenum* type,
                                                             std::string* name,
                                                             int32_t* success) {
  FlushErrors();

  GLuint service_id = GetProgramServiceID(program, resources_);
  GLint active_uniform_max_length = 0;
  glGetProgramiv(service_id, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                 &active_uniform_max_length);
  if (FlushErrors()) {
    *success = 0;
    return error::kNoError;
  }

  std::vector<char> name_buffer(active_uniform_max_length, 0);
  glGetActiveUniform(service_id, index, name_buffer.size(), nullptr, size, type,
                     name_buffer.data());
  *name = std::string(name_buffer.data());
  *success = FlushErrors() ? 0 : 1;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniformBlockiv(
    GLuint program,
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  glGetActiveUniformBlockivRobustANGLE(GetProgramServiceID(program, resources_),
                                       index, pname, bufSize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniformBlockName(
    GLuint program,
    GLuint index,
    std::string* name) {
  FlushErrors();

  GLuint program_service_id = GetProgramServiceID(program, resources_);
  GLint max_name_length = 0;
  glGetProgramiv(program_service_id, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH,
                 &max_name_length);

  if (FlushErrors()) {
    return error::kNoError;
  }

  std::vector<GLchar> buffer(max_name_length, 0);
  GLsizei length = 0;
  glGetActiveUniformBlockName(program_service_id, index, max_name_length,
                              &length, buffer.data());
  DCHECK(length <= max_name_length);
  *name = length > 0 ? std::string(buffer.data(), length) : std::string();

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniformsiv(
    GLuint program,
    GLsizei count,
    const GLuint* indices,
    GLenum pname,
    GLint* params) {
  glGetActiveUniformsiv(GetProgramServiceID(program, resources_), count,
                        indices, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetAttachedShaders(
    GLuint program,
    GLsizei maxcount,
    GLsizei* count,
    GLuint* shaders) {
  glGetAttachedShaders(GetProgramServiceID(program, resources_), maxcount,
                       count, shaders);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetAttribLocation(GLuint program,
                                                              const char* name,
                                                              GLint* result) {
  *result = glGetAttribLocation(GetProgramServiceID(program, resources_), name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetBufferSubDataAsyncCHROMIUM(
    GLenum target,
    GLintptr offset,
    GLsizeiptr size,
    uint8_t* mem) {
  FlushErrors();

  void* mapped_ptr = glMapBufferRange(target, offset, size, GL_MAP_READ_BIT);
  if (FlushErrors() || mapped_ptr == nullptr) {
    // Had an error while mapping, don't copy any data
    return error::kNoError;
  }

  memcpy(mem, mapped_ptr, size);
  glUnmapBuffer(target);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetBooleanv(GLenum pname,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        GLboolean* params) {
  return GetNumericHelper(
      pname, bufsize, length, params,
      [](GLenum pname, GLsizei bufsize, GLsizei* length, GLboolean* params) {
        glGetBooleanvRobustANGLE(pname, bufsize, length, params);
      });
}

error::Error GLES2DecoderPassthroughImpl::DoGetBufferParameteri64v(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint64* params) {
  FlushErrors();
  glGetBufferParameteri64vRobustANGLE(target, pname, bufsize, length, params);
  if (FlushErrors()) {
    return error::kNoError;
  }
  PatchGetBufferResults(target, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetBufferParameteriv(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  FlushErrors();
  glGetBufferParameterivRobustANGLE(target, pname, bufsize, length, params);
  if (FlushErrors()) {
    return error::kNoError;
  }
  PatchGetBufferResults(target, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetError(uint32_t* result) {
  FlushErrors();
  *result = PopError();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFloatv(GLenum pname,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  return GetNumericHelper(
      pname, bufsize, length, params,
      [](GLenum pname, GLsizei bufsize, GLsizei* length, GLfloat* params) {
        glGetFloatvRobustANGLE(pname, bufsize, length, params);
      });
}

error::Error GLES2DecoderPassthroughImpl::DoGetFragDataLocation(
    GLuint program,
    const char* name,
    GLint* result) {
  *result =
      glGetFragDataLocation(GetProgramServiceID(program, resources_), name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFramebufferAttachmentParameteriv(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  GLenum updated_attachment = attachment;
  if (IsEmulatedFramebufferBound(target)) {
    // Update the attachment do the equivalent one in the emulated framebuffer
    if (!ModifyAttachmentForEmulatedFramebuffer(&updated_attachment)) {
      InsertError(GL_INVALID_OPERATION, "Invalid attachment.");
      *length = 0;
      return error::kNoError;
    }

    // Generate errors for parameter names that are only valid for non-default
    // framebuffers
    switch (pname) {
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
        InsertError(GL_INVALID_ENUM, "Invalid parameter name.");
        *length = 0;
        return error::kNoError;
    }
  }

  FlushErrors();

  // Get a scratch buffer to hold the result of the query
  GLint* scratch_params = GetTypedScratchMemory<GLint>(bufsize);
  glGetFramebufferAttachmentParameterivRobustANGLE(
      target, updated_attachment, pname, bufsize, length, scratch_params);

  if (FlushErrors()) {
    DCHECK(*length == 0);
    return error::kNoError;
  }

  // Update the results of the query, if needed
  error::Error error = PatchGetFramebufferAttachmentParameter(
      target, updated_attachment, pname, *length, scratch_params);
  if (error != error::kNoError) {
    *length = 0;
    return error;
  }

  // Copy into the destination
  DCHECK(*length < bufsize);
  std::copy(scratch_params, scratch_params + *length, params);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetInteger64v(GLenum pname,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          GLint64* params) {
  return GetNumericHelper(
      pname, bufsize, length, params,
      [](GLenum pname, GLsizei bufsize, GLsizei* length, GLint64* params) {
        glGetInteger64vRobustANGLE(pname, bufsize, length, params);
      });
}

error::Error GLES2DecoderPassthroughImpl::DoGetIntegeri_v(GLenum pname,
                                                          GLuint index,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          GLint* data) {
  glGetIntegeri_vRobustANGLE(pname, index, bufsize, length, data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetInteger64i_v(GLenum pname,
                                                            GLuint index,
                                                            GLsizei bufsize,
                                                            GLsizei* length,
                                                            GLint64* data) {
  glGetInteger64i_vRobustANGLE(pname, index, bufsize, length, data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetIntegerv(GLenum pname,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        GLint* params) {
  return GetNumericHelper(
      pname, bufsize, length, params,
      [](GLenum pname, GLsizei bufsize, GLsizei* length, GLint* params) {
        glGetIntegervRobustANGLE(pname, bufsize, length, params);
      });
}

error::Error GLES2DecoderPassthroughImpl::DoGetInternalformativ(GLenum target,
                                                                GLenum format,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei* length,
                                                                GLint* params) {
  glGetInternalformativRobustANGLE(target, format, pname, bufSize, length,
                                   params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramiv(GLuint program,
                                                         GLenum pname,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         GLint* params) {
  glGetProgramivRobustANGLE(GetProgramServiceID(program, resources_), pname,
                            bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramInfoLog(
    GLuint program,
    std::string* infolog) {
  FlushErrors();
  GLint info_log_len = 0;
  glGetProgramiv(GetProgramServiceID(program, resources_), GL_INFO_LOG_LENGTH,
                 &info_log_len);

  if (FlushErrors()) {
    return error::kNoError;
  }

  std::vector<char> buffer(info_log_len, 0);
  GLsizei length = 0;
  glGetProgramInfoLog(GetProgramServiceID(program, resources_), info_log_len,
                      &length, buffer.data());
  DCHECK(length <= info_log_len);
  *infolog = length > 0 ? std::string(buffer.data(), length) : std::string();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetRenderbufferParameteriv(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  glGetRenderbufferParameterivRobustANGLE(target, pname, bufsize, length,
                                          params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetSamplerParameterfv(
    GLuint sampler,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLfloat* params) {
  glGetSamplerParameterfvRobustANGLE(GetSamplerServiceID(sampler, resources_),
                                     pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetSamplerParameteriv(
    GLuint sampler,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  glGetSamplerParameterivRobustANGLE(GetSamplerServiceID(sampler, resources_),
                                     pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderiv(GLuint shader,
                                                        GLenum pname,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        GLint* params) {
  glGetShaderivRobustANGLE(GetShaderServiceID(shader, resources_), pname,
                           bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderInfoLog(
    GLuint shader,
    std::string* infolog) {
  FlushErrors();

  GLuint service_id = GetShaderServiceID(shader, resources_);
  GLint info_log_len = 0;
  glGetShaderiv(service_id, GL_INFO_LOG_LENGTH, &info_log_len);
  if (FlushErrors()) {
    return error::kNoError;
  }

  std::vector<char> buffer(info_log_len, 0);
  GLsizei length = 0;
  glGetShaderInfoLog(service_id, info_log_len, &length, buffer.data());
  DCHECK(length <= info_log_len);
  *infolog = length > 0 ? std::string(buffer.data(), length) : std::string();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderPrecisionFormat(
    GLenum shadertype,
    GLenum precisiontype,
    GLint* range,
    GLint* precision,
    int32_t* success) {
  FlushErrors();
  glGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
  *success = FlushErrors() ? 0 : 1;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderSource(
    GLuint shader,
    std::string* source) {
  FlushErrors();

  GLuint shader_service_id = GetShaderServiceID(shader, resources_);
  GLint shader_source_length = 0;
  glGetShaderiv(shader_service_id, GL_SHADER_SOURCE_LENGTH,
                &shader_source_length);
  if (FlushErrors()) {
    return error::kNoError;
  }

  std::vector<char> buffer(shader_source_length, 0);
  GLsizei length = 0;
  glGetShaderSource(shader_service_id, shader_source_length, &length,
                    buffer.data());
  DCHECK(length <= shader_source_length);
  *source = shader_source_length > 0 ? std::string(buffer.data(), length)
                                     : std::string();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetString(GLenum name,
                                                      const char** result) {
  switch (name) {
    case GL_VERSION:
      *result = GetServiceVersionString(feature_info_.get());
      break;
    case GL_SHADING_LANGUAGE_VERSION:
      *result = GetServiceShadingLanguageVersionString(feature_info_.get());
      break;
    case GL_EXTENSIONS:
      *result = feature_info_->extensions().c_str();
      break;
    default:
      *result = reinterpret_cast<const char*>(glGetString(name));
      break;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetSynciv(GLuint sync,
                                                      GLenum pname,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      GLint* values) {
  glGetSynciv(GetSyncServiceID(sync, resources_), pname, bufsize, length,
              values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTexParameterfv(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLfloat* params) {
  glGetTexParameterfvRobustANGLE(target, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTexParameteriv(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLint* params) {
  glGetTexParameterivRobustANGLE(target, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTransformFeedbackVarying(
    GLuint program,
    GLuint index,
    GLsizei* size,
    GLenum* type,
    std::string* name,
    int32_t* success) {
  FlushErrors();

  GLuint service_id = GetProgramServiceID(program, resources_);
  GLint transform_feedback_varying_max_length = 0;
  glGetProgramiv(service_id, GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH,
                 &transform_feedback_varying_max_length);
  if (FlushErrors()) {
    *success = 0;
    return error::kNoError;
  }

  std::vector<char> name_buffer(transform_feedback_varying_max_length, 0);
  glGetTransformFeedbackVarying(service_id, index, name_buffer.size(), nullptr,
                                size, type, name_buffer.data());
  *name = std::string(name_buffer.data());
  *success = FlushErrors() ? 0 : 1;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformBlockIndex(
    GLuint program,
    const char* name,
    GLint* index) {
  *index =
      glGetUniformBlockIndex(GetProgramServiceID(program, resources_), name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformfv(GLuint program,
                                                         GLint location,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         GLfloat* params) {
  // GetUniform*RobustANGLE entry points expect bufsize in bytes like the entry
  // points in GL_EXT_robustness
  glGetUniformfvRobustANGLE(GetProgramServiceID(program, resources_), location,
                            bufsize * sizeof(*params), length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformiv(GLuint program,
                                                         GLint location,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         GLint* params) {
  // GetUniform*RobustANGLE entry points expect bufsize in bytes like the entry
  // points in GL_EXT_robustness
  glGetUniformivRobustANGLE(GetProgramServiceID(program, resources_), location,
                            bufsize * sizeof(*params), length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformuiv(GLuint program,
                                                          GLint location,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          GLuint* params) {
  // GetUniform*RobustANGLE entry points expect bufsize in bytes like the entry
  // points in GL_EXT_robustness
  glGetUniformuivRobustANGLE(GetProgramServiceID(program, resources_), location,
                             bufsize * sizeof(*params), length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformIndices(
    GLuint program,
    GLsizei count,
    const char* const* names,
    GLsizei bufSize,
    GLuint* indices) {
  glGetUniformIndices(GetProgramServiceID(program, resources_), count, names,
                      indices);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformLocation(
    GLuint program,
    const char* name,
    GLint* location) {
  *location =
      glGetUniformLocation(GetProgramServiceID(program, resources_), name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribfv(GLuint index,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLfloat* params) {
  glGetVertexAttribfvRobustANGLE(index, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribiv(GLuint index,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLint* params) {
  glGetVertexAttribivRobustANGLE(index, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribIiv(GLuint index,
                                                               GLenum pname,
                                                               GLsizei bufsize,
                                                               GLsizei* length,
                                                               GLint* params) {
  glGetVertexAttribIivRobustANGLE(index, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribIuiv(
    GLuint index,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLuint* params) {
  glGetVertexAttribIuivRobustANGLE(index, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribPointerv(
    GLuint index,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLuint* pointer) {
  std::array<void*, 1> temp_pointers{{nullptr}};
  GLsizei temp_length = 0;
  glGetVertexAttribPointervRobustANGLE(
      index, pname, static_cast<GLsizei>(temp_pointers.size()), &temp_length,
      temp_pointers.data());
  DCHECK(temp_length >= 0 &&
         temp_length <= static_cast<GLsizei>(temp_pointers.size()) &&
         temp_length <= bufsize);
  for (GLsizei ii = 0; ii < temp_length; ii++) {
    pointer[ii] =
        static_cast<GLuint>(reinterpret_cast<uintptr_t>(temp_pointers[ii]));
  }
  *length = temp_length;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoHint(GLenum target, GLenum mode) {
  glHint(target, mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInvalidateFramebuffer(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments) {
  // Validate that count is non-negative before allocating a vector
  if (count < 0) {
    InsertError(GL_INVALID_VALUE, "count cannot be negative.");
    return error::kNoError;
  }

  std::vector<GLenum> attachments_copy(attachments, attachments + count);
  if (IsEmulatedFramebufferBound(target)) {
    // Update the attachment do the equivalent one in the emulated framebuffer
    if (!ModifyAttachmentsForEmulatedFramebuffer(&attachments_copy)) {
      InsertError(GL_INVALID_OPERATION, "Invalid attachment.");
      return error::kNoError;
    }
  }
  glInvalidateFramebuffer(target, count, attachments_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInvalidateSubFramebuffer(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  // Validate that count is non-negative before allocating a vector
  if (count < 0) {
    InsertError(GL_INVALID_VALUE, "count cannot be negative.");
    return error::kNoError;
  }

  std::vector<GLenum> attachments_copy(attachments, attachments + count);
  if (IsEmulatedFramebufferBound(target)) {
    // Update the attachment do the equivalent one in the emulated framebuffer
    if (!ModifyAttachmentsForEmulatedFramebuffer(&attachments_copy)) {
      InsertError(GL_INVALID_OPERATION, "Invalid attachment.");
      return error::kNoError;
    }
  }
  glInvalidateSubFramebuffer(target, count, attachments_copy.data(), x, y,
                             width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsBuffer(GLuint buffer,
                                                     uint32_t* result) {
  *result = glIsBuffer(GetBufferServiceID(buffer, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsEnabled(GLenum cap,
                                                      uint32_t* result) {
  *result = glIsEnabled(cap);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsFramebuffer(GLuint framebuffer,
                                                          uint32_t* result) {
  *result = glIsFramebufferEXT(
      GetFramebufferServiceID(framebuffer, &framebuffer_id_map_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsProgram(GLuint program,
                                                      uint32_t* result) {
  *result = glIsProgram(GetProgramServiceID(program, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsRenderbuffer(GLuint renderbuffer,
                                                           uint32_t* result) {
  *result = glIsRenderbufferEXT(
      GetRenderbufferServiceID(renderbuffer, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsSampler(GLuint sampler,
                                                      uint32_t* result) {
  *result = glIsSampler(GetSamplerServiceID(sampler, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsShader(GLuint shader,
                                                     uint32_t* result) {
  *result = glIsShader(GetShaderServiceID(shader, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsSync(GLuint sync,
                                                   uint32_t* result) {
  *result = glIsSync(GetSyncServiceID(sync, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsTexture(GLuint texture,
                                                      uint32_t* result) {
  *result = glIsTexture(GetTextureServiceID(texture, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsTransformFeedback(
    GLuint transformfeedback,
    uint32_t* result) {
  *result = glIsTransformFeedback(GetTransformFeedbackServiceID(
      transformfeedback, &transform_feedback_id_map_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoLineWidth(GLfloat width) {
  glLineWidth(width);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoLinkProgram(GLuint program) {
  glLinkProgram(GetProgramServiceID(program, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPauseTransformFeedback() {
  glPauseTransformFeedback();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPixelStorei(GLenum pname,
                                                        GLint param) {
  glPixelStorei(pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPolygonOffset(GLfloat factor,
                                                          GLfloat units) {
  glPolygonOffset(factor, units);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReadBuffer(GLenum src) {
  glReadBuffer(src);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReadPixels(GLint x,
                                                       GLint y,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLenum format,
                                                       GLenum type,
                                                       GLsizei bufsize,
                                                       GLsizei* length,
                                                       GLsizei* columns,
                                                       GLsizei* rows,
                                                       void* pixels,
                                                       int32_t* success) {
  FlushErrors();
  ScopedPackStateRowLengthReset reset_row_length(
      bufsize != 0 && feature_info_->gl_version_info().is_es3);
  glReadPixelsRobustANGLE(x, y, width, height, format, type, bufsize, length,
                          columns, rows, pixels);
  *success = FlushErrors() ? 0 : 1;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReadPixelsAsync(
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    GLsizei bufsize,
    GLsizei* length,
    GLsizei* columns,
    GLsizei* rows,
    uint32_t pixels_shm_id,
    uint32_t pixels_shm_offset,
    uint32_t result_shm_id,
    uint32_t result_shm_offset) {
  DCHECK(feature_info_->feature_flags().use_async_readpixels &&
         bound_buffers_[GL_PIXEL_PACK_BUFFER] == 0);

  FlushErrors();
  ScopedPackStateRowLengthReset reset_row_length(
      bufsize != 0 && feature_info_->gl_version_info().is_es3);

  PendingReadPixels pending_read_pixels;
  pending_read_pixels.pixels_shm_id = pixels_shm_id;
  pending_read_pixels.pixels_shm_offset = pixels_shm_offset;
  pending_read_pixels.result_shm_id = result_shm_id;
  pending_read_pixels.result_shm_offset = result_shm_offset;

  glGenBuffersARB(1, &pending_read_pixels.buffer_service_id);
  glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, pending_read_pixels.buffer_service_id);

  // GL_STREAM_READ is not available until ES3.
  const GLenum usage_hint = feature_info_->gl_version_info().IsAtLeastGLES(3, 0)
                                ? GL_STREAM_READ
                                : GL_STATIC_DRAW;

  const uint32_t bytes_per_pixel =
      GLES2Util::ComputeImageGroupSize(format, type);
  if (bytes_per_pixel == 0) {
    InsertError(GL_INVALID_ENUM, "Invalid ReadPixels format or type.");
    return error::kNoError;
  }

  if (width < 0 || height < 0) {
    InsertError(GL_INVALID_VALUE, "Width and height cannot be negative.");
    return error::kNoError;
  }

  if (!base::CheckMul(bytes_per_pixel, width, height)
           .AssignIfValid(&pending_read_pixels.pixels_size)) {
    return error::kOutOfBounds;
  }

  glBufferData(GL_PIXEL_PACK_BUFFER_ARB, pending_read_pixels.pixels_size,
               nullptr, usage_hint);

  // No need to worry about ES3 pixel pack parameters, because no
  // PIXEL_PACK_BUFFER is bound, and all these settings haven't been
  // sent to GL.
  glReadPixels(x, y, width, height, format, type, nullptr);

  glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);

  // Test for errors now before creating a fence
  if (FlushErrors()) {
    return error::kNoError;
  }

  pending_read_pixels.fence.reset(gl::GLFence::Create());

  if (FlushErrors()) {
    return error::kNoError;
  }

  pending_read_pixels_.push_back(std::move(pending_read_pixels));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReleaseShaderCompiler() {
  glReleaseShaderCompiler();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoRenderbufferStorage(
    GLenum target,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  glRenderbufferStorageEXT(target, internalformat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoResumeTransformFeedback() {
  glResumeTransformFeedback();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSampleCoverage(GLclampf value,
                                                           GLboolean invert) {
  glSampleCoverage(value, invert);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameterf(GLuint sampler,
                                                              GLenum pname,
                                                              GLfloat param) {
  glSamplerParameterf(GetSamplerServiceID(sampler, resources_), pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameterfv(
    GLuint sampler,
    GLenum pname,
    const volatile GLfloat* params) {
  std::array<GLfloat, 1> params_copy{{params[0]}};
  glSamplerParameterfvRobustANGLE(
      GetSamplerServiceID(sampler, resources_), pname,
      static_cast<GLsizei>(params_copy.size()), params_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameteri(GLuint sampler,
                                                              GLenum pname,
                                                              GLint param) {
  glSamplerParameteri(GetSamplerServiceID(sampler, resources_), pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameteriv(
    GLuint sampler,
    GLenum pname,
    const volatile GLint* params) {
  std::array<GLint, 1> params_copy{{params[0]}};
  glSamplerParameterivRobustANGLE(
      GetSamplerServiceID(sampler, resources_), pname,
      static_cast<GLsizei>(params_copy.size()), params_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScissor(GLint x,
                                                    GLint y,
                                                    GLsizei width,
                                                    GLsizei height) {
  glScissor(x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoShaderBinary(GLsizei n,
                                                         const GLuint* shaders,
                                                         GLenum binaryformat,
                                                         const void* binary,
                                                         GLsizei length) {
  std::vector<GLuint> service_shaders(n, 0);
  for (GLsizei i = 0; i < n; i++) {
    service_shaders[i] = GetShaderServiceID(shaders[i], resources_);
  }
  glShaderBinary(n, service_shaders.data(), binaryformat, binary, length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoShaderSource(GLuint shader,
                                                         GLsizei count,
                                                         const char** string,
                                                         const GLint* length) {
  glShaderSource(GetShaderServiceID(shader, resources_), count, string, length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilFunc(GLenum func,
                                                        GLint ref,
                                                        GLuint mask) {
  glStencilFunc(func, ref, mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilFuncSeparate(GLenum face,
                                                                GLenum func,
                                                                GLint ref,
                                                                GLuint mask) {
  glStencilFuncSeparate(face, func, ref, mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilMask(GLuint mask) {
  glStencilMask(mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilMaskSeparate(GLenum face,
                                                                GLuint mask) {
  glStencilMaskSeparate(face, mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilOp(GLenum fail,
                                                      GLenum zfail,
                                                      GLenum zpass) {
  glStencilOp(fail, zfail, zpass);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilOpSeparate(GLenum face,
                                                              GLenum fail,
                                                              GLenum zfail,
                                                              GLenum zpass) {
  glStencilOpSeparate(face, fail, zfail, zpass);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexImage2D(GLenum target,
                                                       GLint level,
                                                       GLint internalformat,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLint border,
                                                       GLenum format,
                                                       GLenum type,
                                                       GLsizei image_size,
                                                       const void* pixels) {
  ScopedUnpackStateButAlignmentReset reset_unpack(
      image_size != 0 && feature_info_->gl_version_info().is_es3, false);
  glTexImage2DRobustANGLE(target, level, internalformat, width, height, border,
                          format, type, image_size, pixels);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexImage3D(GLenum target,
                                                       GLint level,
                                                       GLint internalformat,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLsizei depth,
                                                       GLint border,
                                                       GLenum format,
                                                       GLenum type,
                                                       GLsizei image_size,
                                                       const void* pixels) {
  ScopedUnpackStateButAlignmentReset reset_unpack(
      image_size != 0 && feature_info_->gl_version_info().is_es3, true);
  glTexImage3DRobustANGLE(target, level, internalformat, width, height, depth,
                          border, format, type, image_size, pixels);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameterf(GLenum target,
                                                          GLenum pname,
                                                          GLfloat param) {
  glTexParameterf(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameterfv(
    GLenum target,
    GLenum pname,
    const volatile GLfloat* params) {
  std::array<GLfloat, 1> params_copy{{params[0]}};
  glTexParameterfvRobustANGLE(target, pname,
                              static_cast<GLsizei>(params_copy.size()),
                              params_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameteri(GLenum target,
                                                          GLenum pname,
                                                          GLint param) {
  glTexParameteri(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameteriv(
    GLenum target,
    GLenum pname,
    const volatile GLint* params) {
  std::array<GLint, 1> params_copy{{params[0]}};
  glTexParameterivRobustANGLE(target, pname,
                              static_cast<GLsizei>(params_copy.size()),
                              params_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexStorage3D(GLenum target,
                                                         GLsizei levels,
                                                         GLenum internalFormat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLsizei depth) {
  glTexStorage3D(target, levels, internalFormat, width, height, depth);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexSubImage2D(GLenum target,
                                                          GLint level,
                                                          GLint xoffset,
                                                          GLint yoffset,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLenum format,
                                                          GLenum type,
                                                          GLsizei image_size,
                                                          const void* pixels) {
  ScopedUnpackStateButAlignmentReset reset_unpack(
      image_size != 0 && feature_info_->gl_version_info().is_es3, false);
  glTexSubImage2DRobustANGLE(target, level, xoffset, yoffset, width, height,
                             format, type, image_size, pixels);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexSubImage3D(GLenum target,
                                                          GLint level,
                                                          GLint xoffset,
                                                          GLint yoffset,
                                                          GLint zoffset,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLsizei depth,
                                                          GLenum format,
                                                          GLenum type,
                                                          GLsizei image_size,
                                                          const void* pixels) {
  ScopedUnpackStateButAlignmentReset reset_unpack(
      image_size != 0 && feature_info_->gl_version_info().is_es3, true);
  glTexSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset, width,
                             height, depth, format, type, image_size, pixels);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTransformFeedbackVaryings(
    GLuint program,
    GLsizei count,
    const char** varyings,
    GLenum buffermode) {
  glTransformFeedbackVaryings(GetProgramServiceID(program, resources_), count,
                              varyings, buffermode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1f(GLint location,
                                                      GLfloat x) {
  glUniform1f(location, x);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1fv(
    GLint location,
    GLsizei count,
    const volatile GLfloat* v) {
  glUniform1fv(location, count, const_cast<const GLfloat*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1i(GLint location, GLint x) {
  glUniform1i(location, x);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1iv(
    GLint location,
    GLsizei count,
    const volatile GLint* v) {
  glUniform1iv(location, count, const_cast<const GLint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1ui(GLint location,
                                                       GLuint x) {
  glUniform1ui(location, x);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1uiv(
    GLint location,
    GLsizei count,
    const volatile GLuint* v) {
  glUniform1uiv(location, count, const_cast<const GLuint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2f(GLint location,
                                                      GLfloat x,
                                                      GLfloat y) {
  glUniform2f(location, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2fv(
    GLint location,
    GLsizei count,
    const volatile GLfloat* v) {
  glUniform2fv(location, count, const_cast<const GLfloat*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2i(GLint location,
                                                      GLint x,
                                                      GLint y) {
  glUniform2i(location, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2iv(
    GLint location,
    GLsizei count,
    const volatile GLint* v) {
  glUniform2iv(location, count, const_cast<const GLint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2ui(GLint location,
                                                       GLuint x,
                                                       GLuint y) {
  glUniform2ui(location, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2uiv(
    GLint location,
    GLsizei count,
    const volatile GLuint* v) {
  glUniform2uiv(location, count, const_cast<const GLuint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3f(GLint location,
                                                      GLfloat x,
                                                      GLfloat y,
                                                      GLfloat z) {
  glUniform3f(location, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3fv(
    GLint location,
    GLsizei count,
    const volatile GLfloat* v) {
  glUniform3fv(location, count, const_cast<const GLfloat*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3i(GLint location,
                                                      GLint x,
                                                      GLint y,
                                                      GLint z) {
  glUniform3i(location, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3iv(
    GLint location,
    GLsizei count,
    const volatile GLint* v) {
  glUniform3iv(location, count, const_cast<const GLint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3ui(GLint location,
                                                       GLuint x,
                                                       GLuint y,
                                                       GLuint z) {
  glUniform3ui(location, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3uiv(
    GLint location,
    GLsizei count,
    const volatile GLuint* v) {
  glUniform3uiv(location, count, const_cast<const GLuint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4f(GLint location,
                                                      GLfloat x,
                                                      GLfloat y,
                                                      GLfloat z,
                                                      GLfloat w) {
  glUniform4f(location, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4fv(
    GLint location,
    GLsizei count,
    const volatile GLfloat* v) {
  glUniform4fv(location, count, const_cast<const GLfloat*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4i(GLint location,
                                                      GLint x,
                                                      GLint y,
                                                      GLint z,
                                                      GLint w) {
  glUniform4i(location, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4iv(
    GLint location,
    GLsizei count,
    const volatile GLint* v) {
  glUniform4iv(location, count, const_cast<const GLint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4ui(GLint location,
                                                       GLuint x,
                                                       GLuint y,
                                                       GLuint z,
                                                       GLuint w) {
  glUniform4ui(location, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4uiv(
    GLint location,
    GLsizei count,
    const volatile GLuint* v) {
  glUniform4uiv(location, count, const_cast<const GLuint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformBlockBinding(
    GLuint program,
    GLuint index,
    GLuint binding) {
  glUniformBlockBinding(GetProgramServiceID(program, resources_), index,
                        binding);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix2fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix2fv(location, count, transpose,
                     const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix2x3fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix2x3fv(location, count, transpose,
                       const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix2x4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix2x4fv(location, count, transpose,
                       const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix3fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix3fv(location, count, transpose,
                     const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix3x2fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix3x2fv(location, count, transpose,
                       const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix3x4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix3x4fv(location, count, transpose,
                       const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix4fv(location, count, transpose,
                     const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix4x2fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix4x2fv(location, count, transpose,
                       const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix4x3fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix4x3fv(location, count, transpose,
                       const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUseProgram(GLuint program) {
  glUseProgram(GetProgramServiceID(program, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoValidateProgram(GLuint program) {
  glValidateProgram(GetProgramServiceID(program, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib1f(GLuint indx,
                                                           GLfloat x) {
  glVertexAttrib1f(indx, x);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib1fv(
    GLuint indx,
    const volatile GLfloat* values) {
  glVertexAttrib1fv(indx, const_cast<const GLfloat*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib2f(GLuint indx,
                                                           GLfloat x,
                                                           GLfloat y) {
  glVertexAttrib2f(indx, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib2fv(
    GLuint indx,
    const volatile GLfloat* values) {
  glVertexAttrib2fv(indx, const_cast<const GLfloat*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib3f(GLuint indx,
                                                           GLfloat x,
                                                           GLfloat y,
                                                           GLfloat z) {
  glVertexAttrib3f(indx, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib3fv(
    GLuint indx,
    const volatile GLfloat* values) {
  glVertexAttrib3fv(indx, const_cast<const GLfloat*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib4f(GLuint indx,
                                                           GLfloat x,
                                                           GLfloat y,
                                                           GLfloat z,
                                                           GLfloat w) {
  glVertexAttrib4f(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib4fv(
    GLuint indx,
    const volatile GLfloat* values) {
  glVertexAttrib4fv(indx, const_cast<const GLfloat*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4i(GLuint indx,
                                                            GLint x,
                                                            GLint y,
                                                            GLint z,
                                                            GLint w) {
  glVertexAttribI4i(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4iv(
    GLuint indx,
    const volatile GLint* values) {
  glVertexAttribI4iv(indx, const_cast<const GLint*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4ui(GLuint indx,
                                                             GLuint x,
                                                             GLuint y,
                                                             GLuint z,
                                                             GLuint w) {
  glVertexAttribI4ui(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4uiv(
    GLuint indx,
    const volatile GLuint* values) {
  glVertexAttribI4uiv(indx, const_cast<const GLuint*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribIPointer(
    GLuint indx,
    GLint size,
    GLenum type,
    GLsizei stride,
    const void* ptr) {
  glVertexAttribIPointer(indx, size, type, stride, ptr);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribPointer(
    GLuint indx,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei stride,
    const void* ptr) {
  glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoViewport(GLint x,
                                                     GLint y,
                                                     GLsizei width,
                                                     GLsizei height) {
  glViewport(x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoWaitSync(GLuint sync,
                                                     GLbitfield flags,
                                                     GLuint64 timeout) {
  glWaitSync(GetSyncServiceID(sync, resources_), flags, timeout);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlitFramebufferCHROMIUM(
    GLint srcX0,
    GLint srcY0,
    GLint srcX1,
    GLint srcY1,
    GLint dstX0,
    GLint dstY0,
    GLint dstX1,
    GLint dstY1,
    GLbitfield mask,
    GLenum filter) {
  glBlitFramebufferANGLE(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1,
                         mask, filter);
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoRenderbufferStorageMultisampleCHROMIUM(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  if (!feature_info_->feature_flags().chromium_framebuffer_multisample) {
    return error::kUnknownCommand;
  }

  if (feature_info_->feature_flags().angle_framebuffer_multisample) {
    glRenderbufferStorageMultisampleANGLE(target, samples, internalformat,
                                          width, height);
  } else {
    DCHECK(feature_info_->gl_version_info().is_es3);
    glRenderbufferStorageMultisample(target, samples, internalformat, width,
                                     height);
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoRenderbufferStorageMultisampleEXT(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  if (!feature_info_->feature_flags().chromium_framebuffer_multisample) {
    return error::kUnknownCommand;
  }

  if (feature_info_->feature_flags().angle_framebuffer_multisample) {
    glRenderbufferStorageMultisampleANGLE(target, samples, internalformat,
                                          width, height);
  } else {
    DCHECK(feature_info_->gl_version_info().is_es3);
    glRenderbufferStorageMultisample(target, samples, internalformat, width,
                                     height);
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferTexture2DMultisampleEXT(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level,
    GLsizei samples) {
  if (!feature_info_->feature_flags().multisampled_render_to_texture) {
    return error::kUnknownCommand;
  }

  if (IsEmulatedFramebufferBound(target)) {
    InsertError(GL_INVALID_OPERATION,
                "Cannot change the attachments of the default framebuffer.");
    return error::kNoError;
  }
  glFramebufferTexture2DMultisampleEXT(
      target, attachment, textarget,
      GetTextureServiceID(texture, resources_, false), level, samples);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexStorage2DEXT(
    GLenum target,
    GLsizei levels,
    GLenum internalFormat,
    GLsizei width,
    GLsizei height) {
  glTexStorage2DEXT(target, levels, internalFormat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenQueriesEXT(
    GLsizei n,
    volatile GLuint* queries) {
  return GenHelper(n, queries, &query_id_map_, [](GLsizei n, GLuint* queries) {
    glGenQueries(n, queries);
  });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteQueriesEXT(
    GLsizei n,
    const volatile GLuint* queries) {
  // Validate n is non-negative before allcoating a vector of size n
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "count cannot be negative.");
    return error::kNoError;
  }

  std::vector<GLuint> queries_copy(queries, queries + n);
  // If any of these queries are pending or active, remove them from the lists
  for (GLuint query_client_id : queries_copy) {
    GLuint query_service_id = 0;
    if (!query_id_map_.GetServiceID(query_client_id, &query_service_id) ||
        query_service_id == 0) {
      continue;
    }

    QueryInfo query_info = query_info_map_[query_service_id];
    query_info_map_.erase(query_service_id);

    if (query_info.type == GL_NONE) {
      // Query was never started
      continue;
    }

    auto active_queries_iter = active_queries_.find(query_info.type);
    if (active_queries_iter != active_queries_.end()) {
      active_queries_.erase(active_queries_iter);
    }

    RemovePendingQuery(query_service_id);
  }
  return DeleteHelper(
      queries_copy.size(), queries_copy.data(), &query_id_map_,
      [](GLsizei n, GLuint* queries) { glDeleteQueries(n, queries); });
}

error::Error GLES2DecoderPassthroughImpl::DoQueryCounterEXT(
    GLuint id,
    GLenum target,
    int32_t sync_shm_id,
    uint32_t sync_shm_offset,
    uint32_t submit_count) {
  if (!feature_info_->feature_flags().ext_disjoint_timer_query) {
    return error::kUnknownCommand;
  }

  scoped_refptr<gpu::Buffer> buffer = GetSharedMemoryBuffer(sync_shm_id);
  if (!buffer)
    return error::kInvalidArguments;
  QuerySync* sync = static_cast<QuerySync*>(
      buffer->GetDataAddress(sync_shm_offset, sizeof(QuerySync)));
  if (!sync)
    return error::kOutOfBounds;

  GLuint service_id = GetQueryServiceID(id, &query_id_map_);

  // Flush all previous errors
  FlushErrors();

  glQueryCounter(service_id, target);

  // Check if a new error was generated
  if (FlushErrors()) {
    return error::kNoError;
  }

  QueryInfo* query_info = &query_info_map_[service_id];
  query_info->type = target;

  // Make sure to stop tracking this query if it was still pending a result from
  // a previous glEndQuery
  RemovePendingQuery(service_id);

  PendingQuery pending_query;
  pending_query.target = target;
  pending_query.service_id = service_id;
  pending_query.shm = std::move(buffer);
  pending_query.sync = sync;
  pending_query.submit_count = submit_count;
  pending_queries_.push_back(pending_query);

  return ProcessQueries(false);
}

error::Error GLES2DecoderPassthroughImpl::DoBeginQueryEXT(
    GLenum target,
    GLuint id,
    int32_t sync_shm_id,
    uint32_t sync_shm_offset) {
  GLuint service_id = GetQueryServiceID(id, &query_id_map_);
  QueryInfo* query_info = &query_info_map_[service_id];

  scoped_refptr<gpu::Buffer> buffer = GetSharedMemoryBuffer(sync_shm_id);
  if (!buffer)
    return error::kInvalidArguments;
  QuerySync* sync = static_cast<QuerySync*>(
      buffer->GetDataAddress(sync_shm_offset, sizeof(QuerySync)));
  if (!sync)
    return error::kOutOfBounds;

  if (IsEmulatedQueryTarget(target)) {
    if (active_queries_.find(target) != active_queries_.end()) {
      InsertError(GL_INVALID_OPERATION, "Query already active on target.");
      return error::kNoError;
    }

    if (id == 0) {
      InsertError(GL_INVALID_OPERATION, "Query id is 0.");
      return error::kNoError;
    }

    if (query_info->type != GL_NONE && query_info->type != target) {
      InsertError(GL_INVALID_OPERATION,
                  "Query type does not match the target.");
      return error::kNoError;
    }
  } else {
    // Flush all previous errors
    FlushErrors();

    glBeginQuery(target, service_id);

    // Check if a new error was generated
    if (FlushErrors()) {
      return error::kNoError;
    }
  }

  query_info->type = target;

  // Make sure to stop tracking this query if it was still pending a result from
  // a previous glEndQuery
  RemovePendingQuery(service_id);

  ActiveQuery query;
  query.service_id = service_id;
  query.shm = std::move(buffer);
  query.sync = sync;
  active_queries_[target] = query;

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBeginTransformFeedback(
    GLenum primitivemode) {
  glBeginTransformFeedback(primitivemode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEndQueryEXT(GLenum target,
                                                        uint32_t submit_count) {
  if (IsEmulatedQueryTarget(target)) {
    auto active_query_iter = active_queries_.find(target);
    if (active_query_iter == active_queries_.end()) {
      InsertError(GL_INVALID_OPERATION, "No active query on target.");
      return error::kNoError;
    }
    if (target == GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM &&
        !pending_read_pixels_.empty()) {
      GLuint query_service_id = active_query_iter->second.service_id;
      pending_read_pixels_.back().waiting_async_pack_queries.insert(
          query_service_id);
    }
  } else {
    // Flush all previous errors
    FlushErrors();

    glEndQuery(target);

    // Check if a new error was generated
    if (FlushErrors()) {
      return error::kNoError;
    }
  }

  DCHECK(active_queries_.find(target) != active_queries_.end());
  ActiveQuery active_query = std::move(active_queries_[target]);
  active_queries_.erase(target);

  PendingQuery pending_query;
  pending_query.target = target;
  pending_query.service_id = active_query.service_id;
  pending_query.shm = std::move(active_query.shm);
  pending_query.sync = active_query.sync;
  pending_query.submit_count = submit_count;
  pending_queries_.push_back(pending_query);

  return ProcessQueries(false);
}

error::Error GLES2DecoderPassthroughImpl::DoEndTransformFeedback() {
  glEndTransformFeedback();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSetDisjointValueSyncCHROMIUM(
    DisjointValueSync* sync) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInsertEventMarkerEXT(
    GLsizei length,
    const char* marker) {
  if (!feature_info_->feature_flags().ext_debug_marker) {
    return error::kUnknownCommand;
  }

  glInsertEventMarkerEXT(length, marker);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPushGroupMarkerEXT(
    GLsizei length,
    const char* marker) {
  if (!feature_info_->feature_flags().ext_debug_marker) {
    return error::kUnknownCommand;
  }

  glPushGroupMarkerEXT(length, marker);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPopGroupMarkerEXT() {
  if (!feature_info_->feature_flags().ext_debug_marker) {
    return error::kUnknownCommand;
  }

  glPopGroupMarkerEXT();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenVertexArraysOES(
    GLsizei n,
    volatile GLuint* arrays) {
  return GenHelper(
      n, arrays, &vertex_array_id_map_,
      [](GLsizei n, GLuint* arrays) { glGenVertexArraysOES(n, arrays); });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteVertexArraysOES(
    GLsizei n,
    const volatile GLuint* arrays) {
  return DeleteHelper(
      n, arrays, &vertex_array_id_map_,
      [](GLsizei n, GLuint* arrays) { glDeleteVertexArraysOES(n, arrays); });
}

error::Error GLES2DecoderPassthroughImpl::DoIsVertexArrayOES(GLuint array,
                                                             uint32_t* result) {
  *result =
      glIsVertexArrayOES(GetVertexArrayServiceID(array, &vertex_array_id_map_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindVertexArrayOES(GLuint array) {
  glBindVertexArrayOES(GetVertexArrayServiceID(array, &vertex_array_id_map_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSwapBuffers() {
  if (offscreen_) {
    if (offscreen_single_buffer_) {
      return error::kNoError;
    }

    DCHECK(emulated_back_buffer_);

    // Make sure the emulated front buffer is allocated and the correct size
    if (emulated_front_buffer_ &&
        emulated_front_buffer_->size != emulated_back_buffer_->size) {
      emulated_front_buffer_->Destroy(true);
      emulated_front_buffer_ = nullptr;
    }

    if (emulated_front_buffer_ == nullptr) {
      if (!available_color_textures_.empty()) {
        emulated_front_buffer_ = std::move(available_color_textures_.back());
        available_color_textures_.pop_back();
      } else {
        emulated_front_buffer_.reset(
            new EmulatedColorBuffer(emulated_default_framebuffer_format_));
        if (!emulated_front_buffer_->Resize(emulated_back_buffer_->size)) {
          DLOG(ERROR)
              << "Failed to create a new emulated front buffer texture.";
          return error::kLostContext;
        }
      }
    }

    DCHECK(emulated_front_buffer_->size == emulated_back_buffer_->size);

    if (emulated_default_framebuffer_format_.samples > 0) {
      // Resolve the multisampled renderbuffer into the emulated_front_buffer_
      emulated_back_buffer_->Blit(emulated_front_buffer_.get());
    } else {
      DCHECK(emulated_back_buffer_->color_texture != nullptr);
      // If the offscreen buffer should be preserved, copy the old backbuffer
      // into the new one
      if (offscreen_target_buffer_preserved_) {
        emulated_back_buffer_->Blit(emulated_front_buffer_.get());
      }

      // Swap the front and back buffer textures and update the framebuffer
      // attachment.
      std::unique_ptr<EmulatedColorBuffer> old_front_buffer =
          std::move(emulated_front_buffer_);
      emulated_front_buffer_ =
          emulated_back_buffer_->SetColorBuffer(std::move(old_front_buffer));
    }

    return error::kNoError;
  }

  gfx::SwapResult result = surface_->SwapBuffers();
  if (result == gfx::SwapResult::SWAP_FAILED) {
    LOG(ERROR) << "Context lost because SwapBuffers failed.";
    if (!CheckResetStatus()) {
      MarkContextLost(error::kUnknown);
      group_->LoseContexts(error::kUnknown);
      return error::kLostContext;
    }
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetMaxValueInBufferCHROMIUM(
    GLuint buffer_id,
    GLsizei count,
    GLenum type,
    GLuint offset,
    uint32_t* result) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEnableFeatureCHROMIUM(
    const char* feature) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMapBufferRange(
    GLenum target,
    GLintptr offset,
    GLsizeiptr size,
    GLbitfield access,
    void* ptr,
    int32_t data_shm_id,
    uint32_t data_shm_offset,
    uint32_t* result) {
  FlushErrors();

  GLbitfield filtered_access = access;

  // Always filter out GL_MAP_UNSYNCHRONIZED_BIT to get rid of undefined
  // behaviors.
  filtered_access = (filtered_access & ~GL_MAP_UNSYNCHRONIZED_BIT);

  if ((filtered_access & GL_MAP_INVALIDATE_BUFFER_BIT) != 0) {
    // To be on the safe side, always map GL_MAP_INVALIDATE_BUFFER_BIT to
    // GL_MAP_INVALIDATE_RANGE_BIT.
    filtered_access = (filtered_access & ~GL_MAP_INVALIDATE_BUFFER_BIT);
    filtered_access = (filtered_access | GL_MAP_INVALIDATE_RANGE_BIT);
  }
  if ((filtered_access & GL_MAP_INVALIDATE_RANGE_BIT) == 0) {
    // If this user intends to use this buffer without invalidating the data, we
    // need to also add GL_MAP_READ_BIT to preserve the original data when
    // copying it to shared memory.
    filtered_access = (filtered_access | GL_MAP_READ_BIT);
  }

  void* mapped_ptr = glMapBufferRange(target, offset, size, filtered_access);
  if (FlushErrors() || mapped_ptr == nullptr) {
    // Had an error while mapping, don't copy any data
    *result = 0;
    return error::kNoError;
  }

  if ((filtered_access & GL_MAP_INVALIDATE_RANGE_BIT) == 0) {
    memcpy(ptr, mapped_ptr, size);
  }

  // Track the mapping of this buffer so that data can be synchronized when it
  // is unmapped
  DCHECK(bound_buffers_.find(target) != bound_buffers_.end());
  GLuint client_buffer = bound_buffers_.at(target);

  MappedBuffer mapped_buffer_info;
  mapped_buffer_info.size = size;
  mapped_buffer_info.original_access = access;
  mapped_buffer_info.filtered_access = filtered_access;
  mapped_buffer_info.map_ptr = static_cast<uint8_t*>(mapped_ptr);
  mapped_buffer_info.data_shm_id = data_shm_id;
  mapped_buffer_info.data_shm_offset = data_shm_offset;

  DCHECK(resources_->mapped_buffer_map.find(client_buffer) ==
         resources_->mapped_buffer_map.end());
  resources_->mapped_buffer_map.insert(
      std::make_pair(client_buffer, mapped_buffer_info));

  *result = 1;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUnmapBuffer(GLenum target) {
  auto bound_buffers_iter = bound_buffers_.find(target);
  if (bound_buffers_iter == bound_buffers_.end()) {
    InsertError(GL_INVALID_ENUM, "Invalid buffer target.");
    return error::kNoError;
  }

  if (bound_buffers_iter->second == 0) {
    InsertError(GL_INVALID_OPERATION, "No buffer bound to this target.");
    return error::kNoError;
  }

  GLuint client_buffer = bound_buffers_iter->second;
  auto mapped_buffer_info_iter =
      resources_->mapped_buffer_map.find(client_buffer);
  if (mapped_buffer_info_iter == resources_->mapped_buffer_map.end()) {
    InsertError(GL_INVALID_OPERATION, "Buffer is not mapped.");
    return error::kNoError;
  }

  const MappedBuffer& map_info = mapped_buffer_info_iter->second;
  if ((map_info.filtered_access & GL_MAP_WRITE_BIT) != 0 &&
      (map_info.filtered_access & GL_MAP_FLUSH_EXPLICIT_BIT) == 0) {
    uint8_t* mem = GetSharedMemoryAs<uint8_t*>(
        map_info.data_shm_id, map_info.data_shm_offset, map_info.size);
    if (!mem) {
      return error::kOutOfBounds;
    }

    memcpy(map_info.map_ptr, mem, map_info.size);
  }

  glUnmapBuffer(target);

  resources_->mapped_buffer_map.erase(mapped_buffer_info_iter);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoResizeCHROMIUM(GLuint width,
                                                           GLuint height,
                                                           GLfloat scale_factor,
                                                           GLenum color_space,
                                                           GLboolean alpha) {
  if (offscreen_) {
    if (!ResizeOffscreenFramebuffer(gfx::Size(width, height))) {
      LOG(ERROR) << "GLES2DecoderPassthroughImpl: Context lost because "
                 << "ResizeOffscreenFramebuffer failed.";
      return error::kLostContext;
    }
  } else {
    gl::GLSurface::ColorSpace surface_color_space =
        gl::GLSurface::ColorSpace::UNSPECIFIED;
    switch (color_space) {
      case GL_COLOR_SPACE_UNSPECIFIED_CHROMIUM:
        surface_color_space = gl::GLSurface::ColorSpace::UNSPECIFIED;
        break;
      case GL_COLOR_SPACE_SCRGB_LINEAR_CHROMIUM:
        surface_color_space = gl::GLSurface::ColorSpace::SCRGB_LINEAR;
        break;
      case GL_COLOR_SPACE_SRGB_CHROMIUM:
        surface_color_space = gl::GLSurface::ColorSpace::SRGB;
        break;
      case GL_COLOR_SPACE_DISPLAY_P3_CHROMIUM:
        surface_color_space = gl::GLSurface::ColorSpace::DISPLAY_P3;
        break;
      default:
        LOG(ERROR) << "GLES2DecoderPassthroughImpl: Context lost because "
                      "specified color space was invalid.";
        return error::kLostContext;
    }
    if (!surface_->Resize(gfx::Size(width, height), scale_factor,
                          surface_color_space, !!alpha)) {
      LOG(ERROR)
          << "GLES2DecoderPassthroughImpl: Context lost because resize failed.";
      return error::kLostContext;
    }
    DCHECK(context_->IsCurrent(surface_.get()));
    if (!context_->IsCurrent(surface_.get())) {
      LOG(ERROR) << "GLES2DecoderPassthroughImpl: Context lost because context "
                    "no longer current after resize callback.";
      return error::kLostContext;
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetRequestableExtensionsCHROMIUM(
    const char** extensions) {
  *extensions = reinterpret_cast<const char*>(
      glGetString(GL_REQUESTABLE_EXTENSIONS_ANGLE));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoRequestExtensionCHROMIUM(
    const char* extension) {
  glRequestExtensionANGLE(extension);

  // Make sure there are no pending GL errors before re-initializing feature
  // info
  FlushErrors();

  // Make sure newly enabled extensions are exposed and usable.
  context_->ReinitializeDynamicBindings();
  if (!feature_info_->Initialize(feature_info_->context_type(),
                                 feature_info_->disallowed_features())) {
    return error::kLostContext;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramInfoCHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  GLuint service_program = 0;
  if (!resources_->program_id_map.GetServiceID(program, &service_program)) {
    return error::kNoError;
  }

  GLint num_attributes = 0;
  glGetProgramiv(service_program, GL_ACTIVE_ATTRIBUTES, &num_attributes);

  GLint num_uniforms = 0;
  glGetProgramiv(service_program, GL_ACTIVE_UNIFORMS, &num_uniforms);

  const base::CheckedNumeric<size_t> buffer_header_size(
      sizeof(ProgramInfoHeader));
  const base::CheckedNumeric<size_t> buffer_block_size(
      sizeof(ProgramInput));
  const base::CheckedNumeric<size_t> attribute_block_size =
      buffer_block_size * num_attributes;
  const base::CheckedNumeric<size_t> uniform_block_size =
      buffer_block_size * num_uniforms;
  data->resize((buffer_header_size + attribute_block_size + uniform_block_size)
                   .ValueOrDie(),
               0);

  GLint link_status = 0;
  glGetProgramiv(service_program, GL_LINK_STATUS, &link_status);

  ProgramInfoHeader header;
  header.link_status = link_status;
  header.num_attribs = num_attributes;
  header.num_uniforms = num_uniforms;
  InsertValueIntoBuffer(data, header, 0);

  GLint active_attribute_max_length = 0;
  glGetProgramiv(service_program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                 &active_attribute_max_length);

  std::vector<char> attrib_name_buf(active_attribute_max_length, 0);
  for (GLint attrib_index = 0; attrib_index < num_attributes; attrib_index++) {
    GLsizei length = 0;
    GLint size = 0;
    GLenum type = GL_NONE;
    glGetActiveAttrib(service_program, attrib_index, attrib_name_buf.size(),
                      &length, &size, &type, attrib_name_buf.data());

    ProgramInput input;
    input.size = size;
    input.type = type;

    int32_t location =
        glGetAttribLocation(service_program, attrib_name_buf.data());
    input.location_offset = data->size();
    AppendValueToBuffer(data, location);

    input.name_offset = data->size();
    input.name_length = length;
    AppendStringToBuffer(data, attrib_name_buf.data(), length);

    InsertValueIntoBuffer(
        data, input,
        (buffer_header_size + (buffer_block_size * attrib_index)).ValueOrDie());
  }

  GLint active_uniform_max_length = 0;
  glGetProgramiv(service_program, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                 &active_uniform_max_length);

  std::vector<char> uniform_name_buf(active_uniform_max_length, 0);
  for (GLint uniform_index = 0; uniform_index < num_uniforms; uniform_index++) {
    GLsizei length = 0;
    GLint size = 0;
    GLenum type = GL_NONE;
    glGetActiveUniform(service_program, uniform_index, uniform_name_buf.size(),
                       &length, &size, &type, uniform_name_buf.data());

    ProgramInput input;
    input.size = size;
    input.type = type;

    input.location_offset = data->size();
    int32_t base_location =
        glGetUniformLocation(service_program, uniform_name_buf.data());
    AppendValueToBuffer(data, base_location);

    GLSLArrayName parsed_service_name(uniform_name_buf.data());
    if (size > 1 || parsed_service_name.IsArrayName()) {
      for (GLint location_index = 1; location_index < size; location_index++) {
        std::string array_element_name = parsed_service_name.base_name() + "[" +
                                         base::IntToString(location_index) +
                                         "]";
        int32_t element_location =
            glGetUniformLocation(service_program, array_element_name.c_str());
        AppendValueToBuffer(data, element_location);
      }
    }

    input.name_offset = data->size();
    input.name_length = length;
    AppendStringToBuffer(data, uniform_name_buf.data(), length);

    InsertValueIntoBuffer(data, input,
                          (buffer_header_size + attribute_block_size +
                           (buffer_block_size * uniform_index))
                              .ValueOrDie());
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformBlocksCHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  GLuint service_program = 0;
  if (!resources_->program_id_map.GetServiceID(program, &service_program)) {
    return error::kNoError;
  }

  GLint num_uniform_blocks = 0;
  glGetProgramiv(service_program, GL_ACTIVE_UNIFORM_BLOCKS,
                 &num_uniform_blocks);

  // Resize the data to fit the headers and info objects so that strings can be
  // appended.
  const base::CheckedNumeric<size_t> buffer_header_size(
      sizeof(UniformBlocksHeader));
  const base::CheckedNumeric<size_t> buffer_block_size(
      sizeof(UniformBlockInfo));
  data->resize((buffer_header_size + (num_uniform_blocks * buffer_block_size))
                   .ValueOrDie(),
               0);

  UniformBlocksHeader header;
  header.num_uniform_blocks = num_uniform_blocks;
  InsertValueIntoBuffer(data, header, 0);

  GLint active_uniform_block_max_length = 0;
  glGetProgramiv(service_program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH,
                 &active_uniform_block_max_length);

  std::vector<char> uniform_block_name_buf(active_uniform_block_max_length, 0);
  for (GLint uniform_block_index = 0; uniform_block_index < num_uniform_blocks;
       uniform_block_index++) {
    UniformBlockInfo block_info;

    GLint uniform_block_binding = 0;
    glGetActiveUniformBlockiv(service_program, uniform_block_index,
                              GL_UNIFORM_BLOCK_BINDING, &uniform_block_binding);
    block_info.binding = uniform_block_binding;

    GLint uniform_block_data_size = 0;
    glGetActiveUniformBlockiv(service_program, uniform_block_index,
                              GL_UNIFORM_BLOCK_DATA_SIZE,
                              &uniform_block_data_size);
    block_info.data_size = uniform_block_data_size;

    GLint uniform_block_name_length = 0;
    glGetActiveUniformBlockName(
        service_program, uniform_block_index, active_uniform_block_max_length,
        &uniform_block_name_length, uniform_block_name_buf.data());

    DCHECK(uniform_block_name_length + 1 <= active_uniform_block_max_length);
    block_info.name_offset = data->size();
    block_info.name_length = uniform_block_name_length + 1;
    AppendStringToBuffer(data, uniform_block_name_buf.data(),
                         uniform_block_name_length + 1);

    GLint uniform_block_active_uniforms = 0;
    glGetActiveUniformBlockiv(service_program, uniform_block_index,
                              GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS,
                              &uniform_block_active_uniforms);
    block_info.active_uniforms = uniform_block_active_uniforms;

    std::vector<GLint> uniform_block_indices_buf(uniform_block_active_uniforms,
                                                 0);
    glGetActiveUniformBlockiv(service_program, uniform_block_index,
                              GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,
                              uniform_block_indices_buf.data());
    block_info.active_uniform_offset = data->size();
    for (GLint uniform_block_uniform_index_index = 0;
         uniform_block_uniform_index_index < uniform_block_active_uniforms;
         uniform_block_uniform_index_index++) {
      AppendValueToBuffer(
          data,
          static_cast<uint32_t>(
              uniform_block_indices_buf[uniform_block_uniform_index_index]));
    }

    GLint uniform_block_referenced_by_vertex_shader = 0;
    glGetActiveUniformBlockiv(service_program, uniform_block_index,
                              GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER,
                              &uniform_block_referenced_by_vertex_shader);
    block_info.referenced_by_vertex_shader =
        uniform_block_referenced_by_vertex_shader;

    GLint uniform_block_referenced_by_fragment_shader = 0;
    glGetActiveUniformBlockiv(service_program, uniform_block_index,
                              GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER,
                              &uniform_block_referenced_by_fragment_shader);
    block_info.referenced_by_fragment_shader =
        uniform_block_referenced_by_fragment_shader;

    InsertValueIntoBuffer(
        data, block_info,
        (buffer_header_size + (buffer_block_size * uniform_block_index))
            .ValueOrDie());
  }

  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoGetTransformFeedbackVaryingsCHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  GLuint service_program = 0;
  if (!resources_->program_id_map.GetServiceID(program, &service_program)) {
    return error::kNoError;
  }

  GLint transform_feedback_buffer_mode = 0;
  glGetProgramiv(service_program, GL_TRANSFORM_FEEDBACK_BUFFER_MODE,
                 &transform_feedback_buffer_mode);

  GLint num_transform_feedback_varyings = 0;
  glGetProgramiv(service_program, GL_TRANSFORM_FEEDBACK_VARYINGS,
                 &num_transform_feedback_varyings);

  // Resize the data to fit the headers and info objects so that strings can be
  // appended.
  const base::CheckedNumeric<size_t> buffer_header_size(
      sizeof(TransformFeedbackVaryingsHeader));
  const base::CheckedNumeric<size_t> buffer_block_size(
      sizeof(TransformFeedbackVaryingInfo));
  data->resize((buffer_header_size +
                (num_transform_feedback_varyings * buffer_block_size))
                   .ValueOrDie(),
               0);

  TransformFeedbackVaryingsHeader header;
  header.transform_feedback_buffer_mode = transform_feedback_buffer_mode;
  header.num_transform_feedback_varyings = num_transform_feedback_varyings;
  InsertValueIntoBuffer(data, header, 0);

  GLint max_transform_feedback_varying_length = 0;
  glGetProgramiv(service_program, GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH,
                 &max_transform_feedback_varying_length);

  std::vector<char> transform_feedback_varying_name_buf(
      max_transform_feedback_varying_length, 0);
  for (GLint transform_feedback_varying_index = 0;
       transform_feedback_varying_index < num_transform_feedback_varyings;
       transform_feedback_varying_index++) {
    GLsizei length = 0;
    GLint size = 0;
    GLenum type = GL_NONE;
    glGetTransformFeedbackVarying(
        service_program, transform_feedback_varying_index,
        max_transform_feedback_varying_length, &length, &size, &type,
        transform_feedback_varying_name_buf.data());

    TransformFeedbackVaryingInfo varying_info;
    varying_info.size = size;
    varying_info.type = type;

    DCHECK(length + 1 <= max_transform_feedback_varying_length);
    varying_info.name_length = data->size();
    varying_info.name_length = length + 1;
    AppendStringToBuffer(data, transform_feedback_varying_name_buf.data(),
                         length + 1);

    InsertValueIntoBuffer(
        data, varying_info,
        (buffer_header_size +
         (buffer_block_size * transform_feedback_varying_index))
            .ValueOrDie());
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformsES3CHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  GLuint service_program = 0;
  if (!resources_->program_id_map.GetServiceID(program, &service_program)) {
    return error::kNoError;
  }

  GLint num_uniforms = 0;
  glGetProgramiv(service_program, GL_ACTIVE_UNIFORMS, &num_uniforms);

  UniformsES3Header header;
  header.num_uniforms = num_uniforms;
  AppendValueToBuffer(data, header);

  for (GLuint uniform_index = 0;
       uniform_index < static_cast<GLuint>(num_uniforms); uniform_index++) {
    UniformES3Info uniform_info;

    GLint uniform_block_index = 0;
    glGetActiveUniformsiv(service_program, 1, &uniform_index,
                          GL_UNIFORM_BLOCK_INDEX, &uniform_block_index);
    uniform_info.block_index = uniform_block_index;

    GLint uniform_offset = 0;
    glGetActiveUniformsiv(service_program, 1, &uniform_index, GL_UNIFORM_OFFSET,
                          &uniform_offset);
    uniform_info.offset = uniform_offset;

    GLint uniform_array_stride = 0;
    glGetActiveUniformsiv(service_program, 1, &uniform_index,
                          GL_UNIFORM_ARRAY_STRIDE, &uniform_array_stride);
    uniform_info.array_stride = uniform_array_stride;

    GLint uniform_matrix_stride = 0;
    glGetActiveUniformsiv(service_program, 1, &uniform_index,
                          GL_UNIFORM_MATRIX_STRIDE, &uniform_matrix_stride);
    uniform_info.matrix_stride = uniform_matrix_stride;

    GLint uniform_is_row_major = 0;
    glGetActiveUniformsiv(service_program, 1, &uniform_index,
                          GL_UNIFORM_IS_ROW_MAJOR, &uniform_is_row_major);
    uniform_info.is_row_major = uniform_is_row_major;

    AppendValueToBuffer(data, uniform_info);
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTranslatedShaderSourceANGLE(
    GLuint shader,
    std::string* source) {
  FlushErrors();
  GLuint service_id = GetShaderServiceID(shader, resources_);
  GLint translated_source_length = 0;
  glGetShaderiv(service_id, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE,
                &translated_source_length);
  if (FlushErrors()) {
    return error::kNoError;
  }

  if (translated_source_length > 0) {
    std::vector<char> buffer(translated_source_length, 0);
    glGetTranslatedShaderSourceANGLE(service_id, translated_source_length,
                                     nullptr, buffer.data());
    *source = std::string(buffer.data());
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSwapBuffersWithBoundsCHROMIUM(
    GLsizei count,
    const volatile GLint* rects) {
  if (count < 0) {
    InsertError(GL_INVALID_VALUE, "count cannot be negative.");
    return error::kNoError;
  }

  std::vector<gfx::Rect> bounds(count);
  for (GLsizei i = 0; i < count; ++i) {
    bounds[i] = gfx::Rect(rects[i * 4 + 0], rects[i * 4 + 1], rects[i * 4 + 2],
                          rects[i * 4 + 3]);
  }
  gfx::SwapResult result = surface_->SwapBuffersWithBounds(bounds);
  if (result == gfx::SwapResult::SWAP_FAILED) {
    LOG(ERROR) << "Context lost because SwapBuffersWithBounds failed.";
  }
  // TODO(geofflang): force the context loss?
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPostSubBufferCHROMIUM(
    GLint x,
    GLint y,
    GLint width,
    GLint height) {
  if (!surface_->SupportsPostSubBuffer()) {
    InsertError(GL_INVALID_OPERATION,
                "glPostSubBufferCHROMIUM is not supported for this surface.");
    return error::kNoError;
  }

  gfx::SwapResult result = surface_->PostSubBuffer(x, y, width, height);
  if (result == gfx::SwapResult::SWAP_FAILED) {
    LOG(ERROR) << "Context lost because PostSubBuffer failed.";
    if (!CheckResetStatus()) {
      MarkContextLost(error::kUnknown);
      group_->LoseContexts(error::kUnknown);
      return error::kLostContext;
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyTextureCHROMIUM(
    GLuint source_id,
    GLint source_level,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLint internalformat,
    GLenum dest_type,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  if (!feature_info_->feature_flags().chromium_copy_texture) {
    return error::kUnknownCommand;
  }

  glCopyTextureCHROMIUM(GetTextureServiceID(source_id, resources_, false),
                        source_level, dest_target,
                        GetTextureServiceID(dest_id, resources_, false),
                        dest_level, internalformat, dest_type, unpack_flip_y,
                        unpack_premultiply_alpha, unpack_unmultiply_alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopySubTextureCHROMIUM(
    GLuint source_id,
    GLint source_level,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  if (!feature_info_->feature_flags().chromium_copy_texture) {
    return error::kUnknownCommand;
  }

  glCopySubTextureCHROMIUM(
      GetTextureServiceID(source_id, resources_, false), source_level,
      dest_target, GetTextureServiceID(dest_id, resources_, false), dest_level,
      xoffset, yoffset, x, y, width, height, unpack_flip_y,
      unpack_premultiply_alpha, unpack_unmultiply_alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedCopyTextureCHROMIUM(
    GLuint source_id,
    GLuint dest_id) {
  if (!feature_info_->feature_flags().chromium_copy_compressed_texture) {
    return error::kUnknownCommand;
  }

  glCompressedCopyTextureCHROMIUM(
      GetTextureServiceID(source_id, resources_, false),
      GetTextureServiceID(dest_id, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawArraysInstancedANGLE(
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount) {
  glDrawArraysInstancedANGLE(mode, first, count, primcount);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawElementsInstancedANGLE(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount) {
  glDrawElementsInstancedANGLE(mode, count, type, indices, primcount);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribDivisorANGLE(
    GLuint index,
    GLuint divisor) {
  glVertexAttribDivisorANGLE(index, divisor);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoProduceTextureCHROMIUM(
    GLenum target,
    const volatile GLbyte* mailbox) {
  auto bound_textures_iter = bound_textures_.find(target);
  if (bound_textures_iter == bound_textures_.end()) {
    InsertError(GL_INVALID_OPERATION, "Invalid texture target.");
    return error::kNoError;
  }

  const BoundTexture& bound_texture =
      bound_textures_iter->second[active_texture_unit_];
  if (bound_texture.texture == nullptr) {
    InsertError(GL_INVALID_OPERATION, "Unknown texture for target.");
    return error::kNoError;
  }

  const Mailbox& mb = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox));
  mailbox_manager_->ProduceTexture(mb, bound_texture.texture.get());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoProduceTextureDirectCHROMIUM(
    GLuint texture_client_id,
    GLenum target,
    const volatile GLbyte* mailbox) {
  auto texture_object_iter =
      resources_->texture_object_map.find(texture_client_id);
  if (texture_object_iter == resources_->texture_object_map.end()) {
    InsertError(GL_INVALID_OPERATION, "Unknown texture for target.");
    return error::kNoError;
  }

  scoped_refptr<TexturePassthrough> texture = texture_object_iter->second;
  if (texture->target() != target) {
    InsertError(GL_INVALID_OPERATION, "Texture target does not match.");
    return error::kNoError;
  }

  const Mailbox& mb = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox));
  mailbox_manager_->ProduceTexture(mb, texture.get());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoConsumeTextureCHROMIUM(
    GLenum target,
    const volatile GLbyte* mailbox) {
  auto bound_textures_iter = bound_textures_.find(target);
  if (bound_textures_iter == bound_textures_.end()) {
    InsertError(GL_INVALID_OPERATION, "Invalid texture target.");
    return error::kNoError;
  }

  const BoundTexture& current_texture =
      bound_textures_iter->second[active_texture_unit_];
  if (current_texture.client_id == 0) {
    InsertError(GL_INVALID_OPERATION, "Unknown texture for target.");
    return error::kNoError;
  }

  const Mailbox& mb = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox));
  scoped_refptr<TexturePassthrough> texture = static_cast<TexturePassthrough*>(
      group_->mailbox_manager()->ConsumeTexture(mb));
  if (texture == nullptr) {
    InsertError(GL_INVALID_OPERATION, "Invalid mailbox name.");
    return error::kNoError;
  }

  if (texture->target() != target) {
    InsertError(GL_INVALID_OPERATION, "Texture target does not match.");
    return error::kNoError;
  }

  // Update id mappings
  resources_->texture_id_map.RemoveClientID(current_texture.client_id);
  resources_->texture_id_map.SetIDMapping(current_texture.client_id,
                                          texture->service_id());
  resources_->texture_object_map.erase(current_texture.client_id);
  resources_->texture_object_map.insert(
      std::make_pair(current_texture.client_id, texture));

  // Bind the service id that now represents this texture
  UpdateTextureBinding(target, current_texture.client_id, texture.get());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCreateAndConsumeTextureINTERNAL(
    GLenum target,
    GLuint texture_client_id,
    const volatile GLbyte* mailbox) {
  if (!texture_client_id ||
      resources_->texture_id_map.GetServiceID(texture_client_id, nullptr)) {
    return error::kInvalidArguments;
  }

  const Mailbox& mb = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox));
  scoped_refptr<TexturePassthrough> texture = static_cast<TexturePassthrough*>(
      group_->mailbox_manager()->ConsumeTexture(mb));
  if (texture == nullptr) {
    InsertError(GL_INVALID_OPERATION, "Invalid mailbox name.");
    return error::kNoError;
  }

  if (texture->target() != target) {
    InsertError(GL_INVALID_OPERATION, "Texture target does not match.");
    return error::kNoError;
  }

  // Update id mappings
  resources_->texture_id_map.RemoveClientID(texture_client_id);
  resources_->texture_id_map.SetIDMapping(texture_client_id,
                                          texture->service_id());
  resources_->texture_object_map.erase(texture_client_id);
  resources_->texture_object_map.insert(
      std::make_pair(texture_client_id, texture));

  // Bind the service id that now represents this texture
  UpdateTextureBinding(target, texture_client_id, texture.get());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindUniformLocationCHROMIUM(
    GLuint program,
    GLint location,
    const char* name) {
  glBindUniformLocationCHROMIUM(GetProgramServiceID(program, resources_),
                                location, name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindTexImage2DCHROMIUM(
    GLenum target,
    GLint imageId) {
  return BindTexImage2DCHROMIUMImpl(target, 0, imageId);
}

error::Error
GLES2DecoderPassthroughImpl::DoBindTexImage2DWithInternalformatCHROMIUM(
    GLenum target,
    GLenum internalformat,
    GLint imageId) {
  return BindTexImage2DCHROMIUMImpl(target, internalformat, imageId);
}

error::Error GLES2DecoderPassthroughImpl::DoReleaseTexImage2DCHROMIUM(
    GLenum target,
    GLint imageId) {
  if (target != GL_TEXTURE_2D) {
    InsertError(GL_INVALID_ENUM, "Invalid target");
    return error::kNoError;
  }

  const BoundTexture& bound_texture =
      bound_textures_[GL_TEXTURE_2D][active_texture_unit_];
  if (bound_texture.texture == nullptr) {
    InsertError(GL_INVALID_OPERATION, "No texture bound");
    return error::kNoError;
  }

  gl::GLImage* image = group_->image_manager()->LookupImage(imageId);
  if (image == nullptr) {
    InsertError(GL_INVALID_OPERATION, "No image found with the given ID");
    return error::kNoError;
  }

  // Only release the image if it is currently bound
  if (bound_texture.texture->GetLevelImage(target, 0) != image) {
    image->ReleaseTexImage(target);
    bound_texture.texture->SetLevelImage(target, 0, nullptr);
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTraceBeginCHROMIUM(
    const char* category_name,
    const char* trace_name) {
  if (!gpu_tracer_->Begin(category_name, trace_name, kTraceCHROMIUM)) {
    InsertError(GL_INVALID_OPERATION, "Failed to create begin trace");
    return error::kNoError;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTraceEndCHROMIUM() {
  if (!gpu_tracer_->End(kTraceCHROMIUM)) {
    InsertError(GL_INVALID_OPERATION, "No trace to end");
    return error::kNoError;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDiscardFramebufferEXT(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments) {
  if (!feature_info_->feature_flags().ext_discard_framebuffer) {
    return error::kUnknownCommand;
  }

  // Validate that count is non-negative before allocating a vector
  if (count < 0) {
    InsertError(GL_INVALID_VALUE, "count cannot be negative.");
    return error::kNoError;
  }
  std::vector<GLenum> attachments_copy(attachments, attachments + count);

  if (feature_info_->gl_version_info().is_es3) {
    glInvalidateFramebuffer(target, count, attachments_copy.data());
  } else {
    glDiscardFramebufferEXT(target, count, attachments_copy.data());
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoLoseContextCHROMIUM(GLenum current,
                                                                GLenum other) {
  if (!ValidContextLostReason(current) || !ValidContextLostReason(other)) {
    InsertError(GL_INVALID_ENUM, "invalid context loss reason.");
    return error::kNoError;
  }

  MarkContextLost(GetContextLostReasonFromResetStatus(current));
  group_->LoseContexts(GetContextLostReasonFromResetStatus(other));
  reset_by_robustness_extension_ = true;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDescheduleUntilFinishedCHROMIUM() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInsertFenceSyncCHROMIUM(
    GLuint64 release_count) {
  client_->OnFenceSyncRelease(release_count);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoWaitSyncTokenCHROMIUM(
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id,
    GLuint64 release_count) {
  SyncToken sync_token(namespace_id, 0, command_buffer_id, release_count);
  return client_->OnWaitSyncToken(sync_token) ? error::kDeferCommandUntilLater
                                              : error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawBuffersEXT(
    GLsizei count,
    const volatile GLenum* bufs) {
  // Validate that count is non-negative before allocating a vector
  if (count < 0) {
    InsertError(GL_INVALID_VALUE, "count cannot be negative.");
    return error::kNoError;
  }
  std::vector<GLenum> bufs_copy(bufs, bufs + count);
  glDrawBuffersARB(count, bufs_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDiscardBackbufferCHROMIUM() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScheduleOverlayPlaneCHROMIUM(
    GLint plane_z_order,
    GLenum plane_transform,
    GLuint overlay_texture_id,
    GLint bounds_x,
    GLint bounds_y,
    GLint bounds_width,
    GLint bounds_height,
    GLfloat uv_x,
    GLfloat uv_y,
    GLfloat uv_width,
    GLfloat uv_height) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScheduleCALayerSharedStateCHROMIUM(
    GLfloat opacity,
    GLboolean is_clipped,
    const GLfloat* clip_rect,
    GLint sorting_context_id,
    const GLfloat* transform) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScheduleCALayerCHROMIUM(
    GLuint contents_texture_id,
    const GLfloat* contents_rect,
    GLuint background_color,
    GLuint edge_aa_mask,
    const GLfloat* bounds_rect) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScheduleCALayerInUseQueryCHROMIUM(
    GLuint n,
    const volatile GLuint* textures) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScheduleDCLayerSharedStateCHROMIUM(
    GLfloat opacity,
    GLboolean is_clipped,
    const GLfloat* clip_rect,
    GLint z_order,
    const GLfloat* transform) {
  if (!dc_layer_shared_state_) {
    dc_layer_shared_state_.reset(new DCLayerSharedState);
  }
  dc_layer_shared_state_->opacity = opacity;
  dc_layer_shared_state_->is_clipped = is_clipped ? true : false;
  dc_layer_shared_state_->clip_rect = gfx::ToEnclosingRect(
      gfx::RectF(clip_rect[0], clip_rect[1], clip_rect[2], clip_rect[3]));
  dc_layer_shared_state_->z_order = z_order;
  dc_layer_shared_state_->transform = gfx::Transform(
      transform[0], transform[1], transform[2], transform[3], transform[4],
      transform[5], transform[6], transform[7], transform[8], transform[9],
      transform[10], transform[11], transform[12], transform[13], transform[14],
      transform[15]);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScheduleDCLayerCHROMIUM(
    GLsizei num_textures,
    const volatile GLuint* contents_texture_ids,
    const GLfloat* contents_rect,
    GLuint background_color,
    GLuint edge_aa_mask,
    GLenum filter,
    const GLfloat* bounds_rect) {
  switch (filter) {
    case GL_NEAREST:
    case GL_LINEAR:
      break;
    default:
      InsertError(GL_INVALID_OPERATION, "invalid filter.");
      return error::kNoError;
  }

  if (!dc_layer_shared_state_) {
    InsertError(GL_INVALID_OPERATION,
                "glScheduleDCLayerSharedStateCHROMIUM has not been called.");
    return error::kNoError;
  }

  if (num_textures < 0 || num_textures > 4) {
    InsertError(GL_INVALID_OPERATION,
                "number of textures greater than maximum of 4.");
    return error::kNoError;
  }

  gfx::RectF contents_rect_object(contents_rect[0], contents_rect[1],
                                  contents_rect[2], contents_rect[3]);
  gfx::RectF bounds_rect_object(bounds_rect[0], bounds_rect[1], bounds_rect[2],
                                bounds_rect[3]);

  std::vector<scoped_refptr<gl::GLImage>> images(num_textures);
  for (int i = 0; i < num_textures; ++i) {
    GLuint contents_texture_client_id = contents_texture_ids[i];
    if (contents_texture_client_id != 0) {
      auto texture_iter =
          resources_->texture_object_map.find(contents_texture_client_id);
      if (texture_iter == resources_->texture_object_map.end()) {
        InsertError(GL_INVALID_VALUE, "unknown texture.");
        return error::kNoError;
      }

      scoped_refptr<TexturePassthrough> passthrough_texture =
          texture_iter->second;
      DCHECK(passthrough_texture != nullptr);
      DCHECK(passthrough_texture->target() == GL_TEXTURE_2D);

      scoped_refptr<gl::GLImage> image =
          passthrough_texture->GetLevelImage(GL_TEXTURE_2D, 0);
      if (image == nullptr) {
        InsertError(GL_INVALID_VALUE, "unsupported texture format");
        return error::kNoError;
      }
      images[i] = image;
    }
  }

  ui::DCRendererLayerParams params(
      dc_layer_shared_state_->is_clipped, dc_layer_shared_state_->clip_rect,
      dc_layer_shared_state_->z_order, dc_layer_shared_state_->transform,
      images, contents_rect_object, gfx::ToEnclosingRect(bounds_rect_object),
      background_color, edge_aa_mask, dc_layer_shared_state_->opacity, filter);

  if (!surface_->ScheduleDCLayer(params)) {
    InsertError(GL_INVALID_OPERATION, "failed to schedule DCLayer");
    return error::kNoError;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCommitOverlayPlanesCHROMIUM() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSwapInterval(GLint interval) {
  context_->SetSwapInterval(interval);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFlushDriverCachesCHROMIUM() {
  // On Adreno Android devices we need to use a workaround to force caches to
  // clear.
  if (feature_info_->workarounds().unbind_egl_context_to_flush_driver_caches) {
    context_->ReleaseCurrent(nullptr);
    context_->MakeCurrent(surface_.get());
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMatrixLoadfCHROMIUM(
    GLenum matrixMode,
    const volatile GLfloat* m) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMatrixLoadIdentityCHROMIUM(
    GLenum matrixMode) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenPathsCHROMIUM(GLuint path,
                                                             GLsizei range) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeletePathsCHROMIUM(GLuint path,
                                                                GLsizei range) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsPathCHROMIUM(GLuint path,
                                                           uint32_t* result) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPathCommandsCHROMIUM(
    GLuint path,
    GLsizei numCommands,
    const GLubyte* commands,
    GLsizei numCoords,
    GLenum coordType,
    const GLvoid* coords,
    GLsizei coords_bufsize) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPathParameterfCHROMIUM(
    GLuint path,
    GLenum pname,
    GLfloat value) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPathParameteriCHROMIUM(
    GLuint path,
    GLenum pname,
    GLint value) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPathStencilFuncCHROMIUM(
    GLenum func,
    GLint ref,
    GLuint mask) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilFillPathCHROMIUM(
    GLuint path,
    GLenum fillMode,
    GLuint mask) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilStrokePathCHROMIUM(
    GLuint path,
    GLint reference,
    GLuint mask) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverFillPathCHROMIUM(
    GLuint path,
    GLenum coverMode) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverStrokePathCHROMIUM(
    GLuint path,
    GLenum coverMode) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilThenCoverFillPathCHROMIUM(
    GLuint path,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilThenCoverStrokePathCHROMIUM(
    GLuint path,
    GLint reference,
    GLuint mask,
    GLenum coverMode) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilFillPathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilStrokePathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLint reference,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverFillPathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverStrokePathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoStencilThenCoverFillPathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoStencilThenCoverStrokePathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLint reference,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFragmentInputLocationCHROMIUM(
    GLuint program,
    GLint location,
    const char* name) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoProgramPathFragmentInputGenCHROMIUM(
    GLuint program,
    GLint location,
    GLenum genMode,
    GLint components,
    const GLfloat* coeffs,
    GLsizei coeffsBufsize) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverageModulationCHROMIUM(
    GLenum components) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendBarrierKHR() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoApplyScreenSpaceAntialiasingCHROMIUM() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFragDataLocationIndexedEXT(
    GLuint program,
    GLuint colorNumber,
    GLuint index,
    const char* name) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFragDataLocationEXT(
    GLuint program,
    GLuint colorNumber,
    const char* name) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFragDataIndexEXT(
    GLuint program,
    const char* name,
    GLint* index) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoUniformMatrix4fvStreamTextureMatrixCHROMIUM(
    GLint location,
    GLboolean transpose,
    const volatile GLfloat* defaultValue) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoOverlayPromotionHintCHROMIUM(
    GLuint texture,
    GLboolean promotion_hint,
    GLint display_x,
    GLint display_y,
    GLint display_width,
    GLint display_height) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSetDrawRectangleCHROMIUM(
    GLint x,
    GLint y,
    GLint width,
    GLint height) {
  FlushErrors();

  GLint current_framebuffer = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_framebuffer);
  if (current_framebuffer != 0) {
    InsertError(GL_INVALID_OPERATION, "framebuffer must not be bound.");
    return error::kNoError;
  }

  if (!surface_->SupportsDCLayers()) {
    InsertError(GL_INVALID_OPERATION,
                "surface doesn't support SetDrawRectangle.");
    return error::kNoError;
  }

  gfx::Rect rect(x, y, width, height);
  if (!surface_->SetDrawRectangle(rect)) {
    InsertError(GL_INVALID_OPERATION, "SetDrawRectangle failed on surface");
    return error::kNoError;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSetEnableDCLayersCHROMIUM(
    GLboolean enable) {
  FlushErrors();

  GLint current_framebuffer = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_framebuffer);
  if (current_framebuffer != 0) {
    InsertError(GL_INVALID_OPERATION, "framebuffer must not be bound.");
    return error::kNoError;
  }

  if (!surface_->SupportsDCLayers()) {
    InsertError(GL_INVALID_OPERATION,
                "surface doesn't support SetDrawRectangle.");
    return error::kNoError;
  }

  if (!surface_->SetEnableDCLayers(!!enable)) {
    InsertError(GL_INVALID_OPERATION, "SetEnableDCLayers failed on surface.");
    return error::kNoError;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBeginRasterCHROMIUM(
    GLuint texture_id,
    GLuint sk_color,
    GLuint msaa_sample_count,
    GLboolean can_use_lcd_text,
    GLboolean use_distance_field_text,
    GLint pixel_config) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEndRasterCHROMIUM() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

}  // namespace gles2
}  // namespace gpu
