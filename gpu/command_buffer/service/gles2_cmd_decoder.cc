// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include <stdio.h>

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "app/gfx/gl/gl_context.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/at_exit.h"
#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "base/weak_ptr.h"
#include "build/build_config.h"
#define GLES2_GPU_SERVICE 1
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/shader_translator.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/GLES2/gles2_command_buffer.h"

#if !defined(GL_DEPTH24_STENCIL8)
#define GL_DEPTH24_STENCIL8 0x88F0
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

static bool IsAngle() {
#if defined(OS_WIN)
  return gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2;
#else
  return false;
#endif
}

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

// Temporarily changes a decoder's bound frame buffer to a resolved version of
// the multisampled offscreen render buffer if and only if that buffer is
// currently bound and is multisampled.
class ScopedResolvedFrameBufferBinder {
 public:
  explicit ScopedResolvedFrameBufferBinder(GLES2DecoderImpl* decoder);
  ~ScopedResolvedFrameBufferBinder();

 private:
  GLES2DecoderImpl* decoder_;
  bool resolve_and_bind_;
  DISALLOW_COPY_AND_ASSIGN(ScopedResolvedFrameBufferBinder);
};

// Encapsulates an OpenGL texture.
class Texture {
 public:
  explicit Texture(GLES2DecoderImpl* decoder);
  ~Texture();

  // Create a new render texture.
  void Create();

  // Set the initial size and format of a render texture or resize it.
  bool AllocateStorage(const gfx::Size& size, GLenum format);

  // Copy the contents of the currently bound frame buffer.
  void Copy(const gfx::Size& size);

  // Destroy the render texture. This must be explicitly called before
  // destroying this object.
  void Destroy();

  // Invalidate the texture. This can be used when a context is lost and it is
  // not possible to make it current in order to free the resource.
  void Invalidate();

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
  bool AllocateStorage(const gfx::Size& size, GLenum format, GLsizei samples);

  // Destroy the render buffer. This must be explicitly called before destroying
  // this object.
  void Destroy();

  // Invalidate the render buffer. This can be used when a context is lost and
  // it is not possible to make it current in order to free the resource.
  void Invalidate();

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

  // Attach a render buffer to a frame buffer. Note that this unbinds any
  // currently bound frame buffer.
  void AttachRenderBuffer(GLenum target, RenderBuffer* render_buffer);

  // Clear the given attached buffers.
  void Clear(GLbitfield buffers);

  // Destroy the frame buffer. This must be explicitly called before destroying
  // this object.
  void Destroy();

  // Invalidate the frame buffer. This can be used when a context is lost and it
  // is not possible to make it current in order to free the resource.
  void Invalidate();

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

class ContextCreationAttribParser {
 public:
  ContextCreationAttribParser();
  bool Parse(const std::vector<int32>& attribs);

  // -1 if invalid or unspecified.
  int32 alpha_size_;
  int32 blue_size_;
  int32 green_size_;
  int32 red_size_;
  int32 depth_size_;
  int32 stencil_size_;
  int32 samples_;
  int32 sample_buffers_;
};

ContextCreationAttribParser::ContextCreationAttribParser()
  : alpha_size_(-1),
    blue_size_(-1),
    green_size_(-1),
    red_size_(-1),
    depth_size_(-1),
    stencil_size_(-1),
    samples_(-1),
    sample_buffers_(-1) {
}

bool ContextCreationAttribParser::Parse(const std::vector<int32>& attribs) {
  // From <EGL/egl.h>.
  const int32 EGL_ALPHA_SIZE = 0x3021;
  const int32 EGL_BLUE_SIZE = 0x3022;
  const int32 EGL_GREEN_SIZE = 0x3023;
  const int32 EGL_RED_SIZE = 0x3024;
  const int32 EGL_DEPTH_SIZE = 0x3025;
  const int32 EGL_STENCIL_SIZE = 0x3026;
  const int32 EGL_SAMPLES = 0x3031;
  const int32 EGL_SAMPLE_BUFFERS = 0x3032;
  const int32 EGL_NONE = 0x3038;

  for (size_t i = 0; i < attribs.size(); i += 2) {
    const int32 attrib = attribs[i];
    if (i + 1 >= attribs.size()) {
      if (attrib == EGL_NONE)
        return true;

      DLOG(ERROR) << "Missing value after context creation attribute: "
                  << attrib;
      return false;
    }

    const int32 value = attribs[i+1];
    switch (attrib) {
      case EGL_ALPHA_SIZE:
        alpha_size_ = value;
        break;
      case EGL_BLUE_SIZE:
        blue_size_ = value;
        break;
      case EGL_GREEN_SIZE:
        green_size_ = value;
        break;
      case EGL_RED_SIZE:
        red_size_ = value;
        break;
      case EGL_DEPTH_SIZE:
        depth_size_ = value;
        break;
      case EGL_STENCIL_SIZE:
        stencil_size_ = value;
        break;
      case EGL_SAMPLES:
        samples_ = value;
        break;
      case EGL_SAMPLE_BUFFERS:
        sample_buffers_ = value;
        break;
      case EGL_NONE:
        // Terminate list, even if more attributes.
        return true;
      default:
        DLOG(ERROR) << "Invalid context creation attribute: " << attrib;
        return false;
    }
  }

  return true;
}

// }  // anonymous namespace.

bool GLES2Decoder::GetServiceTextureId(uint32 client_texture_id,
                                       uint32* service_texture_id) {
  return false;
}

GLES2Decoder::GLES2Decoder()
    : debug_(false) {
}

GLES2Decoder::~GLES2Decoder() {
}

class VertexAttribManager {
 public:
  // Info about Vertex Attributes. This is used to track what the user currently
  // has bound on each Vertex Attribute so that checking can be done at
  // glDrawXXX time.
  class VertexAttribInfo {
   public:
    typedef std::list<VertexAttribInfo*> VertexAttribInfoList;
    struct Vec4 {
      float v[4];
    };

    VertexAttribInfo()
        : index_(0),
          enabled_(false),
          size_(4),
          type_(GL_FLOAT),
          offset_(0),
          normalized_(GL_FALSE),
          gl_stride_(0),
          real_stride_(16),
          list_(NULL) {
      value_.v[0] = 0.0f;
      value_.v[1] = 0.0f;
      value_.v[2] = 0.0f;
      value_.v[3] = 1.0f;
    }

    // Returns true if this VertexAttrib can access index.
    bool CanAccess(GLuint index) const;

    BufferManager::BufferInfo* buffer() const {
      return buffer_;
    }

    GLsizei offset() const {
      return offset_;
    }

    GLuint index() const {
      return index_;
    }

    GLint size() const {
      return size_;
    }

    GLenum type() const {
      return type_;
    }

    GLboolean normalized() const {
      return normalized_;
    }

    GLsizei gl_stride() const {
      return gl_stride_;
    }

    bool enabled() const {
      return enabled_;
    }

    void set_value(const Vec4& value) {
      value_ = value;
    }

    const Vec4& value() const {
      return value_;
    }

   private:
    friend class VertexAttribManager;

    void set_enabled(bool enabled) {
      enabled_ = enabled;
    }

    void set_index(GLuint index) {
      index_ = index;
    }

    void SetList(VertexAttribInfoList* new_list) {
      DCHECK(new_list);

      if (list_) {
        list_->erase(it_);
      }

      it_ = new_list->insert(new_list->end(), this);
      list_ = new_list;
    }

    void SetInfo(
        BufferManager::BufferInfo* buffer,
        GLint size,
        GLenum type,
        GLboolean normalized,
        GLsizei gl_stride,
        GLsizei real_stride,
        GLsizei offset) {
      DCHECK_GT(real_stride, 0);
      buffer_ = buffer;
      size_ = size;
      type_ = type;
      normalized_ = normalized;
      gl_stride_ = gl_stride;
      real_stride_ = real_stride;
      offset_ = offset;
    }

    // The index of this attrib.
    GLuint index_;

    // Whether or not this attribute is enabled.
    bool enabled_;

    // number of components (1, 2, 3, 4)
    GLint size_;

    // GL_BYTE, GL_FLOAT, etc. See glVertexAttribPointer.
    GLenum type_;

    // The offset into the buffer.
    GLsizei offset_;

    GLboolean normalized_;

    // The stride passed to glVertexAttribPointer.
    GLsizei gl_stride_;

    // The stride that will be used to access the buffer. This is the actual
    // stide, NOT the GL bogus stride. In other words there is never a stride
    // of 0.
    GLsizei real_stride_;

    // The current value of the attrib.
    Vec4 value_;

    // The buffer bound to this attribute.
    BufferManager::BufferInfo::Ref buffer_;

    // List this info is on.
    VertexAttribInfoList* list_;

    // Iterator for list this info is on. Enabled/Disabled
    VertexAttribInfoList::iterator it_;
  };

  typedef std::list<VertexAttribInfo*> VertexAttribInfoList;

  VertexAttribManager()
      : max_vertex_attribs_(0),
        num_fixed_attribs_(0) {
  }

  void Initialize(uint32 num_vertex_attribs);

  bool Enable(GLuint index, bool enable);

  bool HaveFixedAttribs() const {
    return num_fixed_attribs_ != 0;
  }

  const VertexAttribInfoList& GetEnabledVertexAttribInfos() const {
    return enabled_vertex_attribs_;
  }

  VertexAttribInfo* GetVertexAttribInfo(GLuint index) {
    if (index < max_vertex_attribs_) {
      return &vertex_attrib_infos_[index];
    }
    return NULL;
  }

  void SetAttribInfo(
      GLuint index,
      BufferManager::BufferInfo* buffer,
      GLint size,
      GLenum type,
      GLboolean normalized,
      GLsizei gl_stride,
      GLsizei real_stride,
      GLsizei offset) {
    VertexAttribInfo* info = GetVertexAttribInfo(index);
    if (info) {
      if (info->type() == GL_FIXED) {
        --num_fixed_attribs_;
      }
      if (type == GL_FIXED) {
        ++num_fixed_attribs_;
      }
      info->SetInfo(
          buffer, size, type, normalized, gl_stride, real_stride, offset);
    }
  }


 private:
  uint32 max_vertex_attribs_;

  // number of attribs using type GL_FIXED.
  int num_fixed_attribs_;

  // Info for each vertex attribute saved so we can check at glDrawXXX time
  // if it is safe to draw.
  scoped_array<VertexAttribInfo> vertex_attrib_infos_;

  // Lists for which vertex attribs are enabled, disabled.
  VertexAttribInfoList enabled_vertex_attribs_;
  VertexAttribInfoList disabled_vertex_attribs_;
};

bool VertexAttribManager::VertexAttribInfo::CanAccess(GLuint index) const {
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


void VertexAttribManager::Initialize(uint32 max_vertex_attribs) {
  max_vertex_attribs_ = max_vertex_attribs;
  vertex_attrib_infos_.reset(
      new VertexAttribInfo[max_vertex_attribs]);
  for (uint32 vv = 0; vv < max_vertex_attribs; ++vv) {
    vertex_attrib_infos_[vv].set_index(vv);
    vertex_attrib_infos_[vv].SetList(&disabled_vertex_attribs_);
  }
}

bool VertexAttribManager::Enable(GLuint index, bool enable) {
  if (index >= max_vertex_attribs_) {
    return false;
  }
  VertexAttribInfo& info = vertex_attrib_infos_[index];
  if (info.enabled() != enable) {
    info.set_enabled(enable);
    info.SetList(enable ? &enabled_vertex_attribs_ : &disabled_vertex_attribs_);
  }
  return true;
}

// This class implements GLES2Decoder so we don't have to expose all the GLES2
// cmd stuff to outside this class.
class GLES2DecoderImpl : public base::SupportsWeakPtr<GLES2DecoderImpl>,
                         public GLES2Decoder {
 public:
  explicit GLES2DecoderImpl(ContextGroup* group);

  // Overridden from AsyncAPIInterface.
  virtual Error DoCommand(unsigned int command,
                          unsigned int arg_count,
                          const void* args);

  // Overridden from AsyncAPIInterface.
  virtual const char* GetCommandName(unsigned int command_id) const;

  // Overridden from GLES2Decoder.
  virtual bool Initialize(gfx::GLContext* context,
                          const gfx::Size& size,
                          const char* allowed_extensions,
                          const std::vector<int32>& attribs,
                          GLES2Decoder* parent,
                          uint32 parent_client_texture_id);
  virtual void Destroy();
  virtual void ResizeOffscreenFrameBuffer(const gfx::Size& size);
  virtual bool UpdateOffscreenFrameBufferSize();
  virtual bool MakeCurrent();
  virtual GLES2Util* GetGLES2Util() { return &util_; }
  virtual gfx::GLContext* GetGLContext() { return context_.get(); }
  virtual ContextGroup* GetContextGroup() { return group_.get(); }

  virtual void SetResizeCallback(Callback1<gfx::Size>::Type* callback);
  virtual void SetSwapBuffersCallback(Callback0::Type* callback);
  virtual bool GetServiceTextureId(uint32 client_texture_id,
                                   uint32* service_texture_id);

  // Restores the current state to the user's settings.
  void RestoreCurrentFramebufferBindings();
  void RestoreCurrentRenderbufferBindings();
  void RestoreCurrentTexture2DBindings();

 private:
  friend class ScopedGLErrorSuppressor;
  friend class ScopedResolvedFrameBufferBinder;
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

  // Initialize or re-initialize the shader translator.
  bool InitializeShaderTranslator();

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

  bool IsOffscreenBufferMultisampled() const {
    return offscreen_target_samples_ > 1;
  }

  // Creates a TextureInfo for the given texture.
  TextureManager::TextureInfo* CreateTextureInfo(
      GLuint client_id, GLuint service_id) {
    return texture_manager()->CreateTextureInfo(
        feature_info_, client_id, service_id);
  }

  // Gets the texture info for the given texture. Returns NULL if none exists.
  TextureManager::TextureInfo* GetTextureInfo(GLuint client_id) {
    TextureManager::TextureInfo* info =
        texture_manager()->GetTextureInfo(client_id);
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Deletes the texture info for the given texture.
  void RemoveTextureInfo(GLuint client_id) {
    texture_manager()->RemoveTextureInfo(feature_info_, client_id);
  }

  // Get the size (in pixels) of the currently bound frame buffer (either FBO
  // or regular back buffer).
  gfx::Size GetBoundReadFrameBufferSize();

  // Get the format of the currently bound frame buffer (either FBO or regular
  // back buffer)
  GLenum GetBoundReadFrameBufferInternalFormat();

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

  // Wrapper for CompressedTexSubImage2D.
  void DoCompressedTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLsizei imageSize,
    const void * data);

  // Wrapper for CopyTexImage2D.
  void DoCopyTexImage2D(
    GLenum target,
    GLint level,
    GLenum internal_format,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLint border);

  // Wrapper for CopyTexSubImage2D.
  void DoCopyTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height);

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

  // Wrapper for TexSubImage2D.
  void DoTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    const void * data);

  // Creates a ProgramInfo for the given program.
  void CreateProgramInfo(GLuint client_id, GLuint service_id) {
    program_manager()->CreateProgramInfo(client_id, service_id);
  }

  // Gets the program info for the given program. Returns NULL if none exists.
  ProgramManager::ProgramInfo* GetProgramInfo(GLuint client_id) {
    return program_manager()->GetProgramInfo(client_id);
  }

  // Gets the program info for the given program. If it's not a program
  // generates a GL error. Returns NULL if not program.
  ProgramManager::ProgramInfo* GetProgramInfoNotShader(
      GLuint client_id, const char* function_name) {
    ProgramManager::ProgramInfo* info = GetProgramInfo(client_id);
    if (!info) {
      if (GetShaderInfo(client_id)) {
        SetGLError(GL_INVALID_OPERATION,
                   (std::string(function_name) +
                    ": shader passed for program").c_str());
      } else {
        SetGLError(GL_INVALID_VALUE,
                   (std::string(function_name) + ": unknown program").c_str());
      }
    }
    return info;
  }


  // Creates a ShaderInfo for the given shader.
  void CreateShaderInfo(GLuint client_id,
                        GLuint service_id,
                        GLenum shader_type) {
    shader_manager()->CreateShaderInfo(client_id, service_id, shader_type);
  }

  // Gets the shader info for the given shader. Returns NULL if none exists.
  ShaderManager::ShaderInfo* GetShaderInfo(GLuint client_id) {
    return shader_manager()->GetShaderInfo(client_id);
  }

