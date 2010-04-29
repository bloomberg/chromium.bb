// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include <stdio.h>

#include <algorithm>
#include <vector>
#include <string>
#include <map>

#include "app/gfx/gl/gl_context.h"
#include "base/callback.h"
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#include "base/weak_ptr.h"
#include "build/build_config.h"
#define GLES2_GPU_SERVICE 1
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"

// TODO(alokp): Remove GLES2_GPU_SERVICE_TRANSLATE_SHADER guard
// as soon as translator is ready.
//#define GLES2_GPU_SERVICE_TRANSLATE_SHADER
#if defined(GLES2_GPU_SERVICE_TRANSLATE_SHADER)
#include "third_party/angleproject/include/GLSLANG/ShaderLang.h"
#endif  // GLES2_GPU_SERVICE_TRANSLATE_SHADER

#if !defined(GL_DEPTH24_STENCIL8)
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

#if defined(UNIT_TEST)

// OpenGL constants not defined in OpenGL ES 2.0 needed when compiling
// unit tests. For native OpenGL ES 2.0 backend these are not used. For OpenGL
// backend these must be defined by the local system.
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS 0x8B49
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS 0x8B4A
#define GL_MAX_VARYING_FLOATS 0x8B4B

#endif

namespace gpu {
namespace gles2 {

class GLES2DecoderImpl;

// Check that certain assumptions the code makes are true. There are places in
// the code where shared memory is passed direclty to GL. Example, glUniformiv,
// glShaderSource. The command buffer code assumes GLint and GLsizei (and maybe
// a few others) are 32bits. If they are not 32bits the code will have to change
// to call those GL functions with service side memory and then copy the results
// to shared memory, converting the sizes.
COMPILE_ASSERT(sizeof(GLint) == sizeof(uint32),  // NOLINT
               GLint_not_same_size_as_uint32);
COMPILE_ASSERT(sizeof(GLsizei) == sizeof(uint32),  // NOLINT
               GLint_not_same_size_as_uint32);
COMPILE_ASSERT(sizeof(GLfloat) == sizeof(float),  // NOLINT
               GLfloat_not_same_size_as_float);

// TODO(kbr): the use of this anonymous namespace core dumps the
// linker on Mac OS X 10.6 when the symbol ordering file is used
// namespace {

// Returns the address of the first byte after a struct.
template <typename T>
const void* AddressAfterStruct(const T& pod) {
  return reinterpret_cast<const uint8*>(&pod) + sizeof(pod);
}

// Returns the address of the frst byte after the struct or NULL if size >
// immediate_data_size.
template <typename RETURN_TYPE, typename COMMAND_TYPE>
RETURN_TYPE GetImmediateDataAs(const COMMAND_TYPE& pod,
                               uint32 size,
                               uint32 immediate_data_size) {
  return (size <= immediate_data_size) ?
      static_cast<RETURN_TYPE>(const_cast<void*>(AddressAfterStruct(pod))) :
      NULL;
}

// Computes the data size for certain gl commands like glUniform.
bool ComputeDataSize(
    GLuint count,
    size_t size,
    unsigned int elements_per_unit,
    uint32* dst) {
  uint32 value;
  if (!SafeMultiplyUint32(count, size, &value)) {
    return false;
  }
  if (!SafeMultiplyUint32(value, elements_per_unit, &value)) {
    return false;
  }
  *dst = value;
  return true;
}

// A struct to hold info about each command.
struct CommandInfo {
  int arg_flags;  // How to handle the arguments for this command
  int arg_count;  // How many arguments are expected for this command.
};

// A table of CommandInfo for all the commands.
const CommandInfo g_command_info[] = {
  #define GLES2_CMD_OP(name) {                                            \
    name::kArgFlags,                                                      \
    sizeof(name) / sizeof(CommandBufferEntry) - 1, },  /* NOLINT */       \

  GLES2_COMMAND_LIST(GLES2_CMD_OP)

  #undef GLES2_CMD_OP
};

// This class prevents any GL errors that occur when it is in scope from
// being reported to the client.
class ScopedGLErrorSuppressor {
 public:
  explicit ScopedGLErrorSuppressor(GLES2DecoderImpl* decoder);
  ~ScopedGLErrorSuppressor();
 private:
  GLES2DecoderImpl* decoder_;
  DISALLOW_COPY_AND_ASSIGN(ScopedGLErrorSuppressor);
};

// Temporarily changes a decoder's bound 2D texture and restore it when this
// object goes out of scope. Also temporarily switches to using active texture
// unit zero in case the client has changed that to something invalid.
class ScopedTexture2DBinder {
 public:
  ScopedTexture2DBinder(GLES2DecoderImpl* decoder, GLuint id);
  ~ScopedTexture2DBinder();

 private:
  GLES2DecoderImpl* decoder_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTexture2DBinder);
};

// Temporarily changes a decoder's bound render buffer and restore it when this
// object goes out of scope.
class ScopedRenderBufferBinder {
 public:
  ScopedRenderBufferBinder(GLES2DecoderImpl* decoder, GLuint id);
  ~ScopedRenderBufferBinder();

 private:
  GLES2DecoderImpl* decoder_;
  DISALLOW_COPY_AND_ASSIGN(ScopedRenderBufferBinder);
};

// Temporarily changes a decoder's bound frame buffer and restore it when this
// object goes out of scope.
class ScopedFrameBufferBinder {
 public:
  ScopedFrameBufferBinder(GLES2DecoderImpl* decoder, GLuint id);
  ~ScopedFrameBufferBinder();

 private:
  GLES2DecoderImpl* decoder_;
  DISALLOW_COPY_AND_ASSIGN(ScopedFrameBufferBinder);
};

// Temporarily switch to a decoder's default GL context, having known default
// state.
class ScopedDefaultGLContext {
 public:
  explicit ScopedDefaultGLContext(GLES2DecoderImpl* decoder);
  ~ScopedDefaultGLContext();

 private:
  GLES2DecoderImpl* decoder_;
  DISALLOW_COPY_AND_ASSIGN(ScopedDefaultGLContext);
};

// Encapsulates an OpenGL texture.
class Texture {
 public:
  explicit Texture(GLES2DecoderImpl* decoder);
  ~Texture();

  // Create a new render texture.
  void Create();

  // Set the initial size and format of a render texture or resize it.
  bool AllocateStorage(const gfx::Size& size);

  // Copy the contents of the currently bound frame buffer.
  void Copy(const gfx::Size& size);

  // Destroy the render texture. This must be explicitly called before
  // destroying this object.
  void Destroy();

  GLuint id() const {
    return id_;
  }

  gfx::Size size() const {
    return size_;
  }

 private:
  GLES2DecoderImpl* decoder_;
  GLuint id_;
  gfx::Size size_;
  DISALLOW_COPY_AND_ASSIGN(Texture);
};

// Encapsulates an OpenGL render buffer of any format.
class RenderBuffer {
 public:
  explicit RenderBuffer(GLES2DecoderImpl* decoder);
  ~RenderBuffer();

  // Create a new render buffer.
  void Create();

  // Set the initial size and format of a render buffer or resize it.
  bool AllocateStorage(const gfx::Size& size, GLenum format);

  // Destroy the render buffer. This must be explicitly called before destroying
  // this object.
  void Destroy();

  GLuint id() const {
    return id_;
  }

 private:
  GLES2DecoderImpl* decoder_;
  GLuint id_;
  DISALLOW_COPY_AND_ASSIGN(RenderBuffer);
};

// Encapsulates an OpenGL frame buffer.
class FrameBuffer {
 public:
  explicit FrameBuffer(GLES2DecoderImpl* decoder);
  ~FrameBuffer();

  // Create a new frame buffer.
  void Create();

  // Attach a color render buffer to a frame buffer.
  void AttachRenderTexture(Texture* texture);

  // Attach a depth stencil render buffer to a frame buffer. Note that
  // this unbinds any currently bound frame buffer.
  void AttachDepthStencilRenderBuffer(RenderBuffer* render_buffer);

  // Clear the given attached buffers.
  void Clear(GLbitfield buffers);

  // Destroy the frame buffer. This must be explicitly called before destroying
  // this object.
  void Destroy();

  // See glCheckFramebufferStatusEXT.
  GLenum CheckStatus();

  GLuint id() const {
    return id_;
  }

 private:
  GLES2DecoderImpl* decoder_;
  GLuint id_;
  DISALLOW_COPY_AND_ASSIGN(FrameBuffer);
};
// }  // anonymous namespace.

GLES2Decoder::GLES2Decoder(ContextGroup* group)
    : group_(group),
      debug_(false) {
}

GLES2Decoder::~GLES2Decoder() {
}

