// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

// These functions emluate GLES2 over command buffers.
#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_C_LIB_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_C_LIB_AUTOGEN_H_

void GLES2ActiveTexture(GLenum texture) {
  GPU_CLIENT_LOG("ActiveTexture" << "(" << texture << ")");
  gles2::GetGLContext()->ActiveTexture(texture);
}
void GLES2AttachShader(GLuint program, GLuint shader) {
  GPU_CLIENT_LOG("AttachShader" << "(" << program << ", " << shader << ")");
  gles2::GetGLContext()->AttachShader(program, shader);
}
void GLES2BindAttribLocation(GLuint program, GLuint index, const char* name) {
  GPU_CLIENT_LOG(
      "BindAttribLocation" << "(" << program << ", " << index << ", " << name << ")");  // NOLINT
  gles2::GetGLContext()->BindAttribLocation(program, index, name);
}
void GLES2BindBuffer(GLenum target, GLuint buffer) {
  GPU_CLIENT_LOG("BindBuffer" << "(" << target << ", " << buffer << ")");
  gles2::GetGLContext()->BindBuffer(target, buffer);
}
void GLES2BindFramebuffer(GLenum target, GLuint framebuffer) {
  GPU_CLIENT_LOG(
      "BindFramebuffer" << "(" << target << ", " << framebuffer << ")");
  gles2::GetGLContext()->BindFramebuffer(target, framebuffer);
}
void GLES2BindRenderbuffer(GLenum target, GLuint renderbuffer) {
  GPU_CLIENT_LOG(
      "BindRenderbuffer" << "(" << target << ", " << renderbuffer << ")");
  gles2::GetGLContext()->BindRenderbuffer(target, renderbuffer);
}
void GLES2BindTexture(GLenum target, GLuint texture) {
  GPU_CLIENT_LOG("BindTexture" << "(" << target << ", " << texture << ")");
  gles2::GetGLContext()->BindTexture(target, texture);
}
void GLES2BlendColor(
    GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  GPU_CLIENT_LOG(
      "BlendColor" << "(" << red << ", " << green << ", " << blue << ", " << alpha << ")");  // NOLINT
  gles2::GetGLContext()->BlendColor(red, green, blue, alpha);
}
void GLES2BlendEquation(GLenum mode) {
  GPU_CLIENT_LOG("BlendEquation" << "(" << mode << ")");
  gles2::GetGLContext()->BlendEquation(mode);
}
void GLES2BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
  GPU_CLIENT_LOG(
      "BlendEquationSeparate" << "(" << modeRGB << ", " << modeAlpha << ")");
  gles2::GetGLContext()->BlendEquationSeparate(modeRGB, modeAlpha);
}
void GLES2BlendFunc(GLenum sfactor, GLenum dfactor) {
  GPU_CLIENT_LOG("BlendFunc" << "(" << sfactor << ", " << dfactor << ")");
  gles2::GetGLContext()->BlendFunc(sfactor, dfactor);
}
void GLES2BlendFuncSeparate(
    GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
  GPU_CLIENT_LOG(
      "BlendFuncSeparate" << "(" << srcRGB << ", " << dstRGB << ", " << srcAlpha << ", " << dstAlpha << ")");  // NOLINT
  gles2::GetGLContext()->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}
void GLES2BufferData(
    GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
  GPU_CLIENT_LOG(
      "BufferData" << "(" << target << ", " << size << ", " << data << ", " << usage << ")");  // NOLINT
  gles2::GetGLContext()->BufferData(target, size, data, usage);
}
void GLES2BufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
  GPU_CLIENT_LOG(
      "BufferSubData" << "(" << target << ", " << offset << ", " << size << ", " << data << ")");  // NOLINT
  gles2::GetGLContext()->BufferSubData(target, offset, size, data);
}
GLenum GLES2CheckFramebufferStatus(GLenum target) {
  GPU_CLIENT_LOG("CheckFramebufferStatus" << "(" << target << ")");
  GLenum result = gles2::GetGLContext()->CheckFramebufferStatus(target);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
void GLES2Clear(GLbitfield mask) {
  GPU_CLIENT_LOG("Clear" << "(" << mask << ")");
  gles2::GetGLContext()->Clear(mask);
}
void GLES2ClearColor(
    GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  GPU_CLIENT_LOG(
      "ClearColor" << "(" << red << ", " << green << ", " << blue << ", " << alpha << ")");  // NOLINT
  gles2::GetGLContext()->ClearColor(red, green, blue, alpha);
}
void GLES2ClearDepthf(GLclampf depth) {
  GPU_CLIENT_LOG("ClearDepthf" << "(" << depth << ")");
  gles2::GetGLContext()->ClearDepthf(depth);
}
void GLES2ClearStencil(GLint s) {
  GPU_CLIENT_LOG("ClearStencil" << "(" << s << ")");
  gles2::GetGLContext()->ClearStencil(s);
}
void GLES2ColorMask(
    GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
  GPU_CLIENT_LOG(
      "ColorMask" << "(" << red << ", " << green << ", " << blue << ", " << alpha << ")");  // NOLINT
  gles2::GetGLContext()->ColorMask(red, green, blue, alpha);
}
void GLES2CompileShader(GLuint shader) {
  GPU_CLIENT_LOG("CompileShader" << "(" << shader << ")");
  gles2::GetGLContext()->CompileShader(shader);
}
void GLES2CompressedTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLsizei width,
    GLsizei height, GLint border, GLsizei imageSize, const void* data) {
  GPU_CLIENT_LOG(
      "CompressedTexImage2D" << "(" << target << ", " << level << ", " << internalformat << ", " << width << ", " << height << ", " << border << ", " << imageSize << ", " << data << ")");  // NOLINT
  gles2::GetGLContext()->CompressedTexImage2D(
      target, level, internalformat, width, height, border, imageSize, data);
}
void GLES2CompressedTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
  GPU_CLIENT_LOG(
      "CompressedTexSubImage2D" << "(" << target << ", " << level << ", " << xoffset << ", " << yoffset << ", " << width << ", " << height << ", " << format << ", " << imageSize << ", " << data << ")");  // NOLINT
  gles2::GetGLContext()->CompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}