  // Gets the shader info for the given shader. If it's not a shader generates a
  // GL error. Returns NULL if not shader.
  ShaderManager::ShaderInfo* GetShaderInfoNotProgram(
      GLuint client_id, const char* function_name) {
    ShaderManager::ShaderInfo* info = GetShaderInfo(client_id);
    if (!info) {
      if (GetProgramInfo(client_id)) {
        SetGLError(
            GL_INVALID_OPERATION,
            (std::string(function_name) +
             ": program passed for shader").c_str());
      } else {
        SetGLError(GL_INVALID_VALUE,
                   (std::string(function_name) + ": unknown shader").c_str());
      }
    }
    return info;
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

  // Clears any uncleared render buffers attached to the given frame buffer.
  void ClearUnclearedRenderbuffers(
      GLenum target, FramebufferManager::FramebufferInfo* info);

  // Restore all GL state that affects clearing.
  void RestoreClearState();

  // Remembers the state of some capabilities.
  void SetCapabilityState(GLenum cap, bool enabled);

  // Checks if the current program exists and is valid. If not generates the
  // appropriate GL error.  Returns true if the current program is in a usable
  // state.
  bool CheckCurrentProgram(const char* function_name);

  // Checks if the current program exists and is valid and that location is not
  // -1. If the current program is not valid generates the appropriate GL
  // error. Returns true if the current program is in a usable state and
  // location is not -1.
  bool CheckCurrentProgramForUniform(GLint location, const char* function_name);

  // Gets the type of a uniform for a location in the current program. Sets GL
  // errors if the current program is not valid. Returns true if the current
  // program is valid and the location exists.
  bool GetUniformTypeByLocation(
      GLint location, const char* function_name, GLenum* type);

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

  // Wrapper for glBlitFramebufferEXT.
  void DoBlitFramebufferEXT(
      GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
      GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
      GLbitfield mask, GLenum filter);

  // Wrapper for glBufferData.
  void DoBufferData(
    GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage);

  // Wrapper for glBufferSubData.
  void DoBufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data);

  // Wrapper for glCheckFramebufferStatus
  GLenum DoCheckFramebufferStatus(GLenum target);

  // Wrappers for clear and mask settings functions.
  void DoClearColor(
      GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
  void DoClearDepthf(GLclampf depth);
  void DoClearStencil(GLint s);
  void DoColorMask(
      GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
  void DoDepthMask(GLboolean depth);
  void DoStencilMask(GLuint mask);
  void DoStencilMaskSeparate(GLenum face, GLuint mask);

  // Wrapper for glCompileShader.
  void DoCompileShader(GLuint shader);

  // Helper for DeleteSharedIdsCHROMIUM commands.
  void DoDeleteSharedIdsCHROMIUM(
      GLuint namespace_id, GLsizei n, const GLuint* ids);

  // Wrapper for glDetachShader
  void DoDetachShader(GLuint client_program_id, GLint client_shader_id);

  // Wrapper for glDrawArrays.
  void DoDrawArrays(GLenum mode, GLint first, GLsizei count);

  // Wrapper for glDisable
  void DoDisable(GLenum cap);

  // Wrapper for glDisableVertexAttribArray.
  void DoDisableVertexAttribArray(GLuint index);

  // Wrapper for glEnable
  void DoEnable(GLenum cap);

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

  // Helper for GenSharedIdsCHROMIUM commands.
  void DoGenSharedIdsCHROMIUM(
      GLuint namespace_id, GLuint id_offset, GLsizei n, GLuint* ids);

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
  GLuint DoGetMaxValueInBufferCHROMIUM(
      GLuint buffer_id, GLsizei count, GLenum type, GLuint offset);

  // Wrapper for glGetProgramiv.
  void DoGetProgramiv(
      GLuint program_id, GLenum pname, GLint* params);

  // Wrapper for glRenderbufferParameteriv.
  void DoGetRenderbufferParameteriv(
      GLenum target, GLenum pname, GLint* params);

  // Wrapper for glGetShaderiv
  void DoGetShaderiv(GLuint shader, GLenum pname, GLint* params);

  // Wrappers for glGetVertexAttrib.
  void DoGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params);
  void DoGetVertexAttribiv(GLuint index, GLenum pname, GLint *params);

  // Wrappers for glIsXXX functions.
  bool DoIsBuffer(GLuint client_id);
  bool DoIsFramebuffer(GLuint client_id);
  bool DoIsProgram(GLuint client_id);
  bool DoIsRenderbuffer(GLuint client_id);
  bool DoIsShader(GLuint client_id);
  bool DoIsTexture(GLuint client_id);

  // Wrapper for glLinkProgram
  void DoLinkProgram(GLuint program);

  // Helper for RegisterSharedIdsCHROMIUM.
  void DoRegisterSharedIdsCHROMIUM(
      GLuint namespace_id, GLsizei n, const GLuint* ids);

  // Wrapper for glRenderbufferStorage.
  void DoRenderbufferStorage(
      GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

  // Wrapper for glRenderbufferStorageMultisampleEXT.
  void DoRenderbufferStorageMultisample(
      GLenum target, GLsizei samples, GLenum internalformat,
      GLsizei width, GLsizei height);

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
  void DoUniform1iv(GLint location, GLsizei count, const GLint* value);

  // Wrappers for glUniformfv because some drivers don't correctly accept
  // bool uniforms.
  void DoUniform1fv(GLint location, GLsizei count, const GLfloat* value);
  void DoUniform2fv(GLint location, GLsizei count, const GLfloat* value);
  void DoUniform3fv(GLint location, GLsizei count, const GLfloat* value);
  void DoUniform4fv(GLint location, GLsizei count, const GLfloat* value);

  // Wrappers for glVertexAttrib??
  void DoVertexAttrib1f(GLuint index, GLfloat v0);
  void DoVertexAttrib2f(GLuint index, GLfloat v0, GLfloat v1);
  void DoVertexAttrib3f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
  void DoVertexAttrib4f(
      GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
  void DoVertexAttrib1fv(GLuint index, const GLfloat *v);
  void DoVertexAttrib2fv(GLuint index, const GLfloat *v);
  void DoVertexAttrib3fv(GLuint index, const GLfloat *v);
  void DoVertexAttrib4fv(GLuint index, const GLfloat *v);

  // Wrapper for glUseProgram
  void DoUseProgram(GLuint program);

  // Wrapper for glValidateProgram.
  void DoValidateProgram(GLuint program_client_id);

  void DoCopyTextureToParentTextureCHROMIUM(GLuint client_texture_id,
                                    GLuint parent_client_texture_id);

  void DoResizeCHROMIUM(GLuint width, GLuint height);

  // Gets the number of values that will be returned by glGetXXX. Returns
  // false if pname is unknown.
  bool GetNumValuesReturnedForGLGet(GLenum pname, GLsizei* num_values);

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

  // Returns true if attrib0 was simulated.
  bool SimulateAttrib0(GLuint max_vertex_accessed);
  void RestoreStateForSimulatedAttrib0();

  // Returns true if textures were set.
  bool SetBlackTextureForNonRenderableTextures();
  void RestoreStateForNonRenderableTextures();

  // Returns true if GL_FIXED attribs were simulated.
  bool SimulateFixedAttribs(GLuint max_vertex_accessed, bool* simulated);
  void RestoreStateForSimulatedFixedAttribs();

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

  // Gets the framebuffer info for a particular target.
  FramebufferManager::FramebufferInfo* GetFramebufferInfoForTarget(
      GLenum target) {
    FramebufferManager::FramebufferInfo* info = NULL;
    switch (target) {
      case GL_FRAMEBUFFER:
      case GL_DRAW_FRAMEBUFFER:
        info = bound_draw_framebuffer_;
        break;
      case GL_READ_FRAMEBUFFER:
        info = bound_read_framebuffer_;
        break;
      default:
        NOTREACHED();
        break;
    }
    return (info && !info->IsDeleted()) ? info : NULL;
  }

  // Validates the program and location for a glGetUniform call and returns
  // a SizeResult setup to receive the result. Returns true if glGetUniform
  // should be called.
  bool GetUniformSetup(
      GLuint program, GLint location,
      uint32 shm_id, uint32 shm_offset,
      error::Error* error, GLuint* service_id, void** result,
      GLenum* result_type);

  // Generate a member function prototype for each command in an automated and
  // typesafe way.
  #define GLES2_CMD_OP(name) \
     Error Handle ## name(             \
       uint32 immediate_data_size,          \
       const gles2::name& args);            \

  GLES2_COMMAND_LIST(GLES2_CMD_OP)

  #undef GLES2_CMD_OP

  // The GL context this decoder renders to on behalf of the client.
  scoped_ptr<gfx::GLContext> context_;

  // The ContextGroup for this decoder uses to track resources.
  ContextGroup::Ref group_;

  // A parent decoder can access this decoders saved offscreen frame buffer.
  // The parent pointer is reset if the parent is destroyed.
  base::WeakPtr<GLES2DecoderImpl> parent_;

  // Width and height to which an offscreen frame buffer should be resized on
  // the next call to SwapBuffers.
  gfx::Size pending_offscreen_size_;

  // Current width and height of the offscreen frame buffer.
  gfx::Size offscreen_size_;

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

  // Class that manages vertex attribs.
  VertexAttribManager vertex_attrib_manager_;

  // The buffer we bind to attrib 0 since OpenGL requires it (ES does not).
  GLuint attrib_0_buffer_id_;

  // The value currently in attrib_0.
  VertexAttribManager::VertexAttribInfo::Vec4 attrib_0_value_;

  // Whether or not the attrib_0 buffer holds the attrib_0_value.
  bool attrib_0_buffer_matches_value_;

  // The size of attrib 0.
  GLsizei attrib_0_size_;

  // The buffer used to simulate GL_FIXED attribs.
  GLuint fixed_attrib_buffer_id_;

  // The size of fiixed attrib buffer.
  GLsizei fixed_attrib_buffer_size_;

  // Current active texture by 0 - n index.
  // In other words, if we call glActiveTexture(GL_TEXTURE2) this value would
  // be 2.
  GLuint active_texture_unit_;

  // Which textures are bound to texture units through glActiveTexture.
  scoped_array<TextureUnit> texture_units_;

  // state saved for clearing so we can clear render buffers and then
  // restore to these values.
  GLclampf clear_red_;
  GLclampf clear_green_;
  GLclampf clear_blue_;
  GLclampf clear_alpha_;
  GLboolean mask_red_;
  GLboolean mask_green_;
  GLboolean mask_blue_;
  GLboolean mask_alpha_;
  GLint clear_stencil_;
  GLuint mask_stencil_front_;
  GLuint mask_stencil_back_;
  GLclampf clear_depth_;
  GLboolean mask_depth_;
  bool enable_scissor_test_;

  // The program in use by glUseProgram
  ProgramManager::ProgramInfo::Ref current_program_;

  // The currently bound framebuffers
  FramebufferManager::FramebufferInfo::Ref bound_read_framebuffer_;
  FramebufferManager::FramebufferInfo::Ref bound_draw_framebuffer_;

  // The currently bound renderbuffer
  RenderbufferManager::RenderbufferInfo::Ref bound_renderbuffer_;

  // The offscreen frame buffer that the client renders to. With EGL, the
  // depth and stencil buffers are separate. With regular GL there is a single
  // packed depth stencil buffer in offscreen_target_depth_render_buffer_.
  // offscreen_target_stencil_render_buffer_ is unused.
  scoped_ptr<FrameBuffer> offscreen_target_frame_buffer_;
  scoped_ptr<Texture> offscreen_target_color_texture_;
  scoped_ptr<RenderBuffer> offscreen_target_color_render_buffer_;
  scoped_ptr<RenderBuffer> offscreen_target_depth_render_buffer_;
  scoped_ptr<RenderBuffer> offscreen_target_stencil_render_buffer_;
  GLenum offscreen_target_color_format_;
  GLenum offscreen_target_depth_format_;
  GLenum offscreen_target_stencil_format_;
  GLsizei offscreen_target_samples_;

  GLuint copy_texture_to_parent_texture_fb_;

  // The copy that is saved when SwapBuffers is called.  It is also
  // used as the destination for multi-sample resolves.
  scoped_ptr<FrameBuffer> offscreen_saved_frame_buffer_;
  scoped_ptr<Texture> offscreen_saved_color_texture_;
  GLenum offscreen_saved_color_format_;

  scoped_ptr<Callback1<gfx::Size>::Type> resize_callback_;
  scoped_ptr<Callback0::Type> swap_buffers_callback_;

  // The format of the back buffer_
  GLenum back_buffer_color_format_;

  // The last error message set.
  std::string last_error_;

  // The current decoder error.
  error::Error current_decoder_error_;

  bool use_shader_translator_;
  scoped_ptr<ShaderTranslator> vertex_translator_;
  scoped_ptr<ShaderTranslator> fragment_translator_;

  // Cached from ContextGroup
  const Validators* validators_;
  FeatureInfo* feature_info_;

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
  decoder_->RestoreCurrentTexture2DBindings();
}

ScopedRenderBufferBinder::ScopedRenderBufferBinder(GLES2DecoderImpl* decoder,
                                                   GLuint id)
    : decoder_(decoder) {
  ScopedGLErrorSuppressor suppressor(decoder_);
  glBindRenderbufferEXT(GL_RENDERBUFFER, id);
}

ScopedRenderBufferBinder::~ScopedRenderBufferBinder() {
  ScopedGLErrorSuppressor suppressor(decoder_);
  decoder_->RestoreCurrentRenderbufferBindings();
}

ScopedFrameBufferBinder::ScopedFrameBufferBinder(GLES2DecoderImpl* decoder,
                                                 GLuint id)
    : decoder_(decoder) {
  ScopedGLErrorSuppressor suppressor(decoder_);
  glBindFramebufferEXT(GL_FRAMEBUFFER, id);
}

ScopedFrameBufferBinder::~ScopedFrameBufferBinder() {
  ScopedGLErrorSuppressor suppressor(decoder_);
  decoder_->RestoreCurrentFramebufferBindings();
}

ScopedResolvedFrameBufferBinder::ScopedResolvedFrameBufferBinder(
    GLES2DecoderImpl* decoder) : decoder_(decoder) {
  resolve_and_bind_ = (decoder_->offscreen_target_frame_buffer_.get() &&
                       decoder_->IsOffscreenBufferMultisampled() &&
                       !decoder_->bound_read_framebuffer_.get());
  if (!resolve_and_bind_)
    return;

  ScopedGLErrorSuppressor suppressor(decoder_);
  glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,
                       decoder_->offscreen_target_frame_buffer_->id());
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,
                       decoder_->offscreen_saved_frame_buffer_->id());
  const int width = decoder_->offscreen_size_.width();
  const int height = decoder_->offscreen_size_.height();
  glDisable(GL_SCISSOR_TEST);
  if (IsAngle()) {
    glBlitFramebufferANGLE(0, 0, width, height, 0, 0, width, height,
                           GL_COLOR_BUFFER_BIT, GL_NEAREST);
  } else {
    glBlitFramebufferEXT(0, 0, width, height, 0, 0, width, height,
                         GL_COLOR_BUFFER_BIT, GL_NEAREST);
  }
  glBindFramebufferEXT(GL_FRAMEBUFFER,
                       decoder_->offscreen_saved_frame_buffer_->id());
}

