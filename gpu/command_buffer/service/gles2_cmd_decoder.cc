// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include <stdio.h>

#include <algorithm>
#include <vector>
#include <string>
#include <map>
#include <build/build_config.h>  // NOLINT
#include "base/callback.h"
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#define GLES2_GPU_SERVICE 1
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/command_buffer/service/id_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#if defined(UNIT_TEST)
#elif defined(OS_LINUX)
// XWindowWrapper is stubbed out for unit-tests.
#include "gpu/command_buffer/service/x_utils.h"
#elif defined(OS_MACOSX)
#include "chrome/common/accelerated_surface_mac.h"
#endif

namespace gpu {
namespace gles2 {

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

// }  // anonymous namespace.

GLES2Decoder::GLES2Decoder(ContextGroup* group)
    : group_(group),
#if defined(UNIT_TEST)
      debug_(false) {
#elif defined(OS_LINUX)
      debug_(false),
      window_(NULL) {
#elif defined(OS_WIN)
      debug_(false),
      hwnd_(NULL) {
#else
      debug_(false) {
#endif
}

GLES2Decoder::~GLES2Decoder() {
}

// This class implements GLES2Decoder so we don't have to expose all the GLES2
// cmd stuff to outside this class.
class GLES2DecoderImpl : public GLES2Decoder {
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
  virtual bool Initialize();
  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual uint32 GetServiceIdForTesting(uint32 client_id);
  virtual GLES2Util* GetGLES2Util() { return &util_; }

#if defined(OS_MACOSX)
  // Overridden from GLES2Decoder.

  // The recommended usage is to call SetWindowSizeForIOSurface() first, and if
  // that returns 0, try calling SetWindowSizeForTransportDIB().  A return value
  // of 0 from SetWindowSizeForIOSurface() might mean the IOSurface API is not
  // available, which is true if you are not running on Max OS X 10.6 or later.
  // If SetWindowSizeForTransportDIB() also returns a NULL handle, then an
  // error has occured.
  virtual uint64 SetWindowSizeForIOSurface(int32 width, int32 height);
  virtual TransportDIB::Handle SetWindowSizeForTransportDIB(int32 width,
                                                            int32 height);
  // |allocator| sends a message to the renderer asking for a new
  // TransportDIB big enough to hold the rendered bits.  The parameters to the
  // call back are the size of the DIB and the handle (filled in on return).
  virtual void SetTransportDIBAllocAndFree(
      Callback2<size_t, TransportDIB::Handle*>::Type* allocator,
      Callback1<TransportDIB::Id>::Type* deallocator);
#endif

  virtual void SetSwapBuffersCallback(Callback0::Type* callback);

 private:
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

  friend void GLGenTexturesHelper(
    GLES2DecoderImpl* decoder, GLsizei n, GLuint* ids);
  friend void GLDeleteTexturesHelper(
    GLES2DecoderImpl* decoder, GLsizei n, GLuint* ids);
  friend void GLGenBuffersHelper(
    GLES2DecoderImpl* decoder, GLsizei n, GLuint* ids);
  friend void GLDeleteBuffersHelper(
    GLES2DecoderImpl* decoder, GLsizei n, GLuint* ids);

  // TODO(gman): Cache these pointers?
  IdManager* id_manager() {
    return group_->id_manager();
  }

  BufferManager* buffer_manager() {
    return group_->buffer_manager();
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

#if defined(OS_WIN)
  static bool InitializeOneOff(bool anti_aliased);
#endif


  bool InitPlatformSpecific();
  static bool InitGlew();
  void DestroyPlatformSpecific();

  // Template to help call glGenXXX functions.
  template <void gl_gen_function(GLES2DecoderImpl*, GLsizei, GLuint*)>
  bool GenGLObjects(GLsizei n, const GLuint* client_ids) {
    DCHECK_GE(n, 0);
    if (!ValidateIdsAreUnused(n, client_ids)) {
      return false;
    }
    scoped_array<GLuint>temp(new GLuint[n]);
    gl_gen_function(this, n, temp.get());
    return RegisterObjects(n, client_ids, temp.get());
  }

  // Template to help call glDeleteXXX functions.
  template <void gl_delete_function(GLES2DecoderImpl*, GLsizei, GLuint*)>
  bool DeleteGLObjects(GLsizei n, const GLuint* client_ids) {
    DCHECK_GE(n, 0);
    scoped_array<GLuint>temp(new GLuint[n]);
    UnregisterObjects(n, client_ids, temp.get());
    gl_delete_function(this, n, temp.get());
    return true;
  }

  // Check that the given ids are not used.
  bool ValidateIdsAreUnused(GLsizei n, const GLuint* client_ids);

  // Register client ids with generated service ids.
  bool RegisterObjects(
      GLsizei n, const GLuint* client_ids, const GLuint* service_ids);

  // Unregisters client ids with service ids.
  void UnregisterObjects(
    GLsizei n, const GLuint* client_ids, GLuint* service_ids);

  // Creates a TextureInfo for the given texture.
  void CreateTextureInfo(GLuint texture) {
    texture_manager()->CreateTextureInfo(texture);
  }

  // Gets the texture info for the given texture. Returns NULL if none exists.
  TextureManager::TextureInfo* GetTextureInfo(GLuint texture) {
    TextureManager::TextureInfo* info =
        texture_manager()->GetTextureInfo(texture);
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Deletes the texture info for the given texture.
  void RemoveTextureInfo(GLuint texture) {
    texture_manager()->RemoveTextureInfo(texture);
  }

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
  void CreateProgramInfo(GLuint program) {
    program_manager()->CreateProgramInfo(program);
  }

  // Gets the program info for the given program. Returns NULL if none exists.
  // Programs that have no had glLinkProgram succesfully called on them will
  // not exist.
  ProgramManager::ProgramInfo* GetProgramInfo(GLuint program) {
    ProgramManager::ProgramInfo* info =
        program_manager()->GetProgramInfo(program);
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Deletes the program info for the given program.
  void RemoveProgramInfo(GLuint program) {
    program_manager()->RemoveProgramInfo(program);
  }

  // Creates a ShaderInfo for the given shader.
  void CreateShaderInfo(GLuint shader) {
    shader_manager()->CreateShaderInfo(shader);
  }

  // Gets the shader info for the given shader. Returns NULL if none exists.
  ShaderManager::ShaderInfo* GetShaderInfo(GLuint shader) {
    ShaderManager::ShaderInfo* info = shader_manager()->GetShaderInfo(shader);
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Deletes the shader info for the given shader.
  void RemoveShaderInfo(GLuint shader) {
    shader_manager()->RemoveShaderInfo(shader);
  }

  // Creates a buffer info for the given buffer.
  void CreateBufferInfo(GLuint buffer) {
    return buffer_manager()->CreateBufferInfo(buffer);
  }

  // Gets the buffer info for the given buffer.
  BufferManager::BufferInfo* GetBufferInfo(GLuint buffer) {
    BufferManager::BufferInfo* info = buffer_manager()->GetBufferInfo(buffer);
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Removes any buffers in the VertexAtrribInfos and BufferInfos. This is used
  // on glDeleteBuffers so we can make sure the user does not try to render
  // with deleted buffers.
  void RemoveBufferInfo(GLuint buffer_id);

  // Helper for glShaderSource.
  error::Error ShaderSourceHelper(
      GLuint shader, const char* data, uint32 data_size);

  // Wrapper for glCreateProgram
  void CreateProgramHelper(GLuint client_id);

  // Wrapper for glCreateShader
  void CreateShaderHelper(GLenum type, GLuint client_id);

  // Wrapper for glActiveTexture
  void DoActiveTexture(GLenum texture_unit);

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

  // Wrapper for glGetFramebufferAttachmentParameteriv.
  void DoGetFramebufferAttachmentParameteriv(
      GLenum target, GLenum attachment, GLenum pname, GLint* params);

  // Wrapper for glRenderbufferParameteriv.
  void DoGetRenderbufferParameteriv(
      GLenum target, GLenum pname, GLint* params);

  // Wrapper for glGetShaderiv
  void DoGetShaderiv(GLuint shader, GLenum pname, GLint* params);

  // Wrapper for glGetShaderSource.
  void DoGetShaderSource(
      GLuint shader, GLsizei bufsize, GLsizei* length, char* dst);

  // Wrapper for glLinkProgram
  void DoLinkProgram(GLuint program);

  // Wrapper for glRenderbufferStorage.
  void DoRenderbufferStorage(
    GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

  // Swaps the buffers (copies/renders to the current window).
  void DoSwapBuffers();

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

  // Gets the GLError through our wrapper.
  GLenum GetGLError();

  // Sets our wrapper for the GLError.
  void SetGLError(GLenum error);

  // Copies the real GL errors to the wrapper. This is so we can
  // make sure there are no native GL errors before calling some GL function
  // so that on return we know any error generated was for that specific
  // command.
  void CopyRealGLErrorsToWrapper();

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
  GLuint bound_framebuffer_;

  // The currently bound renderbuffer
  GLuint bound_renderbuffer_;

#if defined(UNIT_TEST)
#elif defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
#elif defined(OS_WIN)
  static int pixel_format_;
  HDC gl_device_context_;
  HGLRC gl_context_;
  HPBUFFERARB pbuffer_;
#elif defined(OS_MACOSX)
  AcceleratedSurface surface_;
#endif

  bool anti_aliased_;

  scoped_ptr<Callback0::Type> swap_buffers_callback_;

  DISALLOW_COPY_AND_ASSIGN(GLES2DecoderImpl);
};

GLES2Decoder* GLES2Decoder::Create(ContextGroup* group) {
  return new GLES2DecoderImpl(group);
}

#if defined(UNIT_TEST)
#elif defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
#elif defined(OS_WIN)
int GLES2DecoderImpl::pixel_format_;
#endif

GLES2DecoderImpl::GLES2DecoderImpl(ContextGroup* group)
    : GLES2Decoder(group),
      error_bits_(0),
      util_(0),  // TODO(gman): Set to actual num compress texture formats.
      pack_alignment_(4),
      unpack_alignment_(4),
      active_texture_unit_(0),
      black_2d_texture_id_(0),
      black_cube_texture_id_(0),
      bound_framebuffer_(0),
      bound_renderbuffer_(0),
#if defined(UNIT_TEST)
#elif defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
#elif defined(OS_WIN)
      gl_device_context_(NULL),
      gl_context_(NULL),
      pbuffer_(NULL),
#endif
      anti_aliased_(false) {
}

bool GLES2DecoderImpl::Initialize() {
  if (!InitPlatformSpecific()) {
    Destroy();
    return false;
  }
  if (!MakeCurrent()) {
    Destroy();
    return false;
  }

  // This happens in InitializeOneOff in windows. TODO(apatrick): generalize to
  // other platforms.
#if !defined(OS_WIN)
  if (!InitGlew()) {
    Destroy();
    return false;
  }
#endif

  CHECK_GL_ERROR();

  if (!group_->Initialize()) {
    Destroy();
    return false;
  }

  vertex_attrib_infos_.reset(
      new VertexAttribInfo[group_->max_vertex_attribs()]);
  texture_units_.reset(
      new TextureUnit[group_->max_texture_units()]);
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
  static GLenum faces[] = {
      GL_TEXTURE_CUBE_MAP_POSITIVE_X,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
  };
  for (size_t ii = 0; ii < arraysize(faces); ++ii) {
    glTexImage2D(faces[ii], 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, black);
  }
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  CHECK_GL_ERROR();

  return true;
}

// TODO(kbr): the use of this anonymous namespace core dumps the
// linker on Mac OS X 10.6 when the symbol ordering file is used
// namespace {

#if defined(UNIT_TEST)
#elif defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
#elif defined(OS_WIN)

const PIXELFORMATDESCRIPTOR kPixelFormatDescriptor = {
  sizeof(kPixelFormatDescriptor),    // Size of structure.
  1,                       // Default version.
  PFD_DRAW_TO_WINDOW |     // Window drawing support.
  PFD_SUPPORT_OPENGL |     // OpenGL support.
  PFD_DOUBLEBUFFER,        // Double buffering support (not stereo).
  PFD_TYPE_RGBA,           // RGBA color mode (not indexed).
  24,                      // 24 bit color mode.
  0, 0, 0, 0, 0, 0,        // Don't set RGB bits & shifts.
  8, 0,                    // 8 bit alpha
  0,                       // No accumulation buffer.
  0, 0, 0, 0,              // Ignore accumulation bits.
  24,                      // 24 bit z-buffer size.
  8,                       // 8-bit stencil buffer.
  0,                       // No aux buffer.
  PFD_MAIN_PLANE,          // Main drawing plane (not overlay).
  0,                       // Reserved.
  0, 0, 0,                 // Layer masks ignored.
};

LRESULT CALLBACK IntermediateWindowProc(HWND window,
                                        UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  return ::DefWindowProc(window, message, w_param, l_param);
}

// Helper routine that does one-off initialization like determining the
// pixel format and initializing glew.
bool GLES2DecoderImpl::InitializeOneOff(bool anti_aliased) {
  // We must initialize a GL context before we can determine the multi-sampling
  // supported on the current hardware, so we create an intermediate window
  // and context here.
  HINSTANCE module_handle;
  if (!::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
                           GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           reinterpret_cast<wchar_t*>(IntermediateWindowProc),
                           &module_handle)) {
    return false;
  }

  WNDCLASS intermediate_class;
  intermediate_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  intermediate_class.lpfnWndProc = IntermediateWindowProc;
  intermediate_class.cbClsExtra = 0;
  intermediate_class.cbWndExtra = 0;
  intermediate_class.hInstance = module_handle;
  intermediate_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  intermediate_class.hCursor = LoadCursor(NULL, IDC_ARROW);
  intermediate_class.hbrBackground = NULL;
  intermediate_class.lpszMenuName = NULL;
  intermediate_class.lpszClassName = L"Intermediate GL Window";

  ATOM class_registration = ::RegisterClass(&intermediate_class);
  if (!class_registration) {
    return false;
  }

  HWND intermediate_window = ::CreateWindow(
      reinterpret_cast<wchar_t*>(class_registration),
      L"",
      WS_OVERLAPPEDWINDOW,
      0, 0,
      CW_USEDEFAULT, CW_USEDEFAULT,
      NULL,
      NULL,
      NULL,
      NULL);

  if (!intermediate_window) {
    ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                      module_handle);
    return false;
  }

  HDC intermediate_dc = ::GetDC(intermediate_window);
  pixel_format_ = ::ChoosePixelFormat(intermediate_dc,
                                      &kPixelFormatDescriptor);
  if (pixel_format_ == 0) {
    DLOG(ERROR) << "Unable to get the pixel format for GL context.";
    ::ReleaseDC(intermediate_window, intermediate_dc);
    ::DestroyWindow(intermediate_window);
    ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                      module_handle);
    return false;
  }
  if (!::SetPixelFormat(intermediate_dc, pixel_format_,
                        &kPixelFormatDescriptor)) {
    DLOG(ERROR) << "Unable to set the pixel format for GL context.";
    ::ReleaseDC(intermediate_window, intermediate_dc);
    ::DestroyWindow(intermediate_window);
    ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                      module_handle);
    return false;
  }

  // Create a temporary GL context to query for multisampled pixel formats.
  HGLRC gl_context = ::wglCreateContext(intermediate_dc);
  if (::wglMakeCurrent(intermediate_dc, gl_context)) {
    // GL context was successfully created and applied to the window's DC.
    // Startup GLEW, the GL extensions wrangler.
    if (InitGlew()) {
      DLOG(INFO) << "Initialized GLEW " << ::glewGetString(GLEW_VERSION);
    } else {
      ::wglMakeCurrent(intermediate_dc, NULL);
      ::wglDeleteContext(gl_context);
      ::ReleaseDC(intermediate_window, intermediate_dc);
      ::DestroyWindow(intermediate_window);
      ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                        module_handle);
      return false;
    }

    // If the multi-sample extensions are present, query the api to determine
    // the pixel format.
    if (anti_aliased && WGLEW_ARB_pixel_format && WGLEW_ARB_multisample) {
      int pixel_attributes[] = {
        WGL_SAMPLES_ARB, 4,
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_COLOR_BITS_ARB, 24,
        WGL_ALPHA_BITS_ARB, 8,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
        0, 0};

      float pixel_attributes_f[] = {0, 0};
      int msaa_pixel_format;
      unsigned int num_formats;

      // Query for the highest sampling rate supported, starting at 4x.
      static const int kSampleCount[] = {4, 2};
      static const int kNumSamples = 2;
      for (int sample = 0; sample < kNumSamples; ++sample) {
        pixel_attributes[1] = kSampleCount[sample];
        if (GL_TRUE == ::wglChoosePixelFormatARB(intermediate_dc,
                                                 pixel_attributes,
                                                 pixel_attributes_f,
                                                 1,
                                                 &msaa_pixel_format,
                                                 &num_formats)) {
          pixel_format_ = msaa_pixel_format;
          break;
        }
      }
    }
  }

  ::wglMakeCurrent(intermediate_dc, NULL);
  ::wglDeleteContext(gl_context);
  ::ReleaseDC(intermediate_window, intermediate_dc);
  ::DestroyWindow(intermediate_window);
  ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                    module_handle);
  return true;
}

#endif  // OS_WIN

// These commands convert from c calls to local os calls.
void GLGenBuffersHelper(
    GLES2DecoderImpl* decoder, GLsizei n, GLuint* ids) {
  glGenBuffersARB(n, ids);
  // TODO(gman): handle error
  for (GLsizei ii = 0; ii < n; ++ii) {
    decoder->CreateBufferInfo(ids[ii]);
  }
}

void GLGenFramebuffersHelper(
    GLES2DecoderImpl*, GLsizei n, GLuint* ids) {
  glGenFramebuffersEXT(n, ids);
}

void GLGenRenderbuffersHelper(
    GLES2DecoderImpl*, GLsizei n, GLuint* ids) {
  glGenRenderbuffersEXT(n, ids);
}

void GLGenTexturesHelper(
    GLES2DecoderImpl* decoder, GLsizei n, GLuint* ids) {
  glGenTextures(n, ids);
  // TODO(gman): handle error
  for (GLsizei ii = 0; ii < n; ++ii) {
    decoder->CreateTextureInfo(ids[ii]);
  }
}

void GLDeleteBuffersHelper(
    GLES2DecoderImpl* decoder, GLsizei n, GLuint* ids) {
  glDeleteBuffersARB(n, ids);
  // TODO(gman): handle error
  for (GLsizei ii = 0; ii < n; ++ii) {
    decoder->RemoveBufferInfo(ids[ii]);
  }
}

void GLDeleteFramebuffersHelper(
    GLES2DecoderImpl*, GLsizei n, GLuint* ids) {
  glDeleteFramebuffersEXT(n, ids);
}

void GLDeleteRenderbuffersHelper(
    GLES2DecoderImpl*, GLsizei n, GLuint* ids) {
  glDeleteRenderbuffersEXT(n, ids);
}

void GLDeleteTexturesHelper(
    GLES2DecoderImpl* decoder, GLsizei n, GLuint* ids) {
  glDeleteTextures(n, ids);
  // TODO(gman): handle error
  for (GLsizei ii = 0; ii < n; ++ii) {
    decoder->RemoveTextureInfo(ids[ii]);
  }
}

// }  // anonymous namespace

bool GLES2DecoderImpl::MakeCurrent() {
#if defined(UNIT_TEST)
  return true;
#elif defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
  return true;
#elif defined(OS_WIN)
  if (::wglGetCurrentDC() == gl_device_context_ &&
      ::wglGetCurrentContext() == gl_context_) {
    return true;
  }
  if (!::wglMakeCurrent(gl_device_context_, gl_context_)) {
    DLOG(ERROR) << "Unable to make gl context current.";
    return false;
  }
  return true;
#elif defined(OS_LINUX)
  return window()->MakeCurrent();
#elif defined(OS_MACOSX)
  return surface_.MakeCurrent();
#else
  NOTREACHED();
  return false;
#endif
}

uint32 GLES2DecoderImpl::GetServiceIdForTesting(uint32 client_id) {
#if defined(UNIT_TEST)
  GLuint service_id;
  bool result = id_manager()->GetServiceId(client_id, &service_id);
  return result ? service_id : 0u;
#else
  DCHECK(false);
  return 0u;
#endif
}

bool GLES2DecoderImpl::ValidateIdsAreUnused(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint service_id;
    if (id_manager()->GetServiceId(client_ids[ii], &service_id)) {
      return false;
    }
  }
  return true;
}

bool GLES2DecoderImpl::RegisterObjects(
    GLsizei n, const GLuint* client_ids, const GLuint* service_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (!id_manager()->AddMapping(client_ids[ii], service_ids[ii])) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

void GLES2DecoderImpl::UnregisterObjects(
    GLsizei n, const GLuint* client_ids, GLuint* service_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (id_manager()->GetServiceId(client_ids[ii], &service_ids[ii])) {
      id_manager()->RemoveMapping(client_ids[ii], service_ids[ii]);
    } else {
      service_ids[ii] = 0;
    }
  }
}

bool GLES2DecoderImpl::InitPlatformSpecific() {
#if defined(UNIT_TEST)
#elif defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
#elif defined(OS_WIN)
  // Do one-off initialization.
  static bool success = InitializeOneOff(anti_aliased_);
  if (!success)
    return false;

  if (hwnd()) {
    // The GL context will render to this window.
    gl_device_context_ = ::GetDC(hwnd());

    if (!::SetPixelFormat(gl_device_context_,
                          pixel_format_,
                          &kPixelFormatDescriptor)) {
      DLOG(ERROR) << "Unable to set the pixel format for GL context.";
      DestroyPlatformSpecific();
      return false;
    }
  } else {
    // Create a device context compatible with the primary display.
    HDC display_device_context = ::CreateDC(L"DISPLAY", NULL, NULL, NULL);

    // Create a 1 x 1 pbuffer suitable for use with the device.
    const int kNoAttributes[] = { 0 };
    pbuffer_ = ::wglCreatePbufferARB(display_device_context,
                                     pixel_format_,
                                     1, 1,
                                     kNoAttributes);
    ::DeleteDC(display_device_context);
    if (!pbuffer_) {
      DLOG(ERROR) << "Unable to create pbuffer.";
      DestroyPlatformSpecific();
      return false;
    }

    gl_device_context_ = ::wglGetPbufferDCARB(pbuffer_);
    if (!gl_device_context_) {
      DLOG(ERROR) << "Unable to get pbuffer device context.";
      DestroyPlatformSpecific();
      return false;
    }
  }

  gl_context_ = ::wglCreateContext(gl_device_context_);
  if (!gl_context_) {
    DLOG(ERROR) << "Failed to create GL context.";
    DestroyPlatformSpecific();
    return false;
  }
#elif defined(OS_LINUX)
  DCHECK(window());
  if (!window()->Initialize())
    return false;
#elif defined(OS_MACOSX)
  return surface_.Initialize();
#endif

  return true;
}

bool GLES2DecoderImpl::InitGlew() {
#if !defined(UNIT_TEST) && !defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
  DLOG(INFO) << "Initializing GL and GLEW for GLES2Decoder.";

  GLenum glew_error = glewInit();
  if (glew_error != GLEW_OK) {
    DLOG(ERROR) << "Unable to initialise GLEW : "
                << ::glewGetErrorString(glew_error);
    return false;
  }

  // Check to see that we can use the OpenGL vertex attribute APIs
  // TODO(petersont):  Return false if this check fails, but because some
  // Intel hardware does not support OpenGL 2.0, yet does support all of the
  // extensions we require, we only log an error.  A future CL should change
  // this check to ensure that all of the extension strings we require are
  // present.
  if (!GLEW_VERSION_2_0) {
    DLOG(ERROR) << "GL drivers do not have OpenGL 2.0 functionality.";
  }

  bool extensions_found = true;
  if (!GLEW_ARB_vertex_buffer_object) {
    // NOTE: Linux NVidia drivers claim to support OpenGL 2.0 when using
    // indirect rendering (e.g. remote X), but it is actually lying. The
    // ARB_vertex_buffer_object functions silently no-op (!) when using
    // indirect rendering, leading to crashes. Fortunately, in that case, the
    // driver claims to not support ARB_vertex_buffer_object, so fail in that
    // case.
    DLOG(ERROR) << "GL drivers do not support vertex buffer objects.";
    extensions_found = false;
  }
  if (!GLEW_EXT_framebuffer_object) {
    DLOG(ERROR) << "GL drivers do not support framebuffer objects.";
    extensions_found = false;
  }
  // Check for necessary extensions
  if (!GLEW_VERSION_2_0 && !GLEW_EXT_stencil_two_side) {
    DLOG(ERROR) << "Two sided stencil extension missing.";
    extensions_found = false;
  }
  if (!GLEW_VERSION_1_4 && !GLEW_EXT_blend_func_separate) {
    DLOG(ERROR) <<"Separate blend func extension missing.";
    extensions_found = false;
  }
  if (!GLEW_VERSION_2_0 && !GLEW_EXT_blend_equation_separate) {
    DLOG(ERROR) << "Separate blend function extension missing.";
    extensions_found = false;
  }
  if (!extensions_found)
    return false;
#endif

  return true;
}

void GLES2DecoderImpl::DestroyPlatformSpecific() {
#if defined(UNIT_TEST)
#elif defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
#elif defined(OS_WIN)
  if (gl_context_) {
    ::wglDeleteContext(gl_context_);
  }

  if (gl_device_context_) {
    if (hwnd())
      ::ReleaseDC(hwnd(), gl_device_context_);
    else
      ::wglReleasePbufferDCARB(pbuffer_, gl_device_context_);

    gl_device_context_ = NULL;
  }

  if (pbuffer_) {
    ::wglDestroyPbufferARB(pbuffer_);
    pbuffer_ = NULL;
  }
#endif
}

#if defined(OS_MACOSX)

uint64 GLES2DecoderImpl::SetWindowSizeForIOSurface(int32 width, int32 height) {
#if defined(UNIT_TEST)
  return 0;
#elif defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
  return 0;
#else
  return surface_.SetSurfaceSize(width, height);
#endif  // !defined(UNIT_TEST)
}

TransportDIB::Handle GLES2DecoderImpl::SetWindowSizeForTransportDIB(
    int32 width, int32 height) {
#if defined(UNIT_TEST)
  return TransportDIB::DefaultHandleValue();
#elif defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
  return TransportDIB::DefaultHandleValue();
#else
  return surface_.SetTransportDIBSize(width, height);
#endif  // !defined(UNIT_TEST)
}

void GLES2DecoderImpl::SetTransportDIBAllocAndFree(
    Callback2<size_t, TransportDIB::Handle*>::Type* allocator,
    Callback1<TransportDIB::Id>::Type* deallocator) {
  surface_.SetTransportDIBAllocAndFree(allocator, deallocator);
}
#endif  // defined(OS_MACOSX)

void GLES2DecoderImpl::SetSwapBuffersCallback(Callback0::Type* callback) {
  swap_buffers_callback_.reset(callback);
}

void GLES2DecoderImpl::Destroy() {
#if defined(UNIT_TEST)
#elif defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
#elif defined(OS_LINUX)
  DCHECK(window());
  window()->Destroy();
#elif defined(OS_MACOSX)
  surface_.Destroy();
#endif

  DestroyPlatformSpecific();
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
          SetGLError(error);
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

void GLES2DecoderImpl::RemoveBufferInfo(GLuint buffer_id) {
  buffer_manager()->RemoveBufferInfo(buffer_id);
  // TODO(gman): See if we can remove the rest of this function as
  //    buffers are now reference counted and have a "IsDeleted" function.
  if (bound_array_buffer_ && bound_array_buffer_->buffer_id() == buffer_id) {
    bound_array_buffer_ = NULL;
  }
  if (bound_element_array_buffer_ &&
      bound_element_array_buffer_->buffer_id() == buffer_id) {
    bound_element_array_buffer_ = NULL;
  }

  // go through VertexAttribInfo and update any info that references the buffer.
  for (GLuint ii = 0; ii < group_->max_vertex_attribs(); ++ii) {
    VertexAttribInfo& info = vertex_attrib_infos_[ii];
    if (info.buffer() && info.buffer()->buffer_id() == buffer_id) {
      info.ClearBuffer();
    }
  }
}

void GLES2DecoderImpl::CreateProgramHelper(GLuint client_id) {
  // TODO(gman): verify client_id is unused.
  GLuint service_id = glCreateProgram();
  if (service_id) {
    id_manager()->AddMapping(client_id, service_id);
    CreateProgramInfo(service_id);
  }
}

void GLES2DecoderImpl::CreateShaderHelper(GLenum type, GLuint client_id) {
  // TODO(gman): verify client_id is unused.
  GLuint service_id = glCreateShader(type);
  if (service_id) {
    id_manager()->AddMapping(client_id, service_id);
    CreateShaderInfo(service_id);
  }
}

bool GLES2DecoderImpl::ValidateGLenumCompressedTextureInternalFormat(GLenum) {
  // TODO(gman): Add support for compressed texture formats.
  return false;
}

void GLES2DecoderImpl::DoActiveTexture(GLenum texture_unit) {
  GLuint texture_index = texture_unit - GL_TEXTURE0;
  if (texture_index > group_->max_texture_units()) {
    SetGLError(GL_INVALID_ENUM);
    return;
  }
  active_texture_unit_ = texture_index;
  glActiveTexture(texture_unit);
}

void GLES2DecoderImpl::DoBindBuffer(GLenum target, GLuint buffer) {
  BufferManager::BufferInfo* info = NULL;
  if (buffer) {
    info = GetBufferInfo(buffer);
    // Check the buffer exists
    // Check that we are not trying to bind it to a different target.
    if (!info || (info->target() != 0 && info->target() != target)) {
      SetGLError(GL_INVALID_OPERATION);
      return;
    }
    if (info->target() == 0) {
      info->set_target(target);
    }
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
  glBindBuffer(target, buffer);
}

void GLES2DecoderImpl::DoBindFramebuffer(GLenum target, GLuint framebuffer) {
  bound_framebuffer_ = framebuffer;
  glBindFramebufferEXT(target, framebuffer);
}

void GLES2DecoderImpl::DoBindRenderbuffer(GLenum target, GLuint renderbuffer) {
  bound_renderbuffer_ = renderbuffer;
  glBindRenderbufferEXT(target, renderbuffer);
}

void GLES2DecoderImpl::DoBindTexture(GLenum target, GLuint texture) {
  TextureManager::TextureInfo* info = NULL;
  if (texture) {
    info = GetTextureInfo(texture);
    // Check the texture exists
    // Check that we are not trying to bind it to a different target.
    if (!info || (info->target() != 0 && info->target() != target)) {
      SetGLError(GL_INVALID_OPERATION);
      return;
    }
    if (info->target() == 0) {
      texture_manager()->SetInfoTarget(info, target);
    }
  }
  glBindTexture(target, texture);
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
    SetGLError(GL_INVALID_VALUE);
  }
}

void GLES2DecoderImpl::DoEnableVertexAttribArray(GLuint index) {
  if (index < group_->max_vertex_attribs()) {
    vertex_attrib_infos_[index].set_enabled(true);
    glEnableVertexAttribArray(index);
  } else {
    SetGLError(GL_INVALID_VALUE);
  }
}

void GLES2DecoderImpl::DoGenerateMipmap(GLenum target) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info || !info->MarkMipmapsGenerated()) {
    SetGLError(GL_INVALID_OPERATION);
    return;
  }
  glGenerateMipmapEXT(target);
}

error::Error GLES2DecoderImpl::HandleDeleteShader(
    uint32 immediate_data_size, const gles2::DeleteShader& c) {
  GLuint shader = c.shader;
  GLuint service_id;
  if (!id_manager()->GetServiceId(shader, &service_id)) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  RemoveShaderInfo(service_id);
  glDeleteShader(service_id);
  id_manager()->RemoveMapping(shader, service_id);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteProgram(
    uint32 immediate_data_size, const gles2::DeleteProgram& c) {
  GLuint program = c.program;
  GLuint service_id;
  if (!id_manager()->GetServiceId(program, &service_id)) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  RemoveProgramInfo(service_id);
  glDeleteProgram(service_id);
  id_manager()->RemoveMapping(program, service_id);
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
    GLuint renderbuffer) {
  if (bound_framebuffer_ == 0) {
    SetGLError(GL_INVALID_OPERATION);
    return;
  }
  glFramebufferRenderbufferEXT(target, attachment, renderbuffertarget,
                               renderbuffer);
}

GLenum GLES2DecoderImpl::DoCheckFramebufferStatus(GLenum target) {
  if (bound_framebuffer_ == 0) {
    return GL_FRAMEBUFFER_COMPLETE;
  }
  return glCheckFramebufferStatusEXT(target);
}

void GLES2DecoderImpl::DoFramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
    GLint level) {
  if (bound_framebuffer_ == 0) {
    SetGLError(GL_INVALID_OPERATION);
    return;
  }
  glFramebufferTexture2DEXT(target, attachment, textarget, texture, level);
}

void GLES2DecoderImpl::DoGetFramebufferAttachmentParameteriv(
    GLenum target, GLenum attachment, GLenum pname, GLint* params) {
  if (bound_framebuffer_ == 0) {
    SetGLError(GL_INVALID_OPERATION);
    return;
  }
  glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, params);
}

void GLES2DecoderImpl::DoGetRenderbufferParameteriv(
    GLenum target, GLenum pname, GLint* params) {
  if (bound_renderbuffer_ == 0) {
    SetGLError(GL_INVALID_OPERATION);
    return;
  }
  glGetRenderbufferParameterivEXT(target, pname, params);
}

void GLES2DecoderImpl::DoRenderbufferStorage(
  GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  if (bound_renderbuffer_ == 0) {
    SetGLError(GL_INVALID_OPERATION);
    return;
  }
  glRenderbufferStorageEXT(target, internalformat, width, height);
}

void GLES2DecoderImpl::DoLinkProgram(GLuint program) {
  ProgramManager::ProgramInfo* info = GetProgramInfo(program);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION);
    return;
  }
  CopyRealGLErrorsToWrapper();
  glLinkProgram(program);
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    RemoveProgramInfo(program);
    SetGLError(error);
  } else {
    info->Update();
  }
};

