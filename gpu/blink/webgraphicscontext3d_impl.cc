// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/blink/webgraphicscontext3d_impl.h"

#include <stdint.h>

#include "base/atomicops.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sys_info.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/sync_token.h"

#include "third_party/khronos/GLES2/gl2.h"
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include "third_party/khronos/GLES2/gl2ext.h"

using blink::WGC3Dbitfield;
using blink::WGC3Dboolean;
using blink::WGC3Dbyte;
using blink::WGC3Dchar;
using blink::WGC3Dclampf;
using blink::WGC3Denum;
using blink::WGC3Dfloat;
using blink::WGC3Dint;
using blink::WGC3Dintptr;
using blink::WGC3Dsizei;
using blink::WGC3Dsizeiptr;
using blink::WGC3Dint64;
using blink::WGC3Duint64;
using blink::WGC3Duint;
using blink::WebGLId;

namespace gpu_blink {

class WebGraphicsContext3DErrorMessageCallback
    : public ::gpu::gles2::GLES2ImplementationErrorMessageCallback {
 public:
  WebGraphicsContext3DErrorMessageCallback(
      WebGraphicsContext3DImpl* context)
      : graphics_context_(context) {
  }

  void OnErrorMessage(const char* msg, int id) override;

 private:
  WebGraphicsContext3DImpl* graphics_context_;

  DISALLOW_COPY_AND_ASSIGN(WebGraphicsContext3DErrorMessageCallback);
};

void WebGraphicsContext3DErrorMessageCallback::OnErrorMessage(
    const char* msg, int id) {
  graphics_context_->OnErrorMessage(msg, id);
}

// Helper macros to reduce the amount of code.

#define DELEGATE_TO_GL(name, glname)                                    \
void WebGraphicsContext3DImpl::name() {                                 \
  gl_->glname();                                                        \
}

#define DELEGATE_TO_GL_R(name, glname, rt)                              \
rt WebGraphicsContext3DImpl::name() {                                   \
  return gl_->glname();                                                 \
}

#define DELEGATE_TO_GL_1(name, glname, t1)                              \
void WebGraphicsContext3DImpl::name(t1 a1) {                            \
  gl_->glname(a1);                                                      \
}

#define DELEGATE_TO_GL_1R(name, glname, t1, rt)                         \
rt WebGraphicsContext3DImpl::name(t1 a1) {                              \
  return gl_->glname(a1);                                               \
}

#define DELEGATE_TO_GL_1RB(name, glname, t1, rt)                        \
rt WebGraphicsContext3DImpl::name(t1 a1) {                              \
  return gl_->glname(a1) ? true : false;                                \
}

#define DELEGATE_TO_GL_2(name, glname, t1, t2)                          \
void WebGraphicsContext3DImpl::name(t1 a1, t2 a2) {                     \
  gl_->glname(a1, a2);                                                  \
}

#define DELEGATE_TO_GL_2R(name, glname, t1, t2, rt)                     \
rt WebGraphicsContext3DImpl::name(t1 a1, t2 a2) {                       \
  return gl_->glname(a1, a2);                                           \
}

#define DELEGATE_TO_GL_3(name, glname, t1, t2, t3)                      \
void WebGraphicsContext3DImpl::name(t1 a1, t2 a2, t3 a3) {              \
  gl_->glname(a1, a2, a3);                                              \
}

#define DELEGATE_TO_GL_3R(name, glname, t1, t2, t3, rt)                 \
rt WebGraphicsContext3DImpl::name(t1 a1, t2 a2, t3 a3) {                \
  return gl_->glname(a1, a2, a3);                                       \
}

#define DELEGATE_TO_GL_4(name, glname, t1, t2, t3, t4)                  \
void WebGraphicsContext3DImpl::name(t1 a1, t2 a2, t3 a3, t4 a4) {       \
  gl_->glname(a1, a2, a3, a4);                                          \
}

#define DELEGATE_TO_GL_4R(name, glname, t1, t2, t3, t4, rt)             \
rt WebGraphicsContext3DImpl::name(t1 a1, t2 a2, t3 a3, t4 a4) {         \
  return gl_->glname(a1, a2, a3, a4);                                   \
}

#define DELEGATE_TO_GL_5(name, glname, t1, t2, t3, t4, t5)              \
void WebGraphicsContext3DImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) {\
                                                                        \
  gl_->glname(a1, a2, a3, a4, a5);                                      \
}