ScopedResolvedFrameBufferBinder::~ScopedResolvedFrameBufferBinder() {
  if (!resolve_and_bind_)
    return;

  ScopedGLErrorSuppressor suppressor(decoder_);
  decoder_->RestoreCurrentFramebufferBindings();
  if (decoder_->enable_scissor_test_) {
    glEnable(GL_SCISSOR_TEST);
  }
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

bool Texture::AllocateStorage(const gfx::Size& size, GLenum format) {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedTexture2DBinder binder(decoder_, id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexImage2D(GL_TEXTURE_2D,
               0,  // mip level
               format,
               size.width(),
               size.height(),
               0,  // border
               format,
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

void Texture::Invalidate() {
  id_ = 0;
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

bool RenderBuffer::AllocateStorage(const gfx::Size& size, GLenum format,
                                   GLsizei samples) {
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedRenderBufferBinder binder(decoder_, id_);
  if (samples <= 1) {
    glRenderbufferStorageEXT(GL_RENDERBUFFER,
                             format,
                             size.width(),
                             size.height());
  } else {
    if (IsAngle()) {
      glRenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER,
                                            samples,
                                            format,
                                            size.width(),
                                            size.height());
    } else {
      glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER,
                                          samples,
                                          format,
                                          size.width(),
                                          size.height());
    }
  }
  return glGetError() == GL_NO_ERROR;
}

void RenderBuffer::Destroy() {
  if (id_ != 0) {
    ScopedGLErrorSuppressor suppressor(decoder_);
    glDeleteRenderbuffersEXT(1, &id_);
    id_ = 0;
  }
}

void RenderBuffer::Invalidate() {
  id_ = 0;
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

void FrameBuffer::AttachRenderBuffer(GLenum target,
                                     RenderBuffer* render_buffer) {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedFrameBufferBinder binder(decoder_, id_);
  GLuint attach_id = render_buffer ? render_buffer->id() : 0;
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER,
                               target,
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

void FrameBuffer::Invalidate() {
  id_ = 0;
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
    : GLES2Decoder(),
      group_(ContextGroup::Ref(group ? group : new ContextGroup())),
      error_bits_(0),
      util_(0),  // TODO(gman): Set to actual num compress texture formats.
      pack_alignment_(4),
      unpack_alignment_(4),
      attrib_0_buffer_id_(0),
      attrib_0_buffer_matches_value_(true),
      attrib_0_size_(0),
      fixed_attrib_buffer_id_(0),
      fixed_attrib_buffer_size_(0),
      active_texture_unit_(0),
      clear_red_(0),
      clear_green_(0),
      clear_blue_(0),
      clear_alpha_(0),
      mask_red_(true),
      mask_green_(true),
      mask_blue_(true),
      mask_alpha_(true),
      clear_stencil_(0),
      mask_stencil_front_(-1),
      mask_stencil_back_(-1),
      clear_depth_(1.0f),
      mask_depth_(true),
      enable_scissor_test_(false),
      offscreen_target_color_format_(0),
      offscreen_target_depth_format_(0),
      offscreen_target_stencil_format_(0),
      offscreen_target_samples_(0),
      offscreen_saved_color_format_(0),
      back_buffer_color_format_(0),
      current_decoder_error_(error::kNoError),
      use_shader_translator_(true),
      validators_(group_->feature_info()->validators()),
      feature_info_(group_->feature_info()) {
  attrib_0_value_.v[0] = 0.0f;
  attrib_0_value_.v[1] = 0.0f;
  attrib_0_value_.v[2] = 0.0f;
  attrib_0_value_.v[3] = 1.0f;

  // The shader translator is used for WebGL even when running on EGL
  // because additional restrictions are needed (like only enabling
  // GL_OES_standard_derivatives on demand).  It is used for the unit
  // tests because
  // GLES2DecoderWithShaderTest.GetShaderInfoLogValidArgs passes the
  // empty string to CompileShader and this is not a valid shader.
  // TODO(apatrick): fix this test.
  if ((gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2 &&
       !feature_info_->feature_flags().chromium_webglsl) ||
      gfx::GetGLImplementation() == gfx::kGLImplementationMockGL) {
    use_shader_translator_ = false;
  }
}

bool GLES2DecoderImpl::Initialize(gfx::GLContext* context,
                                  const gfx::Size& size,
                                  const char* allowed_extensions,
                                  const std::vector<int32>& attribs,
                                  GLES2Decoder* parent,
                                  uint32 parent_client_texture_id) {
  DCHECK(context);
  DCHECK(!context_.get());

  // Take ownership of the GLContext.
  context_.reset(context);

  // Keep only a weak pointer to the parent so we don't unmap its client
  // frame buffer after it has been destroyed.
  if (parent)
    parent_ = static_cast<GLES2DecoderImpl*>(parent)->AsWeakPtr();

  if (!MakeCurrent()) {
    LOG(ERROR) << "GLES2DecoderImpl::Initialize failed because "
               << "MakeCurrent failed.";
    Destroy();
    return false;
  }

  if (!group_->Initialize(allowed_extensions)) {
    LOG(ERROR) << "GPUProcessor::InitializeCommon failed because group "
               << "failed to initialize.";
    Destroy();
    return false;
  }

  CHECK_GL_ERROR();

  vertex_attrib_manager_.Initialize(group_->max_vertex_attribs());

  GLint v = 0;
  glGetIntegerv(GL_ALPHA_BITS, &v);
  back_buffer_color_format_ = v ? GL_RGBA : GL_RGB;

  // We have to enable vertex array 0 on OpenGL or it won't render. Note that
  // OpenGL ES 2.0 does not have this issue.
  glEnableVertexAttribArray(0);
  glGenBuffersARB(1, &attrib_0_buffer_id_);
  glBindBuffer(GL_ARRAY_BUFFER, attrib_0_buffer_id_);
  glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, NULL);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glGenBuffersARB(1, &fixed_attrib_buffer_id_);

  texture_units_.reset(
      new TextureUnit[group_->max_texture_units()]);
  for (uint32 tt = 0; tt < group_->max_texture_units(); ++tt) {
    glActiveTexture(GL_TEXTURE0 + tt);
    // Do cube map first because we want the last bind to be 2D.
    TextureManager::TextureInfo* info =
        texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_CUBE_MAP);
    texture_units_[tt].bound_texture_cube_map = info;
    glBindTexture(GL_TEXTURE_CUBE_MAP, info->service_id());
    info = texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_2D);
    texture_units_[tt].bound_texture_2d = info;
    glBindTexture(GL_TEXTURE_2D, info->service_id());
  }
  glActiveTexture(GL_TEXTURE0);
  CHECK_GL_ERROR();

  if (context_->IsOffscreen()) {
    ContextCreationAttribParser attrib_parser;
    if (!attrib_parser.Parse(attribs))
      return false;

    if (attrib_parser.samples_ > 0 && attrib_parser.sample_buffers_ > 0 &&
        (context_->HasExtension("GL_EXT_framebuffer_multisample") ||
         context_->HasExtension("GL_ANGLE_framebuffer_multisample"))) {
      // Per ext_framebuffer_multisample spec, need max bound on sample count.
      // max_sample_count must be initialized to a sane value.  If
      // glGetIntegerv() throws a GL error, it leaves its argument unchanged.
      GLint max_sample_count = 1;
      glGetIntegerv(GL_MAX_SAMPLES_EXT, &max_sample_count);
      offscreen_target_samples_ = std::min(attrib_parser.samples_,
                                           max_sample_count);
    } else {
      offscreen_target_samples_ = 1;
    }

    if (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2) {
      const bool rgb8_supported =
          context_->HasExtension("GL_OES_rgb8_rgba8");
      // The only available default render buffer formats in GLES2 have very
      // little precision.  Don't enable multisampling unless 8-bit render
      // buffer formats are available--instead fall back to 8-bit textures.
      if (rgb8_supported && offscreen_target_samples_ > 1) {
        offscreen_target_color_format_ = attrib_parser.alpha_size_ > 0 ?
            GL_RGBA8 : GL_RGB8;
      } else {
        offscreen_target_samples_ = 1;
        offscreen_target_color_format_ = attrib_parser.alpha_size_ > 0 ?
            GL_RGBA : GL_RGB;
      }

      // ANGLE only supports packed depth/stencil formats, so use it if it is
      // available.
      const bool depth24_stencil8_supported =
          context_->HasExtension("GL_OES_packed_depth_stencil");
      VLOG(1) << "GL_OES_packed_depth_stencil "
              << (depth24_stencil8_supported ? "" : "not ") << "supported.";
      if ((attrib_parser.depth_size_ > 0 || attrib_parser.stencil_size_ > 0) &&
          depth24_stencil8_supported) {
        offscreen_target_depth_format_ = GL_DEPTH24_STENCIL8;
        offscreen_target_stencil_format_ = 0;
      } else {
        // It may be the case that this depth/stencil combination is not
        // supported, but this will be checked later by CheckFramebufferStatus.
        offscreen_target_depth_format_ = attrib_parser.depth_size_ > 0 ?
            GL_DEPTH_COMPONENT16 : 0;
        offscreen_target_stencil_format_ = attrib_parser.stencil_size_ > 0 ?
            GL_STENCIL_INDEX8 : 0;
      }
    } else {
      offscreen_target_color_format_ = attrib_parser.alpha_size_ > 0 ?
          GL_RGBA : GL_RGB;

      // If depth is requested at all, use the packed depth stencil format if
      // it's available, as some desktop GL drivers don't support any non-packed
      // formats for depth attachments.
      const bool depth24_stencil8_supported =
          context_->HasExtension("GL_EXT_packed_depth_stencil");
      VLOG(1) << "GL_EXT_packed_depth_stencil "
              << (depth24_stencil8_supported ? "" : "not ") << "supported.";

      if ((attrib_parser.depth_size_ > 0 || attrib_parser.stencil_size_ > 0) &&
          depth24_stencil8_supported) {
        offscreen_target_depth_format_ = GL_DEPTH24_STENCIL8;
        offscreen_target_stencil_format_ = 0;
      } else {
        offscreen_target_depth_format_ = attrib_parser.depth_size_ > 0 ?
            GL_DEPTH_COMPONENT : 0;
        offscreen_target_stencil_format_ = attrib_parser.stencil_size_ > 0 ?
            GL_STENCIL_INDEX : 0;
      }
    }

    offscreen_saved_color_format_ = attrib_parser.alpha_size_ > 0 ?
        GL_RGBA : GL_RGB;

    // Create the target frame buffer. This is the one that the client renders
    // directly to.
    offscreen_target_frame_buffer_.reset(new FrameBuffer(this));
    offscreen_target_frame_buffer_->Create();
    // Due to GLES2 format limitations, either the color texture (for
    // non-multisampling) or the color render buffer (for multisampling) will be
    // attached to the offscreen frame buffer.  The render buffer has more
    // limited formats available to it, but the texture can't do multisampling.
    if (IsOffscreenBufferMultisampled()) {
      offscreen_target_color_render_buffer_.reset(new RenderBuffer(this));
      offscreen_target_color_render_buffer_->Create();
    } else {
      offscreen_target_color_texture_.reset(new Texture(this));
      offscreen_target_color_texture_->Create();
    }
    offscreen_target_depth_render_buffer_.reset(new RenderBuffer(this));
    offscreen_target_depth_render_buffer_->Create();
    offscreen_target_stencil_render_buffer_.reset(new RenderBuffer(this));
    offscreen_target_stencil_render_buffer_->Create();

    // Create the saved offscreen texture. The target frame buffer is copied
    // here when SwapBuffers is called.
    offscreen_saved_frame_buffer_.reset(new FrameBuffer(this));
    offscreen_saved_frame_buffer_->Create();
    //
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
      LOG(ERROR) << "Could not allocate offscreen buffer storage.";
      Destroy();
      return false;
    }

    // Bind to the new default frame buffer (the offscreen target frame buffer).
    // This should now be associated with ID zero.
    DoBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenFramebuffersEXT(1, &copy_texture_to_parent_texture_fb_);
  }

  // OpenGL ES 2.0 implicitly enables the desktop GL capability
  // VERTEX_PROGRAM_POINT_SIZE and doesn't expose this enum. This fact
  // isn't well documented; it was discovered in the Khronos OpenGL ES
  // mailing list archives. It also implicitly enables the desktop GL
  // capability GL_POINT_SPRITE to provide access to the gl_PointCoord
  // variable in fragment shaders.
  if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2) {
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);
  }

  if (!InitializeShaderTranslator()) {
    return false;
  }

  return true;
}

bool GLES2DecoderImpl::InitializeShaderTranslator() {
  // Re-check the state of use_shader_translator_ each time this is called.
  if (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2 &&
      feature_info_->feature_flags().chromium_webglsl &&
      !use_shader_translator_) {
    use_shader_translator_ = true;
  }
  if (!use_shader_translator_) {
    return true;
  }
  ShBuiltInResources resources;
  ShInitBuiltInResources(&resources);
  resources.MaxVertexAttribs = group_->max_vertex_attribs();
  resources.MaxVertexUniformVectors =
      group_->max_vertex_uniform_vectors();
  resources.MaxVaryingVectors = group_->max_varying_vectors();
  resources.MaxVertexTextureImageUnits =
      group_->max_vertex_texture_image_units();
  resources.MaxCombinedTextureImageUnits = group_->max_texture_units();
  resources.MaxTextureImageUnits = group_->max_texture_image_units();
  resources.MaxFragmentUniformVectors =
      group_->max_fragment_uniform_vectors();
  resources.MaxDrawBuffers = 1;
  resources.OES_standard_derivatives =
      feature_info_->feature_flags().oes_standard_derivatives ? 1 : 0;
  vertex_translator_.reset(new ShaderTranslator);
  ShShaderSpec shader_spec = feature_info_->feature_flags().chromium_webglsl ?
      SH_WEBGL_SPEC : SH_GLES2_SPEC;
  bool is_glsl_es =
      gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2;
  if (!vertex_translator_->Init(
          SH_VERTEX_SHADER, shader_spec, &resources, is_glsl_es)) {
    LOG(ERROR) << "Could not initialize vertex shader translator.";
    Destroy();
    return false;
  }
  fragment_translator_.reset(new ShaderTranslator);
  if (!fragment_translator_->Init(
          SH_FRAGMENT_SHADER, shader_spec, &resources, is_glsl_es)) {
    LOG(ERROR) << "Could not initialize fragment shader translator.";
    Destroy();
    return false;
  }
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
  return context_.get() ? context_->MakeCurrent() : false;
}

void GLES2DecoderImpl::RestoreCurrentRenderbufferBindings() {
  glBindRenderbufferEXT(
      GL_RENDERBUFFER,
      bound_renderbuffer_ ? bound_renderbuffer_->service_id() : 0);
}

static void RebindCurrentFramebuffer(
    GLenum target,
    FramebufferManager::FramebufferInfo* info,
    FrameBuffer* offscreen_frame_buffer) {
  GLuint framebuffer_id = info ? info->service_id() : 0;
  if (framebuffer_id == 0 && offscreen_frame_buffer) {
    framebuffer_id = offscreen_frame_buffer->id();
  }
  glBindFramebufferEXT(target, framebuffer_id);
}

void GLES2DecoderImpl::RestoreCurrentFramebufferBindings() {
  if (!feature_info_->feature_flags().chromium_framebuffer_multisample) {
    RebindCurrentFramebuffer(
        GL_FRAMEBUFFER,
        bound_draw_framebuffer_.get(),
        offscreen_target_frame_buffer_.get());
  } else {
    RebindCurrentFramebuffer(
        GL_READ_FRAMEBUFFER_EXT,
        bound_read_framebuffer_.get(),
        offscreen_target_frame_buffer_.get());
    RebindCurrentFramebuffer(
        GL_DRAW_FRAMEBUFFER_EXT,
        bound_draw_framebuffer_.get(),
        offscreen_target_frame_buffer_.get());
  }
}

void GLES2DecoderImpl::RestoreCurrentTexture2DBindings() {
  GLES2DecoderImpl::TextureUnit& info = texture_units_[0];
  GLuint last_id;
  if (info.bound_texture_2d) {
    last_id = info.bound_texture_2d->service_id();
  } else {
    last_id = 0;
  }

  glBindTexture(GL_TEXTURE_2D, last_id);
  glActiveTexture(GL_TEXTURE0 + active_texture_unit_);
}

gfx::Size GLES2DecoderImpl::GetBoundReadFrameBufferSize() {
  if (bound_read_framebuffer_ != 0) {
    const FramebufferManager::FramebufferInfo::Attachment* attachment =
        bound_read_framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
    if (attachment) {
      return gfx::Size(attachment->width(), attachment->height());
    }
    return gfx::Size(0, 0);
  } else if (offscreen_target_frame_buffer_.get()) {
    return offscreen_size_;
  } else {
    return context_->GetSize();
  }
}

GLenum GLES2DecoderImpl::GetBoundReadFrameBufferInternalFormat() {
  if (bound_read_framebuffer_ != 0) {
    const FramebufferManager::FramebufferInfo::Attachment* attachment =
        bound_read_framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
    if (attachment) {
      return attachment->internal_format();
    }
    return 0;
  } else if (offscreen_target_frame_buffer_.get()) {
    return offscreen_target_color_format_;
  } else {
    return back_buffer_color_format_;
  }
}