void GLES2DecoderImpl::DoSwapBuffers() {
#if defined(UNIT_TEST)
#elif defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
#elif defined(OS_WIN)
  ::SwapBuffers(gl_device_context_);
#elif defined(OS_LINUX)
  DCHECK(window());
  window()->SwapBuffers();
#elif defined(OS_MACOSX)
  // TODO(kbr): Need to property hook up and track the OpenGL state and hook
  // up the notion of the currently bound FBO.
  surface_.SwapBuffers();
#endif
  if (swap_buffers_callback_.get()) {
    swap_buffers_callback_->Run();
  }
}

void GLES2DecoderImpl::DoTexParameterf(
    GLenum target, GLenum pname, GLfloat param) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE);
  } else {
    info->SetParameter(pname, static_cast<GLint>(param));
    glTexParameterf(target, pname, param);
  }
}

void GLES2DecoderImpl::DoTexParameteri(
    GLenum target, GLenum pname, GLint param) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE);
  } else {
    info->SetParameter(pname, param);
    glTexParameteri(target, pname, param);
  }
}

void GLES2DecoderImpl::DoTexParameterfv(
    GLenum target, GLenum pname, const GLfloat* params) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE);
  } else {
    info->SetParameter(pname, *reinterpret_cast<const GLint*>(params));
    glTexParameterfv(target, pname, params);
  }
}