// This class implements GLES2Decoder so we don't have to expose all the GLES2
// cmd stuff to outside this class.
class GLES2DecoderImpl : public base::SupportsWeakPtr<GLES2DecoderImpl>,
                         public GLES2Decoder {
 public:
  explicit GLES2DecoderImpl(ContextGroup* group);

  // Info about Vertex Attributes. This is used to track what the user currently
  // has bound on each Vertex Attribute so that checking can be done at
  // glDrawXXX time.
  class VertexAttribInfo {
   public:
    VertexAttribInfo()
        : enabled_(false),
          size_(0),
          type_(0),
          offset_(0),
          real_stride_(0) {
    }

    // Returns true if this VertexAttrib can access index.
    bool CanAccess(GLuint index);

    void set_enabled(bool enabled) {
      enabled_ = enabled;
    }

    BufferManager::BufferInfo* buffer() const {
      return buffer_;
    }

    GLsizei offset() const {
      return offset_;
    }

    void SetInfo(
        BufferManager::BufferInfo* buffer,
        GLint size,
        GLenum type,
        GLsizei real_stride,
        GLsizei offset) {
      DCHECK_GT(real_stride, 0);
      buffer_ = buffer;
      size_ = size;
      type_ = type;
      real_stride_ = real_stride;
      offset_ = offset;
    }

    void ClearBuffer() {
      buffer_ = NULL;
    }

   private:
    // Whether or not this attribute is enabled.
    bool enabled_;

    // number of components (1, 2, 3, 4)
    GLint size_;

    // GL_BYTE, GL_FLOAT, etc. See glVertexAttribPointer.
    GLenum type_;

    // The offset into the buffer.
    GLsizei offset_;

    // The stride that will be used to access the buffer. This is the actual
    // stide, NOT the GL bogus stride. In other words there is never a stride
    // of 0.
    GLsizei real_stride_;

    // The buffer bound to this attribute.
    BufferManager::BufferInfo::Ref buffer_;
  };

  // Overridden from AsyncAPIInterface.
  virtual Error DoCommand(unsigned int command,
                          unsigned int arg_count,
                          const void* args);

  // Overridden from AsyncAPIInterface.
  virtual const char* GetCommandName(unsigned int command_id) const;

  // Overridden from GLES2Decoder.
  virtual bool Initialize(gfx::GLContext* context,
                          const gfx::Size& size,
                          GLES2Decoder* parent,
                          uint32 parent_client_texture_id);
  virtual void Destroy();
  virtual void ResizeOffscreenFrameBuffer(const gfx::Size& size);
  virtual bool MakeCurrent();
  virtual GLES2Util* GetGLES2Util() { return &util_; }
  virtual gfx::GLContext* GetGLContext() { return context_; }

  virtual void SetSwapBuffersCallback(Callback0::Type* callback);

 private:
  friend class ScopedGLErrorSuppressor;
  friend class ScopedTexture2DBinder;
  friend class ScopedFrameBufferBinder;
  friend class ScopedRenderBufferBinder;
  friend class ScopedDefaultGLContext;
  friend class RenderBuffer;
  friend class FrameBuffer;

  // State associated with each texture unit.
  struct TextureUnit {
    TextureUnit() : bind_target(GL_TEXTURE_2D) { }

    // The last target that was bound to this texture unit.
    GLenum bind_target;

    // texture currently bound to this unit's GL_TEXTURE_2D with glBindTexture
    TextureManager::TextureInfo::Ref bound_texture_2d;

    // texture currently bound to this unit's GL_TEXTURE_CUBE_MAP with
    // glBindTexture
    TextureManager::TextureInfo::Ref bound_texture_cube_map;
  };

  // Helpers for the glGen and glDelete functions.
  bool GenTexturesHelper(GLsizei n, const GLuint* client_ids);
  void DeleteTexturesHelper(GLsizei n, const GLuint* client_ids);
  bool GenBuffersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteBuffersHelper(GLsizei n, const GLuint* client_ids);
  bool GenFramebuffersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteFramebuffersHelper(GLsizei n, const GLuint* client_ids);
  bool GenRenderbuffersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteRenderbuffersHelper(GLsizei n, const GLuint* client_ids);

  // TODO(gman): Cache these pointers?
  BufferManager* buffer_manager() {
    return group_->buffer_manager();
  }

  RenderbufferManager* renderbuffer_manager() {
    return group_->renderbuffer_manager();
  }

  FramebufferManager* framebuffer_manager() {
    return group_->framebuffer_manager();
  }

  ProgramManager* program_manager() {
    return group_->program_manager();
  }

  ShaderManager* shader_manager() {
    return group_->shader_manager();
  }

  TextureManager* texture_manager() {
    return group_->texture_manager();
  }

  bool UpdateOffscreenFrameBufferSize();

  // Creates a TextureInfo for the given texture.
  TextureManager::TextureInfo* CreateTextureInfo(
      GLuint client_id, GLuint service_id) {
    return texture_manager()->CreateTextureInfo(client_id, service_id);
  }

  // Gets the texture info for the given texture. Returns NULL if none exists.
  TextureManager::TextureInfo* GetTextureInfo(GLuint client_id) {
    TextureManager::TextureInfo* info =
        texture_manager()->GetTextureInfo(client_id);
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Deletes the texture info for the given texture.
  void RemoveTextureInfo(GLuint client_id) {
    texture_manager()->RemoveTextureInfo(client_id);
  }

  // Get the size (in pixels) of the currently bound frame buffer (either FBO
  // or regular back buffer).
  gfx::Size GetBoundFrameBufferSize();

  // Wrapper for CompressedTexImage2D commands.
  error::Error DoCompressedTexImage2D(
    GLenum target,
    GLint level,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLsizei image_size,
    const void* data);

  // Wrapper for TexImage2D commands.
  error::Error DoTexImage2D(
    GLenum target,
    GLint level,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    const void* pixels,
    uint32 pixels_size);

  // Creates a ProgramInfo for the given program.
  void CreateProgramInfo(GLuint client_id, GLuint service_id) {
    program_manager()->CreateProgramInfo(client_id, service_id);
  }

  // Gets the program info for the given program. Returns NULL if none exists.
  ProgramManager::ProgramInfo* GetProgramInfo(GLuint client_id) {
    ProgramManager::ProgramInfo* info =
        program_manager()->GetProgramInfo(client_id);
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Deletes the program info for the given program.
  void RemoveProgramInfo(GLuint client_id) {
    program_manager()->RemoveProgramInfo(client_id);
  }

  // Creates a ShaderInfo for the given shader.
  void CreateShaderInfo(GLuint client_id,
                        GLuint service_id,
                        GLenum shader_type) {
    shader_manager()->CreateShaderInfo(client_id, service_id, shader_type);
  }

  // Gets the shader info for the given shader. Returns NULL if none exists.
  ShaderManager::ShaderInfo* GetShaderInfo(GLuint client_id) {
    ShaderManager::ShaderInfo* info =
        shader_manager()->GetShaderInfo(client_id);
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Deletes the shader info for the given shader.
  void RemoveShaderInfo(GLuint client_id) {
    shader_manager()->RemoveShaderInfo(client_id);
  }

  // Creates a buffer info for the given buffer.
  void CreateBufferInfo(GLuint client_id, GLuint service_id) {
    return buffer_manager()->CreateBufferInfo(client_id, service_id);
  }

  // Gets the buffer info for the given buffer.
  BufferManager::BufferInfo* GetBufferInfo(GLuint client_id) {
    BufferManager::BufferInfo* info =
        buffer_manager()->GetBufferInfo(client_id);
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Removes any buffers in the VertexAtrribInfos and BufferInfos. This is used
  // on glDeleteBuffers so we can make sure the user does not try to render
  // with deleted buffers.
  void RemoveBufferInfo(GLuint client_id);

  // Creates a framebuffer info for the given framebuffer.
  void CreateFramebufferInfo(GLuint client_id, GLuint service_id) {
    return framebuffer_manager()->CreateFramebufferInfo(client_id, service_id);
  }

  // Gets the framebuffer info for the given framebuffer.
  FramebufferManager::FramebufferInfo* GetFramebufferInfo(
      GLuint client_id) {
    FramebufferManager::FramebufferInfo* info =
        framebuffer_manager()->GetFramebufferInfo(client_id);
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Removes the framebuffer info for the given framebuffer.
  void RemoveFramebufferInfo(GLuint client_id) {
    framebuffer_manager()->RemoveFramebufferInfo(client_id);
  }

  // Creates a renderbuffer info for the given renderbuffer.
  void CreateRenderbufferInfo(GLuint client_id, GLuint service_id) {
    return renderbuffer_manager()->CreateRenderbufferInfo(
        client_id, service_id);
  }

  // Gets the renderbuffer info for the given renderbuffer.
  RenderbufferManager::RenderbufferInfo* GetRenderbufferInfo(
      GLuint client_id) {
    RenderbufferManager::RenderbufferInfo* info =
        renderbuffer_manager()->GetRenderbufferInfo(client_id);
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Removes the renderbuffer info for the given renderbuffer.
  void RemoveRenderbufferInfo(GLuint client_id) {
    renderbuffer_manager()->RemoveRenderbufferInfo(client_id);
  }

  error::Error GetAttribLocationHelper(
    GLuint client_id, uint32 location_shm_id, uint32 location_shm_offset,
    const std::string& name_str);

  error::Error GetUniformLocationHelper(
    GLuint client_id, uint32 location_shm_id, uint32 location_shm_offset,
    const std::string& name_str);

  // Helper for glShaderSource.
  error::Error ShaderSourceHelper(
      GLuint client_id, const char* data, uint32 data_size);

  // Helper for glGetBooleanv, glGetFloatv and glGetIntegerv
  bool GetHelper(GLenum pname, GLint* params, GLsizei* num_written);

  // Wrapper for glCreateProgram
  bool CreateProgramHelper(GLuint client_id);

  // Wrapper for glCreateShader
  bool CreateShaderHelper(GLenum type, GLuint client_id);

  // Wrapper for glActiveTexture
  void DoActiveTexture(GLenum texture_unit);

  // Wrapper for glAttachShader
  void DoAttachShader(GLuint client_program_id, GLint client_shader_id);

  // Wrapper for glBindBuffer since we need to track the current targets.
  void DoBindBuffer(GLenum target, GLuint buffer);

  // Wrapper for glBindFramebuffer since we need to track the current targets.
  void DoBindFramebuffer(GLenum target, GLuint framebuffer);

  // Wrapper for glBindRenderbuffer since we need to track the current targets.
  void DoBindRenderbuffer(GLenum target, GLuint renderbuffer);

  // Wrapper for glBindTexture since we need to track the current targets.
  void DoBindTexture(GLenum target, GLuint texture);

  // Wrapper for glBufferData.
  void DoBufferData(
    GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage);

  // Wrapper for glBufferSubData.
  void DoBufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data);

  // Wrapper for glCheckFramebufferStatus
  GLenum DoCheckFramebufferStatus(GLenum target);

  // Wrapper for glCompileShader.
  void DoCompileShader(GLuint shader);

  // Wrapper for glDetachShader
  void DoDetachShader(GLuint client_program_id, GLint client_shader_id);

  // Wrapper for glDrawArrays.
  void DoDrawArrays(GLenum mode, GLint first, GLsizei count);

  // Wrapper for glDisableVertexAttribArray.
  void DoDisableVertexAttribArray(GLuint index);

  // Wrapper for glEnableVertexAttribArray.
  void DoEnableVertexAttribArray(GLuint index);

  // Wrapper for glFramebufferRenderbufffer.
  void DoFramebufferRenderbuffer(
      GLenum target, GLenum attachment, GLenum renderbuffertarget,
      GLuint renderbuffer);

  // Wrapper for glFramebufferTexture2D.
  void DoFramebufferTexture2D(
      GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
      GLint level);

  // Wrapper for glGenerateMipmap
  void DoGenerateMipmap(GLenum target);

  // Wrapper for DoGetBooleanv.
  void DoGetBooleanv(GLenum pname, GLboolean* params);

  // Wrapper for DoGetFloatv.
  void DoGetFloatv(GLenum pname, GLfloat* params);

  // Wrapper for glGetFramebufferAttachmentParameteriv.
  void DoGetFramebufferAttachmentParameteriv(
      GLenum target, GLenum attachment, GLenum pname, GLint* params);

  // Wrapper for glGetIntegerv.
  void DoGetIntegerv(GLenum pname, GLint* params);

  // Gets the max value in a range in a buffer.
  GLuint DoGetMaxValueInBuffer(
      GLuint buffer_id, GLsizei count, GLenum type, GLuint offset);

  // Wrapper for glGetProgramiv.
  void DoGetProgramiv(
      GLuint program_id, GLenum pname, GLint* params);

  // Wrapper for glRenderbufferParameteriv.
  void DoGetRenderbufferParameteriv(
      GLenum target, GLenum pname, GLint* params);

  // Wrapper for glGetShaderiv
  void DoGetShaderiv(GLuint shader, GLenum pname, GLint* params);

  // Wrappers for glIsXXX functions.
  bool DoIsBuffer(GLuint client_id);
  bool DoIsFramebuffer(GLuint client_id);
  bool DoIsProgram(GLuint client_id);
  bool DoIsRenderbuffer(GLuint client_id);
  bool DoIsShader(GLuint client_id);
  bool DoIsTexture(GLuint client_id);

  // Wrapper for glLinkProgram
  void DoLinkProgram(GLuint program);

  // Wrapper for glRenderbufferStorage.
  void DoRenderbufferStorage(
      GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

  // Wrapper for glReleaseShaderCompiler.
  void DoReleaseShaderCompiler() { }

  // Wrappers for glTexParameter functions.
  void DoTexParameterf(GLenum target, GLenum pname, GLfloat param);
  void DoTexParameteri(GLenum target, GLenum pname, GLint param);
  void DoTexParameterfv(GLenum target, GLenum pname, const GLfloat* params);
  void DoTexParameteriv(GLenum target, GLenum pname, const GLint* params);

  // Wrappers for glUniform1i and glUniform1iv as according to the GLES2
  // spec only these 2 functions can be used to set sampler uniforms.
  void DoUniform1i(GLint location, GLint v0);
  void DoUniform1iv(GLint location, GLsizei count, const GLint *value);

  // Wrapper for glUseProgram
  void DoUseProgram(GLuint program);

  // Wrapper for glValidateProgram.
  void DoValidateProgram(GLuint program_client_id);

  // Gets the GLError through our wrapper.
  GLenum GetGLError();

  // Sets our wrapper for the GLError.
  void SetGLError(GLenum error, const char* msg);

  // Copies the real GL errors to the wrapper. This is so we can
  // make sure there are no native GL errors before calling some GL function
  // so that on return we know any error generated was for that specific
  // command.
  void CopyRealGLErrorsToWrapper();

  // Clear all real GL errors. This is to prevent the client from seeing any
  // errors caused by GL calls that it was not responsible for issuing.
  void ClearRealGLErrors();

  // Checks if the current program and vertex attributes are valid for drawing.
  bool IsDrawValid(GLuint max_vertex_accessed);

  void SetBlackTextureForNonRenderableTextures(
      bool* has_non_renderable_textures);
  void RestoreStateForNonRenderableTextures();

  // Gets the buffer id for a given target.
  BufferManager::BufferInfo* GetBufferInfoForTarget(GLenum target) {
    DCHECK(target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER);
    BufferManager::BufferInfo* info = target == GL_ARRAY_BUFFER ?
        bound_array_buffer_ : bound_element_array_buffer_;
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Gets the texture id for a given target.
  TextureManager::TextureInfo* GetTextureInfoForTarget(GLenum target) {
    TextureUnit& unit = texture_units_[active_texture_unit_];
    TextureManager::TextureInfo* info = NULL;
    switch (target) {
      case GL_TEXTURE_2D:
        info = unit.bound_texture_2d;
        break;
      case GL_TEXTURE_CUBE_MAP:
      case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        info = unit.bound_texture_cube_map;
        break;
      // Note: If we ever support TEXTURE_RECTANGLE as a target, be sure to
      // track |texture_| with the currently bound TEXTURE_RECTANGLE texture,
      // because |texture_| is used by the FBO rendering mechanism for readback
      // to the bits that get sent to the browser.
      default:
        NOTREACHED();
        return NULL;
    }
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Validates the program and location for a glGetUniform call and returns
  // a SizeResult setup to receive the result. Returns true if glGetUniform
  // should be called.
  bool GetUniformSetup(
      GLuint program, GLint location,
      uint32 shm_id, uint32 shm_offset,
      error::Error* error, GLuint* service_id, void** result);

  bool ValidateGLenumCompressedTextureInternalFormat(GLenum format);

  // Generate a member function prototype for each command in an automated and
  // typesafe way.
  #define GLES2_CMD_OP(name) \
     Error Handle ## name(             \
       uint32 immediate_data_size,          \
       const gles2::name& args);            \

  GLES2_COMMAND_LIST(GLES2_CMD_OP)

  #undef GLES2_CMD_OP

  // The GL context this decoder renders to on behalf of the client.
  gfx::GLContext* context_;

  // A GLContext that is kept in its default state. It is used to perform
  // operations that should not be dependent on client set GLContext state, like
  // clearing a render buffer when it is created.
  // TODO(apatrick): Decoders in the same ContextGroup could potentially share
  // the same default GL context.
  scoped_ptr<gfx::GLContext> default_context_;

  // A parent decoder can access this decoders saved offscreen frame buffer.
  // The parent pointer is reset if the parent is destroyed.
  base::WeakPtr<GLES2DecoderImpl> parent_;

  // Width and height to which an offscreen frame buffer should be resized on
  // the next call to SwapBuffers.
  gfx::Size pending_offscreen_size_;

  // Current GL error bits.
  uint32 error_bits_;

  // Util to help with GL.
  GLES2Util util_;

  // pack alignment as last set by glPixelStorei
  GLint pack_alignment_;

  // unpack alignment as last set by glPixelStorei
  GLint unpack_alignment_;

  // The currently bound array buffer. If this is 0 it is illegal to call
  // glVertexAttribPointer.
  BufferManager::BufferInfo::Ref bound_array_buffer_;

  // The currently bound element array buffer. If this is 0 it is illegal
  // to call glDrawElements.
  BufferManager::BufferInfo::Ref bound_element_array_buffer_;

  // Info for each vertex attribute saved so we can check at glDrawXXX time
  // if it is safe to draw.
  scoped_array<VertexAttribInfo> vertex_attrib_infos_;

  // Current active texture by 0 - n index.
  // In other words, if we call glActiveTexture(GL_TEXTURE2) this value would
  // be 2.
  GLuint active_texture_unit_;

  // Which textures are bound to texture units through glActiveTexture.
  scoped_array<TextureUnit> texture_units_;

  // Black (0,0,0,0) textures for when non-renderable textures are used.
  // NOTE: There is no corresponding TextureInfo for these textures.
  // TextureInfos are only for textures the client side can access.
  GLuint black_2d_texture_id_;
  GLuint black_cube_texture_id_;

  // The program in use by glUseProgram
  ProgramManager::ProgramInfo::Ref current_program_;

  // The currently bound framebuffer
  FramebufferManager::FramebufferInfo::Ref bound_framebuffer_;

  // The currently bound renderbuffer
  RenderbufferManager::RenderbufferInfo::Ref bound_renderbuffer_;

  bool anti_aliased_;

  // The offscreen frame buffer that the client renders to.
  scoped_ptr<FrameBuffer> offscreen_target_frame_buffer_;
  scoped_ptr<Texture> offscreen_target_color_texture_;
  scoped_ptr<RenderBuffer> offscreen_target_depth_stencil_render_buffer_;

  // The copy that is saved when SwapBuffers is called.
  scoped_ptr<Texture> offscreen_saved_color_texture_;

  scoped_ptr<Callback0::Type> swap_buffers_callback_;

  // The last error message set.
  std::string last_error_;

  DISALLOW_COPY_AND_ASSIGN(GLES2DecoderImpl);
};

ScopedGLErrorSuppressor::ScopedGLErrorSuppressor(GLES2DecoderImpl* decoder)
    : decoder_(decoder) {
  decoder_->CopyRealGLErrorsToWrapper();
}

ScopedGLErrorSuppressor::~ScopedGLErrorSuppressor() {
  decoder_->ClearRealGLErrors();
}

ScopedTexture2DBinder::ScopedTexture2DBinder(GLES2DecoderImpl* decoder,
                                             GLuint id)
    : decoder_(decoder) {
  ScopedGLErrorSuppressor suppressor(decoder_);

  // TODO(apatrick): Check if there are any other states that need to be reset
  // before binding a new texture.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, id);
}

ScopedTexture2DBinder::~ScopedTexture2DBinder() {
  ScopedGLErrorSuppressor suppressor(decoder_);
  GLES2DecoderImpl::TextureUnit& info = decoder_->texture_units_[0];
  GLuint last_id;
  if (info.bound_texture_2d)
    last_id = info.bound_texture_2d->service_id();
  else
    last_id = 0;

  glBindTexture(GL_TEXTURE_2D, last_id);
  glActiveTexture(GL_TEXTURE0 + decoder_->active_texture_unit_);
}

ScopedRenderBufferBinder::ScopedRenderBufferBinder(GLES2DecoderImpl* decoder,
                                                   GLuint id)
    : decoder_(decoder) {
  ScopedGLErrorSuppressor suppressor(decoder_);
  glBindRenderbufferEXT(GL_RENDERBUFFER, id);
}

ScopedRenderBufferBinder::~ScopedRenderBufferBinder() {
  ScopedGLErrorSuppressor suppressor(decoder_);
  glBindRenderbufferEXT(
      GL_RENDERBUFFER,
      decoder_->bound_renderbuffer_ ?
          decoder_->bound_renderbuffer_->service_id() : 0);
}

ScopedFrameBufferBinder::ScopedFrameBufferBinder(GLES2DecoderImpl* decoder,
                                                 GLuint id)
    : decoder_(decoder) {
  ScopedGLErrorSuppressor suppressor(decoder_);
  glBindFramebufferEXT(GL_FRAMEBUFFER, id);
}

ScopedFrameBufferBinder::~ScopedFrameBufferBinder() {
  ScopedGLErrorSuppressor suppressor(decoder_);
  FramebufferManager::FramebufferInfo* info =
      decoder_->bound_framebuffer_.get();
  GLuint framebuffer_id = info ? info->service_id() : 0;
  if (framebuffer_id == 0 &&
      decoder_->offscreen_target_frame_buffer_.get()) {
    glBindFramebufferEXT(GL_FRAMEBUFFER,
                         decoder_->offscreen_target_frame_buffer_->id());
  } else {
    glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer_id);
  }
}

ScopedDefaultGLContext::ScopedDefaultGLContext(GLES2DecoderImpl* decoder)
    : decoder_(decoder) {
  decoder_->default_context_->MakeCurrent();
}

ScopedDefaultGLContext::~ScopedDefaultGLContext() {
  decoder_->context_->MakeCurrent();
}

Texture::Texture(GLES2DecoderImpl* decoder)
    : decoder_(decoder),
      id_(0) {
}

Texture::~Texture() {
  // This does not destroy the render texture because that would require that
  // the associated GL context was current. Just check that it was explicitly
  // destroyed.
  DCHECK_EQ(id_, 0u);
}

void Texture::Create() {
  ScopedGLErrorSuppressor suppressor(decoder_);
  Destroy();
  glGenTextures(1, &id_);
}

bool Texture::AllocateStorage(const gfx::Size& size) {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedTexture2DBinder binder(decoder_, id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

  glTexImage2D(GL_TEXTURE_2D,
               0,  // mip level
               GL_RGBA,
               size.width(),
               size.height(),
               0,  // border
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               NULL);

  size_ = size;

  return glGetError() == GL_NO_ERROR;
}

void Texture::Copy(const gfx::Size& size) {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedTexture2DBinder binder(decoder_, id_);
  glCopyTexImage2D(GL_TEXTURE_2D,
                   0,  // level
                   GL_RGBA,
                   0, 0,
                   size.width(),
                   size.height(),
                   0);  // border
}

void Texture::Destroy() {
  if (id_ != 0) {
    ScopedGLErrorSuppressor suppressor(decoder_);
    glDeleteTextures(1, &id_);
    id_ = 0;
  }
}

RenderBuffer::RenderBuffer(GLES2DecoderImpl* decoder)
    : decoder_(decoder),
      id_(0) {
}

RenderBuffer::~RenderBuffer() {
  // This does not destroy the render buffer because that would require that
  // the associated GL context was current. Just check that it was explicitly
  // destroyed.
  DCHECK_EQ(id_, 0u);
}

void RenderBuffer::Create() {
  ScopedGLErrorSuppressor suppressor(decoder_);
  Destroy();
  glGenRenderbuffersEXT(1, &id_);
}

bool RenderBuffer::AllocateStorage(const gfx::Size& size, GLenum format) {
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedRenderBufferBinder binder(decoder_, id_);
  glRenderbufferStorageEXT(GL_RENDERBUFFER,
                           format,
                           size.width(),
                           size.height());
  return glGetError() == GL_NO_ERROR;
}

void RenderBuffer::Destroy() {
  if (id_ != 0) {
    ScopedGLErrorSuppressor suppressor(decoder_);
    glDeleteRenderbuffersEXT(1, &id_);
    id_ = 0;
  }
}

FrameBuffer::FrameBuffer(GLES2DecoderImpl* decoder)
    : decoder_(decoder),
      id_(0) {
}

FrameBuffer::~FrameBuffer() {
  // This does not destroy the frame buffer because that would require that
  // the associated GL context was current. Just check that it was explicitly
  // destroyed.
  DCHECK_EQ(id_, 0u);
}

void FrameBuffer::Create() {
  ScopedGLErrorSuppressor suppressor(decoder_);
  Destroy();
  glGenFramebuffersEXT(1, &id_);
}

void FrameBuffer::AttachRenderTexture(Texture* texture) {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedFrameBufferBinder binder(decoder_, id_);
  GLuint attach_id = texture ? texture->id() : 0;
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
                            GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D,
                            attach_id,
                            0);
}

void FrameBuffer::AttachDepthStencilRenderBuffer(RenderBuffer* render_buffer) {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedFrameBufferBinder binder(decoder_, id_);
  GLuint attach_id = render_buffer ? render_buffer->id() : 0;
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER,
                               GL_DEPTH_ATTACHMENT,
                               GL_RENDERBUFFER,
                               attach_id);
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER,
                               GL_STENCIL_ATTACHMENT,
                               GL_RENDERBUFFER,
                               attach_id);
}

void FrameBuffer::Clear(GLbitfield buffers) {
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedFrameBufferBinder binder(decoder_, id_);
  glClear(buffers);
}

void FrameBuffer::Destroy() {
  if (id_ != 0) {
    ScopedGLErrorSuppressor suppressor(decoder_);
    glDeleteFramebuffersEXT(1, &id_);
    id_ = 0;
  }
}

GLenum FrameBuffer::CheckStatus() {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedFrameBufferBinder binder(decoder_, id_);
  return glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
}

GLES2Decoder* GLES2Decoder::Create(ContextGroup* group) {
  return new GLES2DecoderImpl(group);
}

GLES2DecoderImpl::GLES2DecoderImpl(ContextGroup* group)
    : GLES2Decoder(group),
      context_(NULL),
      error_bits_(0),
      util_(0),  // TODO(gman): Set to actual num compress texture formats.
      pack_alignment_(4),
      unpack_alignment_(4),
      active_texture_unit_(0),
      black_2d_texture_id_(0),
      black_cube_texture_id_(0),
      anti_aliased_(false) {
}

bool GLES2DecoderImpl::Initialize(gfx::GLContext* context,
                                  const gfx::Size& size,
                                  GLES2Decoder* parent,
                                  uint32 parent_client_texture_id) {
  DCHECK(context);
  DCHECK(!context_);
  context_ = context;

  // Create a GL context that is kept in a default state and shares a namespace
  // with the main GL context.
  default_context_.reset(gfx::GLContext::CreateOffscreenGLContext(
      context_->GetHandle()));
  if (!default_context_.get()) {
    Destroy();
    return false;
  }

  // Keep only a weak pointer to the parent so we don't unmap its client
  // frame buffer after it has been destroyed.
  if (parent)
    parent_ = static_cast<GLES2DecoderImpl*>(parent)->AsWeakPtr();

  if (!MakeCurrent()) {
    Destroy();
    return false;
  }

  CHECK_GL_ERROR();

  if (!group_->Initialize()) {
    Destroy();
    return false;
  }

  vertex_attrib_infos_.reset(
      new VertexAttribInfo[group_->max_vertex_attribs()]);
  texture_units_.reset(
      new TextureUnit[group_->max_texture_units()]);
  for (uint32 tt = 0; tt < group_->max_texture_units(); ++tt) {
    texture_units_[tt].bound_texture_2d =
        texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_2D);
    texture_units_[tt].bound_texture_cube_map =
        texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_CUBE_MAP);
  }
  GLuint ids[2];
  glGenTextures(2, ids);
  // Make black textures for replacing non-renderable textures.
  black_2d_texture_id_ = ids[0];
  black_cube_texture_id_ = ids[1];
  static int8 black[] = {0, 0, 0, 0};
  glBindTexture(GL_TEXTURE_2D, black_2d_texture_id_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, black);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, black_cube_texture_id_);
  for (int ii = 0; ii < GLES2Util::kNumFaces; ++ii) {
    glTexImage2D(GLES2Util::IndexToGLFaceTarget(ii), 0, GL_RGBA, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, black);
  }
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  CHECK_GL_ERROR();

#if !defined(UNIT_TEST)
  if (context_->IsOffscreen()) {
    // Create the target frame buffer. This is the one that the client renders
    // directly to.
    offscreen_target_frame_buffer_.reset(new FrameBuffer(this));
    offscreen_target_frame_buffer_->Create();
    offscreen_target_color_texture_.reset(new Texture(this));
    offscreen_target_color_texture_->Create();
    offscreen_target_depth_stencil_render_buffer_.reset(
        new RenderBuffer(this));
    offscreen_target_depth_stencil_render_buffer_->Create();

    // Create the saved offscreen texture. The target frame buffer is copied
    // here when SwapBuffers is called.
    offscreen_saved_color_texture_.reset(new Texture(this));
    offscreen_saved_color_texture_->Create();

    // Map the ID of the saved offscreen texture into the parent so that
    // it can reference it.
    if (parent_) {
      GLuint service_id = offscreen_saved_color_texture_->id();
      TextureManager::TextureInfo* info =
          parent_->CreateTextureInfo(parent_client_texture_id, service_id);
      parent_->texture_manager()->SetInfoTarget(info, GL_TEXTURE_2D);
    }

    // Allocate the render buffers at their initial size and check the status
    // of the frame buffers is okay.
    pending_offscreen_size_ = size;
    if (!UpdateOffscreenFrameBufferSize()) {
      DLOG(ERROR) << "Could not allocate offscreen buffer storage.";
      Destroy();
      return false;
    }

    // Bind to the new default frame buffer (the offscreen target frame buffer).
    // This should now be associated with ID zero.
    DoBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
#endif  // UNIT_TEST

#if !defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
  // OpenGL ES 2.0 implicitly enables the desktop GL capability
  // VERTEX_PROGRAM_POINT_SIZE and doesn't expose this enum. This fact
  // isn't well documented; it was discovered in the Khronos OpenGL ES
  // mailing list archives.
  glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

#if defined(GLES2_GPU_SERVICE_TRANSLATE_SHADER)
  // Initialize GLSL ES to GLSL translator.
  if (!ShInitialize()) {
    DLOG(ERROR) << "Could not initialize GLSL translator.";
    Destroy();
    return false;
  }
#endif  // GLES2_GPU_SERVICE_TRANSLATE_SHADER
#endif  // GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2

  return true;
}

bool GLES2DecoderImpl::GenBuffersHelper(GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetBufferInfo(client_ids[ii])) {
      return false;
    }
  }
  scoped_array<GLuint> service_ids(new GLuint[n]);
  glGenBuffersARB(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateBufferInfo(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenFramebuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetFramebufferInfo(client_ids[ii])) {
      return false;
    }
  }
  scoped_array<GLuint> service_ids(new GLuint[n]);
  glGenFramebuffersEXT(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateFramebufferInfo(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenRenderbuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetRenderbufferInfo(client_ids[ii])) {
      return false;
    }
  }
  scoped_array<GLuint> service_ids(new GLuint[n]);
  glGenRenderbuffersEXT(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateRenderbufferInfo(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenTexturesHelper(GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetTextureInfo(client_ids[ii])) {
      return false;
    }
  }
  scoped_array<GLuint> service_ids(new GLuint[n]);
  glGenTextures(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateTextureInfo(client_ids[ii], service_ids[ii]);
  }
  return true;
}

void GLES2DecoderImpl::DeleteBuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    BufferManager::BufferInfo* info = GetBufferInfo(client_ids[ii]);
    if (info) {
      GLuint service_id = info->service_id();
      glDeleteBuffersARB(1, &service_id);
      RemoveBufferInfo(client_ids[ii]);
    }
  }
}