bool GLES2DecoderImpl::UpdateOffscreenFrameBufferSize() {
  if (offscreen_size_ == pending_offscreen_size_)
    return true;

  offscreen_size_ = pending_offscreen_size_;

  // Reallocate the offscreen target buffers.
  DCHECK(offscreen_target_color_format_);
  if (IsOffscreenBufferMultisampled()) {
    if (!offscreen_target_color_render_buffer_->AllocateStorage(
        pending_offscreen_size_, offscreen_target_color_format_,
        offscreen_target_samples_)) {
      LOG(ERROR) << "GLES2DecoderImpl::UpdateOffscreenFrameBufferSize failed "
                 << "to allocate storage for offscreen target color buffer.";
      return false;
    }
  } else {
    if (!offscreen_target_color_texture_->AllocateStorage(
        pending_offscreen_size_, offscreen_target_color_format_)) {
      LOG(ERROR) << "GLES2DecoderImpl::UpdateOffscreenFrameBufferSize failed "
                 << "to allocate storage for offscreen target color texture.";
      return false;
    }
  }
  if (offscreen_target_depth_format_ &&
      !offscreen_target_depth_render_buffer_->AllocateStorage(
      pending_offscreen_size_, offscreen_target_depth_format_,
      offscreen_target_samples_)) {
    LOG(ERROR) << "GLES2DecoderImpl::UpdateOffscreenFrameBufferSize failed "
               << "to allocate storage for offscreen target depth buffer.";
    return false;
  }
  if (offscreen_target_stencil_format_ &&
      !offscreen_target_stencil_render_buffer_->AllocateStorage(
      pending_offscreen_size_, offscreen_target_stencil_format_,
      offscreen_target_samples_)) {
    LOG(ERROR) << "GLES2DecoderImpl::UpdateOffscreenFrameBufferSize failed "
               << "to allocate storage for offscreen target stencil buffer.";
    return false;
  }

  // Attach the offscreen target buffers to the target frame buffer.
  if (IsOffscreenBufferMultisampled()) {
    offscreen_target_frame_buffer_->AttachRenderBuffer(
        GL_COLOR_ATTACHMENT0,
        offscreen_target_color_render_buffer_.get());
  } else {
    offscreen_target_frame_buffer_->AttachRenderTexture(
        offscreen_target_color_texture_.get());
  }
  if (offscreen_target_depth_format_) {
    offscreen_target_frame_buffer_->AttachRenderBuffer(
        GL_DEPTH_ATTACHMENT,
        offscreen_target_depth_render_buffer_.get());
  }
  const bool packed_depth_stencil =
      offscreen_target_depth_format_ == GL_DEPTH24_STENCIL8;
  if (packed_depth_stencil) {
    offscreen_target_frame_buffer_->AttachRenderBuffer(
        GL_STENCIL_ATTACHMENT,
        offscreen_target_depth_render_buffer_.get());
  } else if (offscreen_target_stencil_format_) {
    offscreen_target_frame_buffer_->AttachRenderBuffer(
        GL_STENCIL_ATTACHMENT,
        offscreen_target_stencil_render_buffer_.get());
  }

  if (offscreen_target_frame_buffer_->CheckStatus() !=
      GL_FRAMEBUFFER_COMPLETE) {
      LOG(ERROR) << "GLES2DecoderImpl::UpdateOffscreenFrameBufferSize failed "
                 << "because offscreen FBO was incomplete.";
    return false;
  }

  // Clear the target frame buffer.
  {
    ScopedFrameBufferBinder binder(this, offscreen_target_frame_buffer_->id());
    glClearColor(0, 0, 0, 0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearStencil(0);
    glStencilMaskSeparate(GL_FRONT, GL_TRUE);
    glStencilMaskSeparate(GL_BACK, GL_TRUE);
    glClearDepth(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    RestoreClearState();
  }

  if (parent_ || IsOffscreenBufferMultisampled()) {
    DCHECK(offscreen_saved_color_format_);
    offscreen_saved_color_texture_->AllocateStorage(
        pending_offscreen_size_, offscreen_saved_color_format_);

    offscreen_saved_frame_buffer_->AttachRenderTexture(
        offscreen_saved_color_texture_.get());
    if (offscreen_saved_frame_buffer_->CheckStatus() !=
        GL_FRAMEBUFFER_COMPLETE) {
      LOG(ERROR) << "GLES2DecoderImpl::UpdateOffscreenFrameBufferSize failed "
                 << "because offscreen saved FBO was incomplete.";
      return false;
    }
  }

  if (parent_) {
    // Update the info about the offscreen saved color texture in the parent.
    // The reference to the parent is a weak pointer and will become null if the
    // parent is later destroyed.
    GLuint service_id = offscreen_saved_color_texture_->id();
    GLuint client_id;
    TextureManager* parent_texture_manager = parent_->texture_manager();
    CHECK(parent_texture_manager->GetClientId(service_id, &client_id));
    TextureManager::TextureInfo* info = parent_->GetTextureInfo(client_id);
    DCHECK(info);

    parent_texture_manager->SetLevelInfo(
        feature_info_,
        info,
        GL_TEXTURE_2D,
        0,  // level
        GL_RGBA,
        pending_offscreen_size_.width(),
        pending_offscreen_size_.height(),
        1,  // depth
        0,  // border
        GL_RGBA,
        GL_UNSIGNED_BYTE);
    parent_texture_manager->SetParameter(
        feature_info_,
        info,
        GL_TEXTURE_MAG_FILTER,
        GL_NEAREST);
    parent_texture_manager->SetParameter(
        feature_info_,
        info,
        GL_TEXTURE_MIN_FILTER,
        GL_NEAREST);
    parent_texture_manager->SetParameter(
        feature_info_,
        info,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE);
    parent_texture_manager->SetParameter(
        feature_info_,
        info,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE);

    // Clear the offscreen color texture.
    {
      ScopedFrameBufferBinder binder(this, offscreen_saved_frame_buffer_->id());
      glClearColor(0, 0, 0, 0);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glDisable(GL_SCISSOR_TEST);
      glClear(GL_COLOR_BUFFER_BIT);
      RestoreClearState();
    }
  }

  return true;
}

void GLES2DecoderImpl::SetResizeCallback(Callback1<gfx::Size>::Type* callback) {
  resize_callback_.reset(callback);
}

void GLES2DecoderImpl::SetSwapBuffersCallback(Callback0::Type* callback) {
  swap_buffers_callback_.reset(callback);
}

bool GLES2DecoderImpl::GetServiceTextureId(uint32 client_texture_id,
                                           uint32* service_texture_id) {
  TextureManager::TextureInfo* texture =
      texture_manager()->GetTextureInfo(client_texture_id);
  if (texture) {
    *service_texture_id = texture->service_id();
    return true;
  }
  return false;
}

void GLES2DecoderImpl::Destroy() {
  bool have_context = context_.get() && MakeCurrent();

  if (group_.get())
    group_->set_have_context(have_context);

  if (have_context) {
    if (current_program_) {
      program_manager()->UnuseProgram(shader_manager(), current_program_);
      current_program_ = NULL;
    }

    if (attrib_0_buffer_id_) {
      glDeleteBuffersARB(1, &attrib_0_buffer_id_);
    }
    if (fixed_attrib_buffer_id_) {
      glDeleteBuffersARB(1, &fixed_attrib_buffer_id_);
    }

    // Remove the saved frame buffer mapping from the parent decoder. The
    // parent pointer is a weak pointer so it will be null if the parent has
    // already been destroyed.
    if (parent_) {
      // First check the texture has been mapped into the parent. This might not
      // be the case if initialization failed midway through.
      GLuint service_id = offscreen_saved_color_texture_->id();
      GLuint client_id = 0;
      if (parent_->texture_manager()->GetClientId(service_id, &client_id)) {
        parent_->texture_manager()->RemoveTextureInfo(feature_info_, client_id);
      }

      glDeleteFramebuffersEXT(1, &copy_texture_to_parent_texture_fb_);
    }

    if (offscreen_target_frame_buffer_.get())
      offscreen_target_frame_buffer_->Destroy();
    if (offscreen_target_color_texture_.get())
      offscreen_target_color_texture_->Destroy();
    if (offscreen_target_color_render_buffer_.get())
      offscreen_target_color_render_buffer_->Destroy();
    if (offscreen_target_depth_render_buffer_.get())
      offscreen_target_depth_render_buffer_->Destroy();
    if (offscreen_target_stencil_render_buffer_.get())
      offscreen_target_stencil_render_buffer_->Destroy();
    if (offscreen_saved_frame_buffer_.get())
      offscreen_saved_frame_buffer_->Destroy();
    if (offscreen_saved_color_texture_.get())
      offscreen_saved_color_texture_->Destroy();

    // must release the ContextGroup before destroying the context as its
    // destructor uses GL.
    group_ = NULL;

    context_->Destroy();
    context_.reset();
  } else {
    if (offscreen_target_frame_buffer_.get())
      offscreen_target_frame_buffer_->Invalidate();
    if (offscreen_target_color_texture_.get())
      offscreen_target_color_texture_->Invalidate();
    if (offscreen_target_color_render_buffer_.get())
      offscreen_target_color_render_buffer_->Invalidate();
    if (offscreen_target_depth_render_buffer_.get())
      offscreen_target_depth_render_buffer_->Invalidate();
    if (offscreen_target_stencil_render_buffer_.get())
      offscreen_target_stencil_render_buffer_->Invalidate();
    if (offscreen_saved_frame_buffer_.get())
      offscreen_saved_frame_buffer_->Invalidate();
    if (offscreen_saved_color_texture_.get())
      offscreen_saved_color_texture_->Invalidate();
  }

  offscreen_target_frame_buffer_.reset();
  offscreen_target_color_texture_.reset();
  offscreen_target_color_render_buffer_.reset();
  offscreen_target_depth_render_buffer_.reset();
  offscreen_target_stencil_render_buffer_.reset();
  offscreen_saved_frame_buffer_.reset();
  offscreen_saved_color_texture_.reset();
}

void GLES2DecoderImpl::ResizeOffscreenFrameBuffer(const gfx::Size& size) {
  // We can't resize the render buffers immediately because there might be a
  // partial frame rendered into them and we don't want the tail end of that
  // rendered into the reallocated storage. Defer until the next SwapBuffers.
  pending_offscreen_size_ = size;
}

void GLES2DecoderImpl::DoCopyTextureToParentTextureCHROMIUM(
    GLuint client_texture_id,
    GLuint parent_client_texture_id) {
  if (parent_) {
    TextureManager::TextureInfo* texture = texture_manager()->GetTextureInfo(
        client_texture_id);
    TextureManager::TextureInfo* parent_texture =
        parent_->texture_manager()->GetTextureInfo(parent_client_texture_id);
    if (!texture || !parent_texture) {
      current_decoder_error_ = error::kInvalidArguments;
      return;
    }
    ScopedFrameBufferBinder fb_binder(this, copy_texture_to_parent_texture_fb_);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D,
                              texture->service_id(),
                              0);
    ScopedTexture2DBinder tex_binder(this, parent_texture->service_id());
    GLsizei width, height;
    parent_texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height);
    glCopyTexImage2D(GL_TEXTURE_2D,
                     0,  // level
                     GL_RGBA,
                     0, 0,  // x, y
                     width,
                     height,
                     0);  // border
  }
}

void GLES2DecoderImpl::DoResizeCHROMIUM(GLuint width, GLuint height) {
#if defined(OS_LINUX)
  // Make sure that we are done drawing to the back buffer before resizing.
  glFinish();
#endif
  if (resize_callback_.get()) {
    gfx::Size size(width, height);
    resize_callback_->Run(size);
  }
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
    DVLOG(1) << "cmd: " << GetCommandName(command);
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
          DVLOG(1) << "GL ERROR: " << error << " : " << GetCommandName(command);
        }
      }
    } else {
      result = error::kInvalidArguments;
    }
  } else {
    result = DoCommonCommand(command, arg_count, cmd_data);
  }
  if (result == error::kNoError && current_decoder_error_ != error::kNoError) {
      result = current_decoder_error_;
      current_decoder_error_ = error::kNoError;
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
      IdAllocator* id_allocator =
          group_->GetIdAllocator(id_namespaces::kBuffers);
      id_allocator->MarkAsUsed(client_id);
    }
  }
  if (info) {
    if (!buffer_manager()->SetTarget(info, target)) {
      SetGLError(GL_INVALID_OPERATION,
                 "glBindBuffer: buffer bound to more than 1 target");
      return;
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
      IdAllocator* id_allocator =
          group_->GetIdAllocator(id_namespaces::kFramebuffers);
      id_allocator->MarkAsUsed(client_id);
    } else {
      service_id = info->service_id();
    }
    info->MarkAsValid();
  } else {
    service_id = context_->GetBackingFrameBufferObject();
  }

  if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER_EXT) {
    bound_draw_framebuffer_ = info;
  }
  if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER_EXT) {
    bound_read_framebuffer_ = info;
  }

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
      info = GetRenderbufferInfo(client_id);
      IdAllocator* id_allocator =
          group_->GetIdAllocator(id_namespaces::kRenderbuffers);
      id_allocator->MarkAsUsed(client_id);
    } else {
      service_id = info->service_id();
    }
    info->MarkAsValid();
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
      IdAllocator* id_allocator =
          group_->GetIdAllocator(id_namespaces::kTextures);
      id_allocator->MarkAsUsed(client_id);
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
  if (vertex_attrib_manager_.Enable(index, false)) {
    if (index != 0) {
      glDisableVertexAttribArray(index);
    }
  } else {
    SetGLError(GL_INVALID_VALUE,
               "glDisableVertexAttribArray: index out of range");
  }
}

void GLES2DecoderImpl::DoEnableVertexAttribArray(GLuint index) {
  if (vertex_attrib_manager_.Enable(index, true)) {
    glEnableVertexAttribArray(index);
  } else {
    SetGLError(GL_INVALID_VALUE,
               "glEnableVertexAttribArray: index out of range");
  }
}

void GLES2DecoderImpl::DoGenerateMipmap(GLenum target) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info || !texture_manager()->MarkMipmapsGenerated(feature_info_, info)) {
    SetGLError(GL_INVALID_OPERATION,
               "glGenerateMipmaps: Can not generate mips for npot textures");
    return;
  }
  glGenerateMipmapEXT(target);
}