void GLES2DecoderImpl::DoTexParameteriv(
  GLenum target, GLenum pname, const GLint* params) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE);
  } else {
    info->SetParameter(pname, *params);
    glTexParameteriv(target, pname, params);
  }
}

void GLES2DecoderImpl::DoUniform1i(GLint location, GLint v0) {
  if (!current_program_ || current_program_->IsDeleted()) {
    // The program does not exist.
    SetGLError(GL_INVALID_OPERATION);
    return;
  }
  current_program_->SetSamplers(location, 1, &v0);
  glUniform1i(location, v0);
}

void GLES2DecoderImpl::DoUniform1iv(
    GLint location, GLsizei count, const GLint *value) {
  if (!current_program_ || current_program_->IsDeleted()) {
    // The program does not exist.
    SetGLError(GL_INVALID_OPERATION);
    return;
  }
  current_program_->SetSamplers(location, count, value);
  glUniform1iv(location, count, value);
}

void GLES2DecoderImpl::DoUseProgram(GLuint program) {
  ProgramManager::ProgramInfo* info = NULL;
  if (program) {
    info = GetProgramInfo(program);
    if (!info) {
      // Program was not linked successfully. (ie, glLinkProgram)
      SetGLError(GL_INVALID_OPERATION);
      return;
    }
  }
  current_program_ = info;
  glUseProgram(program);
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

void GLES2DecoderImpl::SetGLError(GLenum error) {
  error_bits_ |= GLES2Util::GLErrorToErrorBit(error);
}

void GLES2DecoderImpl::CopyRealGLErrorsToWrapper() {
  GLenum error;
  while ((error = glGetError()) != GL_NO_ERROR) {
    SetGLError(error);
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
                        texture_info ? texture_info->texture_id() : 0);
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
      SetGLError(GL_INVALID_OPERATION);
      return false;
    }
  }
  return true;
};