void GLES2CopyTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
    GLsizei width, GLsizei height, GLint border) {
  GPU_CLIENT_LOG(
      "CopyTexImage2D" << "(" << target << ", " << level << ", " << internalformat << ", " << x << ", " << y << ", " << width << ", " << height << ", " << border << ")");  // NOLINT
  gles2::GetGLContext()->CopyTexImage2D(
      target, level, internalformat, x, y, width, height, border);
}
void GLES2CopyTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
    GLsizei width, GLsizei height) {
  GPU_CLIENT_LOG(
      "CopyTexSubImage2D" << "(" << target << ", " << level << ", " << xoffset << ", " << yoffset << ", " << x << ", " << y << ", " << width << ", " << height << ")");  // NOLINT
  gles2::GetGLContext()->CopyTexSubImage2D(
      target, level, xoffset, yoffset, x, y, width, height);
}
GLuint GLES2CreateProgram() {
  GPU_CLIENT_LOG("CreateProgram" << "(" << ")");
  GLuint result = gles2::GetGLContext()->CreateProgram();
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
GLuint GLES2CreateShader(GLenum type) {
  GPU_CLIENT_LOG("CreateShader" << "(" << type << ")");
  GLuint result = gles2::GetGLContext()->CreateShader(type);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
void GLES2CullFace(GLenum mode) {
  GPU_CLIENT_LOG("CullFace" << "(" << mode << ")");
  gles2::GetGLContext()->CullFace(mode);
}
void GLES2DeleteBuffers(GLsizei n, const GLuint* buffers) {
  GPU_CLIENT_LOG("DeleteBuffers" << "(" << n << ", " << buffers << ")");
  gles2::GetGLContext()->DeleteBuffers(n, buffers);
}
void GLES2DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  GPU_CLIENT_LOG(
      "DeleteFramebuffers" << "(" << n << ", " << framebuffers << ")");
  gles2::GetGLContext()->DeleteFramebuffers(n, framebuffers);
}
void GLES2DeleteProgram(GLuint program) {
  GPU_CLIENT_LOG("DeleteProgram" << "(" << program << ")");
  gles2::GetGLContext()->DeleteProgram(program);
}
void GLES2DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  GPU_CLIENT_LOG(
      "DeleteRenderbuffers" << "(" << n << ", " << renderbuffers << ")");
  gles2::GetGLContext()->DeleteRenderbuffers(n, renderbuffers);
}
void GLES2DeleteShader(GLuint shader) {
  GPU_CLIENT_LOG("DeleteShader" << "(" << shader << ")");
  gles2::GetGLContext()->DeleteShader(shader);
}
void GLES2DeleteTextures(GLsizei n, const GLuint* textures) {
  GPU_CLIENT_LOG("DeleteTextures" << "(" << n << ", " << textures << ")");
  gles2::GetGLContext()->DeleteTextures(n, textures);
}
void GLES2DepthFunc(GLenum func) {
  GPU_CLIENT_LOG("DepthFunc" << "(" << func << ")");
  gles2::GetGLContext()->DepthFunc(func);
}
void GLES2DepthMask(GLboolean flag) {
  GPU_CLIENT_LOG("DepthMask" << "(" << flag << ")");
  gles2::GetGLContext()->DepthMask(flag);
}
void GLES2DepthRangef(GLclampf zNear, GLclampf zFar) {
  GPU_CLIENT_LOG("DepthRangef" << "(" << zNear << ", " << zFar << ")");
  gles2::GetGLContext()->DepthRangef(zNear, zFar);
}
void GLES2DetachShader(GLuint program, GLuint shader) {
  GPU_CLIENT_LOG("DetachShader" << "(" << program << ", " << shader << ")");
  gles2::GetGLContext()->DetachShader(program, shader);
}
void GLES2Disable(GLenum cap) {
  GPU_CLIENT_LOG("Disable" << "(" << cap << ")");
  gles2::GetGLContext()->Disable(cap);
}
void GLES2DisableVertexAttribArray(GLuint index) {
  GPU_CLIENT_LOG("DisableVertexAttribArray" << "(" << index << ")");
  gles2::GetGLContext()->DisableVertexAttribArray(index);
}
void GLES2DrawArrays(GLenum mode, GLint first, GLsizei count) {
  GPU_CLIENT_LOG(
      "DrawArrays" << "(" << mode << ", " << first << ", " << count << ")");
  gles2::GetGLContext()->DrawArrays(mode, first, count);
}
void GLES2DrawElements(
    GLenum mode, GLsizei count, GLenum type, const void* indices) {
  GPU_CLIENT_LOG(
      "DrawElements" << "(" << mode << ", " << count << ", " << type << ", " << indices << ")");  // NOLINT
  gles2::GetGLContext()->DrawElements(mode, count, type, indices);
}
void GLES2Enable(GLenum cap) {
  GPU_CLIENT_LOG("Enable" << "(" << cap << ")");
  gles2::GetGLContext()->Enable(cap);
}
void GLES2EnableVertexAttribArray(GLuint index) {
  GPU_CLIENT_LOG("EnableVertexAttribArray" << "(" << index << ")");
  gles2::GetGLContext()->EnableVertexAttribArray(index);
}
void GLES2Finish() {
  GPU_CLIENT_LOG("Finish" << "(" << ")");
  gles2::GetGLContext()->Finish();
}
void GLES2Flush() {
  GPU_CLIENT_LOG("Flush" << "(" << ")");
  gles2::GetGLContext()->Flush();
}
void GLES2FramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint renderbuffer) {
  GPU_CLIENT_LOG(
      "FramebufferRenderbuffer" << "(" << target << ", " << attachment << ", " << renderbuffertarget << ", " << renderbuffer << ")");  // NOLINT
  gles2::GetGLContext()->FramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
}
void GLES2FramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
    GLint level) {
  GPU_CLIENT_LOG(
      "FramebufferTexture2D" << "(" << target << ", " << attachment << ", " << textarget << ", " << texture << ", " << level << ")");  // NOLINT
  gles2::GetGLContext()->FramebufferTexture2D(
      target, attachment, textarget, texture, level);
}
void GLES2FrontFace(GLenum mode) {
  GPU_CLIENT_LOG("FrontFace" << "(" << mode << ")");
  gles2::GetGLContext()->FrontFace(mode);
}
void GLES2GenBuffers(GLsizei n, GLuint* buffers) {
  GPU_CLIENT_LOG("GenBuffers" << "(" << n << ", " << buffers << ")");
  gles2::GetGLContext()->GenBuffers(n, buffers);
}
void GLES2GenerateMipmap(GLenum target) {
  GPU_CLIENT_LOG("GenerateMipmap" << "(" << target << ")");
  gles2::GetGLContext()->GenerateMipmap(target);
}
void GLES2GenFramebuffers(GLsizei n, GLuint* framebuffers) {
  GPU_CLIENT_LOG("GenFramebuffers" << "(" << n << ", " << framebuffers << ")");
  gles2::GetGLContext()->GenFramebuffers(n, framebuffers);
}
void GLES2GenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  GPU_CLIENT_LOG(
      "GenRenderbuffers" << "(" << n << ", " << renderbuffers << ")");
  gles2::GetGLContext()->GenRenderbuffers(n, renderbuffers);
}
void GLES2GenTextures(GLsizei n, GLuint* textures) {
  GPU_CLIENT_LOG("GenTextures" << "(" << n << ", " << textures << ")");
  gles2::GetGLContext()->GenTextures(n, textures);
}
void GLES2GetActiveAttrib(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLsizei, length);
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, size);
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLenum, type);
  GPU_CLIENT_LOG(
      "GetActiveAttrib" << "(" << program << ", " << index << ", " << bufsize << ", " << length << ", " << size << ", " << type << ", " << name << ")");  // NOLINT
  gles2::GetGLContext()->GetActiveAttrib(
      program, index, bufsize, length, size, type, name);
}
void GLES2GetActiveUniform(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLsizei, length);
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, size);
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLenum, type);
  GPU_CLIENT_LOG(
      "GetActiveUniform" << "(" << program << ", " << index << ", " << bufsize << ", " << length << ", " << size << ", " << type << ", " << name << ")");  // NOLINT
  gles2::GetGLContext()->GetActiveUniform(
      program, index, bufsize, length, size, type, name);
}
void GLES2GetAttachedShaders(
    GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLsizei, count);
  GPU_CLIENT_LOG(
      "GetAttachedShaders" << "(" << program << ", " << maxcount << ", " << count << ", " << shaders << ")");  // NOLINT
  gles2::GetGLContext()->GetAttachedShaders(program, maxcount, count, shaders);
}
GLint GLES2GetAttribLocation(GLuint program, const char* name) {
  GPU_CLIENT_LOG("GetAttribLocation" << "(" << program << ", " << name << ")");
  GLint result = gles2::GetGLContext()->GetAttribLocation(program, name);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
void GLES2GetBooleanv(GLenum pname, GLboolean* params) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLboolean, params);
  GPU_CLIENT_LOG("GetBooleanv" << "(" << pname << ", " << params << ")");
  gles2::GetGLContext()->GetBooleanv(pname, params);
}
void GLES2GetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG(
      "GetBufferParameteriv" << "(" << target << ", " << pname << ", " << params << ")");  // NOLINT
  gles2::GetGLContext()->GetBufferParameteriv(target, pname, params);
}
GLenum GLES2GetError() {
  GPU_CLIENT_LOG("GetError" << "(" << ")");
  GLenum result = gles2::GetGLContext()->GetError();
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
void GLES2GetFloatv(GLenum pname, GLfloat* params) {
  GPU_CLIENT_LOG("GetFloatv" << "(" << pname << ", " << params << ")");
  gles2::GetGLContext()->GetFloatv(pname, params);
}
void GLES2GetFramebufferAttachmentParameteriv(
    GLenum target, GLenum attachment, GLenum pname, GLint* params) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG(
      "GetFramebufferAttachmentParameteriv" << "(" << target << ", " << attachment << ", " << pname << ", " << params << ")");  // NOLINT
  gles2::GetGLContext()->GetFramebufferAttachmentParameteriv(
      target, attachment, pname, params);
}
void GLES2GetIntegerv(GLenum pname, GLint* params) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("GetIntegerv" << "(" << pname << ", " << params << ")");
  gles2::GetGLContext()->GetIntegerv(pname, params);
}
void GLES2GetProgramiv(GLuint program, GLenum pname, GLint* params) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG(
      "GetProgramiv" << "(" << program << ", " << pname << ", " << params << ")");  // NOLINT
  gles2::GetGLContext()->GetProgramiv(program, pname, params);
}
void GLES2GetProgramInfoLog(
    GLuint program, GLsizei bufsize, GLsizei* length, char* infolog) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLsizei, length);
  GPU_CLIENT_LOG(
      "GetProgramInfoLog" << "(" << program << ", " << bufsize << ", " << length << ", " << infolog << ")");  // NOLINT
  gles2::GetGLContext()->GetProgramInfoLog(program, bufsize, length, infolog);
}
void GLES2GetRenderbufferParameteriv(
    GLenum target, GLenum pname, GLint* params) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG(
      "GetRenderbufferParameteriv" << "(" << target << ", " << pname << ", " << params << ")");  // NOLINT
  gles2::GetGLContext()->GetRenderbufferParameteriv(target, pname, params);
}
void GLES2GetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG(
      "GetShaderiv" << "(" << shader << ", " << pname << ", " << params << ")");
  gles2::GetGLContext()->GetShaderiv(shader, pname, params);
}
void GLES2GetShaderInfoLog(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLsizei, length);
  GPU_CLIENT_LOG(
      "GetShaderInfoLog" << "(" << shader << ", " << bufsize << ", " << length << ", " << infolog << ")");  // NOLINT
  gles2::GetGLContext()->GetShaderInfoLog(shader, bufsize, length, infolog);
}
void GLES2GetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, range);
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, precision);
  GPU_CLIENT_LOG(
      "GetShaderPrecisionFormat" << "(" << shadertype << ", " << precisiontype << ", " << range << ", " << precision << ")");  // NOLINT
  gles2::GetGLContext()->GetShaderPrecisionFormat(
      shadertype, precisiontype, range, precision);
}
void GLES2GetShaderSource(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* source) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLsizei, length);
  GPU_CLIENT_LOG(
      "GetShaderSource" << "(" << shader << ", " << bufsize << ", " << length << ", " << source << ")");  // NOLINT
  gles2::GetGLContext()->GetShaderSource(shader, bufsize, length, source);
}
const GLubyte* GLES2GetString(GLenum name) {
  GPU_CLIENT_LOG("GetString" << "(" << name << ")");
  const GLubyte* result = gles2::GetGLContext()->GetString(name);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
void GLES2GetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
  GPU_CLIENT_LOG(
      "GetTexParameterfv" << "(" << target << ", " << pname << ", " << params << ")");  // NOLINT
  gles2::GetGLContext()->GetTexParameterfv(target, pname, params);
}
void GLES2GetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG(
      "GetTexParameteriv" << "(" << target << ", " << pname << ", " << params << ")");  // NOLINT
  gles2::GetGLContext()->GetTexParameteriv(target, pname, params);
}
void GLES2GetUniformfv(GLuint program, GLint location, GLfloat* params) {
  GPU_CLIENT_LOG(
      "GetUniformfv" << "(" << program << ", " << location << ", " << params << ")");  // NOLINT
  gles2::GetGLContext()->GetUniformfv(program, location, params);
}
void GLES2GetUniformiv(GLuint program, GLint location, GLint* params) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG(
      "GetUniformiv" << "(" << program << ", " << location << ", " << params << ")");  // NOLINT
  gles2::GetGLContext()->GetUniformiv(program, location, params);
}
GLint GLES2GetUniformLocation(GLuint program, const char* name) {
  GPU_CLIENT_LOG(
      "GetUniformLocation" << "(" << program << ", " << name << ")");
  GLint result = gles2::GetGLContext()->GetUniformLocation(program, name);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
void GLES2GetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params) {
  GPU_CLIENT_LOG(
      "GetVertexAttribfv" << "(" << index << ", " << pname << ", " << params << ")");  // NOLINT
  gles2::GetGLContext()->GetVertexAttribfv(index, pname, params);
}
void GLES2GetVertexAttribiv(GLuint index, GLenum pname, GLint* params) {
  GL_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG(
      "GetVertexAttribiv" << "(" << index << ", " << pname << ", " << params << ")");  // NOLINT
  gles2::GetGLContext()->GetVertexAttribiv(index, pname, params);
}
void GLES2GetVertexAttribPointerv(GLuint index, GLenum pname, void** pointer) {
  GPU_CLIENT_LOG(
      "GetVertexAttribPointerv" << "(" << index << ", " << pname << ", " << pointer << ")");  // NOLINT
  gles2::GetGLContext()->GetVertexAttribPointerv(index, pname, pointer);
}
void GLES2Hint(GLenum target, GLenum mode) {
  GPU_CLIENT_LOG("Hint" << "(" << target << ", " << mode << ")");
  gles2::GetGLContext()->Hint(target, mode);
}
GLboolean GLES2IsBuffer(GLuint buffer) {
  GPU_CLIENT_LOG("IsBuffer" << "(" << buffer << ")");
  GLboolean result = gles2::GetGLContext()->IsBuffer(buffer);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
GLboolean GLES2IsEnabled(GLenum cap) {
  GPU_CLIENT_LOG("IsEnabled" << "(" << cap << ")");
  GLboolean result = gles2::GetGLContext()->IsEnabled(cap);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
GLboolean GLES2IsFramebuffer(GLuint framebuffer) {
  GPU_CLIENT_LOG("IsFramebuffer" << "(" << framebuffer << ")");
  GLboolean result = gles2::GetGLContext()->IsFramebuffer(framebuffer);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
GLboolean GLES2IsProgram(GLuint program) {
  GPU_CLIENT_LOG("IsProgram" << "(" << program << ")");
  GLboolean result = gles2::GetGLContext()->IsProgram(program);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
GLboolean GLES2IsRenderbuffer(GLuint renderbuffer) {
  GPU_CLIENT_LOG("IsRenderbuffer" << "(" << renderbuffer << ")");
  GLboolean result = gles2::GetGLContext()->IsRenderbuffer(renderbuffer);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
GLboolean GLES2IsShader(GLuint shader) {
  GPU_CLIENT_LOG("IsShader" << "(" << shader << ")");
  GLboolean result = gles2::GetGLContext()->IsShader(shader);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
GLboolean GLES2IsTexture(GLuint texture) {
  GPU_CLIENT_LOG("IsTexture" << "(" << texture << ")");
  GLboolean result = gles2::GetGLContext()->IsTexture(texture);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
void GLES2LineWidth(GLfloat width) {
  GPU_CLIENT_LOG("LineWidth" << "(" << width << ")");
  gles2::GetGLContext()->LineWidth(width);
}
void GLES2LinkProgram(GLuint program) {
  GPU_CLIENT_LOG("LinkProgram" << "(" << program << ")");
  gles2::GetGLContext()->LinkProgram(program);
}
void GLES2PixelStorei(GLenum pname, GLint param) {
  GPU_CLIENT_LOG("PixelStorei" << "(" << pname << ", " << param << ")");
  gles2::GetGLContext()->PixelStorei(pname, param);
}
void GLES2PolygonOffset(GLfloat factor, GLfloat units) {
  GPU_CLIENT_LOG("PolygonOffset" << "(" << factor << ", " << units << ")");
  gles2::GetGLContext()->PolygonOffset(factor, units);
}
void GLES2ReadPixels(
    GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
    void* pixels) {
  GPU_CLIENT_LOG(
      "ReadPixels" << "(" << x << ", " << y << ", " << width << ", " << height << ", " << format << ", " << type << ", " << pixels << ")");  // NOLINT
  gles2::GetGLContext()->ReadPixels(x, y, width, height, format, type, pixels);
}
void GLES2ReleaseShaderCompiler() {
  GPU_CLIENT_LOG("ReleaseShaderCompiler" << "(" << ")");
  gles2::GetGLContext()->ReleaseShaderCompiler();
}
void GLES2RenderbufferStorage(
    GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  GPU_CLIENT_LOG(
      "RenderbufferStorage" << "(" << target << ", " << internalformat << ", " << width << ", " << height << ")");  // NOLINT
  gles2::GetGLContext()->RenderbufferStorage(
      target, internalformat, width, height);
}
void GLES2SampleCoverage(GLclampf value, GLboolean invert) {
  GPU_CLIENT_LOG("SampleCoverage" << "(" << value << ", " << invert << ")");
  gles2::GetGLContext()->SampleCoverage(value, invert);
}
void GLES2Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  GPU_CLIENT_LOG(
      "Scissor" << "(" << x << ", " << y << ", " << width << ", " << height << ")");  // NOLINT
  gles2::GetGLContext()->Scissor(x, y, width, height);
}
void GLES2ShaderBinary(
    GLsizei n, const GLuint* shaders, GLenum binaryformat, const void* binary,
    GLsizei length) {
  GPU_CLIENT_LOG(
      "ShaderBinary" << "(" << n << ", " << shaders << ", " << binaryformat << ", " << binary << ", " << length << ")");  // NOLINT
  gles2::GetGLContext()->ShaderBinary(
      n, shaders, binaryformat, binary, length);
}
void GLES2ShaderSource(
    GLuint shader, GLsizei count, const char** str, const GLint* length) {
  GPU_CLIENT_LOG(
      "ShaderSource" << "(" << shader << ", " << count << ", " << str << ", " << length << ")");  // NOLINT
  gles2::GetGLContext()->ShaderSource(shader, count, str, length);
}
void GLES2StencilFunc(GLenum func, GLint ref, GLuint mask) {
  GPU_CLIENT_LOG(
      "StencilFunc" << "(" << func << ", " << ref << ", " << mask << ")");
  gles2::GetGLContext()->StencilFunc(func, ref, mask);
}
void GLES2StencilFuncSeparate(
    GLenum face, GLenum func, GLint ref, GLuint mask) {
  GPU_CLIENT_LOG(
      "StencilFuncSeparate" << "(" << face << ", " << func << ", " << ref << ", " << mask << ")");  // NOLINT
  gles2::GetGLContext()->StencilFuncSeparate(face, func, ref, mask);
}
void GLES2StencilMask(GLuint mask) {
  GPU_CLIENT_LOG("StencilMask" << "(" << mask << ")");
  gles2::GetGLContext()->StencilMask(mask);
}
void GLES2StencilMaskSeparate(GLenum face, GLuint mask) {
  GPU_CLIENT_LOG("StencilMaskSeparate" << "(" << face << ", " << mask << ")");
  gles2::GetGLContext()->StencilMaskSeparate(face, mask);
}
void GLES2StencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  GPU_CLIENT_LOG(
      "StencilOp" << "(" << fail << ", " << zfail << ", " << zpass << ")");
  gles2::GetGLContext()->StencilOp(fail, zfail, zpass);
}
void GLES2StencilOpSeparate(
    GLenum face, GLenum fail, GLenum zfail, GLenum zpass) {
  GPU_CLIENT_LOG(
      "StencilOpSeparate" << "(" << face << ", " << fail << ", " << zfail << ", " << zpass << ")");  // NOLINT
  gles2::GetGLContext()->StencilOpSeparate(face, fail, zfail, zpass);
}
void GLES2TexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  GPU_CLIENT_LOG(
      "TexImage2D" << "(" << target << ", " << level << ", " << internalformat << ", " << width << ", " << height << ", " << border << ", " << format << ", " << type << ", " << pixels << ")");  // NOLINT
  gles2::GetGLContext()->TexImage2D(
      target, level, internalformat, width, height, border, format, type,
      pixels);
}
void GLES2TexParameterf(GLenum target, GLenum pname, GLfloat param) {
  GPU_CLIENT_LOG(
      "TexParameterf" << "(" << target << ", " << pname << ", " << param << ")");  // NOLINT
  gles2::GetGLContext()->TexParameterf(target, pname, param);
}
void GLES2TexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
  GPU_CLIENT_LOG(
      "TexParameterfv" << "(" << target << ", " << pname << ", " << params << ")");  // NOLINT
  gles2::GetGLContext()->TexParameterfv(target, pname, params);
}
void GLES2TexParameteri(GLenum target, GLenum pname, GLint param) {
  GPU_CLIENT_LOG(
      "TexParameteri" << "(" << target << ", " << pname << ", " << param << ")");  // NOLINT
  gles2::GetGLContext()->TexParameteri(target, pname, param);
}
void GLES2TexParameteriv(GLenum target, GLenum pname, const GLint* params) {
  GPU_CLIENT_LOG(
      "TexParameteriv" << "(" << target << ", " << pname << ", " << params << ")");  // NOLINT
  gles2::GetGLContext()->TexParameteriv(target, pname, params);
}
void GLES2TexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels) {
  GPU_CLIENT_LOG(
      "TexSubImage2D" << "(" << target << ", " << level << ", " << xoffset << ", " << yoffset << ", " << width << ", " << height << ", " << format << ", " << type << ", " << pixels << ")");  // NOLINT
  gles2::GetGLContext()->TexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
}
void GLES2Uniform1f(GLint location, GLfloat x) {
  GPU_CLIENT_LOG("Uniform1f" << "(" << location << ", " << x << ")");
  gles2::GetGLContext()->Uniform1f(location, x);
}
void GLES2Uniform1fv(GLint location, GLsizei count, const GLfloat* v) {
  GPU_CLIENT_LOG(
      "Uniform1fv" << "(" << location << ", " << count << ", " << v << ")");
  gles2::GetGLContext()->Uniform1fv(location, count, v);
}
void GLES2Uniform1i(GLint location, GLint x) {
  GPU_CLIENT_LOG("Uniform1i" << "(" << location << ", " << x << ")");
  gles2::GetGLContext()->Uniform1i(location, x);
}
void GLES2Uniform1iv(GLint location, GLsizei count, const GLint* v) {
  GPU_CLIENT_LOG(
      "Uniform1iv" << "(" << location << ", " << count << ", " << v << ")");
  gles2::GetGLContext()->Uniform1iv(location, count, v);
}
void GLES2Uniform2f(GLint location, GLfloat x, GLfloat y) {
  GPU_CLIENT_LOG(
      "Uniform2f" << "(" << location << ", " << x << ", " << y << ")");
  gles2::GetGLContext()->Uniform2f(location, x, y);
}
void GLES2Uniform2fv(GLint location, GLsizei count, const GLfloat* v) {
  GPU_CLIENT_LOG(
      "Uniform2fv" << "(" << location << ", " << count << ", " << v << ")");
  gles2::GetGLContext()->Uniform2fv(location, count, v);
}
void GLES2Uniform2i(GLint location, GLint x, GLint y) {
  GPU_CLIENT_LOG(
      "Uniform2i" << "(" << location << ", " << x << ", " << y << ")");
  gles2::GetGLContext()->Uniform2i(location, x, y);
}
void GLES2Uniform2iv(GLint location, GLsizei count, const GLint* v) {
  GPU_CLIENT_LOG(
      "Uniform2iv" << "(" << location << ", " << count << ", " << v << ")");
  gles2::GetGLContext()->Uniform2iv(location, count, v);
}
void GLES2Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  GPU_CLIENT_LOG(
      "Uniform3f" << "(" << location << ", " << x << ", " << y << ", " << z << ")");  // NOLINT
  gles2::GetGLContext()->Uniform3f(location, x, y, z);
}
void GLES2Uniform3fv(GLint location, GLsizei count, const GLfloat* v) {
  GPU_CLIENT_LOG(
      "Uniform3fv" << "(" << location << ", " << count << ", " << v << ")");
  gles2::GetGLContext()->Uniform3fv(location, count, v);
}
void GLES2Uniform3i(GLint location, GLint x, GLint y, GLint z) {
  GPU_CLIENT_LOG(
      "Uniform3i" << "(" << location << ", " << x << ", " << y << ", " << z << ")");  // NOLINT
  gles2::GetGLContext()->Uniform3i(location, x, y, z);
}
void GLES2Uniform3iv(GLint location, GLsizei count, const GLint* v) {
  GPU_CLIENT_LOG(
      "Uniform3iv" << "(" << location << ", " << count << ", " << v << ")");
  gles2::GetGLContext()->Uniform3iv(location, count, v);
}
void GLES2Uniform4f(
    GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  GPU_CLIENT_LOG(
      "Uniform4f" << "(" << location << ", " << x << ", " << y << ", " << z << ", " << w << ")");  // NOLINT
  gles2::GetGLContext()->Uniform4f(location, x, y, z, w);
}
void GLES2Uniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  GPU_CLIENT_LOG(
      "Uniform4fv" << "(" << location << ", " << count << ", " << v << ")");
  gles2::GetGLContext()->Uniform4fv(location, count, v);
}
void GLES2Uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
  GPU_CLIENT_LOG(
      "Uniform4i" << "(" << location << ", " << x << ", " << y << ", " << z << ", " << w << ")");  // NOLINT
  gles2::GetGLContext()->Uniform4i(location, x, y, z, w);
}
void GLES2Uniform4iv(GLint location, GLsizei count, const GLint* v) {
  GPU_CLIENT_LOG(
      "Uniform4iv" << "(" << location << ", " << count << ", " << v << ")");
  gles2::GetGLContext()->Uniform4iv(location, count, v);
}
void GLES2UniformMatrix2fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  GPU_CLIENT_LOG(
      "UniformMatrix2fv" << "(" << location << ", " << count << ", " << transpose << ", " << value << ")");  // NOLINT
  gles2::GetGLContext()->UniformMatrix2fv(location, count, transpose, value);
}
void GLES2UniformMatrix3fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  GPU_CLIENT_LOG(
      "UniformMatrix3fv" << "(" << location << ", " << count << ", " << transpose << ", " << value << ")");  // NOLINT
  gles2::GetGLContext()->UniformMatrix3fv(location, count, transpose, value);
}
void GLES2UniformMatrix4fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  GPU_CLIENT_LOG(
      "UniformMatrix4fv" << "(" << location << ", " << count << ", " << transpose << ", " << value << ")");  // NOLINT
  gles2::GetGLContext()->UniformMatrix4fv(location, count, transpose, value);
}
void GLES2UseProgram(GLuint program) {
  GPU_CLIENT_LOG("UseProgram" << "(" << program << ")");
  gles2::GetGLContext()->UseProgram(program);
}
void GLES2ValidateProgram(GLuint program) {
  GPU_CLIENT_LOG("ValidateProgram" << "(" << program << ")");
  gles2::GetGLContext()->ValidateProgram(program);
}
void GLES2VertexAttrib1f(GLuint indx, GLfloat x) {
  GPU_CLIENT_LOG("VertexAttrib1f" << "(" << indx << ", " << x << ")");
  gles2::GetGLContext()->VertexAttrib1f(indx, x);
}
void GLES2VertexAttrib1fv(GLuint indx, const GLfloat* values) {
  GPU_CLIENT_LOG("VertexAttrib1fv" << "(" << indx << ", " << values << ")");
  gles2::GetGLContext()->VertexAttrib1fv(indx, values);
}
void GLES2VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
  GPU_CLIENT_LOG(
      "VertexAttrib2f" << "(" << indx << ", " << x << ", " << y << ")");
  gles2::GetGLContext()->VertexAttrib2f(indx, x, y);
}
void GLES2VertexAttrib2fv(GLuint indx, const GLfloat* values) {
  GPU_CLIENT_LOG("VertexAttrib2fv" << "(" << indx << ", " << values << ")");
  gles2::GetGLContext()->VertexAttrib2fv(indx, values);
}
void GLES2VertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  GPU_CLIENT_LOG(
      "VertexAttrib3f" << "(" << indx << ", " << x << ", " << y << ", " << z << ")");  // NOLINT
  gles2::GetGLContext()->VertexAttrib3f(indx, x, y, z);
}
void GLES2VertexAttrib3fv(GLuint indx, const GLfloat* values) {
  GPU_CLIENT_LOG("VertexAttrib3fv" << "(" << indx << ", " << values << ")");
  gles2::GetGLContext()->VertexAttrib3fv(indx, values);
}
void GLES2VertexAttrib4f(
    GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  GPU_CLIENT_LOG(
      "VertexAttrib4f" << "(" << indx << ", " << x << ", " << y << ", " << z << ", " << w << ")");  // NOLINT
  gles2::GetGLContext()->VertexAttrib4f(indx, x, y, z, w);
}
void GLES2VertexAttrib4fv(GLuint indx, const GLfloat* values) {
  GPU_CLIENT_LOG("VertexAttrib4fv" << "(" << indx << ", " << values << ")");
  gles2::GetGLContext()->VertexAttrib4fv(indx, values);
}
void GLES2VertexAttribPointer(
    GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void* ptr) {
  GPU_CLIENT_LOG(
      "VertexAttribPointer" << "(" << indx << ", " << size << ", " << type << ", " << normalized << ", " << stride << ", " << ptr << ")");  // NOLINT
  gles2::GetGLContext()->VertexAttribPointer(
      indx, size, type, normalized, stride, ptr);
}
void GLES2Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  GPU_CLIENT_LOG(
      "Viewport" << "(" << x << ", " << y << ", " << width << ", " << height << ")");  // NOLINT
  gles2::GetGLContext()->Viewport(x, y, width, height);
}
void GLES2BlitFramebufferEXT(
    GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
    GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
  GPU_CLIENT_LOG(
      "BlitFramebufferEXT" << "(" << srcX0 << ", " << srcY0 << ", " << srcX1 << ", " << srcY1 << ", " << dstX0 << ", " << dstY0 << ", " << dstX1 << ", " << dstY1 << ", " << mask << ", " << filter << ")");  // NOLINT
  gles2::GetGLContext()->BlitFramebufferEXT(
      srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}
void GLES2RenderbufferStorageMultisampleEXT(
    GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
    GLsizei height) {
  GPU_CLIENT_LOG(
      "RenderbufferStorageMultisampleEXT" << "(" << target << ", " << samples << ", " << internalformat << ", " << width << ", " << height << ")");  // NOLINT
  gles2::GetGLContext()->RenderbufferStorageMultisampleEXT(
      target, samples, internalformat, width, height);
}
void GLES2SwapBuffers() {
  GPU_CLIENT_LOG("SwapBuffers" << "(" << ")");
  gles2::GetGLContext()->SwapBuffers();
}
GLuint GLES2GetMaxValueInBufferCHROMIUM(
    GLuint buffer_id, GLsizei count, GLenum type, GLuint offset) {
  GPU_CLIENT_LOG(
      "GetMaxValueInBufferCHROMIUM" << "(" << buffer_id << ", " << count << ", " << type << ", " << offset << ")");  // NOLINT
  GLuint result =
      gles2::GetGLContext()->GetMaxValueInBufferCHROMIUM(
          buffer_id, count, type, offset);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
void GLES2GenSharedIdsCHROMIUM(
    GLuint namespace_id, GLuint id_offset, GLsizei n, GLuint* ids) {
  GPU_CLIENT_LOG(
      "GenSharedIdsCHROMIUM" << "(" << namespace_id << ", " << id_offset << ", " << n << ", " << ids << ")");  // NOLINT
  gles2::GetGLContext()->GenSharedIdsCHROMIUM(namespace_id, id_offset, n, ids);
}
void GLES2DeleteSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids) {
  GPU_CLIENT_LOG(
      "DeleteSharedIdsCHROMIUM" << "(" << namespace_id << ", " << n << ", " << ids << ")");  // NOLINT
  gles2::GetGLContext()->DeleteSharedIdsCHROMIUM(namespace_id, n, ids);
}
void GLES2RegisterSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids) {
  GPU_CLIENT_LOG(
      "RegisterSharedIdsCHROMIUM" << "(" << namespace_id << ", " << n << ", " << ids << ")");  // NOLINT
  gles2::GetGLContext()->RegisterSharedIdsCHROMIUM(namespace_id, n, ids);
}
GLboolean GLES2CommandBufferEnableCHROMIUM(const char* feature) {
  GPU_CLIENT_LOG("CommandBufferEnableCHROMIUM" << "(" << feature << ")");
  GLboolean result =
      gles2::GetGLContext()->CommandBufferEnableCHROMIUM(feature);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
void* GLES2MapBufferSubDataCHROMIUM(
    GLuint target, GLintptr offset, GLsizeiptr size, GLenum access) {
  GPU_CLIENT_LOG(
      "MapBufferSubDataCHROMIUM" << "(" << target << ", " << offset << ", " << size << ", " << access << ")");  // NOLINT
  void* result =
      gles2::GetGLContext()->MapBufferSubDataCHROMIUM(
          target, offset, size, access);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
void GLES2UnmapBufferSubDataCHROMIUM(const void* mem) {
  GPU_CLIENT_LOG("UnmapBufferSubDataCHROMIUM" << "(" << mem << ")");
  gles2::GetGLContext()->UnmapBufferSubDataCHROMIUM(mem);
}
void* GLES2MapTexSubImage2DCHROMIUM(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, GLenum access) {
  GPU_CLIENT_LOG(
      "MapTexSubImage2DCHROMIUM" << "(" << target << ", " << level << ", " << xoffset << ", " << yoffset << ", " << width << ", " << height << ", " << format << ", " << type << ", " << access << ")");  // NOLINT
  void* result =
      gles2::GetGLContext()->MapTexSubImage2DCHROMIUM(
          target, level, xoffset, yoffset, width, height, format, type,
          access);
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
void GLES2UnmapTexSubImage2DCHROMIUM(const void* mem) {
  GPU_CLIENT_LOG("UnmapTexSubImage2DCHROMIUM" << "(" << mem << ")");
  gles2::GetGLContext()->UnmapTexSubImage2DCHROMIUM(mem);
}
void GLES2CopyTextureToParentTextureCHROMIUM(
    GLuint client_child_id, GLuint client_parent_id) {
  GPU_CLIENT_LOG(
      "CopyTextureToParentTextureCHROMIUM" << "(" << client_child_id << ", " << client_parent_id << ")");  // NOLINT
  gles2::GetGLContext()->CopyTextureToParentTextureCHROMIUM(
      client_child_id, client_parent_id);
}
void GLES2ResizeCHROMIUM(GLuint width, GLuint height) {
  GPU_CLIENT_LOG("ResizeCHROMIUM" << "(" << width << ", " << height << ")");
  gles2::GetGLContext()->ResizeCHROMIUM(width, height);
}
const GLchar* GLES2GetRequestableExtensionsCHROMIUM() {
  GPU_CLIENT_LOG("GetRequestableExtensionsCHROMIUM" << "(" << ")");
  const GLchar* result =
      gles2::GetGLContext()->GetRequestableExtensionsCHROMIUM();
  GPU_CLIENT_LOG("return:" << result)
  return result;
}
void GLES2RequestExtensionCHROMIUM(const char* extension) {
  GPU_CLIENT_LOG("RequestExtensionCHROMIUM" << "(" << extension << ")");
  gles2::GetGLContext()->RequestExtensionCHROMIUM(extension);
}

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_C_LIB_AUTOGEN_H_