bool GLES2DecoderImpl::GetHelper(
    GLenum pname, GLint* params, GLsizei* num_written) {
  DCHECK(num_written);
  if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2) {
    switch (pname) {
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      *num_written = 1;
      if (params) {
        *params = GL_RGBA;  // We don't support other formats.
      }
      return true;
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      *num_written = 1;
      if (params) {
        *params = GL_UNSIGNED_BYTE;  // We don't support other types.
      }
      return true;
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      *num_written = 1;
      if (params) {
        *params = group_->max_fragment_uniform_vectors();
      }
      return true;
    case GL_MAX_VARYING_VECTORS:
      *num_written = 1;
      if (params) {
        *params = group_->max_varying_vectors();
      }
      return true;
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
      *num_written = 1;
      if (params) {
        *params = group_->max_vertex_uniform_vectors();
      }
      return true;
    case GL_MAX_VIEWPORT_DIMS:
      if (offscreen_target_frame_buffer_.get()) {
        *num_written = 2;
        if (params) {
          params[0] = renderbuffer_manager()->max_renderbuffer_size();
          params[1] = renderbuffer_manager()->max_renderbuffer_size();
        }
        return true;
      }
    }
  }
  switch (pname) {
    case GL_COMPRESSED_TEXTURE_FORMATS:
      *num_written = 0;
      // We don't support compressed textures.
      return true;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      *num_written = 1;
      if (params) {
        *params = 0;  // We don't support compressed textures.
      }
      return true;
    case GL_NUM_SHADER_BINARY_FORMATS:
      *num_written = 1;
      if (params) {
        *params = 0;  // We don't support binary shader formats.
      }
      return true;
    case GL_SHADER_BINARY_FORMATS:
      *num_written = 0;
      return true;  // We don't support binary shader format.s
    case GL_SHADER_COMPILER:
      *num_written = 1;
      if (params) {
        *params = GL_TRUE;
      }
      return true;
    case GL_ARRAY_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        if (bound_array_buffer_) {
          GLuint client_id = 0;
          buffer_manager()->GetClientId(bound_array_buffer_->service_id(),
                                        &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        if (bound_element_array_buffer_) {
          GLuint client_id = 0;
          buffer_manager()->GetClientId(
              bound_element_array_buffer_->service_id(),
              &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_FRAMEBUFFER_BINDING:
    // case GL_DRAW_FRAMEBUFFER_BINDING_EXT: (same as GL_FRAMEBUFFER_BINDING)
      *num_written = 1;
      if (params) {
        if (bound_draw_framebuffer_) {
          GLuint client_id = 0;
          framebuffer_manager()->GetClientId(
              bound_draw_framebuffer_->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_READ_FRAMEBUFFER_BINDING:
      *num_written = 1;
      if (params) {
        if (bound_read_framebuffer_) {
          GLuint client_id = 0;
          framebuffer_manager()->GetClientId(
              bound_read_framebuffer_->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_RENDERBUFFER_BINDING:
      *num_written = 1;
      if (params) {
        if (bound_renderbuffer_) {
          GLuint client_id = 0;
          renderbuffer_manager()->GetClientId(
              bound_renderbuffer_->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_CURRENT_PROGRAM:
      *num_written = 1;
      if (params) {
        if (current_program_) {
          GLuint client_id = 0;
          program_manager()->GetClientId(
              current_program_->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_2D:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = texture_units_[active_texture_unit_];
        if (unit.bound_texture_2d) {
          GLuint client_id = 0;
          texture_manager()->GetClientId(
              unit.bound_texture_2d->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_CUBE_MAP:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = texture_units_[active_texture_unit_];
        if (unit.bound_texture_cube_map) {
          GLuint client_id = 0;
          texture_manager()->GetClientId(
              unit.bound_texture_cube_map->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    default:
      *num_written = util_.GLGetNumValuesReturned(pname);
      return false;
  }
}

bool GLES2DecoderImpl::GetNumValuesReturnedForGLGet(
    GLenum pname, GLsizei* num_values) {
  return GetHelper(pname, NULL, num_values);
}

void GLES2DecoderImpl::DoGetBooleanv(GLenum pname, GLboolean* params) {
  DCHECK(params);
  GLsizei num_written = 0;
  if (GetHelper(pname, NULL, &num_written)) {
    scoped_array<GLint> values(new GLint[num_written]);
    GetHelper(pname, values.get(), &num_written);
    for (GLsizei ii = 0; ii < num_written; ++ii) {
      params[ii] = static_cast<GLboolean>(values[ii]);
    }
  } else {
    glGetBooleanv(pname, params);
  }
}

void GLES2DecoderImpl::DoGetFloatv(GLenum pname, GLfloat* params) {
  DCHECK(params);
  GLsizei num_written = 0;
  if (GetHelper(pname, NULL, &num_written)) {
    scoped_array<GLint> values(new GLint[num_written]);
    GetHelper(pname, values.get(), &num_written);
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
  ProgramManager::ProgramInfo* info = GetProgramInfoNotShader(
      program_id, "glGetProgramiv");
  if (!info) {
    return;
  }
  info->GetProgramiv(pname, params);
}

error::Error GLES2DecoderImpl::HandleBindAttribLocation(
    uint32 immediate_data_size, const gles2::BindAttribLocation& c) {
  GLuint program = static_cast<GLuint>(c.program);
  ProgramManager::ProgramInfo* info = GetProgramInfoNotShader(
      program, "glBindAttribLocation");
  if (!info) {
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
  GLuint program = static_cast<GLuint>(c.program);
  ProgramManager::ProgramInfo* info = GetProgramInfoNotShader(
      program, "glBindAttribLocation");
  if (!info) {
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
  GLuint program = static_cast<GLuint>(c.program);
  ProgramManager::ProgramInfo* info = GetProgramInfoNotShader(
      program, "glBindAttribLocation");
  if (!info) {
    return error::kNoError;
  }
  GLuint index = static_cast<GLuint>(c.index);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  glBindAttribLocation(info->service_id(), index, name_str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteShader(
    uint32 immediate_data_size, const gles2::DeleteShader& c) {
  GLuint client_id = c.shader;
  if (client_id) {
    ShaderManager::ShaderInfo* info = GetShaderInfo(client_id);
    if (info) {
      if (!info->IsDeleted()) {
        glDeleteShader(info->service_id());
        shader_manager()->MarkAsDeleted(info);
      }
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
      if (!info->IsDeleted()) {
        glDeleteProgram(info->service_id());
        program_manager()->MarkAsDeleted(shader_manager(), info);
      }
    } else {
      SetGLError(GL_INVALID_VALUE, "glDeleteProgram: unknown program");
    }
  }
  return error::kNoError;
}

void GLES2DecoderImpl::DoDeleteSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids) {
  IdAllocator* id_allocator = group_->GetIdAllocator(namespace_id);
  for (GLsizei ii = 0; ii < n; ++ii) {
    id_allocator->FreeID(ids[ii]);
  }
}

error::Error GLES2DecoderImpl::HandleDeleteSharedIdsCHROMIUM(
    uint32 immediate_data_size, const gles2::DeleteSharedIdsCHROMIUM& c) {
  GLuint namespace_id = static_cast<GLuint>(c.namespace_id);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* ids = GetSharedMemoryAs<const GLuint*>(
      c.ids_shm_id, c.ids_shm_offset, data_size);
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "DeleteSharedIdsCHROMIUM: n < 0");
    return error::kNoError;
  }
  if (ids == NULL) {
    return error::kOutOfBounds;
  }
  DoDeleteSharedIdsCHROMIUM(namespace_id, n, ids);
  return error::kNoError;
}

void GLES2DecoderImpl::DoGenSharedIdsCHROMIUM(
    GLuint namespace_id, GLuint id_offset, GLsizei n, GLuint* ids) {
  IdAllocator* id_allocator = group_->GetIdAllocator(namespace_id);
  if (id_offset == 0) {
    for (GLsizei ii = 0; ii < n; ++ii) {
      ids[ii] = id_allocator->AllocateID();
    }
  } else {
    for (GLsizei ii = 0; ii < n; ++ii) {
      ids[ii] = id_allocator->AllocateIDAtOrAbove(id_offset);
      id_offset = ids[ii] + 1;
    }
  }
}

error::Error GLES2DecoderImpl::HandleGenSharedIdsCHROMIUM(
    uint32 immediate_data_size, const gles2::GenSharedIdsCHROMIUM& c) {
  GLuint namespace_id = static_cast<GLuint>(c.namespace_id);
  GLuint id_offset = static_cast<GLuint>(c.id_offset);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* ids = GetSharedMemoryAs<GLuint*>(
      c.ids_shm_id, c.ids_shm_offset, data_size);
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "GenSharedIdsCHROMIUM: n < 0");
    return error::kNoError;
  }
  if (ids == NULL) {
    return error::kOutOfBounds;
  }
  DoGenSharedIdsCHROMIUM(namespace_id, id_offset, n, ids);
  return error::kNoError;
}

void GLES2DecoderImpl::DoRegisterSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids) {
  IdAllocator* id_allocator = group_->GetIdAllocator(namespace_id);
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (!id_allocator->MarkAsUsed(ids[ii])) {
      for (GLsizei jj = 0; jj < ii; ++jj) {
        id_allocator->FreeID(ids[jj]);
      }
      SetGLError(
          GL_INVALID_VALUE,
          "RegisterSharedIdsCHROMIUM: attempt to register "
          "id that already exists");
      return;
    }
  }
}

error::Error GLES2DecoderImpl::HandleRegisterSharedIdsCHROMIUM(
    uint32 immediate_data_size, const gles2::RegisterSharedIdsCHROMIUM& c) {
  GLuint namespace_id = static_cast<GLuint>(c.namespace_id);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* ids = GetSharedMemoryAs<GLuint*>(
      c.ids_shm_id, c.ids_shm_offset, data_size);
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "RegisterSharedIdsCHROMIUM: n < 0");
    return error::kNoError;
  }
  if (ids == NULL) {
    return error::kOutOfBounds;
  }
  DoRegisterSharedIdsCHROMIUM(namespace_id, n, ids);
  return error::kNoError;
}

void GLES2DecoderImpl::DoDrawArrays(
    GLenum mode, GLint first, GLsizei count) {
  // We have to check this here because the prototype for glDrawArrays
  // is GLint not GLsizei.
  if (first < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawArrays: first < 0");
    return;
  }

  if (count == 0) {
    return;
  }

  GLuint max_vertex_accessed = first + count - 1;
  if (IsDrawValid(max_vertex_accessed)) {
    bool simulated_attrib_0 = SimulateAttrib0(max_vertex_accessed);
    bool simulated_fixed_attribs = false;
    if (SimulateFixedAttribs(max_vertex_accessed, &simulated_fixed_attribs)) {
      bool textures_set = SetBlackTextureForNonRenderableTextures();
      glDrawArrays(mode, first, count);
      if (textures_set) {
        RestoreStateForNonRenderableTextures();
      }
      if (simulated_fixed_attribs) {
        RestoreStateForSimulatedFixedAttribs();
      }
    }
    if (simulated_attrib_0) {
      RestoreStateForSimulatedAttrib0();
    }
  }
}

void GLES2DecoderImpl::DoFramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint client_renderbuffer_id) {
  FramebufferManager::FramebufferInfo* framebuffer_info =
      GetFramebufferInfoForTarget(target);
  if (!framebuffer_info) {
    SetGLError(GL_INVALID_OPERATION,
               "glFramebufferRenderbuffer: no framebuffer bound");
    return;
  }
  GLuint service_id = 0;
  RenderbufferManager::RenderbufferInfo* info = NULL;
  if (client_renderbuffer_id) {
    info = GetRenderbufferInfo(client_renderbuffer_id);
    if (!info) {
      SetGLError(GL_INVALID_OPERATION,
                 "glFramebufferRenderbuffer: unknown renderbuffer");
      return;
    }
    service_id = info->service_id();
  }
  CopyRealGLErrorsToWrapper();
  glFramebufferRenderbufferEXT(
      target, attachment, renderbuffertarget, service_id);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    framebuffer_info->AttachRenderbuffer(attachment, info);
    if (service_id == 0 ||
        glCheckFramebufferStatusEXT(target) == GL_FRAMEBUFFER_COMPLETE) {
      if (info) {
        ClearUnclearedRenderbuffers(target, framebuffer_info);
      }
    }
  }
}

void GLES2DecoderImpl::SetCapabilityState(GLenum cap, bool enabled) {
  switch (cap) {
    case GL_SCISSOR_TEST:
      enable_scissor_test_ = enabled;
      break;
    default:
      break;
  }
}

void GLES2DecoderImpl::DoDisable(GLenum cap) {
  SetCapabilityState(cap, false);
  glDisable(cap);
}

void GLES2DecoderImpl::DoEnable(GLenum cap) {
  SetCapabilityState(cap, true);
  glEnable(cap);
}

void GLES2DecoderImpl::DoClearColor(
      GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  clear_red_ = red;
  clear_green_ = green;
  clear_blue_ = blue;
  clear_alpha_ = alpha;
  glClearColor(red, green, blue, alpha);
}

void GLES2DecoderImpl::DoClearDepthf(GLclampf depth) {
  clear_depth_ = depth;
  glClearDepth(depth);
}

void GLES2DecoderImpl::DoClearStencil(GLint s) {
  clear_stencil_ = s;
  glClearStencil(s);
}

void GLES2DecoderImpl::DoColorMask(
    GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
  mask_red_ = red;
  mask_green_ = green;
  mask_blue_ = blue;
  mask_alpha_ = alpha;
  glColorMask(red, green, blue, alpha);
}

void GLES2DecoderImpl::DoDepthMask(GLboolean depth) {
  mask_depth_ = depth;
  glDepthMask(depth);
}

void GLES2DecoderImpl::DoStencilMask(GLuint mask) {
  mask_stencil_front_ = mask;
  mask_stencil_back_ = mask;
  glStencilMask(mask);
}

void GLES2DecoderImpl::DoStencilMaskSeparate(GLenum face, GLuint mask) {
  if (face == GL_FRONT) {
    mask_stencil_front_ = mask;
  } else {
    mask_stencil_back_ = mask;
  }
  glStencilMaskSeparate(face, mask);
}

// NOTE: There's an assumption here that Texture attachments
// are cleared because they are textures so we only need to clear
// the renderbuffers.
void GLES2DecoderImpl::ClearUnclearedRenderbuffers(
    GLenum target, FramebufferManager::FramebufferInfo* info) {
  if (target == GL_READ_FRAMEBUFFER_EXT) {
    // TODO(gman): bind this to the DRAW point, clear then bind back to READ
  }
  GLbitfield clear_bits = 0;
  if (info->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0)) {
    glClearColor(0, 0, 0, 0);
    glColorMask(true, true, true, true);
    clear_bits |= GL_COLOR_BUFFER_BIT;
  }

  if (info->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT) ||
      info->HasUnclearedAttachment(GL_DEPTH_STENCIL_ATTACHMENT)) {
    glClearStencil(0);
    glStencilMask(-1);
    clear_bits |= GL_STENCIL_BUFFER_BIT;
  }

  if (info->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT) ||
      info->HasUnclearedAttachment(GL_DEPTH_STENCIL_ATTACHMENT)) {
    glClearDepth(1.0f);
    glDepthMask(true);
    clear_bits |= GL_DEPTH_BUFFER_BIT;
  }

  glDisable(GL_SCISSOR_TEST);
  glClear(clear_bits);

  info->MarkAttachedRenderbuffersAsCleared();

  RestoreClearState();

  if (target == GL_READ_FRAMEBUFFER_EXT) {
    // TODO(gman): rebind draw.
  }
}

void GLES2DecoderImpl::RestoreClearState() {
  glClearColor(clear_red_, clear_green_, clear_blue_, clear_alpha_);
  glColorMask(mask_red_, mask_green_, mask_blue_, mask_alpha_);
  glClearStencil(clear_stencil_);
  glStencilMaskSeparate(GL_FRONT, mask_stencil_front_);
  glStencilMaskSeparate(GL_BACK, mask_stencil_back_);
  glClearDepth(clear_depth_);
  glDepthMask(mask_depth_);
  if (enable_scissor_test_) {
    glEnable(GL_SCISSOR_TEST);
  }
}

GLenum GLES2DecoderImpl::DoCheckFramebufferStatus(GLenum target) {
  FramebufferManager::FramebufferInfo* info =
      GetFramebufferInfoForTarget(target);
  if (!info) {
    return GL_FRAMEBUFFER_COMPLETE;
  }
  return glCheckFramebufferStatusEXT(target);
}

void GLES2DecoderImpl::DoFramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget,
    GLuint client_texture_id, GLint level) {
  FramebufferManager::FramebufferInfo* framebuffer_info =
      GetFramebufferInfoForTarget(target);
  if (!framebuffer_info) {
    SetGLError(GL_INVALID_OPERATION,
               "glFramebufferTexture2D: no framebuffer bound.");
    return;
  }
  GLuint service_id = 0;
  TextureManager::TextureInfo* info = NULL;
  if (client_texture_id) {
    info = GetTextureInfo(client_texture_id);
    if (!info) {
      SetGLError(GL_INVALID_OPERATION,
                 "glFramebufferTexture2D: unknown texture");
      return;
    }
    service_id = info->service_id();
  }
  CopyRealGLErrorsToWrapper();
  glFramebufferTexture2DEXT(target, attachment, textarget, service_id, level);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    framebuffer_info->AttachTexture(attachment, info, textarget, level);
    if (service_id != 0 &&
        glCheckFramebufferStatusEXT(target) == GL_FRAMEBUFFER_COMPLETE) {
      ClearUnclearedRenderbuffers(target, framebuffer_info);
    }
  }
}

void GLES2DecoderImpl::DoGetFramebufferAttachmentParameteriv(
    GLenum target, GLenum attachment, GLenum pname, GLint* params) {
  FramebufferManager::FramebufferInfo* framebuffer_info =
      GetFramebufferInfoForTarget(target);
  if (!framebuffer_info) {
    SetGLError(GL_INVALID_OPERATION,
               "glFramebufferAttachmentParameteriv: no framebuffer bound");
    return;
  }
  glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, params);
  if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME) {
    GLint type = 0;
    GLuint client_id = 0;
    glGetFramebufferAttachmentParameterivEXT(
        target, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    switch (type) {
      case GL_RENDERBUFFER: {
        renderbuffer_manager()->GetClientId(*params, &client_id);
        break;
      }
      case GL_TEXTURE: {
        texture_manager()->GetClientId(*params, &client_id);
        break;
      }
      default:
        break;
    }
    *params = client_id;
  }
}

void GLES2DecoderImpl::DoGetRenderbufferParameteriv(
    GLenum target, GLenum pname, GLint* params) {
  if (!bound_renderbuffer_) {
    SetGLError(GL_INVALID_OPERATION,
               "glGetRenderbufferParameteriv: no renderbuffer bound");
    return;
  }
  if (pname == GL_RENDERBUFFER_INTERNAL_FORMAT) {
    *params = bound_renderbuffer_->internal_format();
    return;
  }
  glGetRenderbufferParameterivEXT(target, pname, params);
}

void GLES2DecoderImpl::DoBlitFramebufferEXT(
    GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
    GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
    GLbitfield mask, GLenum filter) {
  if (!feature_info_->feature_flags().chromium_framebuffer_multisample) {
    SetGLError(GL_INVALID_OPERATION,
               "glBlitFramebufferEXT: function not available");
  }
  if (IsAngle()) {
    glBlitFramebufferANGLE(
        srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
  } else {
    glBlitFramebufferEXT(
        srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
  }
}

void GLES2DecoderImpl::DoRenderbufferStorageMultisample(
    GLenum target, GLsizei samples, GLenum internalformat,
    GLsizei width, GLsizei height) {
  if (!feature_info_->feature_flags().chromium_framebuffer_multisample) {
    SetGLError(GL_INVALID_OPERATION,
               "glRenderbufferStorageMultisampleEXT: function not available");
    return;
  }

  GLenum impl_format = internalformat;
  if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2) {
    switch (impl_format) {
      case GL_DEPTH_COMPONENT16:
        impl_format = GL_DEPTH_COMPONENT;
        break;
      case GL_RGBA4:
      case GL_RGB5_A1:
        impl_format = GL_RGBA;
        break;
      case GL_RGB565:
        impl_format = GL_RGB;
        break;
    }
  }

  CopyRealGLErrorsToWrapper();
  glRenderbufferStorageMultisampleEXT(
      target, samples, impl_format, width, height);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    bound_renderbuffer_->SetInfo(samples, internalformat, width, height);
  }
}

void GLES2DecoderImpl::DoRenderbufferStorage(
  GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  if (!bound_renderbuffer_) {
    SetGLError(GL_INVALID_OPERATION,
               "glGetRenderbufferStorage: no renderbuffer bound");
    return;
  }

  GLenum impl_format = internalformat;
  if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2) {
    switch (impl_format) {
      case GL_DEPTH_COMPONENT16:
        impl_format = GL_DEPTH_COMPONENT;
        break;
      case GL_RGBA4:
      case GL_RGB5_A1:
        impl_format = GL_RGBA;
        break;
      case GL_RGB565:
        impl_format = GL_RGB;
        break;
    }
  }

  CopyRealGLErrorsToWrapper();
  glRenderbufferStorageEXT(target, impl_format, width, height);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    bound_renderbuffer_->SetInfo(0, internalformat, width, height);
  }
}

void GLES2DecoderImpl::DoLinkProgram(GLuint program) {
  ProgramManager::ProgramInfo* info = GetProgramInfoNotShader(
      program, "glLinkProgram");
  if (!info) {
    return;
  }

  info->ClearLinkStatus();
  if (!info->CanLink()) {
    return;
  }

  glLinkProgram(info->service_id());
  GLint success = 0;
  glGetProgramiv(info->service_id(), GL_LINK_STATUS, &success);
  if (success) {
    info->Update();
  }
};

void GLES2DecoderImpl::DoTexParameterf(
    GLenum target, GLenum pname, GLfloat param) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glTexParameterf: unknown texture");
  } else {
    texture_manager()->SetParameter(
        feature_info_, info, pname, static_cast<GLint>(param));
    glTexParameterf(target, pname, param);
  }
}

void GLES2DecoderImpl::DoTexParameteri(
    GLenum target, GLenum pname, GLint param) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glTexParameteri: unknown texture");
  } else {
    texture_manager()->SetParameter(feature_info_, info, pname, param);
    glTexParameteri(target, pname, param);
  }
}

void GLES2DecoderImpl::DoTexParameterfv(
    GLenum target, GLenum pname, const GLfloat* params) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glTexParameterfv: unknown texture");
  } else {
    texture_manager()->SetParameter(
        feature_info_, info, pname, *reinterpret_cast<const GLint*>(params));
    glTexParameterfv(target, pname, params);
  }
}

void GLES2DecoderImpl::DoTexParameteriv(
  GLenum target, GLenum pname, const GLint* params) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glTexParameteriv: unknown texture");
  } else {
    texture_manager()->SetParameter(feature_info_, info, pname, *params);
    glTexParameteriv(target, pname, params);
  }
}