error::Error GLES2DecoderImpl::HandleDrawElements(
    uint32 immediate_data_size, const gles2::DrawElements& c) {
  if (!bound_element_array_buffer_ ||
      bound_element_array_buffer_->IsDeleted()) {
    SetGLError(GL_INVALID_OPERATION);
  } else {
    GLenum mode = c.mode;
    GLsizei count = c.count;
    GLenum type = c.type;
    int32 offset = c.index_offset;
    if (count < 0 || offset < 0) {
      SetGLError(GL_INVALID_VALUE);
    } else if (!ValidateGLenumDrawMode(mode) ||
               !ValidateGLenumIndexType(type)) {
      SetGLError(GL_INVALID_ENUM);
    } else {
      GLuint max_vertex_accessed;
      if (!bound_element_array_buffer_->GetMaxValueForRange(
          offset, count, type, &max_vertex_accessed)) {
        SetGLError(GL_INVALID_OPERATION);
      } else {
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
      }
    }
  }
  return error::kNoError;
}

// Calls glShaderSource for the various versions of the ShaderSource command.
// Assumes that data / data_size points to a piece of memory that is in range
// of whatever context it came from (shared memory, immediate memory, bucket
// memory.)
error::Error GLES2DecoderImpl::ShaderSourceHelper(
    GLuint shader, const char* data, uint32 data_size) {
  ShaderManager::ShaderInfo* info = GetShaderInfo(shader);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION);
    return error::kNoError;
  }
  // Note: We don't actually call glShaderSource here. We wait until
  // the call to glCompileShader.
  info->Update(std::string(data, data + data_size));
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleShaderSource(
    uint32 immediate_data_size, const gles2::ShaderSource& c) {
  GLuint shader;
  if (!id_manager()->GetServiceId(c.shader, &shader)) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  uint32 data_size = c.data_size;
  const char* data = GetSharedMemoryAs<const char*>(
      c.data_shm_id, c.data_shm_offset, data_size);
  if (!data) {
    return error::kOutOfBounds;
  }
  return ShaderSourceHelper(shader, data, data_size);
}