void GLES2DecoderImpl::DeleteFramebuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    FramebufferManager::FramebufferInfo* info =
        GetFramebufferInfo(client_ids[ii]);
    if (info) {
      GLuint service_id = info->service_id();
      glDeleteFramebuffersEXT(1, &service_id);
      RemoveFramebufferInfo(client_ids[ii]);
    }
  }
}

void GLES2DecoderImpl::DeleteRenderbuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    RenderbufferManager::RenderbufferInfo* info =
        GetRenderbufferInfo(client_ids[ii]);
    if (info) {
      GLuint service_id = info->service_id();
      glDeleteRenderbuffersEXT(1, &service_id);
      RemoveRenderbufferInfo(client_ids[ii]);
    }
  }
}

void GLES2DecoderImpl::DeleteTexturesHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    TextureManager::TextureInfo* info = GetTextureInfo(client_ids[ii]);
    if (info) {
      GLuint service_id = info->service_id();
      glDeleteTextures(1, &service_id);
      RemoveTextureInfo(client_ids[ii]);
    }
  }
}

// }  // anonymous namespace

bool GLES2DecoderImpl::MakeCurrent() {
#if defined(UNIT_TEST)
  return true;
#else
  return context_->MakeCurrent();
#endif
}

gfx::Size GLES2DecoderImpl::GetBoundFrameBufferSize() {
  if (bound_framebuffer_ != 0) {
    int width = 0;
    int height = 0;

    // Assume we have to have COLOR_ATTACHMENT0. Should we check for depth and
    // stencil.
    GLint fb_type = 0;
    glGetFramebufferAttachmentParameterivEXT(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
        &fb_type);
    switch (fb_type) {
      case GL_RENDERBUFFER:
        {
          GLint renderbuffer_id = 0;
          glGetFramebufferAttachmentParameterivEXT(
              GL_FRAMEBUFFER,
              GL_COLOR_ATTACHMENT0,
              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
              &renderbuffer_id);
          if (renderbuffer_id != 0) {
            glGetRenderbufferParameterivEXT(
                GL_RENDERBUFFER,
                GL_RENDERBUFFER_WIDTH,
                &width);
            glGetRenderbufferParameterivEXT(
                GL_RENDERBUFFER,
                GL_RENDERBUFFER_HEIGHT,
                &height);
          }
          break;
        }
      case GL_TEXTURE:
        {
          GLint texture_id = 0;
          glGetFramebufferAttachmentParameterivEXT(
              GL_FRAMEBUFFER,
              GL_COLOR_ATTACHMENT0,
              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
              &texture_id);
          if (texture_id != 0) {
            GLuint client_id = 0;
            if (texture_manager()->GetClientId(texture_id, &client_id)) {
              TextureManager::TextureInfo* texture_info =
                 GetTextureInfo(client_id);
              if (texture_info) {
                GLint level = 0;
                GLint face = 0;
                glGetFramebufferAttachmentParameterivEXT(
                    GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0,
                    GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL,
                    &level);
                glGetFramebufferAttachmentParameterivEXT(
                    GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0,
                    GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE,
                    &face);
                texture_info->GetLevelSize(
                    face ? face : GL_TEXTURE_2D, level, &width, &height);
              }
            }
          }
          break;
        }
      default:
        // unknown so assume width and height are zero.
        break;
    }

    return gfx::Size(width, height);
  } else if (offscreen_target_color_texture_.get()) {
    return offscreen_target_color_texture_->size();
  } else {
#if defined(UNIT_TEST)
    return gfx::Size(INT_MAX, INT_MAX);
#else
    return context_->GetSize();
#endif
  }
}