#define DELEGATE_TO_GL_6(name, glname, t1, t2, t3, t4, t5, t6)          \
void WebGraphicsContext3DImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5,  \
                                    t6 a6) {                            \
  gl_->glname(a1, a2, a3, a4, a5, a6);                                  \
}

#define DELEGATE_TO_GL_7(name, glname, t1, t2, t3, t4, t5, t6, t7)      \
void WebGraphicsContext3DImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5,  \
                                    t6 a6, t7 a7) {                     \
  gl_->glname(a1, a2, a3, a4, a5, a6, a7);                              \
}

#define DELEGATE_TO_GL_8(name, glname, t1, t2, t3, t4, t5, t6, t7, t8)  \
void WebGraphicsContext3DImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5,  \
                                    t6 a6, t7 a7, t8 a8) {              \
  gl_->glname(a1, a2, a3, a4, a5, a6, a7, a8);                          \
}

#define DELEGATE_TO_GL_9(name, glname, t1, t2, t3, t4, t5, t6, t7, t8, t9) \
void WebGraphicsContext3DImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5,  \
                                    t6 a6, t7 a7, t8 a8, t9 a9) {       \
  gl_->glname(a1, a2, a3, a4, a5, a6, a7, a8, a9);                      \
}

#define DELEGATE_TO_GL_9R(name, glname, t1, t2, t3, t4, t5, t6, t7, t8, \
                          t9, rt)                                       \
rt WebGraphicsContext3DImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5,    \
                                    t6 a6, t7 a7, t8 a8, t9 a9) {       \
  return gl_->glname(a1, a2, a3, a4, a5, a6, a7, a8, a9);               \
}

#define DELEGATE_TO_GL_10(name, glname, t1, t2, t3, t4, t5, t6, t7, t8, \
                          t9, t10)                                      \
void WebGraphicsContext3DImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5,  \
                                    t6 a6, t7 a7, t8 a8, t9 a9,         \
                                    t10 a10) {                          \
  gl_->glname(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);                 \
}

#define DELEGATE_TO_GL_11(name, glname, t1, t2, t3, t4, t5, t6, t7, t8, \
                          t9, t10, t11)                                 \
void WebGraphicsContext3DImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5,  \
                                    t6 a6, t7 a7, t8 a8, t9 a9, t10 a10,\
                                    t11 a11) {                          \
  gl_->glname(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);            \
}

#define DELEGATE_TO_GL_12(name, glname, t1, t2, t3, t4, t5, t6, t7, t8, \
                          t9, t10, t11, t12)                            \
void WebGraphicsContext3DImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5,  \
                                    t6 a6, t7 a7, t8 a8, t9 a9, t10 a10,\
                                    t11 a11, t12 a12) {                 \
  gl_->glname(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);       \
}

WebGraphicsContext3DImpl::WebGraphicsContext3DImpl()
    : initialized_(false),
      initialize_failed_(false),
      context_lost_callback_(0),
      error_message_callback_(0),
      gl_(NULL) {}

WebGraphicsContext3DImpl::~WebGraphicsContext3DImpl() {

}

blink::WebString WebGraphicsContext3DImpl::
    getRequestableExtensionsCHROMIUM() {
  return blink::WebString::fromUTF8(
      gl_->GetRequestableExtensionsCHROMIUM());
}

bool WebGraphicsContext3DImpl::getActiveAttrib(
    WebGLId program, WGC3Duint index, ActiveInfo& info) {
  GLint max_name_length = -1;
  gl_->GetProgramiv(
      program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_name_length);
  // The caller already checked that there is some active attribute.
  DCHECK_GT(max_name_length, 0);
  scoped_ptr<GLchar[]> name(new GLchar[max_name_length]);
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  gl_->GetActiveAttrib(
      program, index, max_name_length, &length, &size, &type, name.get());
  if (size < 0) {
    return false;
  }
  info.name = blink::WebString::fromUTF8(name.get(), length);
  info.type = type;
  info.size = size;
  return true;
}