error::Error GLES2DecoderImpl::HandleShaderSourceImmediate(
  uint32 immediate_data_size, const gles2::ShaderSourceImmediate& c) {
  GLuint shader;
  if (!id_manager()->GetServiceId(c.shader, &shader)) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  uint32 data_size = c.data_size;
  const char* data = GetImmediateDataAs<const char*>(
      c, data_size, immediate_data_size);
  if (!data) {
    return error::kOutOfBounds;
  }
  return ShaderSourceHelper(shader, data, data_size);
}

void GLES2DecoderImpl::DoCompileShader(GLuint shader) {
  ShaderManager::ShaderInfo* info = GetShaderInfo(shader);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION);
    return;
  }
  // TODO(gman): Run shader through compiler that converts GL ES 2.0 shader
  // to DesktopGL shader and pass that to glShaderSource and then
  // glCompileShader.
  const char* ptr = info->source().c_str();
  glShaderSource(shader, 1, &ptr, NULL);
  glCompileShader(shader);
};

void GLES2DecoderImpl::DoGetShaderiv(
    GLuint shader, GLenum pname, GLint* params) {
  ShaderManager::ShaderInfo* info = GetShaderInfo(shader);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION);
    return;
  }
  if (pname == GL_SHADER_SOURCE_LENGTH) {
    *params = info->source().size();
  } else {
    glGetShaderiv(shader, pname, params);
  }
}