bool GLES2DecoderImpl::UpdateOffscreenFrameBufferSize() {
  if (offscreen_target_color_texture_->size() == pending_offscreen_size_)
    return true;

  if (parent_) {
    // Create the saved offscreen color texture (only accessible to parent).
    offscreen_saved_color_texture_->AllocateStorage(pending_offscreen_size_);

    // Attach the saved offscreen color texture to a frame buffer so we can
    // clear it with glClear.
    offscreen_target_frame_buffer_->AttachRenderTexture(
        offscreen_saved_color_texture_.get());
    if (offscreen_target_frame_buffer_->CheckStatus() !=
        GL_FRAMEBUFFER_COMPLETE) {
      return false;
    }

#if !defined(UNIT_TEST)
    // Clear the saved offscreen color texture. Use default GL context
    // to ensure clear is not affected by client set state.
    {  // NOLINT
      ScopedDefaultGLContext scoped_context(this);
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,
                           offscreen_target_frame_buffer_->id());
      glClear(GL_COLOR_BUFFER_BIT);
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

      if (glGetError() != GL_NO_ERROR)
        return false;
    }
#endif
  }

  // Reallocate the offscreen target buffers.
  if (!offscreen_target_color_texture_->AllocateStorage(
      pending_offscreen_size_)) {
    return false;
  }

  if (!offscreen_target_depth_stencil_render_buffer_->AllocateStorage(
      pending_offscreen_size_, GL_DEPTH24_STENCIL8)) {
    return false;
  }

  // Attach the offscreen target buffers to the target frame buffer.
  offscreen_target_frame_buffer_->AttachRenderTexture(
      offscreen_target_color_texture_.get());
  offscreen_target_frame_buffer_->AttachDepthStencilRenderBuffer(
      offscreen_target_depth_stencil_render_buffer_.get());
  if (offscreen_target_frame_buffer_->CheckStatus() !=
      GL_FRAMEBUFFER_COMPLETE) {
    return false;
  }

#if !defined(UNIT_TEST)
  // Clear offscreen frame buffer to its initial state. Use default GL context
  // to ensure clear is not affected by client set state.
  {  // NOLINT
    ScopedDefaultGLContext scoped_context(this);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,
                         offscreen_target_frame_buffer_->id());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    if (glGetError() != GL_NO_ERROR)
      return false;
  }
#endif

  if (parent_) {
    // Create the saved offscreen color texture (only accessible to parent).
    offscreen_saved_color_texture_->AllocateStorage(pending_offscreen_size_);

    // Update the info about the offscreen saved color texture in the parent.
    // The reference to the parent is a weak pointer and will become null if the
    // parent is later destroyed.
    GLuint service_id = offscreen_saved_color_texture_->id();

    TextureManager::TextureInfo* info =
        parent_->texture_manager()->GetTextureInfo(service_id);
    DCHECK(info);

    info->SetLevelInfo(GL_TEXTURE_2D,
                       0,  // level
                       GL_RGBA,
                       pending_offscreen_size_.width(),
                       pending_offscreen_size_.height(),
                       1,  // depth
                       0,  // border
                       GL_RGBA,
                       GL_UNSIGNED_BYTE);
  }

  return true;
}

void GLES2DecoderImpl::SetSwapBuffersCallback(Callback0::Type* callback) {
  swap_buffers_callback_.reset(callback);
}

void GLES2DecoderImpl::Destroy() {
  if (context_) {
    MakeCurrent();

    // Remove the saved frame buffer mapping from the parent decoder. The
    // parent pointer is a weak pointer so it will be null if the parent has
    // already been destroyed.
    if (parent_) {
      // First check the texture has been mapped into the parent. This might not
      // be the case if initialization failed midway through.
      GLuint service_id = offscreen_saved_color_texture_->id();
      GLuint client_id = 0;
      if (parent_->texture_manager()->GetClientId(service_id, &client_id)) {
        parent_->texture_manager()->RemoveTextureInfo(client_id);
      }
    }

    if (offscreen_target_frame_buffer_.get()) {
      offscreen_target_frame_buffer_->Destroy();
      offscreen_target_frame_buffer_.reset();
    }

    if (offscreen_target_color_texture_.get()) {
      offscreen_target_color_texture_->Destroy();
      offscreen_target_color_texture_.reset();
    }

    if (offscreen_target_depth_stencil_render_buffer_.get()) {
      offscreen_target_depth_stencil_render_buffer_->Destroy();
      offscreen_target_depth_stencil_render_buffer_.reset();
    }

    if (offscreen_saved_color_texture_.get()) {
      offscreen_saved_color_texture_->Destroy();
      offscreen_saved_color_texture_.reset();
    }
  }

  if (default_context_.get()) {
    default_context_->Destroy();
    default_context_.reset();
  }

#if !defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
#if defined(GLES2_GPU_SERVICE_TRANSLATE_SHADER)
  // Terminate GLSL translator.
  ShFinalize();
#endif  // GLES2_GPU_SERVICE_TRANSLATE_SHADER
#endif  // GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2
}

void GLES2DecoderImpl::ResizeOffscreenFrameBuffer(const gfx::Size& size) {
  // We can't resize the render buffers immediately because there might be a
  // partial frame rendered into them and we don't want the tail end of that
  // rendered into the reallocated storage. Defer until the next SwapBuffers.
  pending_offscreen_size_ = size;
}

const char* GLES2DecoderImpl::GetCommandName(unsigned int command_id) const {
  if (command_id > kStartPoint && command_id < kNumCommands) {
    return gles2::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

// Decode command with its arguments, and call the corresponding GL function.
// Note: args is a pointer to the command buffer. As such, it could be changed
// by a (malicious) client at any time, so if validation has to happen, it
// should operate on a copy of them.
error::Error GLES2DecoderImpl::DoCommand(
    unsigned int command,
    unsigned int arg_count,
    const void* cmd_data) {
  error::Error result = error::kNoError;
  if (debug()) {
    // TODO(gman): Change output to something useful for NaCl.
    printf("cmd: %s\n", GetCommandName(command));
  }
  unsigned int command_index = command - kStartPoint - 1;
  if (command_index < arraysize(g_command_info)) {
    const CommandInfo& info = g_command_info[command_index];
    unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
    if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
        (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
      uint32 immediate_data_size =
          (arg_count - info_arg_count) * sizeof(CommandBufferEntry);  // NOLINT
      switch (command) {
        #define GLES2_CMD_OP(name)                                 \
          case name::kCmdId:                                       \
            result = Handle ## name(                               \
                immediate_data_size,                               \
                *static_cast<const name*>(cmd_data));              \
            break;                                                 \

        GLES2_COMMAND_LIST(GLES2_CMD_OP)
        #undef GLES2_CMD_OP
      }
      if (debug()) {
        GLenum error;
        while ((error = glGetError()) != GL_NO_ERROR) {
          // TODO(gman): Change output to something useful for NaCl.
          SetGLError(error, NULL);
          printf("GL ERROR b4: %s\n", GetCommandName(command));
        }
      }
    } else {
      result = error::kInvalidArguments;
    }
  } else {
    result = DoCommonCommand(command, arg_count, cmd_data);
  }
  return result;
}

void GLES2DecoderImpl::RemoveBufferInfo(GLuint client_id) {
  buffer_manager()->RemoveBufferInfo(client_id);
}

bool GLES2DecoderImpl::CreateProgramHelper(GLuint client_id) {
  if (GetProgramInfo(client_id)) {
    return false;
  }
  GLuint service_id = glCreateProgram();
  if (service_id != 0) {
    CreateProgramInfo(client_id, service_id);
  }
  return true;
}

bool GLES2DecoderImpl::CreateShaderHelper(GLenum type, GLuint client_id) {
  if (GetShaderInfo(client_id)) {
    return false;
  }
  GLuint service_id = glCreateShader(type);
  if (service_id != 0) {
    CreateShaderInfo(client_id, service_id, type);
  }
  return true;
}

bool GLES2DecoderImpl::ValidateGLenumCompressedTextureInternalFormat(GLenum) {
  // TODO(gman): Add support for compressed texture formats.
  return false;
}

void GLES2DecoderImpl::DoActiveTexture(GLenum texture_unit) {
  GLuint texture_index = texture_unit - GL_TEXTURE0;
  if (texture_index > group_->max_texture_units()) {
    SetGLError(GL_INVALID_ENUM, "glActiveTexture: texture_unit out of range.");
    return;
  }
  active_texture_unit_ = texture_index;
  glActiveTexture(texture_unit);
}

void GLES2DecoderImpl::DoBindBuffer(GLenum target, GLuint client_id) {
  BufferManager::BufferInfo* info = NULL;
  GLuint service_id = 0;
  if (client_id != 0) {
    info = GetBufferInfo(client_id);
    if (!info) {
      // It's a new id so make a buffer info for it.
      glGenBuffersARB(1, &service_id);
      CreateBufferInfo(client_id, service_id);
      info = GetBufferInfo(client_id);
    }
  }
  if (info) {
    // Check the buffer exists
    // Check that we are not trying to bind it to a different target.
    if ((info->target() != 0 && info->target() != target)) {
      SetGLError(GL_INVALID_OPERATION,
                 "glBindBuffer: buffer bound to more than 1 target");
      return;
    }
    if (info->target() == 0) {
      info->set_target(target);
    }
    service_id = info->service_id();
  }
  switch (target) {
    case GL_ARRAY_BUFFER:
      bound_array_buffer_ = info;
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      bound_element_array_buffer_ = info;
      break;
    default:
      NOTREACHED();  // Validation should prevent us getting here.
      break;
  }
  glBindBuffer(target, service_id);
}

void GLES2DecoderImpl::DoBindFramebuffer(GLenum target, GLuint client_id) {
  FramebufferManager::FramebufferInfo* info = NULL;
  GLuint service_id = 0;
  if (client_id != 0) {
    info = GetFramebufferInfo(client_id);
    if (!info) {
      // It's a new id so make a framebuffer info for it.
      glGenFramebuffersEXT(1, &service_id);
      CreateFramebufferInfo(client_id, service_id);
      info = GetFramebufferInfo(client_id);
    } else {
      service_id = info->service_id();
    }
  }
  bound_framebuffer_ = info;

  // When rendering to an offscreen frame buffer, instead of unbinding from
  // the current frame buffer, bind to the offscreen target frame buffer.
  if (info == NULL && offscreen_target_frame_buffer_.get())
    service_id = offscreen_target_frame_buffer_->id();

  glBindFramebufferEXT(target, service_id);
}

void GLES2DecoderImpl::DoBindRenderbuffer(GLenum target, GLuint client_id) {
  RenderbufferManager::RenderbufferInfo* info = NULL;
  GLuint service_id = 0;
  if (client_id != 0) {
    info = GetRenderbufferInfo(client_id);
    if (!info) {
      // It's a new id so make a renderbuffer info for it.
      glGenRenderbuffersEXT(1, &service_id);
      CreateRenderbufferInfo(client_id, service_id);
    } else {
      service_id = info->service_id();
    }
  }
  bound_renderbuffer_ = info;
  glBindRenderbufferEXT(target, service_id);
}

void GLES2DecoderImpl::DoBindTexture(GLenum target, GLuint client_id) {
  TextureManager::TextureInfo* info = NULL;
  GLuint service_id = 0;
  if (client_id != 0) {
    info = GetTextureInfo(client_id);
    if (!info) {
      // It's a new id so make a texture info for it.
      glGenTextures(1, &service_id);
      CreateTextureInfo(client_id, service_id);
      info = GetTextureInfo(client_id);
    }
  } else {
    info = texture_manager()->GetDefaultTextureInfo(target);
  }

  // Check the texture exists
  // Check that we are not trying to bind it to a different target.
  if (info->target() != 0 && info->target() != target) {
    SetGLError(GL_INVALID_OPERATION,
               "glBindTexture: texture bound to more than 1 target.");
    return;
  }
  if (info->target() == 0) {
    texture_manager()->SetInfoTarget(info, target);
  }
  glBindTexture(target, info->service_id());
  TextureUnit& unit = texture_units_[active_texture_unit_];
  unit.bind_target = target;
  switch (target) {
    case GL_TEXTURE_2D:
      unit.bound_texture_2d = info;
      break;
    case GL_TEXTURE_CUBE_MAP:
      unit.bound_texture_cube_map = info;
      break;
    default:
      NOTREACHED();  // Validation should prevent us getting here.
      break;
  }
}

void GLES2DecoderImpl::DoDisableVertexAttribArray(GLuint index) {
  if (index < group_->max_vertex_attribs()) {
    vertex_attrib_infos_[index].set_enabled(false);
    glDisableVertexAttribArray(index);
  } else {
    SetGLError(GL_INVALID_VALUE,
               "glDisableVertexAttribArray: index out of range");
  }
}

void GLES2DecoderImpl::DoEnableVertexAttribArray(GLuint index) {
  if (index < group_->max_vertex_attribs()) {
    vertex_attrib_infos_[index].set_enabled(true);
    glEnableVertexAttribArray(index);
  } else {
    SetGLError(GL_INVALID_VALUE,
               "glEnableVertexAttribArray: index out of range");
  }
}

void GLES2DecoderImpl::DoGenerateMipmap(GLenum target) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info || !info->MarkMipmapsGenerated()) {
    SetGLError(GL_INVALID_OPERATION,
               "glGenerateMipmaps: Can not generate mips for npot textures");
    return;
  }
  glGenerateMipmapEXT(target);
}