bool GLES2DecoderImpl::CheckCurrentProgram(const char* function_name) {
  if (!current_program_) {
      // The program does not exist.
      SetGLError(GL_INVALID_OPERATION,
                 (std::string(function_name) + ": no program in use").c_str());
      return false;
  }
  if (!current_program_->InUse()) {
    SetGLError(GL_INVALID_OPERATION,
               (std::string(function_name) + ": program not linked").c_str());
    return false;
  }
  return true;
}

bool GLES2DecoderImpl::CheckCurrentProgramForUniform(
    GLint location, const char* function_name) {
  if (!CheckCurrentProgram(function_name)) {
    return false;
  }
  return location != -1;
}

bool GLES2DecoderImpl::GetUniformTypeByLocation(
    GLint location, const char* function_name, GLenum* type) {
  if (!CheckCurrentProgramForUniform(location, function_name)) {
    return false;
  }
  if (!current_program_->GetUniformTypeByLocation(location, type)) {
    SetGLError(GL_INVALID_OPERATION,
               (std::string(function_name) + ": program not linked").c_str());
    return false;
  }
  return true;
}


void GLES2DecoderImpl::DoUniform1i(GLint location, GLint v0) {
  if (!CheckCurrentProgramForUniform(location, "glUniform1i")) {
    return;
  }
  current_program_->SetSamplers(location, 1, &v0);
  glUniform1i(location, v0);
}

void GLES2DecoderImpl::DoUniform1iv(
    GLint location, GLsizei count, const GLint *value) {
  if (!CheckCurrentProgramForUniform(location, "glUniform1iv")) {
    return;
  }
  current_program_->SetSamplers(location, count, value);
  glUniform1iv(location, count, value);
}

void GLES2DecoderImpl::DoUniform1fv(
    GLint location, GLsizei count, const GLfloat* value) {
  GLenum type;
  if (!GetUniformTypeByLocation(location, "glUniform1fv", &type)) {
    return;
  }
  if (type == GL_BOOL) {
    scoped_array<GLint> temp(new GLint[count]);
    for (GLsizei ii = 0; ii < count; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    DoUniform1iv(location, count, temp.get());
  } else {
    glUniform1fv(location, count, value);
  }
}

void GLES2DecoderImpl::DoUniform2fv(
    GLint location, GLsizei count, const GLfloat* value) {
  GLenum type;
  if (!GetUniformTypeByLocation(location, "glUniform2fv", &type)) {
    return;
  }
  if (type == GL_BOOL_VEC2) {
    GLsizei num_values = count * 2;
    scoped_array<GLint> temp(new GLint[num_values]);
    for (GLsizei ii = 0; ii < num_values; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    glUniform2iv(location, count, temp.get());
  } else {
    glUniform2fv(location, count, value);
  }
}

void GLES2DecoderImpl::DoUniform3fv(
    GLint location, GLsizei count, const GLfloat* value) {
  GLenum type;
  if (!GetUniformTypeByLocation(location, "glUniform3fv", &type)) {
    return;
  }
  if (type == GL_BOOL_VEC3) {
    GLsizei num_values = count * 3;
    scoped_array<GLint> temp(new GLint[num_values]);
    for (GLsizei ii = 0; ii < num_values; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    glUniform3iv(location, count, temp.get());
  } else {
    glUniform3fv(location, count, value);
  }
}

void GLES2DecoderImpl::DoUniform4fv(
    GLint location, GLsizei count, const GLfloat* value) {
  GLenum type;
  if (!GetUniformTypeByLocation(location, "glUniform4fv", &type)) {
    return;
  }
  if (type == GL_BOOL_VEC4) {
    GLsizei num_values = count * 4;
    scoped_array<GLint> temp(new GLint[num_values]);
    for (GLsizei ii = 0; ii < num_values; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    glUniform4iv(location, count, temp.get());
  } else {
    glUniform4fv(location, count, value);
  }
}

void GLES2DecoderImpl::DoUseProgram(GLuint program) {
  GLuint service_id = 0;
  ProgramManager::ProgramInfo* info = NULL;
  if (program) {
    info = GetProgramInfoNotShader(program, "glUseProgram");
    if (!info) {
      return;
    }
    if (!info->IsValid()) {
      // Program was not linked successfully. (ie, glLinkProgram)
      SetGLError(GL_INVALID_OPERATION, "glUseProgram: program not linked");
      return;
    }
    service_id = info->service_id();
  }
  if (current_program_) {
    program_manager()->UnuseProgram(shader_manager(), current_program_);
  }
  current_program_ = info;
  if (current_program_) {
    program_manager()->UseProgram(current_program_);
  }
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
    LOG(ERROR) << last_error_;
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

bool GLES2DecoderImpl::SetBlackTextureForNonRenderableTextures() {
  DCHECK(current_program_);
  // Only check if there are some unrenderable textures.
  if (!texture_manager()->HaveUnrenderableTextures()) {
    return false;
  }
  bool textures_set = false;
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
        if (!texture_info || !texture_info->CanRender(feature_info_)) {
          textures_set = true;
          glActiveTexture(GL_TEXTURE0 + texture_unit_index);
          glBindTexture(
              uniform_info->type == GL_SAMPLER_2D ? GL_TEXTURE_2D :
                                                    GL_TEXTURE_CUBE_MAP,
              texture_manager()->black_texture_id(uniform_info->type));
        }
      }
      // else: should this be an error?
    }
  }
  return textures_set;
}

void GLES2DecoderImpl::RestoreStateForNonRenderableTextures() {
  DCHECK(current_program_);
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
        if (!texture_info || !texture_info->CanRender(feature_info_)) {
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
  // NOTE: We specifically do not check current_program->IsValid() because
  // it could never be invalid since glUseProgram would have failed. While
  // glLinkProgram could later mark the program as invalid the previous
  // valid program will still function if it is still the current program.
  if (!current_program_) {
    // The program does not exist.
    // But GL says no ERROR.
    return false;
  }
  // Validate all attribs currently enabled. If they are used by the current
  // program then check that they have enough elements to handle the draw call.
  // If they are not used by the current program check that they have a buffer
  // assigned.
  const VertexAttribManager::VertexAttribInfoList& infos =
      vertex_attrib_manager_.GetEnabledVertexAttribInfos();
  for (VertexAttribManager::VertexAttribInfoList::const_iterator it =
       infos.begin(); it != infos.end(); ++it) {
    const VertexAttribManager::VertexAttribInfo* info = *it;
    const ProgramManager::ProgramInfo::VertexAttribInfo* attrib_info =
        current_program_->GetAttribInfoByLocation(info->index());
    if (attrib_info) {
      // This attrib is used in the current program.
      if (!info->CanAccess(max_vertex_accessed)) {
        SetGLError(GL_INVALID_OPERATION,
                   "glDrawXXX: attempt to access out of range vertices");
        return false;
      }
    } else {
      // This attrib is not used in the current program.
      if (!info->buffer() || info->buffer()->IsDeleted()) {
        SetGLError(
            GL_INVALID_OPERATION,
            "glDrawXXX: attempt to render with no buffer attached to enabled "
            "attrib");
        return false;
      }
    }
  }
  return true;
}

bool GLES2DecoderImpl::SimulateAttrib0(GLuint max_vertex_accessed) {
  if (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2)
    return false;

  const VertexAttribManager::VertexAttribInfo* info =
      vertex_attrib_manager_.GetVertexAttribInfo(0);
  // If it's enabled or it's not used then we don't need to do anything.
  bool attrib_0_used = current_program_->GetAttribInfoByLocation(0) != NULL;
  if (info->enabled() && attrib_0_used) {
    return false;
  }

  typedef VertexAttribManager::VertexAttribInfo::Vec4 Vec4;

  glBindBuffer(GL_ARRAY_BUFFER, attrib_0_buffer_id_);

  // Make a buffer with a single repeated vec4 value enough to
  // simulate the constant value that is supposed to be here.
  // This is required to emulate GLES2 on GL.
  GLsizei num_vertices = max_vertex_accessed + 1;
  GLsizei size_needed = num_vertices * sizeof(Vec4);  // NOLINT
  if (size_needed > attrib_0_size_) {
    glBufferData(GL_ARRAY_BUFFER, size_needed, NULL, GL_DYNAMIC_DRAW);
    // TODO(gman): check for error here?
    attrib_0_buffer_matches_value_ = false;
  }
  if (attrib_0_used &&
      (!attrib_0_buffer_matches_value_ ||
       (info->value().v[0] != attrib_0_value_.v[0] ||
        info->value().v[1] != attrib_0_value_.v[1] ||
        info->value().v[2] != attrib_0_value_.v[2] ||
        info->value().v[3] != attrib_0_value_.v[3]))) {
    std::vector<Vec4> temp(num_vertices, info->value());
    glBufferSubData(GL_ARRAY_BUFFER, 0, size_needed, &temp[0].v[0]);
    attrib_0_buffer_matches_value_ = true;
    attrib_0_value_ = info->value();
    attrib_0_size_ = size_needed;
  }

  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);

  return true;
}

void GLES2DecoderImpl::RestoreStateForSimulatedAttrib0() {
  const VertexAttribManager::VertexAttribInfo* info =
      vertex_attrib_manager_.GetVertexAttribInfo(0);
  const void* ptr = reinterpret_cast<const void*>(info->offset());
  BufferManager::BufferInfo* buffer_info = info->buffer();
  glBindBuffer(GL_ARRAY_BUFFER, buffer_info ? buffer_info->service_id() : 0);
  glVertexAttribPointer(
      0, info->size(), info->type(), info->normalized(), info->gl_stride(),
      ptr);
  glBindBuffer(GL_ARRAY_BUFFER,
               bound_array_buffer_ ? bound_array_buffer_->service_id() : 0);
}

bool GLES2DecoderImpl::SimulateFixedAttribs(
    GLuint max_vertex_accessed, bool* simulated) {
  DCHECK(simulated);
  *simulated = false;
  if (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2)
    return true;

  if (!vertex_attrib_manager_.HaveFixedAttribs()) {
    return true;
  }

  // NOTE: we could be smart and try to check if a buffer is used
  // twice in 2 different attribs, find the overlapping parts and therefore
  // duplicate the minimum amount of data but this whole code path is not meant
  // to be used normally. It's just here to pass that OpenGL ES 2.0 conformance
  // tests so we just add to the buffer attrib used.

  // Compute the number of elements needed.
  int num_vertices = max_vertex_accessed + 1;
  int elements_needed = 0;
  const VertexAttribManager::VertexAttribInfoList& infos =
      vertex_attrib_manager_.GetEnabledVertexAttribInfos();
  for (VertexAttribManager::VertexAttribInfoList::const_iterator it =
       infos.begin(); it != infos.end(); ++it) {
    const VertexAttribManager::VertexAttribInfo* info = *it;
    const ProgramManager::ProgramInfo::VertexAttribInfo* attrib_info =
        current_program_->GetAttribInfoByLocation(info->index());
    if (attrib_info &&
        info->CanAccess(max_vertex_accessed) &&
        info->type() == GL_FIXED) {
      int elements_used = 0;
      if (!SafeMultiply(
          static_cast<int>(num_vertices),
          info->size(), &elements_used) ||
          !SafeAdd(elements_needed, elements_used, &elements_needed)) {
        SetGLError(GL_OUT_OF_MEMORY, "glDrawXXX: simulating GL_FIXED attribs");
        return false;
      }
    }
  }

  const int kSizeOfFloat = sizeof(float);  // NOLINT
  int size_needed = 0;
  if (!SafeMultiply(elements_needed, kSizeOfFloat, &size_needed)) {
    SetGLError(GL_OUT_OF_MEMORY, "glDrawXXX: simulating GL_FIXED attribs");
    return false;
  }


  glBindBuffer(GL_ARRAY_BUFFER, fixed_attrib_buffer_id_);
  if (size_needed > fixed_attrib_buffer_size_) {
    glBufferData(GL_ARRAY_BUFFER, size_needed, NULL, GL_DYNAMIC_DRAW);
  }

  // Copy the elements and convert to float
  GLintptr offset = 0;
  for (VertexAttribManager::VertexAttribInfoList::const_iterator it =
           infos.begin(); it != infos.end(); ++it) {
    const VertexAttribManager::VertexAttribInfo* info = *it;
    const ProgramManager::ProgramInfo::VertexAttribInfo* attrib_info =
        current_program_->GetAttribInfoByLocation(info->index());
    if (attrib_info &&
        info->CanAccess(max_vertex_accessed) &&
        info->type() == GL_FIXED) {
      int num_elements = info->size() * kSizeOfFloat;
      int size = num_elements * num_vertices;
      scoped_array<float> data(new float[size]);
      const int32* src = reinterpret_cast<const int32 *>(
          info->buffer()->GetRange(info->offset(), size));
      const int32* end = src + num_elements;
      float* dst = data.get();
      while (src != end) {
        *dst++ = static_cast<float>(*src++) / 65536.0f;
      }
      glBufferSubData(GL_ARRAY_BUFFER, offset, size, data.get());
      glVertexAttribPointer(
          info->index(), info->size(), GL_FLOAT, false, 0,
          reinterpret_cast<GLvoid*>(offset));
      offset += size;
    }
  }
  *simulated = true;
  return true;
}

void GLES2DecoderImpl::RestoreStateForSimulatedFixedAttribs() {
  // There's no need to call glVertexAttribPointer because we shadow all the
  // settings and passing GL_FIXED to it will not work.
  glBindBuffer(GL_ARRAY_BUFFER,
               bound_array_buffer_ ? bound_array_buffer_->service_id() : 0);
}

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
  if (!validators_->draw_mode.IsValid(mode)) {
    SetGLError(GL_INVALID_ENUM, "glDrawElements: mode GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->index_type.IsValid(type)) {
    SetGLError(GL_INVALID_ENUM, "glDrawElements: type GL_INVALID_ENUM");
    return error::kNoError;
  }

  if (count == 0) {
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
    bool simulated_attrib_0 = SimulateAttrib0(max_vertex_accessed);
    bool simulated_fixed_attribs = false;
    if (SimulateFixedAttribs(max_vertex_accessed, &simulated_fixed_attribs)) {
      bool textures_set = SetBlackTextureForNonRenderableTextures();
      const GLvoid* indices = reinterpret_cast<const GLvoid*>(offset);
      glDrawElements(mode, count, type, indices);
      if (textures_set) {
        RestoreStateForNonRenderableTextures();
      }
      if (simulated_fixed_attribs) {
        RestoreStateForSimulatedFixedAttribs();
      }
    }
    if (simulated_attrib_0) {
      RestoreStateForSimulatedAttrib0();
    }
  }
  return error::kNoError;
}

GLuint GLES2DecoderImpl::DoGetMaxValueInBufferCHROMIUM(
    GLuint buffer_id, GLsizei count, GLenum type, GLuint offset) {
  GLuint max_vertex_accessed = 0;
  BufferManager::BufferInfo* info = GetBufferInfo(buffer_id);
  if (!info) {
    // TODO(gman): Should this be a GL error or a command buffer error?
    SetGLError(GL_INVALID_VALUE,
               "GetMaxValueInBufferCHROMIUM: unknown buffer");
  } else {
    if (!info->GetMaxValueForRange(offset, count, type, &max_vertex_accessed)) {
      // TODO(gman): Should this be a GL error or a command buffer error?
      SetGLError(
          GL_INVALID_OPERATION,
          "GetMaxValueInBufferCHROMIUM: range out of bounds for buffer");
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
  ShaderManager::ShaderInfo* info = GetShaderInfoNotProgram(
      client_id, "glShaderSource");
  if (!info) {
    return error::kNoError;
  }
  // Note: We don't actually call glShaderSource here. We wait until
  // the call to glCompileShader.
  info->Update(std::string(data, data + data_size).c_str());
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
  ShaderManager::ShaderInfo* info = GetShaderInfoNotProgram(
      client_id, "glCompileShader");
  if (!info) {
    return;
  }
  // Translate GL ES 2.0 shader to Desktop GL shader and pass that to
  // glShaderSource and then glCompileShader.
  const char* shader_src = info->source() ? info->source()->c_str() : "";
  ShaderTranslator* translator = NULL;
  if (use_shader_translator_) {
    translator = info->shader_type() == GL_VERTEX_SHADER ?
        vertex_translator_.get() : fragment_translator_.get();

    if (!translator->Translate(shader_src)) {
      info->SetStatus(false, translator->info_log(), NULL);
      return;
    }
    shader_src = translator->translated_shader();
  }

  glShaderSource(info->service_id(), 1, &shader_src, NULL);
  glCompileShader(info->service_id());
  GLint status = GL_FALSE;
  glGetShaderiv(info->service_id(),  GL_COMPILE_STATUS, &status);
  if (status) {
    info->SetStatus(true, "", translator);
  } else {
    // We cannot reach here if we are using the shader translator.
    // All invalid shaders must be rejected by the translator.
    // All translated shaders must compile.
    LOG_IF(ERROR, use_shader_translator_)
        << "Shader translator allowed/produced an invalid shader.";
    GLint max_len = 0;
    glGetShaderiv(info->service_id(), GL_INFO_LOG_LENGTH, &max_len);
    scoped_array<char> temp(new char[max_len]);
    GLint len = 0;
    glGetShaderInfoLog(info->service_id(), max_len, &len, temp.get());
    DCHECK(max_len == 0 || len < max_len);
    DCHECK(len ==0 || temp[len] == '\0');
    info->SetStatus(false, std::string(temp.get(), len).c_str(), NULL);
  }
};

void GLES2DecoderImpl::DoGetShaderiv(
    GLuint shader, GLenum pname, GLint* params) {
  ShaderManager::ShaderInfo* info = GetShaderInfoNotProgram(
      shader, "glGetShaderiv");
  if (!info) {
    return;
  }
  switch (pname) {
    case GL_SHADER_SOURCE_LENGTH:
      *params = info->source() ? info->source()->size() + 1 : 0;
      return;
    case GL_COMPILE_STATUS:
      *params = info->IsValid();
      return;
    case GL_INFO_LOG_LENGTH:
      *params = info->log_info() ? info->log_info()->size() + 1 : 0;
      return;
    default:
      break;
  }
  glGetShaderiv(info->service_id(), pname, params);
}

error::Error GLES2DecoderImpl::HandleGetShaderSource(
    uint32 immediate_data_size, const gles2::GetShaderSource& c) {
  GLuint shader = c.shader;
  uint32 bucket_id = static_cast<uint32>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  ShaderManager::ShaderInfo* info = GetShaderInfoNotProgram(
      shader, "glGetShaderSource");
  if (!info || !info->source()) {
    bucket->SetSize(0);
    return error::kNoError;
  }
  bucket->SetFromString(info->source()->c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetProgramInfoLog(
    uint32 immediate_data_size, const gles2::GetProgramInfoLog& c) {
  GLuint program = c.program;
  uint32 bucket_id = static_cast<uint32>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  ProgramManager::ProgramInfo* info = GetProgramInfoNotShader(
      program, "glGetProgramInfoLog");
  if (!info || !info->log_info()) {
    bucket->SetSize(0);
    return error::kNoError;
  }
  bucket->SetFromString(info->log_info()->c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetShaderInfoLog(
    uint32 immediate_data_size, const gles2::GetShaderInfoLog& c) {
  GLuint shader = c.shader;
  uint32 bucket_id = static_cast<uint32>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  ShaderManager::ShaderInfo* info = GetShaderInfoNotProgram(
      shader, "glGetShaderInfoLog");
  if (!info || !info->log_info()) {
    bucket->SetSize(0);
    return error::kNoError;
  }
  bucket->SetFromString(info->log_info()->c_str());
  return error::kNoError;
}

bool GLES2DecoderImpl::DoIsBuffer(GLuint client_id) {
  const BufferManager::BufferInfo* info = GetBufferInfo(client_id);
  return info && info->IsValid();
}

bool GLES2DecoderImpl::DoIsFramebuffer(GLuint client_id) {
  const FramebufferManager::FramebufferInfo* info =
      GetFramebufferInfo(client_id);
  return info && info->IsValid();
}

bool GLES2DecoderImpl::DoIsProgram(GLuint client_id) {
  // IsProgram is true for programs as soon as they are created, until they are
  // deleted and no longer in use.
  return GetProgramInfo(client_id) != NULL;
}

bool GLES2DecoderImpl::DoIsRenderbuffer(GLuint client_id) {
  const RenderbufferManager::RenderbufferInfo* info =
      GetRenderbufferInfo(client_id);
  return info && info->IsValid();
}

bool GLES2DecoderImpl::DoIsShader(GLuint client_id) {
  // IsShader is true for shaders as soon as they are created, until they
  // are deleted and not attached to any programs.
  return GetShaderInfo(client_id) != NULL;
}

bool GLES2DecoderImpl::DoIsTexture(GLuint client_id) {
  const TextureManager::TextureInfo* info = GetTextureInfo(client_id);
  return info && info->IsValid();
}

void GLES2DecoderImpl::DoAttachShader(
    GLuint program_client_id, GLint shader_client_id) {
  ProgramManager::ProgramInfo* program_info = GetProgramInfoNotShader(
      program_client_id, "glAttachShader");
  if (!program_info) {
    return;
  }
  ShaderManager::ShaderInfo* shader_info = GetShaderInfoNotProgram(
      shader_client_id, "glAttachShader");
  if (!shader_info) {
    return;
  }
  if (!program_info->AttachShader(shader_manager(), shader_info)) {
    SetGLError(GL_INVALID_OPERATION,
               "glAttachShader: can not attach more than"
               " one shader of the same type.");
    return;
  }
  glAttachShader(program_info->service_id(), shader_info->service_id());
}

void GLES2DecoderImpl::DoDetachShader(
    GLuint program_client_id, GLint shader_client_id) {
  ProgramManager::ProgramInfo* program_info = GetProgramInfoNotShader(
      program_client_id, "glDetachShader");
  if (!program_info) {
    return;
  }
  ShaderManager::ShaderInfo* shader_info = GetShaderInfoNotProgram(
      shader_client_id, "glDetachShader");
  if (!shader_info) {
    return;
  }
  glDetachShader(program_info->service_id(), shader_info->service_id());
  program_info->DetachShader(shader_manager(), shader_info);
}

void GLES2DecoderImpl::DoValidateProgram(GLuint program_client_id) {
  ProgramManager::ProgramInfo* info = GetProgramInfoNotShader(
      program_client_id, "glValidateProgram");
  if (!info) {
    return;
  }
  if (!info->CanLink()) {
    info->set_log_info("Missing Shader");
    return;
  }
  glValidateProgram(info->service_id());
  info->UpdateLogInfo();
}

void GLES2DecoderImpl::DoGetVertexAttribfv(
    GLuint index, GLenum pname, GLfloat* params) {
  VertexAttribManager::VertexAttribInfo* info =
      vertex_attrib_manager_.GetVertexAttribInfo(index);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetVertexAttribfv: index out of range");
    return;
  }
  switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: {
        BufferManager::BufferInfo* buffer = info->buffer();
        if (buffer && !buffer->IsDeleted()) {
          GLuint client_id;
          buffer_manager()->GetClientId(buffer->service_id(), &client_id);
          *params = static_cast<GLfloat>(client_id);
        }
        break;
      }
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
      *params = static_cast<GLfloat>(info->enabled());
      break;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
      *params = static_cast<GLfloat>(info->size());
      break;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
      *params = static_cast<GLfloat>(info->gl_stride());
      break;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE:
      *params = static_cast<GLfloat>(info->type());
      break;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
      *params = static_cast<GLfloat>(info->normalized());
      break;
    case GL_CURRENT_VERTEX_ATTRIB:
      params[0] = info->value().v[0];
      params[1] = info->value().v[1];
      params[2] = info->value().v[2];
      params[3] = info->value().v[3];
      break;
    default:
      NOTREACHED();
      break;
  }
}

void GLES2DecoderImpl::DoGetVertexAttribiv(
    GLuint index, GLenum pname, GLint* params) {
  VertexAttribManager::VertexAttribInfo* info =
      vertex_attrib_manager_.GetVertexAttribInfo(index);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetVertexAttribiv: index out of range");
    return;
  }
  switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: {
        BufferManager::BufferInfo* buffer = info->buffer();
        if (buffer && !buffer->IsDeleted()) {
          GLuint client_id;
          buffer_manager()->GetClientId(buffer->service_id(), &client_id);
          *params = client_id;
        }
        break;
      }
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
      *params = info->enabled();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
      *params = info->size();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
      *params = info->gl_stride();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE:
      *params = info->type();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
      *params = static_cast<GLint>(info->normalized());
      break;
    case GL_CURRENT_VERTEX_ATTRIB:
      params[0] = static_cast<GLint>(info->value().v[0]);
      params[1] = static_cast<GLint>(info->value().v[1]);
      params[2] = static_cast<GLint>(info->value().v[2]);
      params[3] = static_cast<GLint>(info->value().v[3]);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void GLES2DecoderImpl::DoVertexAttrib1f(GLuint index, GLfloat v0) {
  VertexAttribManager::VertexAttribInfo* info =
      vertex_attrib_manager_.GetVertexAttribInfo(index);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glVertexAttrib1f: index out of range");
    return;
  }
  VertexAttribManager::VertexAttribInfo::Vec4 value;
  value.v[0] = v0;
  value.v[1] = 0.0f;
  value.v[2] = 0.0f;
  value.v[3] = 1.0f;
  info->set_value(value);
  glVertexAttrib1f(index, v0);
}

void GLES2DecoderImpl::DoVertexAttrib2f(GLuint index, GLfloat v0, GLfloat v1) {
  VertexAttribManager::VertexAttribInfo* info =
      vertex_attrib_manager_.GetVertexAttribInfo(index);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glVertexAttrib2f: index out of range");
    return;
  }
  VertexAttribManager::VertexAttribInfo::Vec4 value;
  value.v[0] = v0;
  value.v[1] = v1;
  value.v[2] = 0.0f;
  value.v[3] = 1.0f;
  info->set_value(value);
  glVertexAttrib2f(index, v0, v1);
}

void GLES2DecoderImpl::DoVertexAttrib3f(
    GLuint index, GLfloat v0, GLfloat v1, GLfloat v2) {
  VertexAttribManager::VertexAttribInfo* info =
      vertex_attrib_manager_.GetVertexAttribInfo(index);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glVertexAttrib3f: index out of range");
    return;
  }
  VertexAttribManager::VertexAttribInfo::Vec4 value;
  value.v[0] = v0;
  value.v[1] = v1;
  value.v[2] = v2;
  value.v[3] = 1.0f;
  info->set_value(value);
  glVertexAttrib3f(index, v0, v1, v2);
}

void GLES2DecoderImpl::DoVertexAttrib4f(
    GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
  VertexAttribManager::VertexAttribInfo* info =
      vertex_attrib_manager_.GetVertexAttribInfo(index);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glVertexAttrib4f: index out of range");
    return;
  }
  VertexAttribManager::VertexAttribInfo::Vec4 value;
  value.v[0] = v0;
  value.v[1] = v1;
  value.v[2] = v2;
  value.v[3] = v3;
  info->set_value(value);
  glVertexAttrib4f(index, v0, v1, v2, v3);
}

void GLES2DecoderImpl::DoVertexAttrib1fv(GLuint index, const GLfloat* v) {
  VertexAttribManager::VertexAttribInfo* info =
      vertex_attrib_manager_.GetVertexAttribInfo(index);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glVertexAttrib1fv: index out of range");
    return;
  }
  VertexAttribManager::VertexAttribInfo::Vec4 value;
  value.v[0] = v[0];
  value.v[1] = 0.0f;
  value.v[2] = 0.0f;
  value.v[3] = 1.0f;
  info->set_value(value);
  glVertexAttrib1fv(index, v);
}

void GLES2DecoderImpl::DoVertexAttrib2fv(GLuint index, const GLfloat* v) {
  VertexAttribManager::VertexAttribInfo* info =
      vertex_attrib_manager_.GetVertexAttribInfo(index);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glVertexAttrib2fv: index out of range");
    return;
  }
  VertexAttribManager::VertexAttribInfo::Vec4 value;
  value.v[0] = v[0];
  value.v[1] = v[1];
  value.v[2] = 0.0f;
  value.v[3] = 1.0f;
  info->set_value(value);
  glVertexAttrib2fv(index, v);
}