bool WebGraphicsContext3DImpl::getActiveUniform(
    WebGLId program, WGC3Duint index, ActiveInfo& info) {
  GLint max_name_length = -1;
  gl_->GetProgramiv(
      program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_length);
  // The caller already checked that there is some active uniform.
  DCHECK_GT(max_name_length, 0);
  scoped_ptr<GLchar[]> name(new GLchar[max_name_length]);
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  gl_->GetActiveUniform(
      program, index, max_name_length, &length, &size, &type, name.get());
  if (size < 0) {
    return false;
  }
  info.name = blink::WebString::fromUTF8(name.get(), length);
  info.type = type;
  info.size = size;
  return true;
}

blink::WebString WebGraphicsContext3DImpl::getProgramInfoLog(
    WebGLId program) {
  GLint logLength = 0;
  gl_->GetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
  if (!logLength)
    return blink::WebString();
  scoped_ptr<GLchar[]> log(new GLchar[logLength]);
  if (!log)
    return blink::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetProgramInfoLog(
      program, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  blink::WebString res =
      blink::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

blink::WebString WebGraphicsContext3DImpl::getShaderInfoLog(
    WebGLId shader) {
  GLint logLength = 0;
  gl_->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
  if (!logLength)
    return blink::WebString();
  scoped_ptr<GLchar[]> log(new GLchar[logLength]);
  if (!log)
    return blink::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetShaderInfoLog(
      shader, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  blink::WebString res =
      blink::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

blink::WebString WebGraphicsContext3DImpl::getShaderSource(
    WebGLId shader) {
  GLint logLength = 0;
  gl_->GetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &logLength);
  if (!logLength)
    return blink::WebString();
  scoped_ptr<GLchar[]> log(new GLchar[logLength]);
  if (!log)
    return blink::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetShaderSource(
      shader, logLength, &returnedLogLength, log.get());
  if (!returnedLogLength)
    return blink::WebString();
  DCHECK_EQ(logLength, returnedLogLength + 1);
  blink::WebString res =
      blink::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

blink::WebString WebGraphicsContext3DImpl::
    getTranslatedShaderSourceANGLE(WebGLId shader) {
  GLint logLength = 0;
  gl_->GetShaderiv(
      shader, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, &logLength);
  if (!logLength)
    return blink::WebString();
  scoped_ptr<GLchar[]> log(new GLchar[logLength]);
  if (!log)
    return blink::WebString();
  GLsizei returnedLogLength = 0;
  gl_->GetTranslatedShaderSourceANGLE(
      shader, logLength, &returnedLogLength, log.get());
  if (!returnedLogLength)
    return blink::WebString();
  DCHECK_EQ(logLength, returnedLogLength + 1);
  blink::WebString res =
      blink::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

blink::WebString WebGraphicsContext3DImpl::getString(
    WGC3Denum name) {
  return blink::WebString::fromUTF8(
      reinterpret_cast<const char*>(gl_->GetString(name)));
}

void WebGraphicsContext3DImpl::shaderSource(
    WebGLId shader, const WGC3Dchar* string) {
  GLint length = strlen(string);
  gl_->ShaderSource(shader, 1, &string, &length);
}

void WebGraphicsContext3DImpl::setErrorMessageCallback(
    WebGraphicsContext3D::WebGraphicsErrorMessageCallback* cb) {
  error_message_callback_ = cb;
}

void WebGraphicsContext3DImpl::setContextLostCallback(
    WebGraphicsContext3D::WebGraphicsContextLostCallback* cb) {
  context_lost_callback_ = cb;
}

void WebGraphicsContext3DImpl::pushGroupMarkerEXT(
    const WGC3Dchar* marker) {
  gl_->PushGroupMarkerEXT(0, marker);
}

DELEGATE_TO_GL_1(beginTransformFeedback, BeginTransformFeedback, WGC3Denum)
DELEGATE_TO_GL_3(bindBufferBase, BindBufferBase, WGC3Denum, WGC3Duint,
                 WGC3Duint)
DELEGATE_TO_GL_5(bindBufferRange, BindBufferRange, WGC3Denum, WGC3Duint,
                 WGC3Duint, WGC3Dintptr, WGC3Dsizeiptr)
DELEGATE_TO_GL_2(bindSampler, BindSampler, WGC3Duint, WebGLId)
DELEGATE_TO_GL_2(bindTransformFeedback, BindTransformFeedback, WGC3Denum,
                 WebGLId)
DELEGATE_TO_GL_4(clearBufferfi, ClearBufferfi, WGC3Denum, WGC3Dint, WGC3Dfloat,
                 WGC3Dint)
DELEGATE_TO_GL_3(clearBufferfv, ClearBufferfv, WGC3Denum, WGC3Dint,
                 const WGC3Dfloat *)
DELEGATE_TO_GL_3(clearBufferiv, ClearBufferiv, WGC3Denum, WGC3Dint,
                 const WGC3Dint *)
DELEGATE_TO_GL_3(clearBufferuiv, ClearBufferuiv, WGC3Denum, WGC3Dint,
                 const WGC3Duint *)
DELEGATE_TO_GL_9(compressedTexImage3D, CompressedTexImage3D, WGC3Denum,
                 WGC3Dint, WGC3Denum, WGC3Dsizei, WGC3Dsizei, WGC3Dsizei,
                 WGC3Dint, WGC3Dsizei, const void *)
DELEGATE_TO_GL_11(compressedTexSubImage3D, CompressedTexSubImage3D, WGC3Denum,
                  WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dsizei,
                  WGC3Dsizei, WGC3Dsizei, WGC3Denum, WGC3Dsizei, const void *)
DELEGATE_TO_GL_5(copyBufferSubData, CopyBufferSubData, WGC3Denum, WGC3Denum,
                 WGC3Dintptr, WGC3Dintptr, WGC3Dsizeiptr)
DELEGATE_TO_GL_9(copyTexSubImage3D, CopyTexSubImage3D, WGC3Denum, WGC3Dint,
                 WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dsizei,
                 WGC3Dsizei)
DELEGATE_TO_GL(endTransformFeedback, EndTransformFeedback)
DELEGATE_TO_GL_5(getActiveUniformBlockName, GetActiveUniformBlockName,
                 WGC3Duint, WGC3Duint, WGC3Dsizei, WGC3Dsizei *, WGC3Dchar *)
DELEGATE_TO_GL_4(getActiveUniformBlockiv, GetActiveUniformBlockiv, WGC3Duint,
                 WGC3Duint, WGC3Denum, WGC3Dint *)
DELEGATE_TO_GL_5(getActiveUniformsiv, GetActiveUniformsiv, WGC3Duint,
                 WGC3Dsizei, const WGC3Duint *, WGC3Denum, WGC3Dint *)
DELEGATE_TO_GL_2R(getFragDataLocation, GetFragDataLocation, WGC3Duint,
                  const WGC3Dchar *, WGC3Dint)
DELEGATE_TO_GL_5(getInternalformativ, GetInternalformativ, WGC3Denum, WGC3Denum,
                 WGC3Denum, WGC3Dsizei, WGC3Dint *)
DELEGATE_TO_GL_3(getSamplerParameterfv, GetSamplerParameterfv, WGC3Duint,
                 WGC3Denum, WGC3Dfloat *)
DELEGATE_TO_GL_3(getSamplerParameteriv, GetSamplerParameteriv, WGC3Duint,
                 WGC3Denum, WGC3Dint *)
DELEGATE_TO_GL_7(getTransformFeedbackVarying, GetTransformFeedbackVarying,
                 WGC3Duint, WGC3Duint, WGC3Dsizei, WGC3Dsizei *, WGC3Dsizei *,
                 WGC3Denum *, WGC3Dchar *)
DELEGATE_TO_GL_2R(getUniformBlockIndex, GetUniformBlockIndex, WGC3Duint,
                  const WGC3Dchar *, WGC3Duint)
DELEGATE_TO_GL_4(getUniformIndices, GetUniformIndices, WGC3Duint, WGC3Dsizei,
                 const WGC3Dchar *const*, WGC3Duint *)
DELEGATE_TO_GL_3(getUniformuiv, GetUniformuiv, WGC3Duint, WGC3Dint,
                 WGC3Duint *)
DELEGATE_TO_GL_3(invalidateFramebuffer, InvalidateFramebuffer, WGC3Denum,
                 WGC3Dsizei, const WGC3Denum *)
DELEGATE_TO_GL_7(invalidateSubFramebuffer, InvalidateSubFramebuffer, WGC3Denum,
                 WGC3Dsizei, const WGC3Denum *, WGC3Dint, WGC3Dint, WGC3Dsizei,
                 WGC3Dsizei)
DELEGATE_TO_GL_1R(isSampler, IsSampler, WebGLId, WGC3Dboolean)
DELEGATE_TO_GL_1R(isTransformFeedback, IsTransformFeedback, WGC3Duint,
                  WGC3Dboolean)
DELEGATE_TO_GL_4R(mapBufferRange, MapBufferRange, WGC3Denum, WGC3Dintptr,
                  WGC3Dsizeiptr, WGC3Dbitfield, void*);
DELEGATE_TO_GL(pauseTransformFeedback, PauseTransformFeedback)
//DELEGATE_TO_GL_3(programParameteri, ProgramParameteri, WGC3Duint, WGC3Denum,
//                 WGC3Dint)
DELEGATE_TO_GL_1(readBuffer, ReadBuffer, WGC3Denum)
DELEGATE_TO_GL(resumeTransformFeedback, ResumeTransformFeedback)
DELEGATE_TO_GL_3(samplerParameterf, SamplerParameterf, WGC3Duint, WGC3Denum,
                 WGC3Dfloat)
DELEGATE_TO_GL_3(samplerParameterfv, SamplerParameterfv, WGC3Duint, WGC3Denum,
                 const WGC3Dfloat *)
DELEGATE_TO_GL_3(samplerParameteri, SamplerParameteri, WGC3Duint, WGC3Denum,
                 WGC3Dint)
DELEGATE_TO_GL_3(samplerParameteriv, SamplerParameteriv, WGC3Duint, WGC3Denum,
                 const WGC3Dint *)
DELEGATE_TO_GL_4(transformFeedbackVaryings, TransformFeedbackVaryings,
                 WGC3Duint, WGC3Dsizei, const WGC3Dchar *const*, WGC3Denum)
DELEGATE_TO_GL_1R(unmapBuffer, UnmapBuffer, WGC3Denum, WGC3Dboolean);

::gpu::gles2::GLES2Interface* WebGraphicsContext3DImpl::getGLES2Interface() {
  return gl_;
}

::gpu::gles2::GLES2ImplementationErrorMessageCallback*
    WebGraphicsContext3DImpl::getErrorMessageCallback() {
  if (!client_error_message_callback_) {
    client_error_message_callback_.reset(
        new WebGraphicsContext3DErrorMessageCallback(this));
  }
  return client_error_message_callback_.get();
}

void WebGraphicsContext3DImpl::OnErrorMessage(
    const std::string& message, int id) {
  if (error_message_callback_) {
    blink::WebString str = blink::WebString::fromUTF8(message.c_str());
    error_message_callback_->onErrorMessage(str, id);
  }
}

// static
void WebGraphicsContext3DImpl::ConvertAttributes(
    const blink::WebGraphicsContext3D::Attributes& attributes,
    ::gpu::gles2::ContextCreationAttribHelper* output_attribs) {
  output_attribs->alpha_size = attributes.alpha ? 8 : 0;
  output_attribs->depth_size = attributes.depth ? 24 : 0;
  // TODO(jinsukkim): Pass RGBA info directly from client by cleaning up
  //   how this is passed to the constructor.
#if defined(OS_ANDROID)
  if (base::SysInfo::IsLowEndDevice() && !attributes.alpha) {
    output_attribs->red_size = 5;
    output_attribs->green_size = 6;
    output_attribs->blue_size = 5;
    output_attribs->depth_size = 16;
  } else {
    output_attribs->red_size = 8;
    output_attribs->green_size = 8;
    output_attribs->blue_size = 8;
  }
#endif
  output_attribs->stencil_size = attributes.stencil ? 8 : 0;
  output_attribs->samples = attributes.antialias ? 4 : 0;
  output_attribs->sample_buffers = attributes.antialias ? 1 : 0;
  output_attribs->fail_if_major_perf_caveat =
      attributes.failIfMajorPerformanceCaveat;
  output_attribs->bind_generates_resource = false;
  switch (attributes.webGLVersion) {
    case 0:
      output_attribs->context_type = ::gpu::gles2::CONTEXT_TYPE_OPENGLES2;
      break;
    case 1:
      output_attribs->context_type = ::gpu::gles2::CONTEXT_TYPE_WEBGL1;
      break;
    case 2:
      output_attribs->context_type = ::gpu::gles2::CONTEXT_TYPE_WEBGL2;
      break;
    default:
      NOTREACHED();
      output_attribs->context_type = ::gpu::gles2::CONTEXT_TYPE_OPENGLES2;
      break;
  }
}

}  // namespace gpu_blink