bool GLES2DecoderImpl::GetHelper(
    GLenum pname, GLint* params, GLsizei* num_written) {
  DCHECK(params);
  DCHECK(num_written);
  switch (pname) {
#if !defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      *num_written = 1;
      *params = GL_RGBA;  // TODO(gman): get correct format.
      return true;
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      *num_written = 1;
      *params = GL_UNSIGNED_BYTE;  // TODO(gman): get correct type.
      return true;
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, params);
      *num_written = 1;
      *params /= 4;
      return true;
    case GL_MAX_VARYING_VECTORS:
      glGetIntegerv(GL_MAX_VARYING_FLOATS, params);
      *num_written = 1;
      *params /= 4;
      return true;
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
      glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, params);
      *num_written = 1;
      *params /= 4;
      return true;
#endif
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      *num_written = 1;
      *params = 0;  // We don't support compressed textures.
      return true;
    case GL_NUM_SHADER_BINARY_FORMATS:
      *num_written = 1;
      *params = 0;  // We don't support binary shader formats.
      return true;
    case GL_SHADER_BINARY_FORMATS:
      *num_written = 0;
      return true;  // We don't support binary shader format.s
    case GL_SHADER_COMPILER:
      *num_written = 1;
      *params = GL_TRUE;
      return true;
    default:
      return false;
  }
}

void GLES2DecoderImpl::DoGetBooleanv(GLenum pname, GLboolean* params) {
  DCHECK(params);
  GLint values[16];
  GLsizei num_written;
  if (GetHelper(pname, &values[0], &num_written)) {
    DCHECK_LE(static_cast<size_t>(num_written), arraysize(values));
    for (GLsizei ii = 0; ii < num_written; ++ii) {
      params[ii] = static_cast<GLboolean>(values[ii]);
    }
  } else {
    glGetBooleanv(pname, params);
  }
}

void GLES2DecoderImpl::DoGetFloatv(GLenum pname, GLfloat* params) {
  DCHECK(params);
  GLint values[16];
  GLsizei num_written;
  if (GetHelper(pname, &values[0], &num_written)) {
    DCHECK_LE(static_cast<size_t>(num_written), arraysize(values));
    for (GLsizei ii = 0; ii < num_written; ++ii) {
      params[ii] = static_cast<GLfloat>(values[ii]);
    }
  } else {
    glGetFloatv(pname, params);
  }
}

void GLES2DecoderImpl::DoGetIntegerv(GLenum pname, GLint* params) {
  DCHECK(params);
  GLsizei num_written;
  if (!GetHelper(pname, params, &num_written)) {
    glGetIntegerv(pname, params);
  }
}

void GLES2DecoderImpl::DoGetProgramiv(
    GLuint program_id, GLenum pname, GLint* params) {
  ProgramManager::ProgramInfo* info = GetProgramInfo(program_id);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetProgramiv: unknown program");
    return;
  }
  info->GetProgramiv(pname, params);
}

error::Error GLES2DecoderImpl::HandleBindAttribLocation(
    uint32 immediate_data_size, const gles2::BindAttribLocation& c) {
  ProgramManager::ProgramInfo* info = GetProgramInfo(c.program);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glBindAttribLocation: unknown program");
    return error::kNoError;
  }
  GLuint index = static_cast<GLuint>(c.index);
  uint32 name_size = c.data_size;
  const char* name = GetSharedMemoryAs<const char*>(
      c.name_shm_id, c.name_shm_offset, name_size);
  if (name == NULL) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  glBindAttribLocation(info->service_id(), index, name_str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindAttribLocationImmediate(
    uint32 immediate_data_size, const gles2::BindAttribLocationImmediate& c) {
  ProgramManager::ProgramInfo* info = GetProgramInfo(c.program);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glBindAttribLocation: unknown program");
    return error::kNoError;
  }
  GLuint index = static_cast<GLuint>(c.index);
  uint32 name_size = c.data_size;
  const char* name = GetImmediateDataAs<const char*>(
      c, name_size, immediate_data_size);
  if (name == NULL) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  glBindAttribLocation(info->service_id(), index, name_str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindAttribLocationBucket(
    uint32 immediate_data_size, const gles2::BindAttribLocationBucket& c) {
  ProgramManager::ProgramInfo* info = GetProgramInfo(c.program);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glBindAttribLocation: unknown program");
    return error::kNoError;
  }
  GLuint index = static_cast<GLuint>(c.index);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  bucket->GetAsString(&name_str);
  glBindAttribLocation(info->service_id(), index, name_str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteShader(
    uint32 immediate_data_size, const gles2::DeleteShader& c) {
  GLuint client_id = c.shader;
  if (client_id) {
    ShaderManager::ShaderInfo* info = GetShaderInfo(client_id);
    if (info) {
      glDeleteShader(info->service_id());
      RemoveShaderInfo(client_id);
    } else {
      SetGLError(GL_INVALID_VALUE, "glDeleteShader: unknown shader");
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteProgram(
    uint32 immediate_data_size, const gles2::DeleteProgram& c) {
  GLuint client_id = c.program;
  if (client_id) {
    ProgramManager::ProgramInfo* info = GetProgramInfo(client_id);
    if (info) {
      glDeleteProgram(info->service_id());
      RemoveProgramInfo(client_id);
    } else {
      SetGLError(GL_INVALID_VALUE, "glDeleteProgram: unknown program");
    }
  }
  return error::kNoError;
}

void GLES2DecoderImpl::DoDrawArrays(
    GLenum mode, GLint first, GLsizei count) {
  if (IsDrawValid(first + count - 1)) {
    bool has_non_renderable_textures;
    SetBlackTextureForNonRenderableTextures(&has_non_renderable_textures);
    glDrawArrays(mode, first, count);
    if (has_non_renderable_textures) {
      RestoreStateForNonRenderableTextures();
    }
  }
}

void GLES2DecoderImpl::DoFramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint client_renderbuffer_id) {
  if (!bound_framebuffer_) {
    SetGLError(GL_INVALID_OPERATION,
               "glFramebufferRenderbuffer: no framebuffer bound");
    return;
  }
  GLuint service_id = 0;
  if (client_renderbuffer_id) {
    RenderbufferManager::RenderbufferInfo* info =
        GetRenderbufferInfo(client_renderbuffer_id);
    if (!info) {
      SetGLError(GL_INVALID_OPERATION,
                 "glFramebufferRenderbuffer: unknown renderbuffer");
      return;
    }
    service_id = info->service_id();
  }
  glFramebufferRenderbufferEXT(
      target, attachment, renderbuffertarget, service_id);
}

GLenum GLES2DecoderImpl::DoCheckFramebufferStatus(GLenum target) {
  if (!bound_framebuffer_) {
    return GL_FRAMEBUFFER_COMPLETE;
  }
  return glCheckFramebufferStatusEXT(target);
}

void GLES2DecoderImpl::DoFramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget,
    GLuint client_texture_id, GLint level) {
  if (!bound_framebuffer_) {
    SetGLError(GL_INVALID_OPERATION,
               "glFramebufferTexture2D: no framebuffer bound.");
    return;
  }
  GLuint service_id = 0;
  if (client_texture_id) {
    TextureManager::TextureInfo* info = GetTextureInfo(client_texture_id);
    if (!info) {
      SetGLError(GL_INVALID_OPERATION,
                 "glFramebufferTexture2D: unknown texture");
      return;
    }
    service_id = info->service_id();
  }
  glFramebufferTexture2DEXT(target, attachment, textarget, service_id, level);
}

void GLES2DecoderImpl::DoGetFramebufferAttachmentParameteriv(
    GLenum target, GLenum attachment, GLenum pname, GLint* params) {
  if (!bound_framebuffer_) {
    SetGLError(GL_INVALID_OPERATION,
               "glFramebufferAttachmentParameteriv: no framebuffer bound");
    return;
  }
  glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, params);
}

void GLES2DecoderImpl::DoGetRenderbufferParameteriv(
    GLenum target, GLenum pname, GLint* params) {
  if (!bound_renderbuffer_) {
    SetGLError(GL_INVALID_OPERATION,
               "glGetRenderbufferParameteriv: no renderbuffer bound");
    return;
  }
  glGetRenderbufferParameterivEXT(target, pname, params);
}

void GLES2DecoderImpl::DoRenderbufferStorage(
  GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  if (!bound_renderbuffer_) {
    SetGLError(GL_INVALID_OPERATION,
               "glGetRenderbufferStorage: no renderbuffer bound");
    return;
  }
  glRenderbufferStorageEXT(target, internalformat, width, height);
}

void GLES2DecoderImpl::DoLinkProgram(GLuint program) {
  ProgramManager::ProgramInfo* info = GetProgramInfo(program);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION, "glLinkProgram: unknown program");
    return;
  }
  CopyRealGLErrorsToWrapper();
  glLinkProgram(info->service_id());
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    info->Reset();
    SetGLError(error, NULL);
  } else {
    info->Update();
  }
};

void GLES2DecoderImpl::DoTexParameterf(
    GLenum target, GLenum pname, GLfloat param) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glTexParameterf: unknown texture");
  } else {
    info->SetParameter(pname, static_cast<GLint>(param));
    glTexParameterf(target, pname, param);
  }
}

void GLES2DecoderImpl::DoTexParameteri(
    GLenum target, GLenum pname, GLint param) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glTexParameteri: unknown texture");
  } else {
    info->SetParameter(pname, param);
    glTexParameteri(target, pname, param);
  }
}

void GLES2DecoderImpl::DoTexParameterfv(
    GLenum target, GLenum pname, const GLfloat* params) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glTexParameterfv: unknown texture");
  } else {
    info->SetParameter(pname, *reinterpret_cast<const GLint*>(params));
    glTexParameterfv(target, pname, params);
  }
}

void GLES2DecoderImpl::DoTexParameteriv(
  GLenum target, GLenum pname, const GLint* params) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glTexParameteriv: unknown texture");
  } else {
    info->SetParameter(pname, *params);
    glTexParameteriv(target, pname, params);
  }
}

void GLES2DecoderImpl::DoUniform1i(GLint location, GLint v0) {
  if (!current_program_ || current_program_->IsDeleted()) {
    // The program does not exist.
    SetGLError(GL_INVALID_OPERATION, "glUniform1i: no program in use");
    return;
  }
  current_program_->SetSamplers(location, 1, &v0);
  glUniform1i(location, v0);
}

void GLES2DecoderImpl::DoUniform1iv(
    GLint location, GLsizei count, const GLint *value) {
  if (!current_program_ || current_program_->IsDeleted()) {
    // The program does not exist.
    SetGLError(GL_INVALID_OPERATION, "glUniform1iv: no program in use");
    return;
  }
  current_program_->SetSamplers(location, count, value);
  glUniform1iv(location, count, value);
}

void GLES2DecoderImpl::DoUseProgram(GLuint program) {
  GLuint service_id = 0;
  ProgramManager::ProgramInfo* info = NULL;
  if (program) {
    info = GetProgramInfo(program);
    if (!info) {
      SetGLError(GL_INVALID_VALUE, "glUseProgram: unknown program");
      return;
    }
    if (!info->IsValid()) {
      // Program was not linked successfully. (ie, glLinkProgram)
      SetGLError(GL_INVALID_OPERATION, "glUseProgram: program not linked");
      return;
    }
    service_id = info->service_id();
  }
  current_program_ = info;
  glUseProgram(service_id);
}

GLenum GLES2DecoderImpl::GetGLError() {
  // Check the GL error first, then our wrapped error.
  GLenum error = glGetError();
  if (error == GL_NO_ERROR && error_bits_ != 0) {
    for (uint32 mask = 1; mask != 0; mask = mask << 1) {
      if ((error_bits_ & mask) != 0) {
        error = GLES2Util::GLErrorBitToGLError(mask);
        break;
      }
    }
  }

  if (error != GL_NO_ERROR) {
    // There was an error, clear the corresponding wrapped error.
    error_bits_ &= ~GLES2Util::GLErrorToErrorBit(error);
  }
  return error;
}

void GLES2DecoderImpl::SetGLError(GLenum error, const char* msg) {
  if (msg) {
    last_error_ = msg;
    DLOG(ERROR) << last_error_;
  }
  error_bits_ |= GLES2Util::GLErrorToErrorBit(error);
}

void GLES2DecoderImpl::CopyRealGLErrorsToWrapper() {
  GLenum error;
  while ((error = glGetError()) != GL_NO_ERROR) {
    SetGLError(error, NULL);
  }
}

void GLES2DecoderImpl::ClearRealGLErrors() {
  GLenum error;
  while ((error = glGetError()) != GL_NO_ERROR) {
    NOTREACHED() << "GL error " << error << " was unhandled.";
  }
}