void GLES2DecoderImpl::DoVertexAttrib3fv(GLuint index, const GLfloat* v) {
  VertexAttribManager::VertexAttribInfo* info =
      vertex_attrib_manager_.GetVertexAttribInfo(index);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glVertexAttrib3fv: index out of range");
    return;
  }
  VertexAttribManager::VertexAttribInfo::Vec4 value;
  value.v[0] = v[0];
  value.v[1] = v[1];
  value.v[2] = v[2];
  value.v[3] = 1.0f;
  info->set_value(value);
  glVertexAttrib3fv(index, v);
}

void GLES2DecoderImpl::DoVertexAttrib4fv(GLuint index, const GLfloat* v) {
  VertexAttribManager::VertexAttribInfo* info =
      vertex_attrib_manager_.GetVertexAttribInfo(index);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glVertexAttrib4fv: index out of range");
    return;
  }
  VertexAttribManager::VertexAttribInfo::Vec4 value;
  value.v[0] = v[0];
  value.v[1] = v[1];
  value.v[2] = v[2];
  value.v[3] = v[3];
  info->set_value(value);
  glVertexAttrib4fv(index, v);
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
  if (!validators_->vertex_attrib_type.IsValid(type)) {
    SetGLError(GL_INVALID_ENUM,
               "glVertexAttribPointer: type GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->vertex_attrib_size.IsValid(size)) {
    SetGLError(GL_INVALID_VALUE,
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
  if (offset % component_size > 0) {
    SetGLError(GL_INVALID_OPERATION,
               "glVertexAttribPointer: offset not valid for type");
    return error::kNoError;
  }
  if (stride % component_size > 0) {
    SetGLError(GL_INVALID_OPERATION,
               "glVertexAttribPointer: stride not valid for type");
    return error::kNoError;
  }
  vertex_attrib_manager_.SetAttribInfo(
      indx,
      bound_array_buffer_,
      size,
      type,
      normalized,
      stride,
      stride != 0 ? stride : component_size * size,
      offset);
  if (type != GL_FIXED) {
    glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
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

  if (!validators_->read_pixel_format.IsValid(format)) {
    SetGLError(GL_INVALID_ENUM, "glReadPixels: format GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->pixel_type.IsValid(type)) {
    SetGLError(GL_INVALID_ENUM, "glReadPixels: type GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (width == 0 || height == 0) {
    return error::kNoError;
  }

  CopyRealGLErrorsToWrapper();

  ScopedResolvedFrameBufferBinder binder(this);

  // Get the size of the current fbo or backbuffer.
  gfx::Size max_size = GetBoundReadFrameBufferSize();

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
  if (!validators_->pixel_store.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glPixelStorei: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->pixel_store_alignment.IsValid(param)) {
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
      NOTREACHED();
      break;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::GetAttribLocationHelper(
    GLuint client_id, uint32 location_shm_id, uint32 location_shm_offset,
    const std::string& name_str) {
  ProgramManager::ProgramInfo* info = GetProgramInfoNotShader(
      client_id, "glGetAttribLocation");
  if (!info) {
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
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return GetAttribLocationHelper(
    c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::GetUniformLocationHelper(
    GLuint client_id, uint32 location_shm_id, uint32 location_shm_offset,
    const std::string& name_str) {
  ProgramManager::ProgramInfo* info = GetProgramInfoNotShader(
      client_id, "glUniformLocation");
  if (!info) {
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
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return GetUniformLocationHelper(
    c.program, c.location_shm_id, c.location_shm_offset, name_str);
}

error::Error GLES2DecoderImpl::HandleGetString(
    uint32 immediate_data_size, const gles2::GetString& c) {
  GLenum name = static_cast<GLenum>(c.name);
  if (!validators_->string_type.IsValid(name)) {
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
      str = feature_info_->extensions().c_str();
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
  if (!validators_->buffer_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glBufferData: target GL_INVALID_ENUM");
    return;
  }
  if (!validators_->buffer_usage.IsValid(usage)) {
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
    buffer_manager()->SetSize(info, size);
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
  if (!validators_->texture_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM,
               "glCompressedTexImage2D: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->compressed_texture_format.IsValid(
      internal_format)) {
    SetGLError(GL_INVALID_ENUM,
               "glCompressedTexImage2D: internal_format GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!texture_manager()->ValidForTarget(
        feature_info_, target, level, width, height, 1) ||
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
  CopyRealGLErrorsToWrapper();
  glCompressedTexImage2D(
      target, level, internal_format, width, height, border, image_size, data);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    texture_manager()->SetLevelInfo(
        feature_info_,
        info, target, level, internal_format, width, height, 1, border, 0, 0);
  }
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

error::Error GLES2DecoderImpl::HandleCompressedTexImage2DBucket(
    uint32 immediate_data_size, const gles2::CompressedTexImage2DBucket& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  Bucket* bucket = GetBucket(c.bucket_id);
  return DoCompressedTexImage2D(
      target, level, internal_format, width, height, border,
      bucket->size(), bucket->GetData(0, bucket->size()));
}

error::Error GLES2DecoderImpl::HandleCompressedTexSubImage2DBucket(
    uint32 immediate_data_size,
    const gles2::CompressedTexSubImage2DBucket& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  Bucket* bucket = GetBucket(c.bucket_id);
  uint32 data_size = bucket->size();
  GLsizei imageSize = data_size;
  const void* data = bucket->GetData(0, data_size);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM, "glCompressedTexSubImage2D: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->compressed_texture_format.IsValid(format)) {
    SetGLError(GL_INVALID_ENUM,
               "glCompressedTexSubImage2D: format GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D: width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D: height < 0");
    return error::kNoError;
  }
  if (imageSize < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D: imageSize < 0");
    return error::kNoError;
  }
  DoCompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
  return error::kNoError;
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
  if (!validators_->texture_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glTexImage2D: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->texture_format.IsValid(internal_format)) {
    SetGLError(GL_INVALID_ENUM,
               "glTexImage2D: internal_format GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->texture_format.IsValid(format)) {
    SetGLError(GL_INVALID_ENUM, "glTexImage2D: format GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->pixel_type.IsValid(type)) {
    SetGLError(GL_INVALID_ENUM, "glTexImage2D: type GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (format != internal_format) {
    SetGLError(GL_INVALID_OPERATION, "glTexImage2D: format != internalFormat");
    return error::kNoError;
  }
  if (!texture_manager()->ValidForTarget(
        feature_info_, target, level, width, height, 1) ||
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

  GLenum gl_internal_format = internal_format;
  if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2) {
    if (format == GL_BGRA_EXT && internal_format == GL_BGRA_EXT) {
      gl_internal_format = GL_RGBA;
    } else if (type == GL_FLOAT) {
      if (format == GL_RGBA) {
        gl_internal_format = GL_RGBA32F_ARB;
      } else if (format == GL_RGB) {
        gl_internal_format = GL_RGB32F_ARB;
      }
    } else if (type == GL_HALF_FLOAT_OES) {
      if (format == GL_RGBA) {
        gl_internal_format = GL_RGBA16F_ARB;
      } else if (format == GL_RGB) {
        gl_internal_format = GL_RGB16F_ARB;
      }
    }
  }

  CopyRealGLErrorsToWrapper();
  glTexImage2D(
      target, level, gl_internal_format, width, height, border, format, type,
      pixels);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    texture_manager()->SetLevelInfo(feature_info_, info,
        target, level, internal_format, width, height, 1, border, format, type);
  }
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

void GLES2DecoderImpl::DoCompressedTexSubImage2D(
  GLenum target,
  GLint level,
  GLint xoffset,
  GLint yoffset,
  GLsizei width,
  GLsizei height,
  GLenum format,
  GLsizei image_size,
  const void * data) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glCompressedTexSubImage2D: unknown texture for target");
    return;
  }
  GLenum type = 0;
  GLenum internal_format = 0;
  if (!info->GetLevelType(target, level, &type, &internal_format)) {
    SetGLError(
        GL_INVALID_OPERATION,
        "glCompressdTexSubImage2D: level does not exist.");
    return;
  }
  if (internal_format != format) {
    SetGLError(
        GL_INVALID_OPERATION,
        "glCompressdTexSubImage2D: format does not match internal format.");
    return;
  }
  if (!info->ValidForTexture(
      target, level, xoffset, yoffset, width, height, format, type)) {
    SetGLError(GL_INVALID_VALUE,
               "glCompressdTexSubImage2D: bad dimensions.");
    return;
  }
  glCompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, image_size, data);
}

static void Clip(
    GLint start, GLint range, GLint sourceRange,
    GLint* out_start, GLint* out_range) {
  DCHECK(out_start);
  DCHECK(out_range);
  if (start < 0) {
    range += start;
    start = 0;
  }
  GLint end = start + range;
  if (end > sourceRange) {
    range -= end - sourceRange;
  }
  *out_start = start;
  *out_range = range;
}


void GLES2DecoderImpl::DoCopyTexImage2D(
  GLenum target,
  GLint level,
  GLenum internal_format,
  GLint x,
  GLint y,
  GLsizei width,
  GLsizei height,
  GLint border) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glCopyTexImage2D: unknown texture for target");
    return;
  }
  if (!texture_manager()->ValidForTarget(
        feature_info_, target, level, width, height, 1) ||
      border != 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexImage2D: dimensions out of range");
    return;
  }

  // Check we have compatible formats.
  GLenum read_format = GetBoundReadFrameBufferInternalFormat();
  uint32 channels_exist = GLES2Util::GetChannelsForFormat(read_format);
  uint32 channels_needed = GLES2Util::GetChannelsForFormat(internal_format);

  if ((channels_needed & channels_exist) != channels_needed) {
    SetGLError(GL_INVALID_OPERATION, "glCopyTexImage2D: incompatible format");
    return;
  }

  CopyRealGLErrorsToWrapper();
  ScopedResolvedFrameBufferBinder binder(this);
  gfx::Size size = GetBoundReadFrameBufferSize();

  // Clip to size to source dimensions
  GLint copyX = 0;
  GLint copyY = 0;
  GLint copyWidth = 0;
  GLint copyHeight = 0;
  Clip(x, width, size.width(), &copyX, &copyWidth);
  Clip(y, height, size.height(), &copyY, &copyHeight);

  if (copyX != x ||
      copyY != y ||
      copyWidth != width ||
      copyHeight != height) {
    // some part was clipped so clear the texture.
    uint32 pixels_size = 0;
    if (!GLES2Util::ComputeImageDataSize(
        width, height, internal_format, GL_UNSIGNED_BYTE,
        unpack_alignment_, &pixels_size)) {
      SetGLError(GL_INVALID_VALUE, "glCopyTexImage2D: dimensions too large");
      return;
    }
    scoped_array<char> zero(new char[pixels_size]);
    memset(zero.get(), 0, pixels_size);
    glTexImage2D(target, level, internal_format, width, height, 0,
                 internal_format, GL_UNSIGNED_BYTE, zero.get());
    if (copyHeight > 0 && copyWidth > 0) {
      GLint dx = copyX - x;
      GLint dy = copyY - y;
      GLint destX = dx;
      GLint destY = dy;
      glCopyTexSubImage2D(target, level,
                          destX, destY, copyX, copyY,
                          copyWidth, copyHeight);
    }
  } else {
    glCopyTexImage2D(target, level, internal_format,
                     copyX, copyY, copyWidth, copyHeight, border);
  }
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    texture_manager()->SetLevelInfo(
        feature_info_, info, target, level, internal_format, width, height, 1,
        border, internal_format, GL_UNSIGNED_BYTE);
  }
}

void GLES2DecoderImpl::DoCopyTexSubImage2D(
  GLenum target,
  GLint level,
  GLint xoffset,
  GLint yoffset,
  GLint x,
  GLint y,
  GLsizei width,
  GLsizei height) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glCopyTexSubImage2D: unknown texture for target");
    return;
  }
  GLenum type = 0;
  GLenum format = 0;
  if (!info->GetLevelType(target, level, &type, &format) ||
      !info->ValidForTexture(
          target, level, xoffset, yoffset, width, height, format, type)) {
    SetGLError(GL_INVALID_VALUE,
               "glCopyTexSubImage2D: bad dimensions.");
    return;
  }

  // Check we have compatible formats.
  GLenum read_format = GetBoundReadFrameBufferInternalFormat();
  uint32 channels_exist = GLES2Util::GetChannelsForFormat(read_format);
  uint32 channels_needed = GLES2Util::GetChannelsForFormat(format);

  if ((channels_needed & channels_exist) != channels_needed) {
    SetGLError(
        GL_INVALID_OPERATION, "glCopyTexSubImage2D: incompatible format");
    return;
  }

  ScopedResolvedFrameBufferBinder binder(this);
  gfx::Size size = GetBoundReadFrameBufferSize();
  GLint copyX = 0;
  GLint copyY = 0;
  GLint copyWidth = 0;
  GLint copyHeight = 0;
  Clip(x, width, size.width(), &copyX, &copyWidth);
  Clip(y, height, size.height(), &copyY, &copyHeight);
  if (copyX != x ||
      copyY != y ||
      copyWidth != width ||
      copyHeight != height) {
    // some part was clipped so clear the texture.
    uint32 pixels_size = 0;
    if (!GLES2Util::ComputeImageDataSize(
        width, height, format, type, unpack_alignment_, &pixels_size)) {
      SetGLError(GL_INVALID_VALUE, "glCopyTexSubImage2D: dimensions too large");
      return;
    }
    scoped_array<char> zero(new char[pixels_size]);
    memset(zero.get(), 0, pixels_size);
    glTexSubImage2D(
        target, level, xoffset, yoffset, width, height,
        format, type, zero.get());
  }
  if (copyHeight > 0 && copyWidth > 0) {
    GLint dx = copyX - x;
    GLint dy = copyY - y;
    GLint destX = xoffset + dx;
    GLint destY = yoffset + dy;
    glCopyTexSubImage2D(target, level,
                        destX, destY, copyX, copyY,
                        copyWidth, copyHeight);
  }
}

void GLES2DecoderImpl::DoTexSubImage2D(
  GLenum target,
  GLint level,
  GLint xoffset,
  GLint yoffset,
  GLsizei width,
  GLsizei height,
  GLenum format,
  GLenum type,
  const void * data) {
  TextureManager::TextureInfo* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glTexSubImage2D: unknown texture for target");
    return;
  }
  GLenum current_type = 0;
  GLenum internal_format = 0;
  if (!info->GetLevelType(target, level, &current_type, &internal_format)) {
    SetGLError(
        GL_INVALID_OPERATION,
        "glTexSubImage2D: level does not exist.");
    return;
  }
  if (format != internal_format) {
    SetGLError(GL_INVALID_OPERATION,
               "glTexSubImage2D: format does not match internal format.");
    return;
  }
  if (type != current_type) {
    SetGLError(GL_INVALID_OPERATION,
               "glTexSubImage2D: type does not match type of texture.");
    return;
  }

  if (!info->ValidForTexture(
          target, level, xoffset, yoffset, width, height, format, type)) {
    SetGLError(GL_INVALID_VALUE,
               "glTexSubImage2D: bad dimensions.");
    return;
  }
  glTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, data);
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
  if (!validators_->vertex_pointer.IsValid(pname)) {
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
  *result->GetData() =
      vertex_attrib_manager_.GetVertexAttribInfo(index)->offset();
  return error::kNoError;
}

bool GLES2DecoderImpl::GetUniformSetup(
    GLuint program, GLint location,
    uint32 shm_id, uint32 shm_offset,
    error::Error* error, GLuint* service_id, void** result_pointer,
    GLenum* result_type) {
  DCHECK(error);
  DCHECK(service_id);
  DCHECK(result_pointer);
  DCHECK(result_type);
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
  ProgramManager::ProgramInfo* info = GetProgramInfoNotShader(
      program, "glGetUniform");
  if (!info) {
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
  *result_type = type;
  return true;
}

error::Error GLES2DecoderImpl::HandleGetUniformiv(
    uint32 immediate_data_size, const gles2::GetUniformiv& c) {
  GLuint program = c.program;
  GLint location = c.location;
  GLuint service_id;
  GLenum result_type;
  Error error;
  void* result;
  if (GetUniformSetup(
      program, location, c.params_shm_id, c.params_shm_offset,
      &error, &service_id, &result, &result_type)) {
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
  typedef gles2::GetUniformfv::Result Result;
  Result* result;
  GLenum result_type;
  if (GetUniformSetup(
      program, location, c.params_shm_id, c.params_shm_offset,
      &error, &service_id, reinterpret_cast<void**>(&result), &result_type)) {
    if (result_type == GL_BOOL || result_type == GL_BOOL_VEC2 ||
        result_type == GL_BOOL_VEC3 || result_type == GL_BOOL_VEC4) {
      GLsizei num_values = result->GetNumResults();
      scoped_array<GLint> temp(new GLint[num_values]);
      glGetUniformiv(service_id, location, temp.get());
      GLfloat* dst = result->GetData();
      for (GLsizei ii = 0; ii < num_values; ++ii) {
        dst[ii] = (temp[ii] != 0);
      }
    } else {
      glGetUniformfv(service_id, location, result->GetData());
    }
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
  if (!validators_->shader_type.IsValid(shader_type)) {
    SetGLError(GL_INVALID_ENUM,
               "glGetShaderPrecisionFormat: shader_type GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->shader_precision.IsValid(precision_type)) {
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
      break;
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
  GLuint program = static_cast<GLuint>(c.program);
  ProgramManager::ProgramInfo* info = GetProgramInfoNotShader(
      program, "glGetAttachedShaders");
  if (!info) {
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
  ProgramManager::ProgramInfo* info = GetProgramInfoNotShader(
      program, "glGetActiveUniform");
  if (!info) {
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
  bucket->SetFromString(uniform_info->name.c_str());
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
  ProgramManager::ProgramInfo* info = GetProgramInfoNotShader(
      program, "glGetActiveAttrib");
  if (!info) {
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
  bucket->SetFromString(attrib_info->name.c_str());
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
  // If offscreen then don't actually SwapBuffers to the display. Just copy
  // the rendered frame to another frame buffer.
  if (offscreen_target_frame_buffer_.get()) {
    ScopedGLErrorSuppressor suppressor(this);

    // First check to see if a deferred offscreen render buffer resize is
    // pending.
    if (!UpdateOffscreenFrameBufferSize()) {
      LOG(ERROR) << "Context lost because reallocation of offscreen FBO "
                 << "failed.";
      return error::kLostContext;
    }

    if (IsOffscreenBufferMultisampled()) {
      // For multisampled buffers, bind the resolved frame buffer so that
      // callbacks can call ReadPixels or CopyTexImage2D.
      ScopedResolvedFrameBufferBinder binder(this);
      if (swap_buffers_callback_.get()) {
        swap_buffers_callback_->Run();
      }
      return error::kThrottle;
    } else {
      ScopedFrameBufferBinder binder(this,
                                     offscreen_target_frame_buffer_->id());

      if (parent_) {
        // Copy the target frame buffer to the saved offscreen texture.
        offscreen_saved_color_texture_->Copy(
            offscreen_saved_color_texture_->size());

        // Ensure the side effects of the copy are visible to the parent
        // context. There is no need to do this for ANGLE because it uses a
        // single D3D device for all contexts.
        if (!IsAngle())
          glFlush();
      }

      // Run the callback with |binder| in scope, so that the callback can call
      // ReadPixels or CopyTexImage2D.
      if (swap_buffers_callback_.get()) {
        swap_buffers_callback_->Run();
      }
      return error::kThrottle;
    }
  } else {
    if (!context_->SwapBuffers()) {
      LOG(ERROR) << "Context lost because SwapBuffers failed.";
      return error::kLostContext;
    }
  }

  if (swap_buffers_callback_.get()) {
    swap_buffers_callback_->Run();
  }

  // Do not throttle SwapBuffers by returning kThrottle. The intent of
  // throttling the offscreen command buffers to a fixed number of frames
  // ahead is to prevent them from rendering faster than they can be
  // presented, not to limit the rate at which we present.
  //
  // This does not hold for ANGLE, possibly because all the GL contexts in a
  // share group are actually one D3D device. Found by trial and error.
  return IsAngle() ? error::kThrottle : error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCommandBufferEnableCHROMIUM(
    uint32 immediate_data_size, const gles2::CommandBufferEnableCHROMIUM& c) {
  Bucket* bucket = GetBucket(c.bucket_id);
  typedef gles2::CommandBufferEnableCHROMIUM::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*result != 0) {
    return error::kInvalidArguments;
  }
  std::string feature_str;
  if (!bucket->GetAsString(&feature_str)) {
    return error::kInvalidArguments;
  }

  // TODO(gman): make this some kind of table to function pointer thingy.
  if (feature_str.compare(PEPPER3D_ALLOW_BUFFERS_ON_MULTIPLE_TARGETS) == 0) {
    buffer_manager()->set_allow_buffers_on_multiple_targets(true);
  } else if (feature_str.compare(PEPPER3D_SUPPORT_FIXED_ATTRIBS) == 0) {
    buffer_manager()->set_allow_buffers_on_multiple_targets(true);
    // TODO(gman): decide how to remove the need for this const_cast.
    // I could make validators_ non const but that seems bad as this is the only
    // place it is needed. I could make some special friend class of validators
    // just to allow this to set them. That seems silly. I could refactor this
    // code to use the extension mechanism or the initialization attributes to
    // turn this feature on. Given that the only real point of this is to make
    // the conformance tests pass and given that there is lots of real work that
    // needs to be done it seems like refactoring for one to one of those
    // methods is a very low priority.
    const_cast<Validators*>(validators_)->vertex_attrib_type.AddValue(GL_FIXED);
  } else if (feature_str.compare(PEPPER3D_SKIP_GLSL_TRANSLATION) == 0) {
    use_shader_translator_ = false;
  } else {
    return error::kNoError;
  }

  *result = 1;  // true.
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetRequestableExtensionsCHROMIUM(
    uint32 immediate_data_size,
    const gles2::GetRequestableExtensionsCHROMIUM& c) {
  Bucket* bucket = CreateBucket(c.bucket_id);
  scoped_ptr<FeatureInfo> info(new FeatureInfo());
  info->Initialize(NULL);
  bucket->SetFromString(info->extensions().c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleRequestExtensionCHROMIUM(
    uint32 immediate_data_size, const gles2::RequestExtensionCHROMIUM& c) {
  Bucket* bucket = GetBucket(c.bucket_id);
  std::string feature_str;
  if (!bucket->GetAsString(&feature_str)) {
    return error::kInvalidArguments;
  }

  bool std_derivatives_enabled =
      feature_info_->feature_flags().oes_standard_derivatives;
  bool webglsl_enabled =
      feature_info_->feature_flags().chromium_webglsl;

  feature_info_->AddFeatures(feature_str.c_str());

  // If we just enabled a feature which affects the shader translator,
  // we may need to re-initialize it.
  if (std_derivatives_enabled !=
          feature_info_->feature_flags().oes_standard_derivatives ||
      webglsl_enabled !=
          feature_info_->feature_flags().chromium_webglsl) {
    InitializeShaderTranslator();
  }

  return error::kNoError;
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/gles2_cmd_decoder_autogen.h"

}  // namespace gles2
}  // namespace gpu
