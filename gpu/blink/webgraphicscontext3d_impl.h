// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_BLINK_WEBGRAPHICSCONTEXT3D_IMPL_H_
#define GPU_BLINK_WEBGRAPHICSCONTEXT3D_IMPL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/blink/gpu_blink_export.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace gpu {

namespace gles2 {
class GLES2Interface;
class GLES2ImplementationErrorMessageCallback;
struct ContextCreationAttribHelper;
}
}

namespace gpu_blink {

class WebGraphicsContext3DErrorMessageCallback;

class GPU_BLINK_EXPORT WebGraphicsContext3DImpl
    : public NON_EXPORTED_BASE(blink::WebGraphicsContext3D) {
 public:
  ~WebGraphicsContext3DImpl() override;

  //----------------------------------------------------------------------
  // WebGraphicsContext3D methods

  void drawElements(blink::WGC3Denum mode,
                    blink::WGC3Dsizei count,
                    blink::WGC3Denum type,
                    blink::WGC3Dintptr offset) override;

  bool getActiveAttrib(blink::WebGLId program,
                       blink::WGC3Duint index,
                       ActiveInfo&) override;
  bool getActiveUniform(blink::WebGLId program,
                        blink::WGC3Duint index,
                        ActiveInfo&) override;

  blink::WGC3Denum getError() override;
  blink::WebString getProgramInfoLog(blink::WebGLId program) override;
  blink::WebString getShaderInfoLog(blink::WebGLId shader) override;
  blink::WebString getShaderSource(blink::WebGLId shader) override;
  blink::WebString getString(blink::WGC3Denum name) override;

  void getSynciv(blink::WGC3Dsync sync,
                 blink::WGC3Denum pname,
                 blink::WGC3Dsizei bufSize,
                 blink::WGC3Dsizei *length,
                 blink::WGC3Dint *params) override;

  blink::WGC3Dsizeiptr getVertexAttribOffset(blink::WGC3Duint index,
                                             blink::WGC3Denum pname) override;

  void shaderSource(blink::WebGLId shader,
                    const blink::WGC3Dchar* string) override;

  void vertexAttribPointer(blink::WGC3Duint index,
                           blink::WGC3Dint size,
                           blink::WGC3Denum type,
                           blink::WGC3Dboolean normalized,
                           blink::WGC3Dsizei stride,
                           blink::WGC3Dintptr offset) override;

  blink::WebGLId createBuffer() override;
  blink::WebGLId createFramebuffer() override;
  blink::WebGLId createRenderbuffer() override;
  blink::WebGLId createTexture() override;

  void deleteBuffer(blink::WebGLId) override;
  void deleteFramebuffer(blink::WebGLId) override;
  void deleteRenderbuffer(blink::WebGLId) override;
  void deleteTexture(blink::WebGLId) override;

  void synthesizeGLError(blink::WGC3Denum) override;

  blink::WebString getRequestableExtensionsCHROMIUM() override;

  void blitFramebufferCHROMIUM(blink::WGC3Dint srcX0,
                               blink::WGC3Dint srcY0,
                               blink::WGC3Dint srcX1,
                               blink::WGC3Dint srcY1,
                               blink::WGC3Dint dstX0,
                               blink::WGC3Dint dstY0,
                               blink::WGC3Dint dstX1,
                               blink::WGC3Dint dstY1,
                               blink::WGC3Dbitfield mask,
                               blink::WGC3Denum filter) override;
  blink::WebString getTranslatedShaderSourceANGLE(
      blink::WebGLId shader) override;

  void setContextLostCallback(
      WebGraphicsContext3D::WebGraphicsContextLostCallback* callback) override;

  void setErrorMessageCallback(
      WebGraphicsContext3D::WebGraphicsErrorMessageCallback* callback) override;

  blink::WebGLId createQueryEXT() override;
  void deleteQueryEXT(blink::WebGLId query) override;

  blink::WebGLId createValuebufferCHROMIUM() override;
  void deleteValuebufferCHROMIUM(blink::WebGLId) override;

  void pushGroupMarkerEXT(const blink::WGC3Dchar* marker) override;

  // GL_OES_vertex_array_object
  blink::WebGLId createVertexArrayOES() override;
  void deleteVertexArrayOES(blink::WebGLId array) override;

  // OpenGL ES 3.0 functions not represented by pre-existing extensions
  void beginTransformFeedback(blink::WGC3Denum primitiveMode) override;
  void bindBufferBase(blink::WGC3Denum target,
                      blink::WGC3Duint index,
                      blink::WGC3Duint buffer) override;
  void bindBufferRange(blink::WGC3Denum target,
                       blink::WGC3Duint index,
                       blink::WGC3Duint buffer,
                       blink::WGC3Dintptr offset,
                       blink::WGC3Dsizeiptr size) override;
  void bindSampler(blink::WGC3Duint unit, blink::WebGLId sampler) override;
  void bindTransformFeedback(blink::WGC3Denum target,
                             blink::WebGLId transformfeedback) override;
  void clearBufferfi(blink::WGC3Denum buffer,
                     blink::WGC3Dint drawbuffer,
                     blink::WGC3Dfloat depth,
                     blink::WGC3Dint stencil) override;
  void clearBufferfv(blink::WGC3Denum buffer,
                     blink::WGC3Dint drawbuffer,
                     const blink::WGC3Dfloat* value) override;
  void clearBufferiv(blink::WGC3Denum buffer,
                     blink::WGC3Dint drawbuffer,
                     const blink::WGC3Dint* value) override;
  void clearBufferuiv(blink::WGC3Denum buffer,
                      blink::WGC3Dint drawbuffer,
                      const blink::WGC3Duint* value) override;
  blink::WGC3Denum clientWaitSync(blink::WGC3Dsync sync,
                                  blink::WGC3Dbitfield flags,
                                  blink::WGC3Duint64 timeout) override;
  void compressedTexImage3D(blink::WGC3Denum target,
                            blink::WGC3Dint level,
                            blink::WGC3Denum internalformat,
                            blink::WGC3Dsizei width,
                            blink::WGC3Dsizei height,
                            blink::WGC3Dsizei depth,
                            blink::WGC3Dint border,
                            blink::WGC3Dsizei imageSize,
                            const void *data) override;
  void compressedTexSubImage3D(blink::WGC3Denum target,
                               blink::WGC3Dint level,
                               blink::WGC3Dint xoffset,
                               blink::WGC3Dint yoffset,
                               blink::WGC3Dint zoffset,
                               blink::WGC3Dsizei width,
                               blink::WGC3Dsizei height,
                               blink::WGC3Dsizei depth,
                               blink::WGC3Denum format,
                               blink::WGC3Dsizei imageSize,
                               const void *data) override;
  void copyBufferSubData(blink::WGC3Denum readTarget,
                         blink::WGC3Denum writeTarget,
                         blink::WGC3Dintptr readOffset,
                         blink::WGC3Dintptr writeOffset,
                         blink::WGC3Dsizeiptr size) override;
  void copyTexSubImage3D(blink::WGC3Denum target,
                         blink::WGC3Dint level,
                         blink::WGC3Dint xoffset,
                         blink::WGC3Dint yoffset,
                         blink::WGC3Dint zoffset,
                         blink::WGC3Dint x,
                         blink::WGC3Dint y,
                         blink::WGC3Dsizei width,
                         blink::WGC3Dsizei height) override;
  blink::WebGLId createSampler() override;
  blink::WebGLId createTransformFeedback() override;
  void deleteSampler(blink::WebGLId sampler) override;
  void deleteSync(blink::WGC3Dsync sync) override;
  void deleteTransformFeedback(blink::WebGLId transformfeedback) override;
  void drawRangeElements(blink::WGC3Denum mode,
                         blink::WGC3Duint start,
                         blink::WGC3Duint end,
                         blink::WGC3Dsizei count,
                         blink::WGC3Denum type,
                         blink::WGC3Dintptr offset) override;
  void endTransformFeedback(void) override;
  blink::WGC3Dsync fenceSync(blink::WGC3Denum condition,
                             blink::WGC3Dbitfield flags) override;
  void getActiveUniformBlockName(blink::WGC3Duint program,
                                 blink::WGC3Duint uniformBlockIndex,
                                 blink::WGC3Dsizei bufSize,
                                 blink::WGC3Dsizei* length,
                                 blink::WGC3Dchar* uniformBlockName) override;
  void getActiveUniformBlockiv(blink::WGC3Duint program,
                               blink::WGC3Duint uniformBlockIndex,
                               blink::WGC3Denum pname,
                               blink::WGC3Dint* params) override;
  void getActiveUniformsiv(blink::WGC3Duint program,
                           blink::WGC3Dsizei uniformCount,
                           const blink::WGC3Duint *uniformIndices,
                           blink::WGC3Denum pname,
                           blink::WGC3Dint *params) override;
  blink::WGC3Dint getFragDataLocation(blink::WGC3Duint program,
                                      const blink::WGC3Dchar* name) override;
  void getInternalformativ(blink::WGC3Denum target,
                           blink::WGC3Denum internalformat,
                           blink::WGC3Denum pname,
                           blink::WGC3Dsizei bufSize,
                           blink::WGC3Dint* params) override;
  void getSamplerParameterfv(blink::WGC3Duint sampler,
                             blink::WGC3Denum pname,
                             blink::WGC3Dfloat* params) override;
  void getSamplerParameteriv(blink::WGC3Duint sampler,
                             blink::WGC3Denum pname,
                             blink::WGC3Dint* params) override;
  void getTransformFeedbackVarying(blink::WGC3Duint program,
                                   blink::WGC3Duint index,
                                   blink::WGC3Dsizei bufSize,
                                   blink::WGC3Dsizei *length,
                                   blink::WGC3Dsizei *size,
                                   blink::WGC3Denum *type,
                                   blink::WGC3Dchar *name) override;
  blink::WGC3Duint getUniformBlockIndex(
      blink::WGC3Duint program,
      const blink::WGC3Dchar* uniformBlockName) override;
  void getUniformIndices(blink::WGC3Duint program,
                         blink::WGC3Dsizei uniformCount,
                         const blink::WGC3Dchar *const*uniformNames,
                         blink::WGC3Duint *uniformIndices) override;
  void getUniformuiv(blink::WGC3Duint program,
                     blink::WGC3Dint location,
                     blink::WGC3Duint *params) override;
  void invalidateFramebuffer(blink::WGC3Denum target,
                             blink::WGC3Dsizei numAttachments,
                             const blink::WGC3Denum* attachments) override;
  void invalidateSubFramebuffer(blink::WGC3Denum target,
                                blink::WGC3Dsizei numAttachments,
                                const blink::WGC3Denum* attachments,
                                blink::WGC3Dint x,
                                blink::WGC3Dint y,
                                blink::WGC3Dsizei width,
                                blink::WGC3Dsizei height) override;
  blink::WGC3Dboolean isSampler(blink::WebGLId sampler) override;
  blink::WGC3Dboolean isSync(blink::WGC3Dsync sync) override;
  blink::WGC3Dboolean isTransformFeedback(blink::WGC3Duint id) override;
  void* mapBufferRange(blink::WGC3Denum target,
                       blink::WGC3Dintptr offset,
                       blink::WGC3Dsizeiptr length,
                       blink::WGC3Dbitfield access) override;
  void pauseTransformFeedback(void) override;
  //void programParameteri(blink::WGC3Duint program,
  //                       blink::WGC3Denum pname,
  //                       blink::WGC3Dint value) override;
  void readBuffer(blink::WGC3Denum src) override;
  void resumeTransformFeedback(void) override;
  void samplerParameterf(blink::WGC3Duint sampler,
                         blink::WGC3Denum pname,
                         blink::WGC3Dfloat param) override;
  void samplerParameterfv(blink::WGC3Duint sampler,
                          blink::WGC3Denum pname,
                          const blink::WGC3Dfloat* param) override;
  void samplerParameteri(blink::WGC3Duint sampler,
                         blink::WGC3Denum pname,
                         blink::WGC3Dint param) override;
  void samplerParameteriv(blink::WGC3Duint sampler,
                          blink::WGC3Denum pname,
                          const blink::WGC3Dint* param) override;
  void transformFeedbackVaryings(
      blink::WGC3Duint program,
      blink::WGC3Dsizei count,
      const blink::WGC3Dchar* const* varyings,
      blink::WGC3Denum bufferMode) override;
  blink::WGC3Dboolean unmapBuffer(blink::WGC3Denum target) override;
  void vertexAttribIPointer(blink::WGC3Duint index,
                            blink::WGC3Dint size,
                            blink::WGC3Denum type,
                            blink::WGC3Dsizei stride,
                            blink::WGC3Dintptr pointer) override;
  void waitSync(blink::WGC3Dsync sync,
                blink::WGC3Dbitfield flags,
                blink::WGC3Duint64 timeout) override;

  // WebGraphicsContext3D implementation.
  ::gpu::gles2::GLES2Interface* getGLES2Interface() override;

  ::gpu::gles2::GLES2Interface* GetGLInterface() {
    return gl_;
  }

  // Convert WebGL context creation attributes into command buffer / EGL size
  // requests.
  static void ConvertAttributes(
      const blink::WebGraphicsContext3D::Attributes& attributes,
      ::gpu::gles2::ContextCreationAttribHelper* output_attribs);

 protected:
  friend class WebGraphicsContext3DErrorMessageCallback;

  WebGraphicsContext3DImpl();

  ::gpu::gles2::GLES2ImplementationErrorMessageCallback*
      getErrorMessageCallback();
  virtual void OnErrorMessage(const std::string& message, int id);

  void SetGLInterface(::gpu::gles2::GLES2Interface* gl) { gl_ = gl; }

  bool initialized_;
  bool initialize_failed_;

  WebGraphicsContext3D::WebGraphicsContextLostCallback* context_lost_callback_;

  WebGraphicsContext3D::WebGraphicsErrorMessageCallback*
      error_message_callback_;
  scoped_ptr<WebGraphicsContext3DErrorMessageCallback>
      client_error_message_callback_;

  // Errors raised by synthesizeGLError().
  std::vector<blink::WGC3Denum> synthetic_errors_;

  ::gpu::gles2::GLES2Interface* gl_;
  bool lose_context_when_out_of_memory_;
};

}  // namespace gpu_blink

#endif  // GPU_BLINK_WEBGRAPHICSCONTEXT3D_IMPL_H_