bool GLES2DecoderImpl::VertexAttribInfo::CanAccess(GLuint index) {
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

void GLES2DecoderImpl::SetBlackTextureForNonRenderableTextures(
    bool* has_non_renderable_textures) {
  DCHECK(has_non_renderable_textures);
  DCHECK(current_program_);
  DCHECK(!current_program_->IsDeleted());
  *has_non_renderable_textures = false;
  const ProgramManager::ProgramInfo::SamplerIndices& sampler_indices =
     current_program_->sampler_indices();
  for (size_t ii = 0; ii < sampler_indices.size(); ++ii) {
    const ProgramManager::ProgramInfo::UniformInfo* uniform_info =
        current_program_->GetUniformInfo(sampler_indices[ii]);
    DCHECK(uniform_info);
    for (size_t jj = 0; jj < uniform_info->texture_units.size(); ++jj) {
      GLuint texture_unit_index = uniform_info->texture_units[jj];
      if (texture_unit_index < group_->max_texture_units()) {
        TextureUnit& texture_unit = texture_units_[texture_unit_index];
        TextureManager::TextureInfo* texture_info =
            uniform_info->type == GL_SAMPLER_2D ?
                texture_unit.bound_texture_2d :
                texture_unit.bound_texture_cube_map;
        if (!texture_info || !texture_info->CanRender()) {
          *has_non_renderable_textures = true;
          glActiveTexture(GL_TEXTURE0 + texture_unit_index);
          glBindTexture(
              uniform_info->type == GL_SAMPLER_2D ? GL_TEXTURE_2D :
                                                    GL_TEXTURE_CUBE_MAP,
              uniform_info->type == GL_SAMPLER_2D ? black_2d_texture_id_ :
                                                    black_cube_texture_id_);
        }
      }
      // else: should this be an error?
    }
  }
}

void GLES2DecoderImpl::RestoreStateForNonRenderableTextures() {
  DCHECK(current_program_);
  DCHECK(!current_program_->IsDeleted());
  const ProgramManager::ProgramInfo::SamplerIndices& sampler_indices =
     current_program_->sampler_indices();
  for (size_t ii = 0; ii < sampler_indices.size(); ++ii) {
    const ProgramManager::ProgramInfo::UniformInfo* uniform_info =
        current_program_->GetUniformInfo(sampler_indices[ii]);
    DCHECK(uniform_info);
    for (size_t jj = 0; jj < uniform_info->texture_units.size(); ++jj) {
      GLuint texture_unit_index = uniform_info->texture_units[jj];
      if (texture_unit_index < group_->max_texture_units()) {
        TextureUnit& texture_unit = texture_units_[texture_unit_index];
        TextureManager::TextureInfo* texture_info =
            uniform_info->type == GL_SAMPLER_2D ?
                texture_unit.bound_texture_2d :
                texture_unit.bound_texture_cube_map;
        if (!texture_info || !texture_info->CanRender()) {
          glActiveTexture(GL_TEXTURE0 + texture_unit_index);
          // Get the texture info that was previously bound here.
          texture_info = texture_unit.bind_target == GL_TEXTURE_2D ?
              texture_unit.bound_texture_2d :
              texture_unit.bound_texture_cube_map;
          glBindTexture(texture_unit.bind_target,
                        texture_info ? texture_info->service_id() : 0);
        }
      }
    }
  }
  // Set the active texture back to whatever the user had it as.
  glActiveTexture(GL_TEXTURE0 + active_texture_unit_);
}

bool GLES2DecoderImpl::IsDrawValid(GLuint max_vertex_accessed) {
  if (!current_program_ || current_program_->IsDeleted()) {
    // The program does not exist.
    // But GL says no ERROR.
    return false;
  }
  // Validate that all attribs current program needs are setup correctly.
  const ProgramManager::ProgramInfo::AttribInfoVector& infos =
      current_program_->GetAttribInfos();
  for (size_t ii = 0; ii < infos.size(); ++ii) {
    GLint location = infos[ii].location;
    if (location < 0) {
      return false;
    }
    DCHECK_LT(static_cast<GLuint>(location), group_->max_vertex_attribs());
    if (!vertex_attrib_infos_[location].CanAccess(max_vertex_accessed)) {
      SetGLError(GL_INVALID_OPERATION,
                 "glDrawXXX: attempt to access out of range vertices");
      return false;
    }
  }
  return true;
};

error::Error GLES2DecoderImpl::HandleDrawElements(
    uint32 immediate_data_size, const gles2::DrawElements& c) {
  if (!bound_element_array_buffer_ ||
      bound_element_array_buffer_->IsDeleted()) {
    SetGLError(GL_INVALID_OPERATION,
               "glDrawElements: No element array buffer bound");
    return error::kNoError;
  }

  GLenum mode = c.mode;
  GLsizei count = c.count;
  GLenum type = c.type;
  int32 offset = c.index_offset;
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawElements: count < 0");
    return error::kNoError;
  }
  if (offset < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawElements: offset < 0");
    return error::kNoError;
  }
  if (!ValidateGLenumDrawMode(mode)) {
    SetGLError(GL_INVALID_ENUM, "glDrawElements: mode GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!ValidateGLenumIndexType(type)) {
    SetGLError(GL_INVALID_ENUM, "glDrawElements: type GL_INVALID_ENUM");
    return error::kNoError;
  }

  GLuint max_vertex_accessed;
  if (!bound_element_array_buffer_->GetMaxValueForRange(
      offset, count, type, &max_vertex_accessed)) {
    SetGLError(GL_INVALID_OPERATION,
               "glDrawElements: range out of bounds for buffer");
    return error::kNoError;
  }

  if (IsDrawValid(max_vertex_accessed)) {
    bool has_non_renderable_textures;
    SetBlackTextureForNonRenderableTextures(
        &has_non_renderable_textures);
    const GLvoid* indices = reinterpret_cast<const GLvoid*>(offset);
    glDrawElements(mode, count, type, indices);
    if (has_non_renderable_textures) {
      RestoreStateForNonRenderableTextures();
    }
  }
  return error::kNoError;
}

GLuint GLES2DecoderImpl::DoGetMaxValueInBuffer(
    GLuint buffer_id, GLsizei count, GLenum type, GLuint offset) {
  GLuint max_vertex_accessed = 0;
  BufferManager::BufferInfo* info = GetBufferInfo(buffer_id);
  if (!info) {
    // TODO(gman): Should this be a GL error or a command buffer error?
    SetGLError(GL_INVALID_VALUE,
               "GetMaxValueInBuffer: unknown buffer");
  } else if (info->target() != GL_ELEMENT_ARRAY_BUFFER) {
    // TODO(gman): Should this be a GL error or a command buffer error?
    SetGLError(GL_INVALID_OPERATION,
               "GetMaxValueInBuffer: buffer not element array buffer");
  } else {
    if (!info->GetMaxValueForRange(offset, count, type, &max_vertex_accessed)) {
      // TODO(gman): Should this be a GL error or a command buffer error?
      SetGLError(GL_INVALID_OPERATION,
                 "GetMaxValueInBuffer: range out of bounds for buffer");
    }
  }
  return max_vertex_accessed;
}

// Calls glShaderSource for the various versions of the ShaderSource command.
// Assumes that data / data_size points to a piece of memory that is in range
// of whatever context it came from (shared memory, immediate memory, bucket
// memory.)
error::Error GLES2DecoderImpl::ShaderSourceHelper(
    GLuint client_id, const char* data, uint32 data_size) {
  ShaderManager::ShaderInfo* info = GetShaderInfo(client_id);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glShaderSource: unknown shader");
    return error::kNoError;
  }
  // Note: We don't actually call glShaderSource here. We wait until
  // the call to glCompileShader.
  info->Update(std::string(data, data + data_size));
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleShaderSource(
    uint32 immediate_data_size, const gles2::ShaderSource& c) {
  uint32 data_size = c.data_size;
  const char* data = GetSharedMemoryAs<const char*>(
      c.data_shm_id, c.data_shm_offset, data_size);
  if (!data) {
    return error::kOutOfBounds;
  }
  return ShaderSourceHelper(c.shader, data, data_size);
}

error::Error GLES2DecoderImpl::HandleShaderSourceImmediate(
  uint32 immediate_data_size, const gles2::ShaderSourceImmediate& c) {
  uint32 data_size = c.data_size;
  const char* data = GetImmediateDataAs<const char*>(
      c, data_size, immediate_data_size);
  if (!data) {
    return error::kOutOfBounds;
  }
  return ShaderSourceHelper(c.shader, data, data_size);
}

error::Error GLES2DecoderImpl::HandleShaderSourceBucket(
  uint32 immediate_data_size, const gles2::ShaderSourceBucket& c) {
  Bucket* bucket = GetBucket(c.data_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  return ShaderSourceHelper(
      c.shader, bucket->GetDataAs<const char*>(0, bucket->size() - 1),
      bucket->size() - 1);
}

void GLES2DecoderImpl::DoCompileShader(GLuint client_id) {
  ShaderManager::ShaderInfo* info = GetShaderInfo(client_id);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glCompileShader: unknown shader");
    return;
  }
  // Translate GL ES 2.0 shader to Desktop GL shader and pass that to
  // glShaderSource and then glCompileShader.
  const char* shader_src = info->source().c_str();
#if !defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
#if defined(GLES2_GPU_SERVICE_TRANSLATE_SHADER)
  int dbg_options = 0;
  EShLanguage language = info->shader_type() == GL_VERTEX_SHADER ?
      EShLangVertex : EShLangFragment;
  TBuiltInResource resources;
  // TODO(alokp): Ask gman how to get appropriate values.
  resources.maxVertexAttribs = 8;
  resources.maxVertexUniformVectors = 128;
  resources.maxVaryingVectors = 8;
  resources.maxVertexTextureImageUnits = 0;
  resources.maxCombinedTextureImageUnits = 8;
  resources.maxTextureImageUnits = 8;
  resources.maxFragmentUniformVectors = 16;
  resources.maxDrawBuffers = 1;
  ShHandle compiler = ShConstructCompiler(language, dbg_options);
  if (!ShCompile(compiler, &shader_src, 1, EShOptNone, &resources,
                 dbg_options)) {
    // TODO(alokp): Ask gman where to set compile-status and info-log.
    // May be add member variables to ShaderManager::ShaderInfo?
    const char* info_log = ShGetInfoLog(compiler);
    ShDestruct(compiler);
    return;
  }
  shader_src = ShGetObjectCode(compiler);
#endif  // GLES2_GPU_SERVICE_TRANSLATE_SHADER
#endif  // GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2

  glShaderSource(info->service_id(), 1, &shader_src, NULL);
  glCompileShader(info->service_id());

#if !defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
#ifdef GLES2_GPU_SERVICE_TRANSLATE_SHADER
  ShDestruct(compiler);
#endif  // GLES2_GPU_SERVICE_TRANSLATE_SHADER
#endif  // GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2
};

void GLES2DecoderImpl::DoGetShaderiv(
    GLuint shader, GLenum pname, GLint* params) {
  ShaderManager::ShaderInfo* info = GetShaderInfo(shader);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetShaderiv: unknown shader");
    return;
  }
  if (pname == GL_SHADER_SOURCE_LENGTH) {
    *params = info->source().size();
  } else {
    glGetShaderiv(info->service_id(), pname, params);
  }
}

error::Error GLES2DecoderImpl::HandleGetShaderSource(
    uint32 immediate_data_size, const gles2::GetShaderSource& c) {
  GLuint shader = c.shader;
  ShaderManager::ShaderInfo* info = GetShaderInfo(shader);
  uint32 bucket_id = static_cast<uint32>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  if (!info) {
    bucket->SetSize(0);
    SetGLError(GL_INVALID_VALUE, "glGetShaderSource: unknown shader");
    return error::kNoError;
  }
  bucket->SetFromString(info->source());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetProgramInfoLog(
    uint32 immediate_data_size, const gles2::GetProgramInfoLog& c) {
  GLuint program = c.program;
  ProgramManager::ProgramInfo* info = GetProgramInfo(program);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetProgramInfoLog: unknown program");
    return error::kNoError;
  }
  uint32 bucket_id = static_cast<uint32>(c.bucket_id);
  GLint len = 0;
  glGetProgramiv(info->service_id(), GL_INFO_LOG_LENGTH, &len);
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(len + 1);
  glGetProgramInfoLog(
      info->service_id(),
      len + 1, &len, bucket->GetDataAs<GLchar*>(0, len + 1));
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetShaderInfoLog(
    uint32 immediate_data_size, const gles2::GetShaderInfoLog& c) {
  GLuint shader = c.shader;
  ShaderManager::ShaderInfo* info = GetShaderInfo(shader);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetShaderInfoLog: unknown shader");
    return error::kNoError;
  }
  uint32 bucket_id = static_cast<uint32>(c.bucket_id);
  GLint len = 0;
  glGetShaderiv(info->service_id(), GL_INFO_LOG_LENGTH, &len);
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(len + 1);
  glGetShaderInfoLog(
      info->service_id(),
      len + 1, &len, bucket->GetDataAs<GLchar*>(0, len + 1));
  return error::kNoError;
}

bool GLES2DecoderImpl::DoIsBuffer(GLuint client_id) {
  return GetBufferInfo(client_id) != NULL;
}

bool GLES2DecoderImpl::DoIsFramebuffer(GLuint client_id) {
  return GetFramebufferInfo(client_id) != NULL;
}

bool GLES2DecoderImpl::DoIsProgram(GLuint client_id) {
  return GetProgramInfo(client_id) != NULL;
}

bool GLES2DecoderImpl::DoIsRenderbuffer(GLuint client_id) {
  return GetRenderbufferInfo(client_id) != NULL;
}

bool GLES2DecoderImpl::DoIsShader(GLuint client_id) {
  return GetShaderInfo(client_id) != NULL;
}

bool GLES2DecoderImpl::DoIsTexture(GLuint client_id) {
  return GetTextureInfo(client_id) != NULL;
}

void GLES2DecoderImpl::DoAttachShader(
    GLuint program_client_id, GLint shader_client_id) {
  ProgramManager::ProgramInfo* program_info = GetProgramInfo(program_client_id);
  if (!program_info) {
    SetGLError(GL_INVALID_VALUE, "glAttachShader: unknown program");
    return;
  }
  ShaderManager::ShaderInfo* shader_info = GetShaderInfo(shader_client_id);
  if (!shader_info) {
    SetGLError(GL_INVALID_VALUE, "glAttachShader: unknown shader");
    return;
  }
  glAttachShader(program_info->service_id(), shader_info->service_id());
}

void GLES2DecoderImpl::DoDetachShader(
    GLuint program_client_id, GLint shader_client_id) {
  ProgramManager::ProgramInfo* program_info = GetProgramInfo(program_client_id);
  if (!program_info) {
    SetGLError(GL_INVALID_VALUE, "glDetachShader: unknown program");
    return;
  }
  ShaderManager::ShaderInfo* shader_info = GetShaderInfo(shader_client_id);
  if (!shader_info) {
    SetGLError(GL_INVALID_VALUE, "glDetachShader: unknown shader");
    return;
  }
  glDetachShader(program_info->service_id(), shader_info->service_id());
}

void GLES2DecoderImpl::DoValidateProgram(GLuint program_client_id) {
  ProgramManager::ProgramInfo* info = GetProgramInfo(program_client_id);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glValidateProgram: unknown program");
    return;
  }
  glValidateProgram(info->service_id());
}

