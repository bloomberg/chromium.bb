// This file is auto-generated. DO NOT EDIT!

// It is included by gles2_cmd_decoder.cc

parse_error::ParseError GLES2DecoderImpl::HandleActiveTexture(
    uint32 immediate_data_size, const gles2::ActiveTexture& c) {
  GLenum texture = static_cast<GLenum>(c.texture);
  parse_error::ParseError result =
      ValidateActiveTexture(this, immediate_data_size, texture);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glActiveTexture(texture);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleAttachShader(
    uint32 immediate_data_size, const gles2::AttachShader& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLuint shader;
  if (!id_map_.GetServiceId(c.shader, &shader)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  parse_error::ParseError result =
      ValidateAttachShader(this, immediate_data_size, program, shader);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glAttachShader(program, shader);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBindAttribLocation(
    uint32 immediate_data_size, const gles2::BindAttribLocation& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLuint index = static_cast<GLuint>(c.index);
  uint32 name_size = c.data_size;
  const char* name = GetSharedMemoryAs<const char*>(
      c.name_shm_id, c.name_shm_offset, name_size);
  parse_error::ParseError result =
      ValidateBindAttribLocation(
          this, immediate_data_size, program, index, name);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  String name_str(name, name_size);
  glBindAttribLocation(program, index, name_str.c_str());
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBindAttribLocationImmediate(
    uint32 immediate_data_size, const gles2::BindAttribLocationImmediate& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLuint index = static_cast<GLuint>(c.index);
  uint32 name_size = c.data_size;
  const char* name = GetImmediateDataAs<const char*>(c);
  // TODO(gman): Make sure validate checks
  //     immediate_data_size covers data_size.
  parse_error::ParseError result =
      ValidateBindAttribLocationImmediate(
          this, immediate_data_size, program, index, name);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  String name_str(name, name_size);
  glBindAttribLocation(program, index, name_str.c_str());
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBindBuffer(
    uint32 immediate_data_size, const gles2::BindBuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint buffer;
  if (!id_map_.GetServiceId(c.buffer, &buffer)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  parse_error::ParseError result =
      ValidateBindBuffer(this, immediate_data_size, target, buffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DoBindBuffer(target, buffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBindFramebuffer(
    uint32 immediate_data_size, const gles2::BindFramebuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint framebuffer;
  if (!id_map_.GetServiceId(c.framebuffer, &framebuffer)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  parse_error::ParseError result =
      ValidateBindFramebuffer(this, immediate_data_size, target, framebuffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBindFramebufferEXT(target, framebuffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBindRenderbuffer(
    uint32 immediate_data_size, const gles2::BindRenderbuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint renderbuffer;
  if (!id_map_.GetServiceId(c.renderbuffer, &renderbuffer)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  parse_error::ParseError result =
      ValidateBindRenderbuffer(
          this, immediate_data_size, target, renderbuffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBindRenderbufferEXT(target, renderbuffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBindTexture(
    uint32 immediate_data_size, const gles2::BindTexture& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint texture;
  if (!id_map_.GetServiceId(c.texture, &texture)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  parse_error::ParseError result =
      ValidateBindTexture(this, immediate_data_size, target, texture);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBindTexture(target, texture);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBlendColor(
    uint32 immediate_data_size, const gles2::BlendColor& c) {
  GLclampf red = static_cast<GLclampf>(c.red);
  GLclampf green = static_cast<GLclampf>(c.green);
  GLclampf blue = static_cast<GLclampf>(c.blue);
  GLclampf alpha = static_cast<GLclampf>(c.alpha);
  parse_error::ParseError result =
      ValidateBlendColor(this, immediate_data_size, red, green, blue, alpha);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBlendColor(red, green, blue, alpha);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBlendEquation(
    uint32 immediate_data_size, const gles2::BlendEquation& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  parse_error::ParseError result =
      ValidateBlendEquation(this, immediate_data_size, mode);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBlendEquation(mode);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBlendEquationSeparate(
    uint32 immediate_data_size, const gles2::BlendEquationSeparate& c) {
  GLenum modeRGB = static_cast<GLenum>(c.modeRGB);
  GLenum modeAlpha = static_cast<GLenum>(c.modeAlpha);
  parse_error::ParseError result =
      ValidateBlendEquationSeparate(
          this, immediate_data_size, modeRGB, modeAlpha);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBlendEquationSeparate(modeRGB, modeAlpha);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBlendFunc(
    uint32 immediate_data_size, const gles2::BlendFunc& c) {
  GLenum sfactor = static_cast<GLenum>(c.sfactor);
  GLenum dfactor = static_cast<GLenum>(c.dfactor);
  parse_error::ParseError result =
      ValidateBlendFunc(this, immediate_data_size, sfactor, dfactor);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBlendFunc(sfactor, dfactor);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBlendFuncSeparate(
    uint32 immediate_data_size, const gles2::BlendFuncSeparate& c) {
  GLenum srcRGB = static_cast<GLenum>(c.srcRGB);
  GLenum dstRGB = static_cast<GLenum>(c.dstRGB);
  GLenum srcAlpha = static_cast<GLenum>(c.srcAlpha);
  GLenum dstAlpha = static_cast<GLenum>(c.dstAlpha);
  parse_error::ParseError result =
      ValidateBlendFuncSeparate(
          this, immediate_data_size, srcRGB, dstRGB, srcAlpha, dstAlpha);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBufferSubData(
    uint32 immediate_data_size, const gles2::BufferSubData& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32 data_shm_id = static_cast<uint32>(c.data_shm_id);
  uint32 data_shm_offset = static_cast<uint32>(c.data_shm_offset);
  uint32 data_size = size;
  const void* data = GetSharedMemoryAs<const void*>(
      data_shm_id, data_shm_offset, data_size);
  parse_error::ParseError result =
      ValidateBufferSubData(
          this, immediate_data_size, target, offset, size, data);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBufferSubData(target, offset, size, data);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBufferSubDataImmediate(
    uint32 immediate_data_size, const gles2::BufferSubDataImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  const void* data = GetImmediateDataAs<const void*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateBufferSubDataImmediate(
          this, immediate_data_size, target, offset, size, data);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBufferSubData(target, offset, size, data);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleCheckFramebufferStatus(
    uint32 immediate_data_size, const gles2::CheckFramebufferStatus& c) {
  GLenum target = static_cast<GLenum>(c.target);
  parse_error::ParseError result =
      ValidateCheckFramebufferStatus(this, immediate_data_size, target);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCheckFramebufferStatusEXT(target);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleClear(
    uint32 immediate_data_size, const gles2::Clear& c) {
  GLbitfield mask = static_cast<GLbitfield>(c.mask);
  parse_error::ParseError result =
      ValidateClear(this, immediate_data_size, mask);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glClear(mask);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleClearColor(
    uint32 immediate_data_size, const gles2::ClearColor& c) {
  GLclampf red = static_cast<GLclampf>(c.red);
  GLclampf green = static_cast<GLclampf>(c.green);
  GLclampf blue = static_cast<GLclampf>(c.blue);
  GLclampf alpha = static_cast<GLclampf>(c.alpha);
  parse_error::ParseError result =
      ValidateClearColor(this, immediate_data_size, red, green, blue, alpha);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glClearColor(red, green, blue, alpha);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleClearDepthf(
    uint32 immediate_data_size, const gles2::ClearDepthf& c) {
  GLclampf depth = static_cast<GLclampf>(c.depth);
  parse_error::ParseError result =
      ValidateClearDepthf(this, immediate_data_size, depth);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glClearDepth(depth);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleClearStencil(
    uint32 immediate_data_size, const gles2::ClearStencil& c) {
  GLint s = static_cast<GLint>(c.s);
  parse_error::ParseError result =
      ValidateClearStencil(this, immediate_data_size, s);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glClearStencil(s);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleColorMask(
    uint32 immediate_data_size, const gles2::ColorMask& c) {
  GLboolean red = static_cast<GLboolean>(c.red);
  GLboolean green = static_cast<GLboolean>(c.green);
  GLboolean blue = static_cast<GLboolean>(c.blue);
  GLboolean alpha = static_cast<GLboolean>(c.alpha);
  parse_error::ParseError result =
      ValidateColorMask(this, immediate_data_size, red, green, blue, alpha);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glColorMask(red, green, blue, alpha);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleCompileShader(
    uint32 immediate_data_size, const gles2::CompileShader& c) {
  GLuint shader;
  if (!id_map_.GetServiceId(c.shader, &shader)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  parse_error::ParseError result =
      ValidateCompileShader(this, immediate_data_size, shader);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCompileShader(shader);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleCompressedTexSubImage2D(
    uint32 immediate_data_size, const gles2::CompressedTexSubImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLsizei imageSize = static_cast<GLsizei>(c.imageSize);
  uint32 data_shm_id = static_cast<uint32>(c.data_shm_id);
  uint32 data_shm_offset = static_cast<uint32>(c.data_shm_offset);
  uint32 data_size = imageSize;
  const void* data = GetSharedMemoryAs<const void*>(
      data_shm_id, data_shm_offset, data_size);
  parse_error::ParseError result =
      ValidateCompressedTexSubImage2D(
          this, immediate_data_size, target, level, xoffset, yoffset, width,
          height, format, imageSize, data);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleCompressedTexSubImage2DImmediate(
    
    uint32 immediate_data_size,
    const gles2::CompressedTexSubImage2DImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLsizei imageSize = static_cast<GLsizei>(c.imageSize);
  const void* data = GetImmediateDataAs<const void*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateCompressedTexSubImage2DImmediate(
          this, immediate_data_size, target, level, xoffset, yoffset, width,
          height, format, imageSize, data);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleCopyTexImage2D(
    uint32 immediate_data_size, const gles2::CopyTexImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  parse_error::ParseError result =
      ValidateCopyTexImage2D(
          this, immediate_data_size, target, level, internalformat, x, y, width,
          height, border);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleCopyTexSubImage2D(
    uint32 immediate_data_size, const gles2::CopyTexSubImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  parse_error::ParseError result =
      ValidateCopyTexSubImage2D(
          this, immediate_data_size, target, level, xoffset, yoffset, x, y,
          width, height);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleCreateProgram(
    uint32 immediate_data_size, const gles2::CreateProgram& c) {
  uint32 client_id = c.client_id;
  parse_error::ParseError result =
      ValidateCreateProgram(this, immediate_data_size);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  CreateProgramHelper(client_id);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleCreateShader(
    uint32 immediate_data_size, const gles2::CreateShader& c) {
  GLenum type = static_cast<GLenum>(c.type);
  uint32 client_id = c.client_id;
  parse_error::ParseError result =
      ValidateCreateShader(this, immediate_data_size, type);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  CreateShaderHelper(type, client_id);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleCullFace(
    uint32 immediate_data_size, const gles2::CullFace& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  parse_error::ParseError result =
      ValidateCullFace(this, immediate_data_size, mode);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCullFace(mode);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDeleteBuffers(
    uint32 immediate_data_size, const gles2::DeleteBuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* buffers = GetSharedMemoryAs<const GLuint*>(
      c.buffers_shm_id, c.buffers_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateDeleteBuffers(this, immediate_data_size, n, buffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteBuffersHelper>(n, buffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDeleteBuffersImmediate(
    uint32 immediate_data_size, const gles2::DeleteBuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* buffers = GetImmediateDataAs<const GLuint*>(c);
  parse_error::ParseError result =
      ValidateDeleteBuffersImmediate(this, immediate_data_size, n, buffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteBuffersHelper>(n, buffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDeleteFramebuffers(
    uint32 immediate_data_size, const gles2::DeleteFramebuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* framebuffers = GetSharedMemoryAs<const GLuint*>(
      c.framebuffers_shm_id, c.framebuffers_shm_offset, 0 /* TODO(
          gman): size */);
  parse_error::ParseError result =
      ValidateDeleteFramebuffers(this, immediate_data_size, n, framebuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteFramebuffersHelper>(n, framebuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDeleteFramebuffersImmediate(
    uint32 immediate_data_size, const gles2::DeleteFramebuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* framebuffers = GetImmediateDataAs<const GLuint*>(c);
  parse_error::ParseError result =
      ValidateDeleteFramebuffersImmediate(
          this, immediate_data_size, n, framebuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteFramebuffersHelper>(n, framebuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDeleteProgram(
    uint32 immediate_data_size, const gles2::DeleteProgram& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  parse_error::ParseError result =
      ValidateDeleteProgram(this, immediate_data_size, program);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DoDeleteProgram(program);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDeleteRenderbuffers(
    uint32 immediate_data_size, const gles2::DeleteRenderbuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* renderbuffers = GetSharedMemoryAs<const GLuint*>(
      c.renderbuffers_shm_id, c.renderbuffers_shm_offset, 0 /* TODO(
          gman): size */);
  parse_error::ParseError result =
      ValidateDeleteRenderbuffers(this, immediate_data_size, n, renderbuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteRenderbuffersHelper>(n, renderbuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDeleteRenderbuffersImmediate(
    uint32 immediate_data_size, const gles2::DeleteRenderbuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* renderbuffers = GetImmediateDataAs<const GLuint*>(c);
  parse_error::ParseError result =
      ValidateDeleteRenderbuffersImmediate(
          this, immediate_data_size, n, renderbuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteRenderbuffersHelper>(n, renderbuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDeleteShader(
    uint32 immediate_data_size, const gles2::DeleteShader& c) {
  GLuint shader;
  if (!id_map_.GetServiceId(c.shader, &shader)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  parse_error::ParseError result =
      ValidateDeleteShader(this, immediate_data_size, shader);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DoDeleteShader(shader);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDeleteTextures(
    uint32 immediate_data_size, const gles2::DeleteTextures& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* textures = GetSharedMemoryAs<const GLuint*>(
      c.textures_shm_id, c.textures_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateDeleteTextures(this, immediate_data_size, n, textures);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteTexturesHelper>(n, textures);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDeleteTexturesImmediate(
    uint32 immediate_data_size, const gles2::DeleteTexturesImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  const GLuint* textures = GetImmediateDataAs<const GLuint*>(c);
  parse_error::ParseError result =
      ValidateDeleteTexturesImmediate(this, immediate_data_size, n, textures);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DeleteGLObjects<GLDeleteTexturesHelper>(n, textures);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDepthFunc(
    uint32 immediate_data_size, const gles2::DepthFunc& c) {
  GLenum func = static_cast<GLenum>(c.func);
  parse_error::ParseError result =
      ValidateDepthFunc(this, immediate_data_size, func);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDepthFunc(func);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDepthMask(
    uint32 immediate_data_size, const gles2::DepthMask& c) {
  GLboolean flag = static_cast<GLboolean>(c.flag);
  parse_error::ParseError result =
      ValidateDepthMask(this, immediate_data_size, flag);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDepthMask(flag);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDepthRangef(
    uint32 immediate_data_size, const gles2::DepthRangef& c) {
  GLclampf zNear = static_cast<GLclampf>(c.zNear);
  GLclampf zFar = static_cast<GLclampf>(c.zFar);
  parse_error::ParseError result =
      ValidateDepthRangef(this, immediate_data_size, zNear, zFar);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDepthRange(zNear, zFar);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDetachShader(
    uint32 immediate_data_size, const gles2::DetachShader& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLuint shader;
  if (!id_map_.GetServiceId(c.shader, &shader)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  parse_error::ParseError result =
      ValidateDetachShader(this, immediate_data_size, program, shader);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDetachShader(program, shader);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDisable(
    uint32 immediate_data_size, const gles2::Disable& c) {
  GLenum cap = static_cast<GLenum>(c.cap);
  parse_error::ParseError result =
      ValidateDisable(this, immediate_data_size, cap);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDisable(cap);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDisableVertexAttribArray(
    uint32 immediate_data_size, const gles2::DisableVertexAttribArray& c) {
  GLuint index = static_cast<GLuint>(c.index);
  parse_error::ParseError result =
      ValidateDisableVertexAttribArray(this, immediate_data_size, index);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDisableVertexAttribArray(index);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleDrawArrays(
    uint32 immediate_data_size, const gles2::DrawArrays& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  GLint first = static_cast<GLint>(c.first);
  GLsizei count = static_cast<GLsizei>(c.count);
  parse_error::ParseError result =
      ValidateDrawArrays(this, immediate_data_size, mode, first, count);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glDrawArrays(mode, first, count);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleEnable(
    uint32 immediate_data_size, const gles2::Enable& c) {
  GLenum cap = static_cast<GLenum>(c.cap);
  parse_error::ParseError result =
      ValidateEnable(this, immediate_data_size, cap);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glEnable(cap);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleEnableVertexAttribArray(
    uint32 immediate_data_size, const gles2::EnableVertexAttribArray& c) {
  GLuint index = static_cast<GLuint>(c.index);
  parse_error::ParseError result =
      ValidateEnableVertexAttribArray(this, immediate_data_size, index);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glEnableVertexAttribArray(index);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleFinish(
    uint32 immediate_data_size, const gles2::Finish& c) {
  parse_error::ParseError result =
      ValidateFinish(this, immediate_data_size);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glFinish();
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleFlush(
    uint32 immediate_data_size, const gles2::Flush& c) {
  parse_error::ParseError result =
      ValidateFlush(this, immediate_data_size);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glFlush();
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleFramebufferRenderbuffer(
    uint32 immediate_data_size, const gles2::FramebufferRenderbuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum renderbuffertarget = static_cast<GLenum>(c.renderbuffertarget);
  GLuint renderbuffer;
  if (!id_map_.GetServiceId(c.renderbuffer, &renderbuffer)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  parse_error::ParseError result =
      ValidateFramebufferRenderbuffer(
          this, immediate_data_size, target, attachment, renderbuffertarget,
          renderbuffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glFramebufferRenderbufferEXT(
      target, attachment, renderbuffertarget, renderbuffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleFramebufferTexture2D(
    uint32 immediate_data_size, const gles2::FramebufferTexture2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum textarget = static_cast<GLenum>(c.textarget);
  GLuint texture;
  if (!id_map_.GetServiceId(c.texture, &texture)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLint level = static_cast<GLint>(c.level);
  parse_error::ParseError result =
      ValidateFramebufferTexture2D(
          this, immediate_data_size, target, attachment, textarget, texture,
          level);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glFramebufferTexture2DEXT(target, attachment, textarget, texture, level);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleFrontFace(
    uint32 immediate_data_size, const gles2::FrontFace& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  parse_error::ParseError result =
      ValidateFrontFace(this, immediate_data_size, mode);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glFrontFace(mode);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGenBuffers(
    uint32 immediate_data_size, const gles2::GenBuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* buffers = GetSharedMemoryAs<GLuint*>(
      c.buffers_shm_id, c.buffers_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateGenBuffers(this, immediate_data_size, n, buffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenBuffersHelper>(n, buffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGenBuffersImmediate(
    uint32 immediate_data_size, const gles2::GenBuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* buffers = GetImmediateDataAs<GLuint*>(c);
  parse_error::ParseError result =
      ValidateGenBuffersImmediate(this, immediate_data_size, n, buffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenBuffersHelper>(n, buffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGenerateMipmap(
    uint32 immediate_data_size, const gles2::GenerateMipmap& c) {
  GLenum target = static_cast<GLenum>(c.target);
  parse_error::ParseError result =
      ValidateGenerateMipmap(this, immediate_data_size, target);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGenerateMipmapEXT(target);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGenFramebuffers(
    uint32 immediate_data_size, const gles2::GenFramebuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* framebuffers = GetSharedMemoryAs<GLuint*>(
      c.framebuffers_shm_id, c.framebuffers_shm_offset, 0 /* TODO(
          gman): size */);
  parse_error::ParseError result =
      ValidateGenFramebuffers(this, immediate_data_size, n, framebuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenFramebuffersHelper>(n, framebuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGenFramebuffersImmediate(
    uint32 immediate_data_size, const gles2::GenFramebuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* framebuffers = GetImmediateDataAs<GLuint*>(c);
  parse_error::ParseError result =
      ValidateGenFramebuffersImmediate(
          this, immediate_data_size, n, framebuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenFramebuffersHelper>(n, framebuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGenRenderbuffers(
    uint32 immediate_data_size, const gles2::GenRenderbuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* renderbuffers = GetSharedMemoryAs<GLuint*>(
      c.renderbuffers_shm_id, c.renderbuffers_shm_offset, 0 /* TODO(
          gman): size */);
  parse_error::ParseError result =
      ValidateGenRenderbuffers(this, immediate_data_size, n, renderbuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenRenderbuffersHelper>(n, renderbuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGenRenderbuffersImmediate(
    uint32 immediate_data_size, const gles2::GenRenderbuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* renderbuffers = GetImmediateDataAs<GLuint*>(c);
  parse_error::ParseError result =
      ValidateGenRenderbuffersImmediate(
          this, immediate_data_size, n, renderbuffers);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenRenderbuffersHelper>(n, renderbuffers);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGenTextures(
    uint32 immediate_data_size, const gles2::GenTextures& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* textures = GetSharedMemoryAs<GLuint*>(
      c.textures_shm_id, c.textures_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateGenTextures(this, immediate_data_size, n, textures);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenTexturesHelper>(n, textures);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGenTexturesImmediate(
    uint32 immediate_data_size, const gles2::GenTexturesImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  GLuint* textures = GetImmediateDataAs<GLuint*>(c);
  parse_error::ParseError result =
      ValidateGenTexturesImmediate(this, immediate_data_size, n, textures);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  GenGLObjects<GLGenTexturesHelper>(n, textures);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetBooleanv(
    uint32 immediate_data_size, const gles2::GetBooleanv& c) {
  GLenum pname = static_cast<GLenum>(c.pname);
  GLboolean* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLboolean*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetBooleanv(this, immediate_data_size, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetBooleanv(pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetBufferParameteriv(
    uint32 immediate_data_size, const gles2::GetBufferParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetBufferParameteriv(
          this, immediate_data_size, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetBufferParameteriv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetError(
    uint32 immediate_data_size, const gles2::GetError& c) {
  GLenum* result_dst = GetSharedMemoryAs<GLenum*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateGetError(this, immediate_data_size);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = GetGLError();
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetFloatv(
    uint32 immediate_data_size, const gles2::GetFloatv& c) {
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLfloat*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetFloatv(this, immediate_data_size, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetFloatv(pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetFramebufferAttachmentParameteriv(
    
    uint32 immediate_data_size,
    const gles2::GetFramebufferAttachmentParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetFramebufferAttachmentParameteriv(
          this, immediate_data_size, target, attachment, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetIntegerv(
    uint32 immediate_data_size, const gles2::GetIntegerv& c) {
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetIntegerv(this, immediate_data_size, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetIntegerv(pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetProgramiv(
    uint32 immediate_data_size, const gles2::GetProgramiv& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetProgramiv(this, immediate_data_size, program, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetProgramiv(program, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetProgramInfoLog(
    uint32 immediate_data_size, const gles2::GetProgramInfoLog& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLsizei bufsize = static_cast<GLsizei>(c.bufsize);
  GLsizei* length = GetSharedMemoryAs<GLsizei*>(
      c.length_shm_id, c.length_shm_offset, 0 /* TODO(gman): size */);
  char* infolog = GetSharedMemoryAs<char*>(
      c.infolog_shm_id, c.infolog_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateGetProgramInfoLog(
          this, immediate_data_size, program, bufsize, length, infolog);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetProgramInfoLog(program, bufsize, length, infolog);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetRenderbufferParameteriv(
    uint32 immediate_data_size, const gles2::GetRenderbufferParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetRenderbufferParameteriv(
          this, immediate_data_size, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetRenderbufferParameterivEXT(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetShaderiv(
    uint32 immediate_data_size, const gles2::GetShaderiv& c) {
  GLuint shader;
  if (!id_map_.GetServiceId(c.shader, &shader)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetShaderiv(this, immediate_data_size, shader, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetShaderiv(shader, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetShaderInfoLog(
    uint32 immediate_data_size, const gles2::GetShaderInfoLog& c) {
  GLuint shader;
  if (!id_map_.GetServiceId(c.shader, &shader)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLsizei bufsize = static_cast<GLsizei>(c.bufsize);
  GLsizei* length = GetSharedMemoryAs<GLsizei*>(
      c.length_shm_id, c.length_shm_offset, 0 /* TODO(gman): size */);
  char* infolog = GetSharedMemoryAs<char*>(
      c.infolog_shm_id, c.infolog_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateGetShaderInfoLog(
          this, immediate_data_size, shader, bufsize, length, infolog);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetShaderInfoLog(shader, bufsize, length, infolog);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetShaderSource(
    uint32 immediate_data_size, const gles2::GetShaderSource& c) {
  GLuint shader;
  if (!id_map_.GetServiceId(c.shader, &shader)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLsizei bufsize = static_cast<GLsizei>(c.bufsize);
  GLsizei* length = GetSharedMemoryAs<GLsizei*>(
      c.length_shm_id, c.length_shm_offset, 0 /* TODO(gman): size */);
  char* source = GetSharedMemoryAs<char*>(
      c.source_shm_id, c.source_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateGetShaderSource(
          this, immediate_data_size, shader, bufsize, length, source);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetShaderSource(shader, bufsize, length, source);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetString(
    uint32 immediate_data_size, const gles2::GetString& c) {
  GLenum name = static_cast<GLenum>(c.name);
  parse_error::ParseError result =
      ValidateGetString(this, immediate_data_size, name);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetString(name);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetTexParameterfv(
    uint32 immediate_data_size, const gles2::GetTexParameterfv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLfloat*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetTexParameterfv(
          this, immediate_data_size, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetTexParameterfv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetTexParameteriv(
    uint32 immediate_data_size, const gles2::GetTexParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetTexParameteriv(
          this, immediate_data_size, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetTexParameteriv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetVertexAttribfv(
    uint32 immediate_data_size, const gles2::GetVertexAttribfv& c) {
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLfloat*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetVertexAttribfv(
          this, immediate_data_size, index, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetVertexAttribfv(index, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetVertexAttribiv(
    uint32 immediate_data_size, const gles2::GetVertexAttribiv& c) {
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint* params;
  GLsizei num_values = util_.GLGetNumValuesReturned(pname);
  uint32 params_size = num_values * sizeof(*params);
  params = GetSharedMemoryAs<GLint*>(
      c.params_shm_id, c.params_shm_offset, params_size);
  parse_error::ParseError result =
      ValidateGetVertexAttribiv(
          this, immediate_data_size, index, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glGetVertexAttribiv(index, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleHint(
    uint32 immediate_data_size, const gles2::Hint& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum mode = static_cast<GLenum>(c.mode);
  parse_error::ParseError result =
      ValidateHint(this, immediate_data_size, target, mode);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glHint(target, mode);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleIsBuffer(
    uint32 immediate_data_size, const gles2::IsBuffer& c) {
  GLuint buffer;
  if (!id_map_.GetServiceId(c.buffer, &buffer)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsBuffer(this, immediate_data_size, buffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsBuffer(buffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleIsEnabled(
    uint32 immediate_data_size, const gles2::IsEnabled& c) {
  GLenum cap = static_cast<GLenum>(c.cap);
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsEnabled(this, immediate_data_size, cap);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsEnabled(cap);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleIsFramebuffer(
    uint32 immediate_data_size, const gles2::IsFramebuffer& c) {
  GLuint framebuffer;
  if (!id_map_.GetServiceId(c.framebuffer, &framebuffer)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsFramebuffer(this, immediate_data_size, framebuffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsFramebufferEXT(framebuffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleIsProgram(
    uint32 immediate_data_size, const gles2::IsProgram& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsProgram(this, immediate_data_size, program);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsProgram(program);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleIsRenderbuffer(
    uint32 immediate_data_size, const gles2::IsRenderbuffer& c) {
  GLuint renderbuffer;
  if (!id_map_.GetServiceId(c.renderbuffer, &renderbuffer)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsRenderbuffer(this, immediate_data_size, renderbuffer);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsRenderbufferEXT(renderbuffer);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleIsShader(
    uint32 immediate_data_size, const gles2::IsShader& c) {
  GLuint shader;
  if (!id_map_.GetServiceId(c.shader, &shader)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsShader(this, immediate_data_size, shader);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsShader(shader);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleIsTexture(
    uint32 immediate_data_size, const gles2::IsTexture& c) {
  GLuint texture;
  if (!id_map_.GetServiceId(c.texture, &texture)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLboolean* result_dst = GetSharedMemoryAs<GLboolean*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  parse_error::ParseError result =
      ValidateIsTexture(this, immediate_data_size, texture);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  *result_dst = glIsTexture(texture);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleLineWidth(
    uint32 immediate_data_size, const gles2::LineWidth& c) {
  GLfloat width = static_cast<GLfloat>(c.width);
  parse_error::ParseError result =
      ValidateLineWidth(this, immediate_data_size, width);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glLineWidth(width);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleLinkProgram(
    uint32 immediate_data_size, const gles2::LinkProgram& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  parse_error::ParseError result =
      ValidateLinkProgram(this, immediate_data_size, program);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glLinkProgram(program);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandlePolygonOffset(
    uint32 immediate_data_size, const gles2::PolygonOffset& c) {
  GLfloat factor = static_cast<GLfloat>(c.factor);
  GLfloat units = static_cast<GLfloat>(c.units);
  parse_error::ParseError result =
      ValidatePolygonOffset(this, immediate_data_size, factor, units);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glPolygonOffset(factor, units);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleRenderbufferStorage(
    uint32 immediate_data_size, const gles2::RenderbufferStorage& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  parse_error::ParseError result =
      ValidateRenderbufferStorage(
          this, immediate_data_size, target, internalformat, width, height);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glRenderbufferStorageEXT(target, internalformat, width, height);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleSampleCoverage(
    uint32 immediate_data_size, const gles2::SampleCoverage& c) {
  GLclampf value = static_cast<GLclampf>(c.value);
  GLboolean invert = static_cast<GLboolean>(c.invert);
  parse_error::ParseError result =
      ValidateSampleCoverage(this, immediate_data_size, value, invert);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glSampleCoverage(value, invert);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleScissor(
    uint32 immediate_data_size, const gles2::Scissor& c) {
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  parse_error::ParseError result =
      ValidateScissor(this, immediate_data_size, x, y, width, height);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glScissor(x, y, width, height);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleStencilFunc(
    uint32 immediate_data_size, const gles2::StencilFunc& c) {
  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  parse_error::ParseError result =
      ValidateStencilFunc(this, immediate_data_size, func, ref, mask);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glStencilFunc(func, ref, mask);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleStencilFuncSeparate(
    uint32 immediate_data_size, const gles2::StencilFuncSeparate& c) {
  GLenum face = static_cast<GLenum>(c.face);
  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  parse_error::ParseError result =
      ValidateStencilFuncSeparate(
          this, immediate_data_size, face, func, ref, mask);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glStencilFuncSeparate(face, func, ref, mask);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleStencilMask(
    uint32 immediate_data_size, const gles2::StencilMask& c) {
  GLuint mask = static_cast<GLuint>(c.mask);
  parse_error::ParseError result =
      ValidateStencilMask(this, immediate_data_size, mask);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glStencilMask(mask);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleStencilMaskSeparate(
    uint32 immediate_data_size, const gles2::StencilMaskSeparate& c) {
  GLenum face = static_cast<GLenum>(c.face);
  GLuint mask = static_cast<GLuint>(c.mask);
  parse_error::ParseError result =
      ValidateStencilMaskSeparate(this, immediate_data_size, face, mask);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glStencilMaskSeparate(face, mask);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleStencilOp(
    uint32 immediate_data_size, const gles2::StencilOp& c) {
  GLenum fail = static_cast<GLenum>(c.fail);
  GLenum zfail = static_cast<GLenum>(c.zfail);
  GLenum zpass = static_cast<GLenum>(c.zpass);
  parse_error::ParseError result =
      ValidateStencilOp(this, immediate_data_size, fail, zfail, zpass);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glStencilOp(fail, zfail, zpass);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleStencilOpSeparate(
    uint32 immediate_data_size, const gles2::StencilOpSeparate& c) {
  GLenum face = static_cast<GLenum>(c.face);
  GLenum fail = static_cast<GLenum>(c.fail);
  GLenum zfail = static_cast<GLenum>(c.zfail);
  GLenum zpass = static_cast<GLenum>(c.zpass);
  parse_error::ParseError result =
      ValidateStencilOpSeparate(
          this, immediate_data_size, face, fail, zfail, zpass);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glStencilOpSeparate(face, fail, zfail, zpass);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleTexParameterf(
    uint32 immediate_data_size, const gles2::TexParameterf& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat param = static_cast<GLfloat>(c.param);
  parse_error::ParseError result =
      ValidateTexParameterf(this, immediate_data_size, target, pname, param);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexParameterf(target, pname, param);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleTexParameterfv(
    uint32 immediate_data_size, const gles2::TexParameterfv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  const GLfloat* params = GetSharedMemoryAs<const GLfloat*>(
      c.params_shm_id, c.params_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateTexParameterfv(this, immediate_data_size, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexParameterfv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleTexParameterfvImmediate(
    uint32 immediate_data_size, const gles2::TexParameterfvImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  const GLfloat* params = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateTexParameterfvImmediate(
          this, immediate_data_size, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexParameterfv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleTexParameteri(
    uint32 immediate_data_size, const gles2::TexParameteri& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint param = static_cast<GLint>(c.param);
  parse_error::ParseError result =
      ValidateTexParameteri(this, immediate_data_size, target, pname, param);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexParameteri(target, pname, param);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleTexParameteriv(
    uint32 immediate_data_size, const gles2::TexParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  const GLint* params = GetSharedMemoryAs<const GLint*>(
      c.params_shm_id, c.params_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateTexParameteriv(this, immediate_data_size, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexParameteriv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleTexParameterivImmediate(
    uint32 immediate_data_size, const gles2::TexParameterivImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  const GLint* params = GetImmediateDataAs<const GLint*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateTexParameterivImmediate(
          this, immediate_data_size, target, pname, params);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexParameteriv(target, pname, params);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleTexSubImage2D(
    uint32 immediate_data_size, const gles2::TexSubImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32 pixels_shm_id = static_cast<uint32>(c.pixels_shm_id);
  uint32 pixels_shm_offset = static_cast<uint32>(c.pixels_shm_offset);
  uint32 pixels_size = GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_);
  const void* pixels = GetSharedMemoryAs<const void*>(
      pixels_shm_id, pixels_shm_offset, pixels_size);
  parse_error::ParseError result =
      ValidateTexSubImage2D(
          this, immediate_data_size, target, level, xoffset, yoffset, width,
          height, format, type, pixels);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleTexSubImage2DImmediate(
    uint32 immediate_data_size, const gles2::TexSubImage2DImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  const void* pixels = GetImmediateDataAs<const void*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateTexSubImage2DImmediate(
          this, immediate_data_size, target, level, xoffset, yoffset, width,
          height, format, type, pixels);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform1f(
    uint32 immediate_data_size, const gles2::Uniform1f& c) {
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  parse_error::ParseError result =
      ValidateUniform1f(this, immediate_data_size, location, x);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform1f(location, x);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform1fv(
    uint32 immediate_data_size, const gles2::Uniform1fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetSharedMemoryAs<const GLfloat*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform1fv(this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform1fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform1fvImmediate(
    uint32 immediate_data_size, const gles2::Uniform1fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform1fvImmediate(
          this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform1fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform1i(
    uint32 immediate_data_size, const gles2::Uniform1i& c) {
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  parse_error::ParseError result =
      ValidateUniform1i(this, immediate_data_size, location, x);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform1i(location, x);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform1iv(
    uint32 immediate_data_size, const gles2::Uniform1iv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetSharedMemoryAs<const GLint*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform1iv(this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform1iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform1ivImmediate(
    uint32 immediate_data_size, const gles2::Uniform1ivImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetImmediateDataAs<const GLint*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform1ivImmediate(
          this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform1iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform2f(
    uint32 immediate_data_size, const gles2::Uniform2f& c) {
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  parse_error::ParseError result =
      ValidateUniform2f(this, immediate_data_size, location, x, y);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform2f(location, x, y);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform2fv(
    uint32 immediate_data_size, const gles2::Uniform2fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetSharedMemoryAs<const GLfloat*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform2fv(this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform2fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform2fvImmediate(
    uint32 immediate_data_size, const gles2::Uniform2fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform2fvImmediate(
          this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform2fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform2i(
    uint32 immediate_data_size, const gles2::Uniform2i& c) {
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  parse_error::ParseError result =
      ValidateUniform2i(this, immediate_data_size, location, x, y);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform2i(location, x, y);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform2iv(
    uint32 immediate_data_size, const gles2::Uniform2iv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetSharedMemoryAs<const GLint*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform2iv(this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform2iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform2ivImmediate(
    uint32 immediate_data_size, const gles2::Uniform2ivImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetImmediateDataAs<const GLint*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform2ivImmediate(
          this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform2iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform3f(
    uint32 immediate_data_size, const gles2::Uniform3f& c) {
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  parse_error::ParseError result =
      ValidateUniform3f(this, immediate_data_size, location, x, y, z);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform3f(location, x, y, z);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform3fv(
    uint32 immediate_data_size, const gles2::Uniform3fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetSharedMemoryAs<const GLfloat*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform3fv(this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform3fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform3fvImmediate(
    uint32 immediate_data_size, const gles2::Uniform3fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform3fvImmediate(
          this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform3fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform3i(
    uint32 immediate_data_size, const gles2::Uniform3i& c) {
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  parse_error::ParseError result =
      ValidateUniform3i(this, immediate_data_size, location, x, y, z);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform3i(location, x, y, z);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform3iv(
    uint32 immediate_data_size, const gles2::Uniform3iv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetSharedMemoryAs<const GLint*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform3iv(this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform3iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform3ivImmediate(
    uint32 immediate_data_size, const gles2::Uniform3ivImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetImmediateDataAs<const GLint*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform3ivImmediate(
          this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform3iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform4f(
    uint32 immediate_data_size, const gles2::Uniform4f& c) {
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat w = static_cast<GLfloat>(c.w);
  parse_error::ParseError result =
      ValidateUniform4f(this, immediate_data_size, location, x, y, z, w);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform4f(location, x, y, z, w);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform4fv(
    uint32 immediate_data_size, const gles2::Uniform4fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetSharedMemoryAs<const GLfloat*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform4fv(this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform4fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform4fvImmediate(
    uint32 immediate_data_size, const gles2::Uniform4fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLfloat* v = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform4fvImmediate(
          this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform4fv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform4i(
    uint32 immediate_data_size, const gles2::Uniform4i& c) {
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  GLint w = static_cast<GLint>(c.w);
  parse_error::ParseError result =
      ValidateUniform4i(this, immediate_data_size, location, x, y, z, w);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform4i(location, x, y, z, w);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform4iv(
    uint32 immediate_data_size, const gles2::Uniform4iv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetSharedMemoryAs<const GLint*>(
      c.v_shm_id, c.v_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniform4iv(this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform4iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniform4ivImmediate(
    uint32 immediate_data_size, const gles2::Uniform4ivImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  const GLint* v = GetImmediateDataAs<const GLint*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniform4ivImmediate(
          this, immediate_data_size, location, count, v);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniform4iv(location, count, v);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniformMatrix2fv(
    uint32 immediate_data_size, const gles2::UniformMatrix2fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  const GLfloat* value = GetSharedMemoryAs<const GLfloat*>(
      c.value_shm_id, c.value_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniformMatrix2fv(
          this, immediate_data_size, location, count, transpose, value);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniformMatrix2fv(location, count, transpose, value);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniformMatrix2fvImmediate(
    uint32 immediate_data_size, const gles2::UniformMatrix2fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  const GLfloat* value = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniformMatrix2fvImmediate(
          this, immediate_data_size, location, count, transpose, value);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniformMatrix2fv(location, count, transpose, value);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniformMatrix3fv(
    uint32 immediate_data_size, const gles2::UniformMatrix3fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  const GLfloat* value = GetSharedMemoryAs<const GLfloat*>(
      c.value_shm_id, c.value_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniformMatrix3fv(
          this, immediate_data_size, location, count, transpose, value);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniformMatrix3fv(location, count, transpose, value);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniformMatrix3fvImmediate(
    uint32 immediate_data_size, const gles2::UniformMatrix3fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  const GLfloat* value = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniformMatrix3fvImmediate(
          this, immediate_data_size, location, count, transpose, value);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniformMatrix3fv(location, count, transpose, value);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniformMatrix4fv(
    uint32 immediate_data_size, const gles2::UniformMatrix4fv& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  const GLfloat* value = GetSharedMemoryAs<const GLfloat*>(
      c.value_shm_id, c.value_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateUniformMatrix4fv(
          this, immediate_data_size, location, count, transpose, value);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniformMatrix4fv(location, count, transpose, value);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUniformMatrix4fvImmediate(
    uint32 immediate_data_size, const gles2::UniformMatrix4fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  const GLfloat* value = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateUniformMatrix4fvImmediate(
          this, immediate_data_size, location, count, transpose, value);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUniformMatrix4fv(location, count, transpose, value);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleUseProgram(
    uint32 immediate_data_size, const gles2::UseProgram& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  parse_error::ParseError result =
      ValidateUseProgram(this, immediate_data_size, program);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glUseProgram(program);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleValidateProgram(
    uint32 immediate_data_size, const gles2::ValidateProgram& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  parse_error::ParseError result =
      ValidateValidateProgram(this, immediate_data_size, program);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glValidateProgram(program);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleVertexAttrib1f(
    uint32 immediate_data_size, const gles2::VertexAttrib1f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  parse_error::ParseError result =
      ValidateVertexAttrib1f(this, immediate_data_size, indx, x);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib1f(indx, x);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleVertexAttrib1fv(
    uint32 immediate_data_size, const gles2::VertexAttrib1fv& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetSharedMemoryAs<const GLfloat*>(
      c.values_shm_id, c.values_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateVertexAttrib1fv(this, immediate_data_size, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib1fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleVertexAttrib1fvImmediate(
    uint32 immediate_data_size, const gles2::VertexAttrib1fvImmediate& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateVertexAttrib1fvImmediate(
          this, immediate_data_size, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib1fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleVertexAttrib2f(
    uint32 immediate_data_size, const gles2::VertexAttrib2f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  parse_error::ParseError result =
      ValidateVertexAttrib2f(this, immediate_data_size, indx, x, y);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib2f(indx, x, y);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleVertexAttrib2fv(
    uint32 immediate_data_size, const gles2::VertexAttrib2fv& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetSharedMemoryAs<const GLfloat*>(
      c.values_shm_id, c.values_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateVertexAttrib2fv(this, immediate_data_size, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib2fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleVertexAttrib2fvImmediate(
    uint32 immediate_data_size, const gles2::VertexAttrib2fvImmediate& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateVertexAttrib2fvImmediate(
          this, immediate_data_size, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib2fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleVertexAttrib3f(
    uint32 immediate_data_size, const gles2::VertexAttrib3f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  parse_error::ParseError result =
      ValidateVertexAttrib3f(this, immediate_data_size, indx, x, y, z);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib3f(indx, x, y, z);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleVertexAttrib3fv(
    uint32 immediate_data_size, const gles2::VertexAttrib3fv& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetSharedMemoryAs<const GLfloat*>(
      c.values_shm_id, c.values_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateVertexAttrib3fv(this, immediate_data_size, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib3fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleVertexAttrib3fvImmediate(
    uint32 immediate_data_size, const gles2::VertexAttrib3fvImmediate& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateVertexAttrib3fvImmediate(
          this, immediate_data_size, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib3fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleVertexAttrib4f(
    uint32 immediate_data_size, const gles2::VertexAttrib4f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat w = static_cast<GLfloat>(c.w);
  parse_error::ParseError result =
      ValidateVertexAttrib4f(this, immediate_data_size, indx, x, y, z, w);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib4f(indx, x, y, z, w);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleVertexAttrib4fv(
    uint32 immediate_data_size, const gles2::VertexAttrib4fv& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetSharedMemoryAs<const GLfloat*>(
      c.values_shm_id, c.values_shm_offset, 0 /* TODO(gman): size */);
  parse_error::ParseError result =
      ValidateVertexAttrib4fv(this, immediate_data_size, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib4fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleVertexAttrib4fvImmediate(
    uint32 immediate_data_size, const gles2::VertexAttrib4fvImmediate& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  const GLfloat* values = GetImmediateDataAs<const GLfloat*>(c);
  // Immediate version.
  parse_error::ParseError result =
      ValidateVertexAttrib4fvImmediate(
          this, immediate_data_size, indx, values);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glVertexAttrib4fv(indx, values);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleViewport(
    uint32 immediate_data_size, const gles2::Viewport& c) {
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  parse_error::ParseError result =
      ValidateViewport(this, immediate_data_size, x, y, width, height);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glViewport(x, y, width, height);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleSwapBuffers(
    uint32 immediate_data_size, const gles2::SwapBuffers& c) {
  parse_error::ParseError result =
      ValidateSwapBuffers(this, immediate_data_size);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  DoSwapBuffers();
  return parse_error::kParseNoError;
}