void GLES2DecoderImpl::DoGetShaderSource(
      GLuint shader, GLsizei bufsize, GLsizei* length, char* dst) {
  ShaderManager::ShaderInfo* info = GetShaderInfo(shader);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION);
    return;
  }
  // bufsize is set by the service side code and should always be positive.
  DCHECK_GT(bufsize, 0);
  const std::string& source = info->source();
  GLsizei size = std::min(bufsize - 1, static_cast<GLsizei>(source.size()));
  if (length) {
    *length = size;
  }
  memcpy(dst, source.c_str(), size);
  dst[size] = '\0';
}

error::Error GLES2DecoderImpl::HandleVertexAttribPointer(
    uint32 immediate_data_size, const gles2::VertexAttribPointer& c) {
  if (bound_array_buffer_ && !bound_array_buffer_->IsDeleted()) {
    GLuint indx = c.indx;
    GLint size = c.size;
    GLenum type = c.type;
    GLboolean normalized = c.normalized;
    GLsizei stride = c.stride;
    GLsizei offset = c.offset;
    const void* ptr = reinterpret_cast<const void*>(offset);
    if (!ValidateGLenumVertexAttribType(type) ||
        !ValidateGLintVertexAttribSize(size)) {
      SetGLError(GL_INVALID_ENUM);
      return error::kNoError;
    }
    if (indx >= group_->max_vertex_attribs() ||
        stride < 0 ||
        offset < 0) {
      SetGLError(GL_INVALID_VALUE);
      return error::kNoError;
    }
    GLsizei component_size =
        GLES2Util::GetGLTypeSizeForTexturesAndBuffers(type);
    GLsizei real_stride = stride != 0 ? stride : component_size * size;
    if (offset % component_size > 0) {
      SetGLError(GL_INVALID_VALUE);
      return error::kNoError;
    }
    vertex_attrib_infos_[indx].SetInfo(
        bound_array_buffer_,
        size,
        type,
        real_stride,
        offset);
    glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
  } else {
    SetGLError(GL_INVALID_VALUE);
  }
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
    SetGLError(GL_INVALID_VALUE);
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

  if (!ValidateGLenumReadPixelFormat(format) ||
      !ValidateGLenumPixelType(type)) {
    SetGLError(GL_INVALID_ENUM);
    return error::kNoError;
  }
  if (width == 0 || height == 0) {
    return error::kNoError;
  }

  CopyRealGLErrorsToWrapper();

  // Get the size of the current fbo or backbuffer.
  GLsizei max_width = 0;
  GLsizei max_height = 0;
  if (bound_framebuffer_ != 0) {
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
                &max_width);
            glGetRenderbufferParameterivEXT(
                GL_RENDERBUFFER,
                GL_RENDERBUFFER_HEIGHT,
                &max_height);
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
            TextureManager::TextureInfo* texture_info =
                GetTextureInfo(texture_id);
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
                  face ? face : GL_TEXTURE_2D, level, &max_width, &max_height);
            }
          }
          break;
        }
      default:
        // unknown so assume max_width = 0.
        break;
    }
  } else {
    // TODO(gman): Get these values from the proper place.
    max_width = 300;
    max_height = 150;
  }

  GLint max_x;
  GLint max_y;
  if (!SafeAdd(x, width, &max_x) || !SafeAdd(y, height, &max_y)) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }

  if (x < 0 || y < 0 || max_x > max_width || max_y > max_height) {
    // The user requested an out of range area. Get the results 1 line
    // at a time.
    uint32 temp_size;
    if (!GLES2Util::ComputeImageDataSize(
        width, 1, format, type, pack_alignment_, &temp_size)) {
      SetGLError(GL_INVALID_VALUE);
      return error::kNoError;
    }
    GLsizei unpadded_row_size = temp_size;
    if (!GLES2Util::ComputeImageDataSize(
        width, 2, format, type, pack_alignment_, &temp_size)) {
      SetGLError(GL_INVALID_VALUE);
      return error::kNoError;
    }
    GLsizei padded_row_size = temp_size - unpadded_row_size;
    if (padded_row_size < 0 || unpadded_row_size < 0) {
      SetGLError(GL_INVALID_VALUE);
      return error::kNoError;
    }

    GLint dest_x_offset = std::max(-x, 0);
    uint32 dest_row_offset;
    if (!GLES2Util::ComputeImageDataSize(
      dest_x_offset, 1, format, type, pack_alignment_, &dest_row_offset)) {
      SetGLError(GL_INVALID_VALUE);
      return error::kNoError;
    }

    // Copy each row into the larger dest rect.
    int8* dst = static_cast<int8*>(pixels);
    GLint read_x = std::max(0, x);
    GLint read_end_x = std::max(0, std::min(max_width, max_x));
    GLint read_width = read_end_x - read_x;
    for (GLint yy = 0; yy < height; ++yy) {
      GLint ry = y + yy;

      // Clear the row.
      memset(dst, 0, unpadded_row_size);

      // If the row is in range, copy it.
      if (ry >= 0 && ry < max_height && read_width > 0) {
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
    SetGLError(error);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePixelStorei(
    uint32 immediate_data_size, const gles2::PixelStorei& c) {
  GLenum pname = c.pname;
  GLenum param = c.param;
  if (!ValidateGLenumPixelStore(pname)) {
    SetGLError(GL_INVALID_ENUM);
    return error::kNoError;
  }
  if (!ValidateGLintPixelStoreAlignment(param)) {
    SetGLError(GL_INVALID_VALUE);
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

error::Error GLES2DecoderImpl::HandleGetAttribLocation(
    uint32 immediate_data_size, const gles2::GetAttribLocation& c) {
  GLuint program;
  if (!id_manager()->GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  ProgramManager::ProgramInfo* info = GetProgramInfo(program);
  if (!info) {
    // Program was not linked successfully. (ie, glLinkProgram)
    SetGLError(GL_INVALID_OPERATION);
    return error::kNoError;
  }
  uint32 name_size = c.data_size;
  const char* name = GetSharedMemoryAs<const char*>(
      c.name_shm_id, c.name_shm_offset, name_size);
  GLint* location = GetSharedMemoryAs<GLint*>(
      c.location_shm_id, c.location_shm_offset, sizeof(GLint));
  if (!location || !name) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  *location = info->GetAttribLocation(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetAttribLocationImmediate(
    uint32 immediate_data_size, const gles2::GetAttribLocationImmediate& c) {
  GLuint program;
  if (!id_manager()->GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  ProgramManager::ProgramInfo* info = GetProgramInfo(program);
  if (!info) {
    // Program was not linked successfully. (ie, glLinkProgram)
    SetGLError(GL_INVALID_OPERATION);
    return error::kNoError;
  }
  uint32 name_size = c.data_size;
  const char* name = GetImmediateDataAs<const char*>(
      c, name_size, immediate_data_size);
  GLint* location = GetSharedMemoryAs<GLint*>(
      c.location_shm_id, c.location_shm_offset, sizeof(GLint));
  if (!location || !name) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  *location = info->GetAttribLocation(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetUniformLocation(
    uint32 immediate_data_size, const gles2::GetUniformLocation& c) {
  GLuint program;
  if (!id_manager()->GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  ProgramManager::ProgramInfo* info = GetProgramInfo(program);
  if (!info) {
    // Program was not linked successfully. (ie, glLinkProgram)
    SetGLError(GL_INVALID_OPERATION);
    return error::kNoError;
  }
  uint32 name_size = c.data_size;
  const char* name = GetSharedMemoryAs<const char*>(
      c.name_shm_id, c.name_shm_offset, name_size);
  GLint* location = GetSharedMemoryAs<GLint*>(
      c.location_shm_id, c.location_shm_offset, sizeof(GLint));
  if (!location || !name) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  *location = info->GetUniformLocation(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetUniformLocationImmediate(
    uint32 immediate_data_size, const gles2::GetUniformLocationImmediate& c) {
  GLuint program;
  if (!id_manager()->GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  ProgramManager::ProgramInfo* info = GetProgramInfo(program);
  if (!info) {
    // Program was not linked successfully. (ie, glLinkProgram)
    SetGLError(GL_INVALID_OPERATION);
    return error::kNoError;
  }
  uint32 name_size = c.data_size;
  const char* name = GetImmediateDataAs<const char*>(
      c, name_size, immediate_data_size);
  GLint* location = GetSharedMemoryAs<GLint*>(
      c.location_shm_id, c.location_shm_offset, sizeof(GLint));
  if (!location || !name) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  *location = info->GetUniformLocation(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetString(
    uint32 immediate_data_size, const gles2::GetString& c) {
  GLenum name = static_cast<GLenum>(c.name);
  if (!ValidateGLenumStringType(name)) {
    SetGLError(GL_INVALID_ENUM);
    return error::kNoError;
  }
  Bucket* bucket = CreateBucket(c.bucket_id);
  bucket->SetFromString(reinterpret_cast<const char*>(glGetString(name)));
  return error::kNoError;
}

void GLES2DecoderImpl::DoBufferData(
  GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage) {
  if (!ValidateGLenumBufferTarget(target) ||
      !ValidateGLenumBufferUsage(usage)) {
    SetGLError(GL_INVALID_ENUM);
    return;
  }
  if (size < 0) {
    SetGLError(GL_INVALID_VALUE);
    DoBufferData(target, size, data, usage);
  }
  BufferManager::BufferInfo* info = GetBufferInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION);
    DoBufferData(target, size, data, usage);
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
    SetGLError(error);
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
    SetGLError(GL_INVALID_OPERATION);
  }
  if (!info->SetRange(offset, size, data)) {
    SetGLError(GL_INVALID_VALUE);
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
  if (!ValidateGLenumTextureTarget(target) ||
      !ValidateGLenumCompressedTextureInternalFormat(internal_format)) {
    SetGLError(GL_INVALID_ENUM);
    return error::kNoError;
  }
  if (!texture_manager()->ValidForTarget(target, level, width, height, 1) ||
      border != 0) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION);
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
  if (!ValidateGLenumTextureTarget(target) ||
      !ValidateGLenumTextureFormat(internal_format) ||
      !ValidateGLenumTextureFormat(format) ||
      !ValidateGLenumPixelType(type)) {
    SetGLError(GL_INVALID_ENUM);
    return error::kNoError;
  }
  if (!texture_manager()->ValidForTarget(target, level, width, height, 1) ||
      border != 0) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION);
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
    SetGLError(GL_INVALID_ENUM);
    return error::kNoError;
  }
  if (index >= group_->max_vertex_attribs()) {
    SetGLError(GL_INVALID_VALUE);
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
  if (!id_manager()->GetServiceId(program, service_id)) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  ProgramManager::ProgramInfo* info = GetProgramInfo(*service_id);
  if (!info) {
    // Program was not linked successfully. (ie, glLinkProgram)
    SetGLError(GL_INVALID_OPERATION);
    return false;
  }
  GLenum type;
  if (!info->GetUniformTypeByLocation(location, &type)) {
    // No such location.
    SetGLError(GL_INVALID_OPERATION);
    return false;
  }
  GLsizei size = GLES2Util::GetGLDataTypeSizeForUniforms(type);
  if (size == 0) {
    SetGLError(GL_INVALID_OPERATION);
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
  if (!ValidateGLenumShaderType(shader_type) ||
      !ValidateGLenumShaderPrecision(precision_type)) {
    SetGLError(GL_INVALID_ENUM);
  } else {
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
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetAttachedShaders(
    uint32 immediate_data_size, const gles2::GetAttachedShaders& c) {
  GLuint service_id;
  uint32 result_size = c.result_size;
  if (!id_manager()->GetServiceId(c.program, &service_id)) {
    SetGLError(GL_INVALID_VALUE);
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
  glGetAttachedShaders(service_id, max_count, &count, result->GetData());
  for (GLsizei ii = 0; ii < count; ++ii) {
    if (!id_manager()->GetClientId(result->GetData()[ii],
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
  GLuint service_id;
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
  if (!id_manager()->GetServiceId(program, &service_id)) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  ProgramManager::ProgramInfo* info = GetProgramInfo(service_id);
  if (!info) {
    // Program was not linked successfully. (ie, glLinkProgram)
    SetGLError(GL_INVALID_OPERATION);
    return error::kNoError;
  }
  const ProgramManager::ProgramInfo::UniformInfo* uniform_info =
      info->GetUniformInfo(index);
  if (!uniform_info) {
    SetGLError(GL_INVALID_VALUE);
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
  GLuint service_id;
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
  if (!id_manager()->GetServiceId(program, &service_id)) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  ProgramManager::ProgramInfo* info = GetProgramInfo(service_id);
  if (!info) {
    // Program was not linked successfully. (ie, glLinkProgram)
    SetGLError(GL_INVALID_OPERATION);
    return error::kNoError;
  }
  const ProgramManager::ProgramInfo::VertexAttribInfo* attrib_info =
      info->GetAttribInfo(index);
  if (!attrib_info) {
    SetGLError(GL_INVALID_VALUE);
    return error::kNoError;
  }
  result->success = 1;  // true.
  result->size = attrib_info->size;
  result->type = attrib_info->type;
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(attrib_info->name);
  return error::kNoError;
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/gles2_cmd_decoder_autogen.h"

}  // namespace gles2
}  // namespace gpu