error::Error GLES2DecoderImpl::HandleVertexAttribPointer(
    uint32 immediate_data_size, const gles2::VertexAttribPointer& c) {
  if (!bound_array_buffer_ || bound_array_buffer_->IsDeleted()) {
    SetGLError(GL_INVALID_VALUE,
               "glVertexAttribPointer: no array buffer bound");
    return error::kNoError;
  }

  GLuint indx = c.indx;
  GLint size = c.size;
  GLenum type = c.type;
  GLboolean normalized = c.normalized;
  GLsizei stride = c.stride;
  GLsizei offset = c.offset;
  const void* ptr = reinterpret_cast<const void*>(offset);
  if (!ValidateGLenumVertexAttribType(type)) {
    SetGLError(GL_INVALID_ENUM,
               "glVertexAttribPointer: type GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!ValidateGLintVertexAttribSize(size)) {
    SetGLError(GL_INVALID_ENUM,
               "glVertexAttribPointer: size GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (indx >= group_->max_vertex_attribs()) {
    SetGLError(GL_INVALID_VALUE, "glVertexAttribPointer: index out of range");
    return error::kNoError;
  }
  if (stride < 0) {
    SetGLError(GL_INVALID_VALUE,
               "glVertexAttribPointer: stride < 0");
    return error::kNoError;
  }
  if (stride > 255) {
    SetGLError(GL_INVALID_VALUE,
               "glVertexAttribPointer: stride > 255");
    return error::kNoError;
  }
  if (offset < 0) {
    SetGLError(GL_INVALID_VALUE,
               "glVertexAttribPointer: offset < 0");
    return error::kNoError;
  }
  GLsizei component_size =
    GLES2Util::GetGLTypeSizeForTexturesAndBuffers(type);
  GLsizei real_stride = stride != 0 ? stride : component_size * size;
  if (offset % component_size > 0) {
    SetGLError(GL_INVALID_VALUE,
               "glVertexAttribPointer: stride not valid for type");
    return error::kNoError;
  }
  vertex_attrib_infos_[indx].SetInfo(
      bound_array_buffer_,
      size,
      type,
      real_stride,
      offset);
  glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleReadPixels(
    uint32 immediate_data_size, const gles2::ReadPixels& c) {
  GLint x = c.x;
  GLint y = c.y;
  GLsizei width = c.width;
  GLsizei height = c.height;
  GLenum format = c.format;
  GLenum type = c.type;
  if (width < 0 || height < 0) {
    SetGLError(GL_INVALID_VALUE, "glReadPixels: dimensions < 0");
    return error::kNoError;
  }
  typedef gles2::ReadPixels::Result Result;
  uint32 pixels_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, height, format, type, pack_alignment_, &pixels_size)) {
    return error::kOutOfBounds;
  }
  void* pixels = GetSharedMemoryAs<void*>(
      c.pixels_shm_id, c.pixels_shm_offset, pixels_size);
  Result* result = GetSharedMemoryAs<Result*>(
        c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!pixels || !result) {
    return error::kOutOfBounds;
  }

  if (!ValidateGLenumReadPixelFormat(format)) {
    SetGLError(GL_INVALID_ENUM, "glReadPixels: format GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!ValidateGLenumPixelType(type)) {
    SetGLError(GL_INVALID_ENUM, "glReadPixels: type GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (width == 0 || height == 0) {
    return error::kNoError;
  }

  CopyRealGLErrorsToWrapper();

  // Get the size of the current fbo or backbuffer.
  gfx::Size max_size = GetBoundFrameBufferSize();

  GLint max_x;
  GLint max_y;
  if (!SafeAdd(x, width, &max_x) || !SafeAdd(y, height, &max_y)) {
    SetGLError(GL_INVALID_VALUE, "glReadPixels: dimensions out of range");
    return error::kNoError;
  }

  if (x < 0 || y < 0 || max_x > max_size.width() || max_y > max_size.height()) {
    // The user requested an out of range area. Get the results 1 line
    // at a time.
    uint32 temp_size;
    if (!GLES2Util::ComputeImageDataSize(
        width, 1, format, type, pack_alignment_, &temp_size)) {
      SetGLError(GL_INVALID_VALUE, "glReadPixels: dimensions out of range");
      return error::kNoError;
    }
    GLsizei unpadded_row_size = temp_size;
    if (!GLES2Util::ComputeImageDataSize(
        width, 2, format, type, pack_alignment_, &temp_size)) {
      SetGLError(GL_INVALID_VALUE, "glReadPixels: dimensions out of range");
      return error::kNoError;
    }
    GLsizei padded_row_size = temp_size - unpadded_row_size;
    if (padded_row_size < 0 || unpadded_row_size < 0) {
      SetGLError(GL_INVALID_VALUE, "glReadPixels: dimensions out of range");
      return error::kNoError;
    }

    GLint dest_x_offset = std::max(-x, 0);
    uint32 dest_row_offset;
    if (!GLES2Util::ComputeImageDataSize(
      dest_x_offset, 1, format, type, pack_alignment_, &dest_row_offset)) {
      SetGLError(GL_INVALID_VALUE, "glReadPixels: dimensions out of range");
      return error::kNoError;
    }

    // Copy each row into the larger dest rect.
    int8* dst = static_cast<int8*>(pixels);
    GLint read_x = std::max(0, x);
    GLint read_end_x = std::max(0, std::min(max_size.width(), max_x));
    GLint read_width = read_end_x - read_x;
    for (GLint yy = 0; yy < height; ++yy) {
      GLint ry = y + yy;

      // Clear the row.
      memset(dst, 0, unpadded_row_size);

      // If the row is in range, copy it.
      if (ry >= 0 && ry < max_size.height() && read_width > 0) {
        glReadPixels(
            read_x, ry, read_width, 1, format, type, dst + dest_row_offset);
      }
      dst += padded_row_size;
    }
  } else {
    glReadPixels(x, y, width, height, format, type, pixels);
  }
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    *result = true;
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePixelStorei(
    uint32 immediate_data_size, const gles2::PixelStorei& c) {
  GLenum pname = c.pname;
  GLenum param = c.param;
  if (!ValidateGLenumPixelStore(pname)) {
    SetGLError(GL_INVALID_ENUM, "glPixelStorei: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!ValidateGLintPixelStoreAlignment(param)) {
    SetGLError(GL_INVALID_VALUE, "glPixelSTore: param GL_INVALID_VALUE");
    return error::kNoError;
  }
  glPixelStorei(pname, param);
  switch (pname) {
  case GL_PACK_ALIGNMENT:
      pack_alignment_ = param;
      break;
  case GL_UNPACK_ALIGNMENT:
      unpack_alignment_ = param;
      break;
  default:
      // Validation should have prevented us from getting here.
      DCHECK(false);
      break;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::GetAttribLocationHelper(
    GLuint client_id, uint32 location_shm_id, uint32 location_shm_offset,
    const std::string& name_str) {
  ProgramManager::ProgramInfo* info = GetProgramInfo(client_id);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetAttribLocation: unknown program");
    return error::kNoError;
  }
  if (!info->IsValid()) {
    SetGLError(GL_INVALID_OPERATION, "glGetAttribLocation: program not linked");
    return error::kNoError;
  }
  GLint* location = GetSharedMemoryAs<GLint*>(
      location_shm_id, location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  // Require the client to init this incase the context is lost and we are no
  // longer executing commands.
  if (*location != -1) {
    return error::kGenericError;
  }
  *location = info->GetAttribLocation(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetAttribLocation(
    uint32 immediate_data_size, const gles2::GetAttribLocation& c) {
  uint32 name_size = c.data_size;
  const char* name = GetSharedMemoryAs<const char*>(
      c.name_shm_id, c.name_shm_offset, name_size);
  if (!name) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  return GetAttribLocationHelper(
    c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::HandleGetAttribLocationImmediate(
    uint32 immediate_data_size, const gles2::GetAttribLocationImmediate& c) {
  uint32 name_size = c.data_size;
  const char* name = GetImmediateDataAs<const char*>(
      c, name_size, immediate_data_size);
  if (!name) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  return GetAttribLocationHelper(
    c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::HandleGetAttribLocationBucket(
    uint32 immediate_data_size, const gles2::GetAttribLocationBucket& c) {
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  bucket->GetAsString(&name_str);
  return GetAttribLocationHelper(
    c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::GetUniformLocationHelper(
    GLuint client_id, uint32 location_shm_id, uint32 location_shm_offset,
    const std::string& name_str) {
  ProgramManager::ProgramInfo* info = GetProgramInfo(client_id);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetUniformLocation: unknown program");
    return error::kNoError;
  }
  if (!info->IsValid()) {
    SetGLError(GL_INVALID_OPERATION,
               "glGetUniformLocation: program not linked");
    return error::kNoError;
  }
  GLint* location = GetSharedMemoryAs<GLint*>(
      location_shm_id, location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  // Require the client to init this incase the context is lost an we are no
  // longer executing commands.
  if (*location != -1) {
    return error::kGenericError;
  }
  *location = info->GetUniformLocation(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetUniformLocation(
    uint32 immediate_data_size, const gles2::GetUniformLocation& c) {
  uint32 name_size = c.data_size;
  const char* name = GetSharedMemoryAs<const char*>(
      c.name_shm_id, c.name_shm_offset, name_size);
  if (!name) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  return GetUniformLocationHelper(
    c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::HandleGetUniformLocationImmediate(
    uint32 immediate_data_size, const gles2::GetUniformLocationImmediate& c) {
  uint32 name_size = c.data_size;
  const char* name = GetImmediateDataAs<const char*>(
      c, name_size, immediate_data_size);
  if (!name) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  return GetUniformLocationHelper(
    c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::HandleGetUniformLocationBucket(
    uint32 immediate_data_size, const gles2::GetUniformLocationBucket& c) {
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  bucket->GetAsString(&name_str);
  return GetUniformLocationHelper(
    c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::HandleGetString(
    uint32 immediate_data_size, const gles2::GetString& c) {
  GLenum name = static_cast<GLenum>(c.name);
  if (!ValidateGLenumStringType(name)) {
    SetGLError(GL_INVALID_ENUM, "glGetString: name GL_INVALID_ENUM");
    return error::kNoError;
  }
  const char* gl_str = reinterpret_cast<const char*>(glGetString(name));
  const char* str = NULL;
  switch (name) {
    case GL_VERSION:
      str = "OpenGL ES 2.0 Chromium";
      break;
    case GL_SHADING_LANGUAGE_VERSION:
      str = "OpenGL ES GLSL ES 1.0 Chromium";
      break;
    case GL_EXTENSIONS:
      str = "";
      break;
    default:
      str = gl_str;
      break;
  }
  Bucket* bucket = CreateBucket(c.bucket_id);
  bucket->SetFromString(str);
  return error::kNoError;
}

void GLES2DecoderImpl::DoBufferData(
  GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage) {
  if (!ValidateGLenumBufferTarget(target)) {
    SetGLError(GL_INVALID_ENUM, "glBufferData: target GL_INVALID_ENUM");
    return;
  }
  if (!ValidateGLenumBufferUsage(usage)) {
    SetGLError(GL_INVALID_ENUM, "glBufferData: usage GL_INVALID_ENUM");
    return;
  }
  if (size < 0) {
    SetGLError(GL_INVALID_VALUE, "glBufferData: size < 0");
    return;
  }
  BufferManager::BufferInfo* info = GetBufferInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glBufferData: unknown buffer");
    return;
  }
  // Clear the buffer to 0 if no initial data was passed in.
  scoped_array<int8> zero;
  if (!data) {
    zero.reset(new int8[size]);
    memset(zero.get(), 0, size);
    data = zero.get();
  }
  CopyRealGLErrorsToWrapper();
  glBufferData(target, size, data, usage);
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    SetGLError(error, NULL);
  } else {
    info->SetSize(size);
    info->SetRange(0, size, data);
  }
}

error::Error GLES2DecoderImpl::HandleBufferData(
    uint32 immediate_data_size, const gles2::BufferData& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32 data_shm_id = static_cast<uint32>(c.data_shm_id);
  uint32 data_shm_offset = static_cast<uint32>(c.data_shm_offset);
  GLenum usage = static_cast<GLenum>(c.usage);
  const void* data = NULL;
  if (data_shm_id != 0 || data_shm_offset != 0) {
    data = GetSharedMemoryAs<const void*>(data_shm_id, data_shm_offset, size);
    if (!data) {
      return error::kOutOfBounds;
    }
  }
  DoBufferData(target, size, data, usage);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBufferDataImmediate(
    uint32 immediate_data_size, const gles2::BufferDataImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  const void* data = GetImmediateDataAs<const void*>(
      c, size, immediate_data_size);
  if (!data) {
    return error::kOutOfBounds;
  }
  GLenum usage = static_cast<GLenum>(c.usage);
  DoBufferData(target, size, data, usage);
  return error::kNoError;
}

void GLES2DecoderImpl::DoBufferSubData(
  GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data) {
  BufferManager::BufferInfo* info = GetBufferInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glBufferSubData: unknown buffer");
    return;
  }
  if (!info->SetRange(offset, size, data)) {
    SetGLError(GL_INVALID_VALUE, "glBufferSubData: out of range");
  } else {
    glBufferSubData(target, offset, size, data);
  }
}

error::Error GLES2DecoderImpl::DoCompressedTexImage2D(
  GLenum target,
  GLint level,
  GLenum internal_format,
  GLsizei width,
  GLsizei height,
  GLint border,
  GLsizei image_size,
  const void* data) {
  // TODO(gman): Validate image_size is correct for width, height and format.
  if (!ValidateGLenumTextureTarget(target)) {
    SetGLError(GL_INVALID_ENUM,
               "glCompressedTexImage2D: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!ValidateGLenumCompressedTextureInternalFormat(internal_format)) {
    SetGLError(GL_INVALID_ENUM,
               "glCompressedTexImage2D: internal_format GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!texture_manager()->ValidForTarget(target, level, width, height, 1) ||
      border != 0) {
    SetGLError(GL_INVALID_VALUE,
               "glCompressedTexImage2D: dimensions out of range");
    return error::kNoError;
  }
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE,
               "glCompressedTexImage2D: unknown texture target");
    return error::kNoError;
  }
  scoped_array<int8> zero;
  if (!data) {
    zero.reset(new int8[image_size]);
    memset(zero.get(), 0, image_size);
    data = zero.get();
  }
  info->SetLevelInfo(
      target, level, internal_format, width, height, 1, border, 0, 0);
  glCompressedTexImage2D(
      target, level, internal_format, width, height, border, image_size, data);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCompressedTexImage2D(
    uint32 immediate_data_size, const gles2::CompressedTexImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  uint32 data_shm_id = static_cast<uint32>(c.data_shm_id);
  uint32 data_shm_offset = static_cast<uint32>(c.data_shm_offset);
  const void* data = NULL;
  if (data_shm_id != 0 || data_shm_offset != 0) {
    data = GetSharedMemoryAs<const void*>(
        data_shm_id, data_shm_offset, image_size);
    if (!data) {
      return error::kOutOfBounds;
    }
  }
  return DoCompressedTexImage2D(
      target, level, internal_format, width, height, border, image_size, data);
}

error::Error GLES2DecoderImpl::HandleCompressedTexImage2DImmediate(
    uint32 immediate_data_size, const gles2::CompressedTexImage2DImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  const void* data = GetImmediateDataAs<const void*>(
      c, image_size, immediate_data_size);
  if (!data) {
    return error::kOutOfBounds;
  }
  return DoCompressedTexImage2D(
      target, level, internal_format, width, height, border, image_size, data);
}

error::Error GLES2DecoderImpl::DoTexImage2D(
  GLenum target,
  GLint level,
  GLenum internal_format,
  GLsizei width,
  GLsizei height,
  GLint border,
  GLenum format,
  GLenum type,
  const void* pixels,
  uint32 pixels_size) {
  if (!ValidateGLenumTextureTarget(target)) {
    SetGLError(GL_INVALID_ENUM, "glTexImage2D: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!ValidateGLenumTextureFormat(internal_format)) {
    SetGLError(GL_INVALID_ENUM,
               "glTexImage2D: internal_format GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!ValidateGLenumTextureFormat(format)) {
    SetGLError(GL_INVALID_ENUM, "glTexImage2D: format GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!ValidateGLenumPixelType(type)) {
    SetGLError(GL_INVALID_ENUM, "glTexImage2D: type GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!texture_manager()->ValidForTarget(target, level, width, height, 1) ||
      border != 0) {
    SetGLError(GL_INVALID_VALUE, "glTexImage2D: dimensions out of range");
    return error::kNoError;
  }
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glTexImage2D: unknown texture for target");
    return error::kNoError;
  }
  scoped_array<int8> zero;
  if (!pixels) {
    zero.reset(new int8[pixels_size]);
    memset(zero.get(), 0, pixels_size);
    pixels = zero.get();
  }
  info->SetLevelInfo(
      target, level, internal_format, width, height, 1, border, format, type);
  glTexImage2D(
      target, level, internal_format, width, height, border, format, type,
      pixels);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexImage2D(
    uint32 immediate_data_size, const gles2::TexImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint internal_format = static_cast<GLint>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32 pixels_shm_id = static_cast<uint32>(c.pixels_shm_id);
  uint32 pixels_shm_offset = static_cast<uint32>(c.pixels_shm_offset);
  uint32 pixels_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_, &pixels_size)) {
    return error::kOutOfBounds;
  }
  const void* pixels = NULL;
  if (pixels_shm_id != 0 || pixels_shm_offset != 0) {
    pixels = GetSharedMemoryAs<const void*>(
        pixels_shm_id, pixels_shm_offset, pixels_size);
    if (!pixels) {
      return error::kOutOfBounds;
    }
  }
  return DoTexImage2D(
      target, level, internal_format, width, height, border, format, type,
      pixels, pixels_size);
}

error::Error GLES2DecoderImpl::HandleTexImage2DImmediate(
    uint32 immediate_data_size, const gles2::TexImage2DImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint internal_format = static_cast<GLint>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32 size;
  if (!GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_, &size)) {
    return error::kOutOfBounds;
  }
  const void* pixels = GetImmediateDataAs<const void*>(
      c, size, immediate_data_size);
  if (!pixels) {
    return error::kOutOfBounds;
  }
  DoTexImage2D(
      target, level, internal_format, width, height, border, format, type,
      pixels, size);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetVertexAttribPointerv(
    uint32 immediate_data_size, const gles2::GetVertexAttribPointerv& c) {
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef gles2::GetVertexAttribPointerv::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
        c.pointer_shm_id, c.pointer_shm_offset, Result::ComputeSize(1));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  if (!ValidateGLenumVertexPointer(pname)) {
    SetGLError(GL_INVALID_ENUM,
               "glGetVertexAttribPointerv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (index >= group_->max_vertex_attribs()) {
    SetGLError(GL_INVALID_VALUE,
               "glGetVertexAttribPointerv: index out of range.");
    return error::kNoError;
  }
  result->SetNumResults(1);
  *result->GetData() = vertex_attrib_infos_[index].offset();
  return error::kNoError;
}

bool GLES2DecoderImpl::GetUniformSetup(
    GLuint program, GLint location,
    uint32 shm_id, uint32 shm_offset,
    error::Error* error, GLuint* service_id, void** result_pointer) {
  *error = error::kNoError;
  // Make sure we have enough room for the result on failure.
  SizedResult<GLint>* result;
  result = GetSharedMemoryAs<SizedResult<GLint>*>(
      shm_id, shm_offset, SizedResult<GLint>::ComputeSize(0));
  if (!result) {
    *error = error::kOutOfBounds;
    return false;
  }
  *result_pointer = result;
  // Set the result size to 0 so the client does not have to check for success.
  result->SetNumResults(0);
  ProgramManager::ProgramInfo* info = GetProgramInfo(program);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetUniform: unknown program");
    return false;
  }
  if (!info->IsValid()) {
    // Program was not linked successfully. (ie, glLinkProgram)
    SetGLError(GL_INVALID_OPERATION, "glGetUniform: program not linked");
    return false;
  }
  *service_id = info->service_id();
  GLenum type;
  if (!info->GetUniformTypeByLocation(location, &type)) {
    // No such location.
    SetGLError(GL_INVALID_OPERATION, "glGetUniform: unknown location");
    return false;
  }
  GLsizei size = GLES2Util::GetGLDataTypeSizeForUniforms(type);
  if (size == 0) {
    SetGLError(GL_INVALID_OPERATION, "glGetUniform: unknown type");
    return false;
  }
  result = GetSharedMemoryAs<SizedResult<GLint>*>(
      shm_id, shm_offset, SizedResult<GLint>::ComputeSizeFromBytes(size));
  if (!result) {
    *error = error::kOutOfBounds;
    return false;
  }
  result->size = size;
  return true;
}

error::Error GLES2DecoderImpl::HandleGetUniformiv(
    uint32 immediate_data_size, const gles2::GetUniformiv& c) {
  GLuint program = c.program;
  GLint location = c.location;
  GLuint service_id;
  Error error;
  void* result;
  if (GetUniformSetup(
      program, location, c.params_shm_id, c.params_shm_offset,
      &error, &service_id, &result)) {
    glGetUniformiv(
        service_id, location,
        static_cast<gles2::GetUniformiv::Result*>(result)->GetData());
  }
  return error;
}

error::Error GLES2DecoderImpl::HandleGetUniformfv(
    uint32 immediate_data_size, const gles2::GetUniformfv& c) {
  GLuint program = c.program;
  GLint location = c.location;
  GLuint service_id;
  Error error;
  void* result;
  typedef gles2::GetUniformfv::Result Result;
  if (GetUniformSetup(
      program, location, c.params_shm_id, c.params_shm_offset,
      &error, &service_id, &result)) {
    glGetUniformfv(
        service_id,
        location,
        static_cast<gles2::GetUniformfv::Result*>(result)->GetData());
  }
  return error;
}

error::Error GLES2DecoderImpl::HandleGetShaderPrecisionFormat(
    uint32 immediate_data_size, const gles2::GetShaderPrecisionFormat& c) {
  GLenum shader_type = static_cast<GLenum>(c.shadertype);
  GLenum precision_type = static_cast<GLenum>(c.precisiontype);
  typedef gles2::GetShaderPrecisionFormat::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }
  if (!ValidateGLenumShaderType(shader_type)) {
    SetGLError(GL_INVALID_ENUM,
               "glGetShaderPrecisionFormat: shader_type GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!ValidateGLenumShaderPrecision(precision_type)) {
    SetGLError(GL_INVALID_ENUM,
               "glGetShaderPrecisionFormat: precision_type GL_INVALID_ENUM");
    return error::kNoError;
  }

  result->success = 1;  // true
  switch (precision_type) {
    case GL_LOW_INT:
    case GL_MEDIUM_INT:
    case GL_HIGH_INT:
      result->min_range = -31;
      result->max_range = 31;
      result->precision = 0;
    case GL_LOW_FLOAT:
    case GL_MEDIUM_FLOAT:
    case GL_HIGH_FLOAT:
      result->min_range = -62;
      result->max_range = 62;
      result->precision = -16;
      break;
    default:
      NOTREACHED();
      break;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetAttachedShaders(
    uint32 immediate_data_size, const gles2::GetAttachedShaders& c) {
  uint32 result_size = c.result_size;
  ProgramManager::ProgramInfo* info = GetProgramInfo(c.program);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetAttachedShaders: unknown program");
    return error::kNoError;
  }
  typedef gles2::GetAttachedShaders::Result Result;
  uint32 max_count = Result::ComputeMaxResults(result_size);
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, Result::ComputeSize(max_count));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  glGetAttachedShaders(
      info->service_id(), max_count, &count, result->GetData());
  for (GLsizei ii = 0; ii < count; ++ii) {
    if (!shader_manager()->GetClientId(result->GetData()[ii],
                                       &result->GetData()[ii])) {
      NOTREACHED();
      return error::kGenericError;
    }
  }
  result->SetNumResults(count);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetActiveUniform(
    uint32 immediate_data_size, const gles2::GetActiveUniform& c) {
  GLuint program = c.program;
  GLuint index = c.index;
  uint32 name_bucket_id = c.name_bucket_id;
  typedef gles2::GetActiveUniform::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }
  ProgramManager::ProgramInfo* info = GetProgramInfo(program);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveUniform: unknown program");
    return error::kNoError;
  }
  if (!info->IsValid()) {
    // Program was not linked successfully. (ie, glLinkProgram)
    SetGLError(GL_INVALID_OPERATION, "glGetActiveUniform: program not linked");
    return error::kNoError;
  }
  const ProgramManager::ProgramInfo::UniformInfo* uniform_info =
      info->GetUniformInfo(index);
  if (!uniform_info) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveUniform: index out of range");
    return error::kNoError;
  }
  result->success = 1;  // true.
  result->size = uniform_info->size;
  result->type = uniform_info->type;
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(uniform_info->name);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetActiveAttrib(
    uint32 immediate_data_size, const gles2::GetActiveAttrib& c) {
  GLuint program = c.program;
  GLuint index = c.index;
  uint32 name_bucket_id = c.name_bucket_id;
  typedef gles2::GetActiveAttrib::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }
  ProgramManager::ProgramInfo* info = GetProgramInfo(program);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveAttrib: unknown program");
    return error::kNoError;
  }
  if (!info->IsValid()) {
    // Program was not linked successfully. (ie, glLinkProgram)
    SetGLError(GL_INVALID_OPERATION, "glGetActiveAttrib: program not linked");
    return error::kNoError;
  }
  const ProgramManager::ProgramInfo::VertexAttribInfo* attrib_info =
      info->GetAttribInfo(index);
  if (!attrib_info) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveAttrib: index out of range");
    return error::kNoError;
  }
  result->success = 1;  // true.
  result->size = attrib_info->size;
  result->type = attrib_info->type;
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(attrib_info->name);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleShaderBinary(
    uint32 immediate_data_size, const gles2::ShaderBinary& c) {
#if 1  // No binary shader support.
  SetGLError(GL_INVALID_OPERATION, "glShaderBinary: not supported");
  return error::kNoError;
#else
  GLsizei n = static_cast<GLsizei>(c.n);
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glShaderBinary: n < 0");
    return error::kNoError;
  }
  GLsizei length = static_cast<GLsizei>(c.length);
  if (length < 0) {
    SetGLError(GL_INVALID_VALUE, "glShaderBinary: length < 0");
    return error::kNoError;
  }
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* shaders = GetSharedMemoryAs<const GLuint*>(
      c.shaders_shm_id, c.shaders_shm_offset, data_size);
  GLenum binaryformat = static_cast<GLenum>(c.binaryformat);
  const void* binary = GetSharedMemoryAs<const void*>(
      c.binary_shm_id, c.binary_shm_offset, length);
  if (shaders == NULL || binary == NULL) {
    return error::kOutOfBounds;
  }
  scoped_array<GLuint> service_ids(new GLuint[n]);
  for (GLsizei ii = 0; ii < n; ++ii) {
    ShaderManager::ShaderInfo* info = GetShaderInfo(shaders[ii]);
    if (!info) {
      SetGLError(GL_INVALID_VALUE, "glShaderBinary: unknown shader");
      return error::kNoError;
    }
    service_ids[ii] = info->service_id();
  }
  // TODO(gman): call glShaderBinary
  return error::kNoError;
#endif
}

error::Error GLES2DecoderImpl::HandleSwapBuffers(
    uint32 immediate_data_size, const gles2::SwapBuffers& c) {
  // Check a client created frame buffer is not bound. TODO(apatrick):
  // this error is overkill. It will require that the client recreate the
  // context to continue.
  if (bound_framebuffer_)
    return error::kLostContext;

  // If offscreen then don't actually SwapBuffers to the display. Just copy
  // the rendered frame to another frame buffer.
  if (offscreen_target_frame_buffer_.get()) {
    ScopedGLErrorSuppressor suppressor(this);

    // First check to see if a deferred offscreen render buffer resize is
    // pending.
    if (!UpdateOffscreenFrameBufferSize())
      return error::kLostContext;

    ScopedFrameBufferBinder binder(this, offscreen_target_frame_buffer_->id());
    offscreen_saved_color_texture_->Copy(
        offscreen_saved_color_texture_->size());
  } else {
#if !defined(UNIT_TEST)
    context_->SwapBuffers();
#endif
  }

  // TODO(kbr): when the back buffer is multisampled, then at least on Mac
  // OS X (and probably on all platforms, for best semantics), we will need
  // to perform the resolve step and bind the offscreen_saved_color_texture_
  // as the color attachment before calling the swap buffers callback, which
  // expects a normal (non-multisampled) frame buffer for glCopyTexImage2D /
  // glReadPixels. After the callback runs, the multisampled frame buffer
  // needs to be bound again.

  if (swap_buffers_callback_.get()) {
    swap_buffers_callback_->Run();
  }

  return error::kNoError;
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/gles2_cmd_decoder_autogen.h"

}  // namespace gles2
}  // namespace gpu
