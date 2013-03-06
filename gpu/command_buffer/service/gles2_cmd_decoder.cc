// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include <stdio.h>

#include <algorithm>
#include <list>
#include <map>
#include <stack>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/atomicops.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#if defined(OS_MACOSX)
#include "base/mac/scoped_cftyperef.h"
#endif
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "build/build_config.h"
#define GLES2_GPU_SERVICE 1
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/shader_translator.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/stream_texture.h"
#include "gpu/command_buffer/service/stream_texture_manager.h"
#include "gpu/command_buffer/service/texture_definition.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/vertex_attrib_manager.h"
#include "gpu/command_buffer/service/vertex_array_manager.h"
#include "ui/gl/async_pixel_transfer_delegate.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#if defined(OS_MACOSX)
#include "ui/surface/io_surface_support_mac.h"
#endif

// TODO(zmo): we can't include "City.h" due to type def conflicts.
extern uint64 CityHash64(const char*, size_t);

namespace gpu {
namespace gles2 {

namespace {

static const char kOESDerivativeExtension[] = "GL_OES_standard_derivatives";

#if !defined(ANGLE_SH_VERSION) || ANGLE_SH_VERSION < 108
khronos_uint64_t CityHashForAngle(const char* name, unsigned int len) {
  return static_cast<khronos_uint64_t>(
      CityHash64(name, static_cast<size_t>(len)));
}
#endif

}  // namespace

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
  #define GLES2_CMD_OP(name) {                                             \
    cmds::name::kArgFlags,                                                 \
    sizeof(cmds::name) / sizeof(CommandBufferEntry) - 1, },  /* NOLINT */  \

  GLES2_COMMAND_LIST(GLES2_CMD_OP)

  #undef GLES2_CMD_OP
};

// Return true if a character belongs to the ASCII subset as defined in
// GLSL ES 1.0 spec section 3.1.
static bool CharacterIsValidForGLES(unsigned char c) {
  // Printing characters are valid except " $ ` @ \ ' DEL.
  if (c >= 32 && c <= 126 &&
      c != '"' &&
      c != '$' &&
      c != '`' &&
      c != '@' &&
      c != '\\' &&
      c != '\'') {
    return true;
  }
  // Horizontal tab, line feed, vertical tab, form feed, carriage return
  // are also valid.
  if (c >= 9 && c <= 13) {
    return true;
  }

  return false;
}

static bool StringIsValidForGLES(const char* str) {
  for (; *str; ++str) {
    if (!CharacterIsValidForGLES(*str)) {
      return false;
    }
  }
  return true;
}

static inline GLenum GetTexInternalFormat(GLenum internal_format) {
  if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2) {
    if (internal_format == GL_BGRA_EXT || internal_format == GL_BGRA8_EXT)
      return GL_RGBA8;
  }
  return internal_format;
}

// TODO(epenner): Could the above function be merged into this and removed?
static inline GLenum GetTexInternalFormat(GLenum internal_format,
                                          GLenum format,
                                          GLenum type) {
  GLenum gl_internal_format = GetTexInternalFormat(internal_format);

  if (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2)
    return gl_internal_format;

  if (type == GL_FLOAT) {
    switch (format) {
      case GL_RGBA:
        gl_internal_format = GL_RGBA32F_ARB;
        break;
      case GL_RGB:
        gl_internal_format = GL_RGB32F_ARB;
        break;
      case GL_LUMINANCE_ALPHA:
        gl_internal_format = GL_LUMINANCE_ALPHA32F_ARB;
        break;
      case GL_LUMINANCE:
        gl_internal_format = GL_LUMINANCE32F_ARB;
        break;
      case GL_ALPHA:
        gl_internal_format = GL_ALPHA32F_ARB;
        break;
      default:
        NOTREACHED();
        break;
    }
  } else if (type == GL_HALF_FLOAT_OES) {
    switch (format) {
      case GL_RGBA:
        gl_internal_format = GL_RGBA16F_ARB;
        break;
      case GL_RGB:
        gl_internal_format = GL_RGB16F_ARB;
        break;
      case GL_LUMINANCE_ALPHA:
        gl_internal_format = GL_LUMINANCE_ALPHA16F_ARB;
        break;
      case GL_LUMINANCE:
        gl_internal_format = GL_LUMINANCE16F_ARB;
        break;
      case GL_ALPHA:
        gl_internal_format = GL_ALPHA16F_ARB;
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  return gl_internal_format;
}

static void WrappedTexImage2D(
    GLenum target,
    GLint level,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    const void* pixels) {
  glTexImage2D(
      target, level, GetTexInternalFormat(internal_format, format, type),
      width, height, border, format, type, pixels);
}

// Wrapper for glEnable/glDisable that doesn't suck.
static void EnableDisable(GLenum pname, bool enable) {
  if (enable) {
    glEnable(pname);
  } else {
    glDisable(pname);
  }
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
// the multisampled offscreen render buffer if that buffer is multisampled, and,
// if it is bound or enforce_internal_framebuffer is true. If internal is
// true, the resolved framebuffer is not visible to the parent.
class ScopedResolvedFrameBufferBinder {
 public:
  ScopedResolvedFrameBufferBinder(GLES2DecoderImpl* decoder,
                                  bool enforce_internal_framebuffer,
                                  bool internal);
  ~ScopedResolvedFrameBufferBinder();

 private:
  GLES2DecoderImpl* decoder_;
  bool resolve_and_bind_;
  DISALLOW_COPY_AND_ASSIGN(ScopedResolvedFrameBufferBinder);
};

// This class records texture upload time when in scope.
class ScopedTextureUploadTimer {
 public:
  explicit ScopedTextureUploadTimer(GLES2DecoderImpl* decoder);
  ~ScopedTextureUploadTimer();

 private:
  GLES2DecoderImpl* decoder_;
  base::TimeTicks begin_time_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTextureUploadTimer);
};

// Encapsulates an OpenGL texture.
class BackTexture {
 public:
  explicit BackTexture(GLES2DecoderImpl* decoder);
  ~BackTexture();

  // Create a new render texture.
  void Create();

  // Set the initial size and format of a render texture or resize it.
  bool AllocateStorage(const gfx::Size& size, GLenum format, bool zero);

  // Copy the contents of the currently bound frame buffer.
  void Copy(const gfx::Size& size, GLenum format);

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

  size_t estimated_size() const {
    return memory_tracker_.GetMemRepresented();
  }

 private:
  GLES2DecoderImpl* decoder_;
  MemoryTypeTracker memory_tracker_;
  size_t bytes_allocated_;
  GLuint id_;
  gfx::Size size_;
  DISALLOW_COPY_AND_ASSIGN(BackTexture);
};

// Encapsulates an OpenGL render buffer of any format.
class BackRenderbuffer {
 public:
  explicit BackRenderbuffer(GLES2DecoderImpl* decoder);
  ~BackRenderbuffer();

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

  size_t estimated_size() const {
    return memory_tracker_.GetMemRepresented();
  }

 private:
  GLES2DecoderImpl* decoder_;
  MemoryTypeTracker memory_tracker_;
  size_t bytes_allocated_;
  GLuint id_;
  DISALLOW_COPY_AND_ASSIGN(BackRenderbuffer);
};

// Encapsulates an OpenGL frame buffer.
class BackFramebuffer {
 public:
  explicit BackFramebuffer(GLES2DecoderImpl* decoder);
  ~BackFramebuffer();

  // Create a new frame buffer.
  void Create();

  // Attach a color render buffer to a frame buffer.
  void AttachRenderTexture(BackTexture* texture);

  // Attach a render buffer to a frame buffer. Note that this unbinds any
  // currently bound frame buffer.
  void AttachRenderBuffer(GLenum target, BackRenderbuffer* render_buffer);

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
  DISALLOW_COPY_AND_ASSIGN(BackFramebuffer);
};

// }  // anonymous namespace.

bool GLES2Decoder::GetServiceTextureId(uint32 client_texture_id,
                                       uint32* service_texture_id) {
  return false;
}

GLES2Decoder::GLES2Decoder()
    : debug_(false),
      log_commands_(false),
      log_synthesized_gl_errors_(true) {
}

GLES2Decoder::~GLES2Decoder() {
}

bool GLES2Decoder::testing_force_is_angle_;

void GLES2Decoder::set_testing_force_is_angle(bool force) {
  testing_force_is_angle_ = force;
}

bool GLES2Decoder::IsAngle() {
#if defined(OS_WIN)
  return testing_force_is_angle_ ||
         gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2;
#else
  return testing_force_is_angle_;
#endif
}

// This class implements GLES2Decoder so we don't have to expose all the GLES2
// cmd stuff to outside this class.
class GLES2DecoderImpl : public GLES2Decoder {
 public:
  static const int kMaxLogMessages = 256;

  // Used by PrepForSetUniformByLocation to validate types.
  struct BaseUniformInfo {
    const GLenum* const valid_types;
    size_t num_valid_types;
  };

  explicit GLES2DecoderImpl(ContextGroup* group);
  virtual ~GLES2DecoderImpl();

  // Overridden from AsyncAPIInterface.
  virtual Error DoCommand(unsigned int command,
                          unsigned int arg_count,
                          const void* args) OVERRIDE;

  // Overridden from AsyncAPIInterface.
  virtual const char* GetCommandName(unsigned int command_id) const OVERRIDE;

  // Overridden from GLES2Decoder.
  virtual bool Initialize(const scoped_refptr<gfx::GLSurface>& surface,
                          const scoped_refptr<gfx::GLContext>& context,
                          bool offscreen,
                          const gfx::Size& size,
                          const DisallowedFeatures& disallowed_features,
                          const char* allowed_extensions,
                          const std::vector<int32>& attribs) OVERRIDE;
  virtual void Destroy(bool have_context) OVERRIDE;
  virtual void SetSurface(
      const scoped_refptr<gfx::GLSurface>& surface) OVERRIDE;
  virtual bool SetParent(GLES2Decoder* parent_decoder,
                         uint32 parent_texture_id) OVERRIDE;
  virtual bool ResizeOffscreenFrameBuffer(const gfx::Size& size) OVERRIDE;
  void UpdateParentTextureInfo();
  virtual bool MakeCurrent() OVERRIDE;
  virtual void ReleaseCurrent() OVERRIDE;
  virtual GLES2Util* GetGLES2Util() OVERRIDE { return &util_; }
  virtual gfx::GLContext* GetGLContext() OVERRIDE { return context_.get(); }
  virtual ContextGroup* GetContextGroup() OVERRIDE { return group_.get(); }
  virtual void RestoreState() const OVERRIDE;

  virtual void RestoreActiveTexture() const OVERRIDE {
    state_.RestoreActiveTexture();
  }
  virtual void RestoreAttribute(unsigned index) const OVERRIDE {
    state_.RestoreAttribute(index);
  }
  virtual void RestoreBufferBindings() const OVERRIDE {
    state_.RestoreBufferBindings();
  }
  virtual void RestoreGlobalState() const OVERRIDE {
    state_.RestoreGlobalState();
  }
  virtual void RestoreProgramBindings() const OVERRIDE {
    state_.RestoreProgramBindings();
  }
  virtual void RestoreRenderbufferBindings() const OVERRIDE {
    state_.RestoreRenderbufferBindings();
  }
  virtual void RestoreTextureUnitBindings(unsigned unit) const OVERRIDE {
    state_.RestoreTextureUnitBindings(unit);
  }
  virtual void RestoreFramebufferBindings() const OVERRIDE;
  virtual void RestoreTextureState(unsigned service_id) const OVERRIDE;

  virtual QueryManager* GetQueryManager() OVERRIDE {
    return query_manager_.get();
  }
  virtual VertexArrayManager* GetVertexArrayManager() OVERRIDE {
    return vertex_array_manager_.get();
  }
  virtual bool ProcessPendingQueries() OVERRIDE;

  virtual void SetResizeCallback(
      const base::Callback<void(gfx::Size)>& callback) OVERRIDE;

  virtual void SetMsgCallback(const MsgCallback& callback) OVERRIDE;
  virtual void SetWaitSyncPointCallback(
      const WaitSyncPointCallback& callback) OVERRIDE;

  virtual void SetStreamTextureManager(StreamTextureManager* manager) OVERRIDE;

  virtual gfx::AsyncPixelTransferDelegate*
      GetAsyncPixelTransferDelegate() OVERRIDE;
  virtual void SetAsyncPixelTransferDelegate(
      gfx::AsyncPixelTransferDelegate* delegate) OVERRIDE;

  virtual bool GetServiceTextureId(uint32 client_texture_id,
                                   uint32* service_texture_id) OVERRIDE;

  virtual uint32 GetGLError() OVERRIDE;

  virtual uint32 GetTextureUploadCount() OVERRIDE;
  virtual base::TimeDelta GetTotalTextureUploadTime() OVERRIDE;
  virtual base::TimeDelta GetTotalProcessingCommandsTime() OVERRIDE;
  virtual void AddProcessingCommandsTime(base::TimeDelta) OVERRIDE;

  // Restores the current state to the user's settings.
  void RestoreCurrentFramebufferBindings();
  void RestoreCurrentRenderbufferBindings();
  void RestoreCurrentTexture2DBindings();

  // Sets DEPTH_TEST, STENCIL_TEST and color mask for the current framebuffer.
  void ApplyDirtyState();

  // These check the state of the currently bound framebuffer or the
  // backbuffer if no framebuffer is bound.
  bool BoundFramebufferHasColorAttachmentWithAlpha();
  bool BoundFramebufferHasDepthAttachment();
  bool BoundFramebufferHasStencilAttachment();

  virtual error::ContextLostReason GetContextLostReason() OVERRIDE;

 private:
  friend class ScopedFrameBufferBinder;
  friend class ScopedGLErrorSuppressor;
  friend class ScopedResolvedFrameBufferBinder;
  friend class ScopedTextureUploadTimer;
  friend class BackTexture;
  friend class BackRenderbuffer;
  friend class BackFramebuffer;

  // Initialize or re-initialize the shader translator.
  bool InitializeShaderTranslator();

  void UpdateCapabilities();

  // Helpers for the glGen and glDelete functions.
  bool GenTexturesHelper(GLsizei n, const GLuint* client_ids);
  void DeleteTexturesHelper(GLsizei n, const GLuint* client_ids);
  bool GenBuffersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteBuffersHelper(GLsizei n, const GLuint* client_ids);
  bool GenFramebuffersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteFramebuffersHelper(GLsizei n, const GLuint* client_ids);
  bool GenRenderbuffersHelper(GLsizei n, const GLuint* client_ids);
  void DeleteRenderbuffersHelper(GLsizei n, const GLuint* client_ids);
  bool GenQueriesEXTHelper(GLsizei n, const GLuint* client_ids);
  void DeleteQueriesEXTHelper(GLsizei n, const GLuint* client_ids);
  bool GenVertexArraysOESHelper(GLsizei n, const GLuint* client_ids);
  void DeleteVertexArraysOESHelper(GLsizei n, const GLuint* client_ids);

  // Workarounds
  void OnFboChanged() const;

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

  const TextureManager* texture_manager() const {
    return group_->texture_manager();
  }

  TextureManager* texture_manager() {
    return group_->texture_manager();
  }

  MailboxManager* mailbox_manager() {
    return group_->mailbox_manager();
  }

  ImageManager* image_manager() {
    return group_->image_manager();
  }

  VertexArrayManager* vertex_array_manager() {
    return vertex_array_manager_.get();
  }

  MemoryTracker* memory_tracker() {
    return group_->memory_tracker();
  }

  bool EnsureGPUMemoryAvailable(size_t estimated_size) {
    MemoryTracker* tracker = memory_tracker();
    if (tracker) {
      return tracker->EnsureGPUMemoryAvailable(estimated_size);
    }
    return true;
  }

  bool IsOffscreenBufferMultisampled() const {
    return offscreen_target_samples_ > 1;
  }

  // Creates a Texture for the given texture.
  Texture* CreateTexture(
      GLuint client_id, GLuint service_id) {
    return texture_manager()->CreateTexture(client_id, service_id);
  }

  // Gets the texture info for the given texture. Returns NULL if none exists.
  Texture* GetTexture(GLuint client_id) const {
    Texture* info =
        texture_manager()->GetTexture(client_id);
    return info;
  }

  // Deletes the texture info for the given texture.
  void RemoveTexture(GLuint client_id) {
    texture_manager()->RemoveTexture(client_id);
  }

  // Get the size (in pixels) of the currently bound frame buffer (either FBO
  // or regular back buffer).
  gfx::Size GetBoundReadFrameBufferSize();

  // Get the format of the currently bound frame buffer (either FBO or regular
  // back buffer)
  GLenum GetBoundReadFrameBufferInternalFormat();
  GLenum GetBoundDrawFrameBufferInternalFormat();

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

  // Validation for TexImage2D commands.
  bool ValidateTexImage2D(
      const char* function_name,
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

  // Wrapper for TexImage2D commands.
  void DoTexImage2D(
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

  // Validation for TexSubImage2D.
  bool ValidateTexSubImage2D(
      error::Error* error,
      const char* function_name,
      GLenum target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLsizei width,
      GLsizei height,
      GLenum format,
      GLenum type,
      const void * data);

  // Wrapper for TexSubImage2D.
  error::Error DoTexSubImage2D(
      GLenum target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLsizei width,
      GLsizei height,
      GLenum format,
      GLenum type,
      const void * data);

  // Extra validation for async tex(Sub)Image2D.
  bool ValidateAsyncTransfer(
      const char* function_name,
      Texture* info,
      GLenum target,
      GLint level,
      const void * data);

  // Wrapper for TexImageIOSurface2DCHROMIUM.
  void DoTexImageIOSurface2DCHROMIUM(
      GLenum target,
      GLsizei width,
      GLsizei height,
      GLuint io_surface_id,
      GLuint plane);

  void DoCopyTextureCHROMIUM(
      GLenum target,
      GLuint source_id,
      GLuint target_id,
      GLint level,
      GLenum internal_format);

  // Wrapper for TexStorage2DEXT.
  void DoTexStorage2DEXT(
      GLenum target,
      GLint levels,
      GLenum internal_format,
      GLsizei width,
      GLsizei height);

  void DoProduceTextureCHROMIUM(GLenum target, const GLbyte* key);
  void DoConsumeTextureCHROMIUM(GLenum target, const GLbyte* key);

  void DoBindTexImage2DCHROMIUM(
      GLenum target,
      GLint image_id);
  void DoReleaseTexImage2DCHROMIUM(
      GLenum target,
      GLint image_id);

  void DoTraceEndCHROMIUM(void);

  // Creates a Program for the given program.
  Program* CreateProgram(
      GLuint client_id, GLuint service_id) {
    return program_manager()->CreateProgram(client_id, service_id);
  }

  // Gets the program info for the given program. Returns NULL if none exists.
  Program* GetProgram(GLuint client_id) {
    return program_manager()->GetProgram(client_id);
  }

#if defined(NDEBUG)
  void LogClientServiceMapping(
      const char* /* function_name */,
      GLuint /* client_id */,
      GLuint /* service_id */) {
  }
  template<typename T>
  void LogClientServiceForInfo(
      T* /* info */, GLuint /* client_id */, const char* /* function_name */) {
  }
#else
  void LogClientServiceMapping(
      const char* function_name, GLuint client_id, GLuint service_id) {
    if (service_logging_) {
      DLOG(INFO) << "[" << GetLogPrefix() << "] " << function_name
                 << ": client_id = " << client_id
                 << ", service_id = " << service_id;
    }
  }
  template<typename T>
  void LogClientServiceForInfo(
      T* info, GLuint client_id, const char* function_name) {
    if (info) {
      LogClientServiceMapping(function_name, client_id, info->service_id());
    }
  }
#endif

  // Gets the program info for the given program. If it's not a program
  // generates a GL error. Returns NULL if not program.
  Program* GetProgramInfoNotShader(
      GLuint client_id, const char* function_name) {
    Program* info = GetProgram(client_id);
    if (!info) {
      if (GetShader(client_id)) {
        SetGLError(
            GL_INVALID_OPERATION, function_name, "shader passed for program");
      } else {
        SetGLError(GL_INVALID_VALUE, function_name, "unknown program");
      }
    }
    LogClientServiceForInfo(info, client_id, function_name);
    return info;
  }


  // Creates a Shader for the given shader.
  Shader* CreateShader(
      GLuint client_id,
      GLuint service_id,
      GLenum shader_type) {
    return shader_manager()->CreateShader(
        client_id, service_id, shader_type);
  }

  // Gets the shader info for the given shader. Returns NULL if none exists.
  Shader* GetShader(GLuint client_id) {
    return shader_manager()->GetShader(client_id);
  }

  // Gets the shader info for the given shader. If it's not a shader generates a
  // GL error. Returns NULL if not shader.
  Shader* GetShaderInfoNotProgram(
      GLuint client_id, const char* function_name) {
    Shader* info = GetShader(client_id);
    if (!info) {
      if (GetProgram(client_id)) {
        SetGLError(
            GL_INVALID_OPERATION, function_name, "program passed for shader");
      } else {
        SetGLError(
            GL_INVALID_VALUE, function_name, "unknown shader");
      }
    }
    LogClientServiceForInfo(info, client_id, function_name);
    return info;
  }

  // Creates a buffer info for the given buffer.
  void CreateBuffer(GLuint client_id, GLuint service_id) {
    return buffer_manager()->CreateBuffer(client_id, service_id);
  }

  // Gets the buffer info for the given buffer.
  Buffer* GetBuffer(GLuint client_id) {
    Buffer* info =
        buffer_manager()->GetBuffer(client_id);
    return info;
  }

  // Removes any buffers in the VertexAtrribInfos and BufferInfos. This is used
  // on glDeleteBuffers so we can make sure the user does not try to render
  // with deleted buffers.
  void RemoveBuffer(GLuint client_id);

  // Creates a framebuffer info for the given framebuffer.
  void CreateFramebuffer(GLuint client_id, GLuint service_id) {
    return framebuffer_manager()->CreateFramebuffer(client_id, service_id);
  }

  // Gets the framebuffer info for the given framebuffer.
  Framebuffer* GetFramebuffer(
      GLuint client_id) {
    Framebuffer* info =
        framebuffer_manager()->GetFramebuffer(client_id);
    return info;
  }

  // Removes the framebuffer info for the given framebuffer.
  void RemoveFramebuffer(GLuint client_id) {
    framebuffer_manager()->RemoveFramebuffer(client_id);
  }

  // Creates a renderbuffer info for the given renderbuffer.
  void CreateRenderbuffer(GLuint client_id, GLuint service_id) {
    return renderbuffer_manager()->CreateRenderbuffer(
        client_id, service_id);
  }

  // Gets the renderbuffer info for the given renderbuffer.
  Renderbuffer* GetRenderbuffer(
      GLuint client_id) {
    Renderbuffer* info =
        renderbuffer_manager()->GetRenderbuffer(client_id);
    return info;
  }

  // Removes the renderbuffer info for the given renderbuffer.
  void RemoveRenderbuffer(GLuint client_id) {
    renderbuffer_manager()->RemoveRenderbuffer(client_id);
  }

  // Gets the vertex attrib manager for the given vertex array.
  VertexAttribManager* GetVertexAttribManager(GLuint client_id) {
    VertexAttribManager* info =
        vertex_array_manager()->GetVertexAttribManager(client_id);
    return info;
  }

  // Removes the vertex attrib manager for the given vertex array.
  void RemoveVertexAttribManager(GLuint client_id) {
    vertex_array_manager()->RemoveVertexAttribManager(client_id);
  }

  // Creates a vertex attrib manager for the given vertex array.
  void CreateVertexAttribManager(GLuint client_id, GLuint service_id) {
    return vertex_array_manager()->CreateVertexAttribManager(
      client_id, service_id, group_->max_vertex_attribs());
  }

  void DoBindAttribLocation(GLuint client_id, GLuint index, const char* name);
  void DoBindUniformLocationCHROMIUM(
      GLuint client_id, GLint location, const char* name);

  error::Error GetAttribLocationHelper(
    GLuint client_id, uint32 location_shm_id, uint32 location_shm_offset,
    const std::string& name_str);

  error::Error GetUniformLocationHelper(
    GLuint client_id, uint32 location_shm_id, uint32 location_shm_offset,
    const std::string& name_str);

  // Helper for glShaderSource.
  error::Error ShaderSourceHelper(
      GLuint client_id, const char* data, uint32 data_size);

  // Clear any textures used by the current program.
  bool ClearUnclearedTextures();

  // Clear any uncleared level in texture.
  // Returns false if there was a generated GL error.
  bool ClearTexture(Texture* info);

  // Clears any uncleared attachments attached to the given frame buffer.
  // Returns false if there was a generated GL error.
  void ClearUnclearedAttachments(
      GLenum target, Framebuffer* info);

  // overridden from GLES2Decoder
  virtual bool ClearLevel(unsigned service_id,
                          unsigned bind_target,
                          unsigned target,
                          int level,
                          unsigned format,
                          unsigned type,
                          int width,
                          int height,
                          bool is_texture_immutable) OVERRIDE;

  // Restore all GL state that affects clearing.
  void RestoreClearState();

  // Remembers the state of some capabilities.
  // Returns: true if glEnable/glDisable should actually be called.
  bool SetCapabilityState(GLenum cap, bool enabled);

  // Check that the currently bound framebuffers are valid.
  // Generates GL error if not.
  bool CheckBoundFramebuffersValid(const char* func_name);

  // Check if a framebuffer meets our requirements.
  bool CheckFramebufferValid(
      Framebuffer* framebuffer,
      GLenum target,
      const char* func_name);

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
  // program is valid and the location exists. Adjusts count so it
  // does not overflow the uniform.
  bool PrepForSetUniformByLocation(
      GLint fake_location, const char* function_name,
      const BaseUniformInfo& base_info,
      GLint* real_location, GLenum* type, GLsizei* count);

  // Gets the service id for any simulated backbuffer fbo.
  GLuint GetBackbufferServiceId() const;

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

  // Wrapper for glBindVertexArrayOES
  void DoBindVertexArrayOES(GLuint array);
  void EmulateVertexArrayState();

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

  // Wrapper for glClear
  error::Error DoClear(GLbitfield mask);

  // Wrappers for various state.
  void DoDepthRangef(GLclampf znear, GLclampf zfar);
  void DoHint(GLenum target, GLenum mode);
  void DoSampleCoverage(GLclampf value, GLboolean invert);

  // Wrapper for glCompileShader.
  void DoCompileShader(GLuint shader);

  // Helper for DeleteSharedIdsCHROMIUM commands.
  void DoDeleteSharedIdsCHROMIUM(
      GLuint namespace_id, GLsizei n, const GLuint* ids);

  // Wrapper for glDetachShader
  void DoDetachShader(GLuint client_program_id, GLint client_shader_id);

  // Wrapper for glDisable
  void DoDisable(GLenum cap);

  // Wrapper for glDisableVertexAttribArray.
  void DoDisableVertexAttribArray(GLuint index);

  // Wrapper for glDiscardFramebufferEXT, since we need to track undefined
  // attachments.
  void DoDiscardFramebufferEXT(GLenum target,
                               GLsizei numAttachments,
                               const GLenum* attachments);

  // Wrapper for glEnable
  void DoEnable(GLenum cap);

  // Wrapper for glEnableVertexAttribArray.
  void DoEnableVertexAttribArray(GLuint index);

  // Wrapper for glFinish.
  void DoFinish();

  // Wrapper for glFlush.
  void DoFlush();

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
  bool DoIsEnabled(GLenum cap);
  bool DoIsBuffer(GLuint client_id);
  bool DoIsFramebuffer(GLuint client_id);
  bool DoIsProgram(GLuint client_id);
  bool DoIsRenderbuffer(GLuint client_id);
  bool DoIsShader(GLuint client_id);
  bool DoIsTexture(GLuint client_id);
  bool DoIsVertexArrayOES(GLuint client_id);

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
  void DoUniform1i(GLint fake_location, GLint v0);
  void DoUniform1iv(GLint fake_location, GLsizei count, const GLint* value);
  void DoUniform2iv(GLint fake_location, GLsizei count, const GLint* value);
  void DoUniform3iv(GLint fake_location, GLsizei count, const GLint* value);
  void DoUniform4iv(GLint fake_location, GLsizei count, const GLint* value);

  // Wrappers for glUniformfv because some drivers don't correctly accept
  // bool uniforms.
  void DoUniform1fv(GLint fake_location, GLsizei count, const GLfloat* value);
  void DoUniform2fv(GLint fake_location, GLsizei count, const GLfloat* value);
  void DoUniform3fv(GLint fake_location, GLsizei count, const GLfloat* value);
  void DoUniform4fv(GLint fake_location, GLsizei count, const GLfloat* value);

  void DoUniformMatrix2fv(
      GLint fake_location, GLsizei count, GLboolean transpose,
      const GLfloat* value);
  void DoUniformMatrix3fv(
      GLint fake_location, GLsizei count, GLboolean transpose,
      const GLfloat* value);
  void DoUniformMatrix4fv(
      GLint fake_location, GLsizei count, GLboolean transpose,
      const GLfloat* value);

  bool SetVertexAttribValue(
    const char* function_name, GLuint index, const GLfloat* value);

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

  // Wrapper for glViewport
  void DoViewport(GLint x, GLint y, GLsizei width, GLsizei height);

  // Wrapper for glUseProgram
  void DoUseProgram(GLuint program);

  // Wrapper for glValidateProgram.
  void DoValidateProgram(GLuint program_client_id);

  void DoInsertEventMarkerEXT(GLsizei length, const GLchar* marker);
  void DoPushGroupMarkerEXT(GLsizei length, const GLchar* group);
  void DoPopGroupMarkerEXT(void);

  // Gets the number of values that will be returned by glGetXXX. Returns
  // false if pname is unknown.
  bool GetNumValuesReturnedForGLGet(GLenum pname, GLsizei* num_values);

  virtual void SetGLError(unsigned error,
                          const char* function_name,
                          const char* msg) OVERRIDE;
  virtual void SetGLErrorInvalidEnum(const char* function_name,
                                     unsigned value,
                                     const char* label) OVERRIDE;
  // Generates a GL error for a bad parameter.
  virtual void SetGLErrorInvalidParam(
      unsigned error,
      const char* function_name,
      unsigned pname,
      int param) OVERRIDE;

  virtual unsigned PeekGLError() OVERRIDE;
  virtual void CopyRealGLErrorsToWrapper() OVERRIDE;
  virtual void ClearRealGLErrors() OVERRIDE;

  // Checks if the current program and vertex attributes are valid for drawing.
  bool IsDrawValid(
      const char* function_name, GLuint max_vertex_accessed, GLsizei primcount);

  // Returns true if successful, simulated will be true if attrib0 was
  // simulated.
  bool SimulateAttrib0(
      const char* function_name, GLuint max_vertex_accessed, bool* simulated);
  void RestoreStateForAttrib(GLuint attrib);

  // Returns true if textures were set.
  bool SetBlackTextureForNonRenderableTextures();
  void RestoreStateForNonRenderableTextures();

  // Returns true if GL_FIXED attribs were simulated.
  bool SimulateFixedAttribs(
      const char* function_name,
      GLuint max_vertex_accessed, bool* simulated, GLsizei primcount);
  void RestoreStateForSimulatedFixedAttribs();

  // Handle DrawArrays and DrawElements for both instanced and non-instanced
  // cases (primcount is 0 for non-instanced).
  error::Error DoDrawArrays(
      const char* function_name,
      bool instanced, GLenum mode, GLint first, GLsizei count,
      GLsizei primcount);
  error::Error DoDrawElements(
      const char* function_name,
      bool instanced, GLenum mode, GLsizei count, GLenum type,
      int32 offset, GLsizei primcount);

  // Gets the buffer id for a given target.
  Buffer* GetBufferInfoForTarget(GLenum target) {
    DCHECK(target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER);
    if (target == GL_ARRAY_BUFFER) {
      return state_.bound_array_buffer;
    } else {
      return state_.vertex_attrib_manager->element_array_buffer();
    }
  }

  // Gets the texture id for a given target.
  Texture* GetTextureInfoForTarget(GLenum target) {
    TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
    Texture* info = NULL;
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
      case GL_TEXTURE_EXTERNAL_OES:
        info = unit.bound_texture_external_oes;
        break;
      case GL_TEXTURE_RECTANGLE_ARB:
        info = unit.bound_texture_rectangle_arb;
        break;
      default:
        NOTREACHED();
        return NULL;
    }
    return info;
  }

  Texture* GetTextureInfoForTargetUnlessDefault(
      GLenum target) {
    Texture* info = GetTextureInfoForTarget(target);
    if (!info)
      return NULL;
    if (info == texture_manager()->GetDefaultTextureInfo(target))
      return NULL;
    return info;
  }

  GLenum GetBindTargetForSamplerType(GLenum type) {
    DCHECK(type == GL_SAMPLER_2D || type == GL_SAMPLER_CUBE ||
           type == GL_SAMPLER_EXTERNAL_OES || type == GL_SAMPLER_2D_RECT_ARB);
    switch (type) {
      case GL_SAMPLER_2D:
        return GL_TEXTURE_2D;
      case GL_SAMPLER_CUBE:
        return GL_TEXTURE_CUBE_MAP;
      case GL_SAMPLER_EXTERNAL_OES:
        return GL_TEXTURE_EXTERNAL_OES;
      case GL_SAMPLER_2D_RECT_ARB:
        return GL_TEXTURE_RECTANGLE_ARB;
    }

    NOTREACHED();
    return 0;
  }

  // Gets the framebuffer info for a particular target.
  Framebuffer* GetFramebufferInfoForTarget(
      GLenum target) {
    Framebuffer* info = NULL;
    switch (target) {
      case GL_FRAMEBUFFER:
      case GL_DRAW_FRAMEBUFFER_EXT:
        info = state_.bound_draw_framebuffer;
        break;
      case GL_READ_FRAMEBUFFER_EXT:
        info = state_.bound_read_framebuffer;
        break;
      default:
        NOTREACHED();
        break;
    }
    return info;
  }

  Renderbuffer* GetRenderbufferInfoForTarget(
      GLenum target) {
    Renderbuffer* info = NULL;
    switch (target) {
      case GL_RENDERBUFFER:
        info = state_.bound_renderbuffer;
        break;
      default:
        NOTREACHED();
        break;
    }
    return info;
  }

  // Validates the program and location for a glGetUniform call and returns
  // a SizeResult setup to receive the result. Returns true if glGetUniform
  // should be called.
  bool GetUniformSetup(
      GLuint program, GLint fake_location,
      uint32 shm_id, uint32 shm_offset,
      error::Error* error, GLint* real_location, GLuint* service_id,
      void** result, GLenum* result_type);

  // Computes the estimated memory used for the backbuffer and passes it to
  // the tracing system.
  size_t GetBackbufferMemoryTotal();

  virtual bool WasContextLost() OVERRIDE;
  virtual void LoseContext(uint32 reset_status) OVERRIDE;

#if defined(OS_MACOSX)
  void ReleaseIOSurfaceForTexture(GLuint texture_id);
#endif

  // Validates the combination of texture parameters. For example validates that
  // for a given format the specific type, level and targets are valid.
  // Synthesizes the correct GL error if invalid. Returns true if valid.
  bool ValidateTextureParameters(
      const char* function_name,
      GLenum target, GLenum format, GLenum type, GLint level);

  bool ValidateCompressedTexDimensions(
      const char* function_name,
      GLint level, GLsizei width, GLsizei height, GLenum format);
  bool ValidateCompressedTexFuncData(
      const char* function_name,
      GLsizei width, GLsizei height, GLenum format, size_t size);
  bool ValidateCompressedTexSubDimensions(
    const char* function_name,
    GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLsizei width, GLsizei height, GLenum format,
    Texture* texture);

  void LogMessage(const std::string& msg);
  void RenderWarning(const std::string& msg);
  void PerformanceWarning(const std::string& msg);
  const std::string& GetLogPrefix() const;

  const FeatureInfo::FeatureFlags& features() const {
    return feature_info_->feature_flags();
  }

  const FeatureInfo::Workarounds& workarounds() const {
    return feature_info_->workarounds();
  }

  bool ShouldDeferDraws() {
    return !offscreen_target_frame_buffer_.get() &&
           state_.bound_draw_framebuffer == NULL &&
           surface_->DeferDraws();
  }

  bool ShouldDeferReads() {
    return !offscreen_target_frame_buffer_.get() &&
           state_.bound_read_framebuffer == NULL &&
           surface_->DeferDraws();
  }

  void ForceCompileShaderIfPending(Shader* info);

  // Generate a member function prototype for each command in an automated and
  // typesafe way.
  #define GLES2_CMD_OP(name) \
     Error Handle ## name(             \
       uint32 immediate_data_size,     \
       const cmds::name& args);        \

  GLES2_COMMAND_LIST(GLES2_CMD_OP)

  #undef GLES2_CMD_OP

  // The GL context this decoder renders to on behalf of the client.
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_refptr<gfx::GLContext> context_;

  // The ContextGroup for this decoder uses to track resources.
  scoped_refptr<ContextGroup> group_;

  // All the state for this context.
  ContextState state_;

  // A parent decoder can access this decoders saved offscreen frame buffer.
  // The parent pointer is reset if the parent is destroyed.
  base::WeakPtr<GLES2DecoderImpl> parent_;

  // Current width and height of the offscreen frame buffer.
  gfx::Size offscreen_size_;

  // Current GL error bits.
  uint32 error_bits_;

  // Util to help with GL.
  GLES2Util util_;

  // unpack flip y as last set by glPixelStorei
  bool unpack_flip_y_;

  // unpack (un)premultiply alpha as last set by glPixelStorei
  bool unpack_premultiply_alpha_;
  bool unpack_unpremultiply_alpha_;

  // Default vertex attribs manager, used when no VAOs are bound.
  scoped_refptr<VertexAttribManager> default_vertex_attrib_manager_;

  // The buffer we bind to attrib 0 since OpenGL requires it (ES does not).
  GLuint attrib_0_buffer_id_;

  // The value currently in attrib_0.
  Vec4 attrib_0_value_;

  // Whether or not the attrib_0 buffer holds the attrib_0_value.
  bool attrib_0_buffer_matches_value_;

  // The size of attrib 0.
  GLsizei attrib_0_size_;

  // The buffer used to simulate GL_FIXED attribs.
  GLuint fixed_attrib_buffer_id_;

  // The size of fiixed attrib buffer.
  GLsizei fixed_attrib_buffer_size_;

  // state saved for clearing so we can clear render buffers and then
  // restore to these values.
  bool clear_state_dirty_;

  // The offscreen frame buffer that the client renders to. With EGL, the
  // depth and stencil buffers are separate. With regular GL there is a single
  // packed depth stencil buffer in offscreen_target_depth_render_buffer_.
  // offscreen_target_stencil_render_buffer_ is unused.
  scoped_ptr<BackFramebuffer> offscreen_target_frame_buffer_;
  scoped_ptr<BackTexture> offscreen_target_color_texture_;
  scoped_ptr<BackRenderbuffer> offscreen_target_color_render_buffer_;
  scoped_ptr<BackRenderbuffer> offscreen_target_depth_render_buffer_;
  scoped_ptr<BackRenderbuffer> offscreen_target_stencil_render_buffer_;
  GLenum offscreen_target_color_format_;
  GLenum offscreen_target_depth_format_;
  GLenum offscreen_target_stencil_format_;
  GLsizei offscreen_target_samples_;
  GLboolean offscreen_target_buffer_preserved_;

  // The copy that is saved when SwapBuffers is called.
  scoped_ptr<BackFramebuffer> offscreen_saved_frame_buffer_;
  scoped_ptr<BackTexture> offscreen_saved_color_texture_;
  scoped_refptr<Texture>
      offscreen_saved_color_texture_info_;

  // The copy that is used as the destination for multi-sample resolves.
  scoped_ptr<BackFramebuffer> offscreen_resolved_frame_buffer_;
  scoped_ptr<BackTexture> offscreen_resolved_color_texture_;
  GLenum offscreen_saved_color_format_;

  scoped_ptr<QueryManager> query_manager_;

  scoped_ptr<VertexArrayManager> vertex_array_manager_;

  base::Callback<void(gfx::Size)> resize_callback_;

  MsgCallback msg_callback_;
  WaitSyncPointCallback wait_sync_point_callback_;

  StreamTextureManager* stream_texture_manager_;
  scoped_ptr<gfx::AsyncPixelTransferDelegate> async_pixel_transfer_delegate_;

  // The format of the back buffer_
  GLenum back_buffer_color_format_;
  bool back_buffer_has_depth_;
  bool back_buffer_has_stencil_;

  // Backbuffer attachments that are currently undefined.
  uint32 backbuffer_needs_clear_bits_;

  bool teximage2d_faster_than_texsubimage2d_;

  // The last error message set.
  std::string last_error_;

  int log_message_count_;

  // The current decoder error.
  error::Error current_decoder_error_;

  DebugMarkerManager debug_marker_manager_;
  std::string this_in_hex_;

  bool use_shader_translator_;
  scoped_refptr<ShaderTranslator> vertex_translator_;
  scoped_refptr<ShaderTranslator> fragment_translator_;

  DisallowedFeatures disallowed_features_;

  // Cached from ContextGroup
  const Validators* validators_;
  scoped_refptr<FeatureInfo> feature_info_;

  // This indicates all the following texSubImage2D calls that are part of the
  // failed texImage2D call should be ignored.
  bool tex_image_2d_failed_;

  int frame_number_;

  bool has_robustness_extension_;
  GLenum reset_status_;

  // These flags are used to override the state of the shared feature_info_
  // member.  Because the same FeatureInfo instance may be shared among many
  // contexts, the assumptions on the availablity of extensions in WebGL
  // contexts may be broken.  These flags override the shared state to preserve
  // WebGL semantics.
  bool force_webgl_glsl_validation_;
  bool derivatives_explicitly_enabled_;

  bool compile_shader_always_succeeds_;

  // Log extra info.
  bool service_logging_;

#if defined(OS_MACOSX)
  typedef std::map<GLuint, CFTypeRef> TextureToIOSurfaceMap;
  TextureToIOSurfaceMap texture_to_io_surface_map_;
#endif

  typedef std::vector<GLES2DecoderImpl*> ChildList;
  ChildList children_;

  scoped_ptr<CopyTextureCHROMIUMResourceManager> copy_texture_CHROMIUM_;

  // Cached values of the currently assigned viewport dimensions.
  GLsizei viewport_max_width_;
  GLsizei viewport_max_height_;

  // Command buffer stats.
  int texture_upload_count_;
  base::TimeDelta total_texture_upload_time_;
  base::TimeDelta total_processing_commands_time_;

  scoped_ptr<GPUTracer> gpu_tracer_;

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
  decoder->OnFboChanged();
}

ScopedFrameBufferBinder::~ScopedFrameBufferBinder() {
  ScopedGLErrorSuppressor suppressor(decoder_);
  decoder_->RestoreCurrentFramebufferBindings();
}

ScopedResolvedFrameBufferBinder::ScopedResolvedFrameBufferBinder(
    GLES2DecoderImpl* decoder, bool enforce_internal_framebuffer, bool internal)
    : decoder_(decoder) {
  resolve_and_bind_ = (decoder_->offscreen_target_frame_buffer_.get() &&
                       decoder_->IsOffscreenBufferMultisampled() &&
                       (!decoder_->state_.bound_read_framebuffer.get() ||
                        enforce_internal_framebuffer));
  if (!resolve_and_bind_)
    return;

  ScopedGLErrorSuppressor suppressor(decoder_);
  glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,
                       decoder_->offscreen_target_frame_buffer_->id());
  GLuint targetid;
  if (internal) {
    if (!decoder_->offscreen_resolved_frame_buffer_.get()) {
      decoder_->offscreen_resolved_frame_buffer_.reset(
          new BackFramebuffer(decoder_));
      decoder_->offscreen_resolved_frame_buffer_->Create();
      decoder_->offscreen_resolved_color_texture_.reset(
          new BackTexture(decoder_));
      decoder_->offscreen_resolved_color_texture_->Create();

      DCHECK(decoder_->offscreen_saved_color_format_);
      decoder_->offscreen_resolved_color_texture_->AllocateStorage(
          decoder_->offscreen_size_, decoder_->offscreen_saved_color_format_,
          false);
      decoder_->offscreen_resolved_frame_buffer_->AttachRenderTexture(
          decoder_->offscreen_resolved_color_texture_.get());
      if (decoder_->offscreen_resolved_frame_buffer_->CheckStatus() !=
          GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR) << "ScopedResolvedFrameBufferBinder failed "
                   << "because offscreen resolved FBO was incomplete.";
        return;
      }
    }
    targetid = decoder_->offscreen_resolved_frame_buffer_->id();
  } else {
    targetid = decoder_->offscreen_saved_frame_buffer_->id();
  }
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, targetid);
  const int width = decoder_->offscreen_size_.width();
  const int height = decoder_->offscreen_size_.height();
  glDisable(GL_SCISSOR_TEST);
  if (GLES2Decoder::IsAngle()) {
    glBlitFramebufferANGLE(0, 0, width, height, 0, 0, width, height,
                           GL_COLOR_BUFFER_BIT, GL_NEAREST);
  } else {
    glBlitFramebufferEXT(0, 0, width, height, 0, 0, width, height,
                         GL_COLOR_BUFFER_BIT, GL_NEAREST);
  }
  glBindFramebufferEXT(GL_FRAMEBUFFER, targetid);
}

ScopedResolvedFrameBufferBinder::~ScopedResolvedFrameBufferBinder() {
  if (!resolve_and_bind_)
    return;

  ScopedGLErrorSuppressor suppressor(decoder_);
  decoder_->RestoreCurrentFramebufferBindings();
  if (decoder_->state_.enable_flags.scissor_test) {
    glEnable(GL_SCISSOR_TEST);
  }
}

ScopedTextureUploadTimer::ScopedTextureUploadTimer(GLES2DecoderImpl* decoder)
    : decoder_(decoder),
      begin_time_(base::TimeTicks::HighResNow()) {
}

ScopedTextureUploadTimer::~ScopedTextureUploadTimer() {
  decoder_->texture_upload_count_++;
  decoder_->total_texture_upload_time_ +=
      base::TimeTicks::HighResNow() - begin_time_;
}

BackTexture::BackTexture(GLES2DecoderImpl* decoder)
    : decoder_(decoder),
      memory_tracker_(decoder->memory_tracker(), MemoryTracker::kUnmanaged),
      bytes_allocated_(0),
      id_(0) {
}

BackTexture::~BackTexture() {
  // This does not destroy the render texture because that would require that
  // the associated GL context was current. Just check that it was explicitly
  // destroyed.
  DCHECK_EQ(id_, 0u);
}

void BackTexture::Create() {
  ScopedGLErrorSuppressor suppressor(decoder_);
  Destroy();
  glGenTextures(1, &id_);
  ScopedTexture2DBinder binder(decoder_, id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // TODO(apatrick): Attempt to diagnose crbug.com/97775. If SwapBuffers is
  // never called on an offscreen context, no data will ever be uploaded to the
  // saved offscreen color texture (it is deferred until to when SwapBuffers
  // is called). My idea is that some nvidia drivers might have a bug where
  // deleting a texture that has never been populated might cause a
  // crash.
  glTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  bytes_allocated_ = 16u * 16u * 4u;
  memory_tracker_.TrackMemAlloc(bytes_allocated_);
}

bool BackTexture::AllocateStorage(
    const gfx::Size& size, GLenum format, bool zero) {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedTexture2DBinder binder(decoder_, id_);
  uint32 image_size = 0;
  GLES2Util::ComputeImageDataSizes(
      size.width(), size.height(), format, GL_UNSIGNED_BYTE, 8, &image_size,
      NULL, NULL);

  if (!memory_tracker_.EnsureGPUMemoryAvailable(image_size)) {
    return false;
  }

  scoped_array<char> zero_data;
  if (zero) {
    zero_data.reset(new char[image_size]);
    memset(zero_data.get(), 0, image_size);
  }

  WrappedTexImage2D(GL_TEXTURE_2D,
                    0,  // mip level
                    format,
                    size.width(),
                    size.height(),
                    0,  // border
                    format,
                    GL_UNSIGNED_BYTE,
                    zero_data.get());

  size_ = size;

  bool success = glGetError() == GL_NO_ERROR;
  if (success) {
    memory_tracker_.TrackMemFree(bytes_allocated_);
    bytes_allocated_ = image_size;
    memory_tracker_.TrackMemAlloc(bytes_allocated_);
  }
  return success;
}

void BackTexture::Copy(const gfx::Size& size, GLenum format) {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedTexture2DBinder binder(decoder_, id_);
  glCopyTexImage2D(GL_TEXTURE_2D,
                   0,  // level
                   format,
                   0, 0,
                   size.width(),
                   size.height(),
                   0);  // border
}

void BackTexture::Destroy() {
  if (id_ != 0) {
    ScopedGLErrorSuppressor suppressor(decoder_);
    glDeleteTextures(1, &id_);
    id_ = 0;
  }
  memory_tracker_.TrackMemFree(bytes_allocated_);
  bytes_allocated_ = 0;
}

void BackTexture::Invalidate() {
  id_ = 0;
}

BackRenderbuffer::BackRenderbuffer(GLES2DecoderImpl* decoder)
    : decoder_(decoder),
      memory_tracker_(decoder->memory_tracker(), MemoryTracker::kUnmanaged),
      bytes_allocated_(0),
      id_(0) {
}

BackRenderbuffer::~BackRenderbuffer() {
  // This does not destroy the render buffer because that would require that
  // the associated GL context was current. Just check that it was explicitly
  // destroyed.
  DCHECK_EQ(id_, 0u);
}

void BackRenderbuffer::Create() {
  ScopedGLErrorSuppressor suppressor(decoder_);
  Destroy();
  glGenRenderbuffersEXT(1, &id_);
}

bool BackRenderbuffer::AllocateStorage(const gfx::Size& size, GLenum format,
                                       GLsizei samples) {
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedRenderBufferBinder binder(decoder_, id_);

  uint32 estimated_size = 0;
  if (!RenderbufferManager::ComputeEstimatedRenderbufferSize(
      size.width(), size.height(), samples, format, &estimated_size)) {
    return false;
  }

  if (!memory_tracker_.EnsureGPUMemoryAvailable(estimated_size)) {
    return false;
  }

  if (samples <= 1) {
    glRenderbufferStorageEXT(GL_RENDERBUFFER,
                             format,
                             size.width(),
                             size.height());
  } else {
    if (GLES2Decoder::IsAngle()) {
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
  bool success = glGetError() == GL_NO_ERROR;
  if (success) {
    memory_tracker_.TrackMemFree(bytes_allocated_);
    bytes_allocated_ = estimated_size;
    memory_tracker_.TrackMemAlloc(bytes_allocated_);
  }
  return success;
}

void BackRenderbuffer::Destroy() {
  if (id_ != 0) {
    ScopedGLErrorSuppressor suppressor(decoder_);
    glDeleteRenderbuffersEXT(1, &id_);
    id_ = 0;
  }
  memory_tracker_.TrackMemFree(bytes_allocated_);
  bytes_allocated_ = 0;
}

void BackRenderbuffer::Invalidate() {
  id_ = 0;
}

BackFramebuffer::BackFramebuffer(GLES2DecoderImpl* decoder)
    : decoder_(decoder),
      id_(0) {
}

BackFramebuffer::~BackFramebuffer() {
  // This does not destroy the frame buffer because that would require that
  // the associated GL context was current. Just check that it was explicitly
  // destroyed.
  DCHECK_EQ(id_, 0u);
}

void BackFramebuffer::Create() {
  ScopedGLErrorSuppressor suppressor(decoder_);
  Destroy();
  glGenFramebuffersEXT(1, &id_);
}

void BackFramebuffer::AttachRenderTexture(BackTexture* texture) {
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

void BackFramebuffer::AttachRenderBuffer(GLenum target,
                                         BackRenderbuffer* render_buffer) {
  DCHECK_NE(id_, 0u);
  ScopedGLErrorSuppressor suppressor(decoder_);
  ScopedFrameBufferBinder binder(decoder_, id_);
  GLuint attach_id = render_buffer ? render_buffer->id() : 0;
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER,
                               target,
                               GL_RENDERBUFFER,
                               attach_id);
}

void BackFramebuffer::Destroy() {
  if (id_ != 0) {
    ScopedGLErrorSuppressor suppressor(decoder_);
    glDeleteFramebuffersEXT(1, &id_);
    id_ = 0;
  }
}

void BackFramebuffer::Invalidate() {
  id_ = 0;
}

GLenum BackFramebuffer::CheckStatus() {
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
      group_(group),
      state_(group_->feature_info()),
      error_bits_(0),
      unpack_flip_y_(false),
      unpack_premultiply_alpha_(false),
      unpack_unpremultiply_alpha_(false),
      attrib_0_buffer_id_(0),
      attrib_0_buffer_matches_value_(true),
      attrib_0_size_(0),
      fixed_attrib_buffer_id_(0),
      fixed_attrib_buffer_size_(0),
      clear_state_dirty_(true),
      offscreen_target_color_format_(0),
      offscreen_target_depth_format_(0),
      offscreen_target_stencil_format_(0),
      offscreen_target_samples_(0),
      offscreen_target_buffer_preserved_(true),
      offscreen_saved_color_format_(0),
      stream_texture_manager_(NULL),
      back_buffer_color_format_(0),
      back_buffer_has_depth_(false),
      back_buffer_has_stencil_(false),
      backbuffer_needs_clear_bits_(0),
      teximage2d_faster_than_texsubimage2d_(true),
      log_message_count_(0),
      current_decoder_error_(error::kNoError),
      use_shader_translator_(true),
      validators_(group_->feature_info()->validators()),
      feature_info_(group_->feature_info()),
      tex_image_2d_failed_(false),
      frame_number_(0),
      has_robustness_extension_(false),
      reset_status_(GL_NO_ERROR),
      force_webgl_glsl_validation_(false),
      derivatives_explicitly_enabled_(false),
      compile_shader_always_succeeds_(false),
      service_logging_(CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGPUServiceLoggingGPU)),
      viewport_max_width_(0),
      viewport_max_height_(0),
      texture_upload_count_(0) {
  DCHECK(group);

  GLES2DecoderImpl* this_temp = this;
  this_in_hex_ = HexEncode(&this_temp, sizeof(this_temp));

  attrib_0_value_.v[0] = 0.0f;
  attrib_0_value_.v[1] = 0.0f;
  attrib_0_value_.v[2] = 0.0f;
  attrib_0_value_.v[3] = 1.0f;

  // The shader translator is used for WebGL even when running on EGL
  // because additional restrictions are needed (like only enabling
  // GL_OES_standard_derivatives on demand).  It is used for the unit
  // tests because GLES2DecoderWithShaderTest.GetShaderInfoLogValidArgs passes
  // the empty string to CompileShader and this is not a valid shader.
  if (gfx::GetGLImplementation() == gfx::kGLImplementationMockGL ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGLSLTranslator)) {
    use_shader_translator_ = false;
  }

  // TODO(gman): Consider setting this based on GPU and/or driver.
  if (IsAngle()) {
    teximage2d_faster_than_texsubimage2d_ = false;
  }
}

GLES2DecoderImpl::~GLES2DecoderImpl() {
}

bool GLES2DecoderImpl::Initialize(
    const scoped_refptr<gfx::GLSurface>& surface,
    const scoped_refptr<gfx::GLContext>& context,
    bool offscreen,
    const gfx::Size& size,
    const DisallowedFeatures& disallowed_features,
    const char* allowed_extensions,
    const std::vector<int32>& attribs) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::Initialize");
  DCHECK(context->IsCurrent(surface.get()));
  DCHECK(!context_.get());

  gpu_tracer_ = GPUTracer::Create();

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableGPUDebugging)) {
    set_debug(true);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableGPUCommandLogging)) {
    set_log_commands(true);
  }

  compile_shader_always_succeeds_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kCompileShaderAlwaysSucceeds);


  // Take ownership of the context and surface. The surface can be replaced with
  // SetSurface.
  context_ = context;
  surface_ = surface;

  if (!group_->Initialize(this, disallowed_features, allowed_extensions)) {
    LOG(ERROR) << "GpuScheduler::InitializeCommon failed because group "
               << "failed to initialize.";
    group_ = NULL;  // Must not destroy ContextGroup if it is not initialized.
    Destroy(true);
    return false;
  }
  CHECK_GL_ERROR();

  disallowed_features_ = disallowed_features;

  state_.attrib_values.resize(group_->max_vertex_attribs());
  default_vertex_attrib_manager_ = new VertexAttribManager();
  default_vertex_attrib_manager_->Initialize(group_->max_vertex_attribs());

  // vertex_attrib_manager is set to default_vertex_attrib_manager_ by this call
  DoBindVertexArrayOES(0);

  query_manager_.reset(new QueryManager(this, feature_info_));
  vertex_array_manager_.reset(new VertexArrayManager());

  util_.set_num_compressed_texture_formats(
      validators_->compressed_texture_format.GetValues().size());

  if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2) {
    // We have to enable vertex array 0 on OpenGL or it won't render. Note that
    // OpenGL ES 2.0 does not have this issue.
    glEnableVertexAttribArray(0);
  }
  glGenBuffersARB(1, &attrib_0_buffer_id_);
  glBindBuffer(GL_ARRAY_BUFFER, attrib_0_buffer_id_);
  glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, NULL);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glGenBuffersARB(1, &fixed_attrib_buffer_id_);

  state_.texture_units.resize(group_->max_texture_units());
  for (uint32 tt = 0; tt < state_.texture_units.size(); ++tt) {
    glActiveTexture(GL_TEXTURE0 + tt);
    // We want the last bind to be 2D.
    Texture* info;
    if (features().oes_egl_image_external) {
      info = texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_EXTERNAL_OES);
      state_.texture_units[tt].bound_texture_external_oes = info;
      glBindTexture(GL_TEXTURE_EXTERNAL_OES, info->service_id());
    }
    if (features().arb_texture_rectangle) {
      info = texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_RECTANGLE_ARB);
      state_.texture_units[tt].bound_texture_rectangle_arb = info;
      glBindTexture(GL_TEXTURE_RECTANGLE_ARB, info->service_id());
    }
    info = texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_CUBE_MAP);
    state_.texture_units[tt].bound_texture_cube_map = info;
    glBindTexture(GL_TEXTURE_CUBE_MAP, info->service_id());
    info = texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_2D);
    state_.texture_units[tt].bound_texture_2d = info;
    glBindTexture(GL_TEXTURE_2D, info->service_id());
  }
  glActiveTexture(GL_TEXTURE0);
  CHECK_GL_ERROR();

  ContextCreationAttribParser attrib_parser;
  if (!attrib_parser.Parse(attribs))
    return false;

  // These are NOT if the back buffer has these proprorties. They are
  // if we want the command buffer to enforce them regardless of what
  // the real backbuffer is assuming the real back buffer gives us more than
  // we ask for. In other words, if we ask for RGB and we get RGBA then we'll
  // make it appear RGB. If on the other hand we ask for RGBA nd get RGB we
  // can't do anything about that.

  GLint v = 0;
  glGetIntegerv(GL_ALPHA_BITS, &v);
  // This checks if the user requested RGBA and we have RGBA then RGBA. If the
  // user requested RGB then RGB. If the user did not specify a preference than
  // use whatever we were given. Same for DEPTH and STENCIL.
  back_buffer_color_format_ =
      (attrib_parser.alpha_size_ != 0 && v > 0) ? GL_RGBA : GL_RGB;
  glGetIntegerv(GL_DEPTH_BITS, &v);
  back_buffer_has_depth_ = attrib_parser.depth_size_ != 0 && v > 0;
  glGetIntegerv(GL_STENCIL_BITS, &v);
  back_buffer_has_stencil_ = attrib_parser.stencil_size_ != 0 && v > 0;

  if (offscreen) {
    if (attrib_parser.samples_ > 0 && attrib_parser.sample_buffers_ > 0 &&
        features().chromium_framebuffer_multisample) {
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
    offscreen_target_buffer_preserved_ = attrib_parser.buffer_preserved_;

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
    offscreen_target_frame_buffer_.reset(new BackFramebuffer(this));
    offscreen_target_frame_buffer_->Create();
    // Due to GLES2 format limitations, either the color texture (for
    // non-multisampling) or the color render buffer (for multisampling) will be
    // attached to the offscreen frame buffer.  The render buffer has more
    // limited formats available to it, but the texture can't do multisampling.
    if (IsOffscreenBufferMultisampled()) {
      offscreen_target_color_render_buffer_.reset(new BackRenderbuffer(this));
      offscreen_target_color_render_buffer_->Create();
    } else {
      offscreen_target_color_texture_.reset(new BackTexture(this));
      offscreen_target_color_texture_->Create();
    }
    offscreen_target_depth_render_buffer_.reset(new BackRenderbuffer(this));
    offscreen_target_depth_render_buffer_->Create();
    offscreen_target_stencil_render_buffer_.reset(new BackRenderbuffer(this));
    offscreen_target_stencil_render_buffer_->Create();

    // Create the saved offscreen texture. The target frame buffer is copied
    // here when SwapBuffers is called.
    offscreen_saved_frame_buffer_.reset(new BackFramebuffer(this));
    offscreen_saved_frame_buffer_->Create();
    //
    offscreen_saved_color_texture_.reset(new BackTexture(this));
    offscreen_saved_color_texture_->Create();

    // Allocate the render buffers at their initial size and check the status
    // of the frame buffers is okay.
    if (!ResizeOffscreenFrameBuffer(size)) {
      LOG(ERROR) << "Could not allocate offscreen buffer storage.";
      Destroy(true);
      return false;
    }

    // Allocate the offscreen saved color texture.
    DCHECK(offscreen_saved_color_format_);
    offscreen_saved_color_texture_->AllocateStorage(
        gfx::Size(1, 1), offscreen_saved_color_format_, true);

    offscreen_saved_frame_buffer_->AttachRenderTexture(
        offscreen_saved_color_texture_.get());
    if (offscreen_saved_frame_buffer_->CheckStatus() !=
        GL_FRAMEBUFFER_COMPLETE) {
      LOG(ERROR) << "Offscreen saved FBO was incomplete.";
      Destroy(true);
      return false;
    }

    // Bind to the new default frame buffer (the offscreen target frame buffer).
    // This should now be associated with ID zero.
    DoBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  // Clear the backbuffer.
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

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

  has_robustness_extension_ =
      context->HasExtension("GL_ARB_robustness") ||
      context->HasExtension("GL_EXT_robustness");

  if (!InitializeShaderTranslator()) {
    return false;
  }

  state_.viewport_width = size.width();
  state_.viewport_height = size.height();

  GLint viewport_params[4] = { 0 };
  glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport_params);
  viewport_max_width_ = viewport_params[0];
  viewport_max_height_ = viewport_params[1];

  state_.scissor_width = state_.viewport_width;
  state_.scissor_height = state_.viewport_height;

  // Set all the default state because some GL drivers get it wrong.
  state_.InitCapabilities();
  state_.InitState();
  glActiveTexture(GL_TEXTURE0 + state_.active_texture_unit);
  glHint(GL_GENERATE_MIPMAP_HINT, state_.hint_generate_mipmap);
  glPixelStorei(GL_PACK_ALIGNMENT, state_.pack_alignment);
  glPixelStorei(GL_UNPACK_ALIGNMENT, state_.unpack_alignment);

  DoBindBuffer(GL_ARRAY_BUFFER, 0);
  DoBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  DoBindFramebuffer(GL_FRAMEBUFFER, 0);
  DoBindRenderbuffer(GL_RENDERBUFFER, 0);

  // AMD and Intel drivers on Mac OS apparently get gl_PointCoord
  // backward from the spec and this setting makes them work
  // correctly. rdar://problem/11883495
  if (feature_info_->workarounds().reverse_point_sprite_coord_origin) {
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);
  }

  // Create a delegate to perform async pixel transfers.
  async_pixel_transfer_delegate_ =
      gfx::AsyncPixelTransferDelegate::Create(context.get());

  return true;
}

void GLES2DecoderImpl::UpdateCapabilities() {
  util_.set_num_compressed_texture_formats(
      validators_->compressed_texture_format.GetValues().size());
  util_.set_num_shader_binary_formats(
      validators_->shader_binary_format.GetValues().size());
}

bool GLES2DecoderImpl::InitializeShaderTranslator() {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::InitializeShaderTranslator");

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

#if (ANGLE_SH_VERSION >= 110)
  resources.FragmentPrecisionHigh = 1;
#endif

  if (force_webgl_glsl_validation_) {
    resources.OES_standard_derivatives = derivatives_explicitly_enabled_;
  } else {
    resources.OES_standard_derivatives =
        features().oes_standard_derivatives ? 1 : 0;
    resources.ARB_texture_rectangle =
        features().arb_texture_rectangle ? 1 : 0;
    resources.OES_EGL_image_external =
        features().oes_egl_image_external ? 1 : 0;
  }

  ShShaderSpec shader_spec = force_webgl_glsl_validation_ ||
      force_webgl_glsl_validation_ ? SH_WEBGL_SPEC : SH_GLES2_SPEC;
  if (shader_spec == SH_WEBGL_SPEC && features().enable_shader_name_hashing)
#if !defined(ANGLE_SH_VERSION) || ANGLE_SH_VERSION < 108
    resources.HashFunction = &CityHashForAngle;
#else
    resources.HashFunction = &CityHash64;
#endif
  else
    resources.HashFunction = NULL;
  ShaderTranslatorInterface::GlslImplementationType implementation_type =
      gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2 ?
          ShaderTranslatorInterface::kGlslES : ShaderTranslatorInterface::kGlsl;
  ShaderTranslatorInterface::GlslBuiltInFunctionBehavior function_behavior =
      workarounds().needs_glsl_built_in_function_emulation ?
          ShaderTranslatorInterface::kGlslBuiltInFunctionEmulated :
          ShaderTranslatorInterface::kGlslBuiltInFunctionOriginal;

  ShaderTranslatorCache* cache = ShaderTranslatorCache::GetInstance();
  vertex_translator_ = cache->GetTranslator(
      SH_VERTEX_SHADER, shader_spec, &resources,
      implementation_type, function_behavior);
  if (!vertex_translator_.get()) {
    LOG(ERROR) << "Could not initialize vertex shader translator.";
    Destroy(true);
    return false;
  }

  fragment_translator_ = cache->GetTranslator(
      SH_FRAGMENT_SHADER, shader_spec, &resources,
      implementation_type, function_behavior);
  if (!fragment_translator_.get()) {
    LOG(ERROR) << "Could not initialize fragment shader translator.";
    Destroy(true);
    return false;
  }
  return true;
}

bool GLES2DecoderImpl::GenBuffersHelper(GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetBuffer(client_ids[ii])) {
      return false;
    }
  }
  scoped_array<GLuint> service_ids(new GLuint[n]);
  glGenBuffersARB(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateBuffer(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenFramebuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetFramebuffer(client_ids[ii])) {
      return false;
    }
  }
  scoped_array<GLuint> service_ids(new GLuint[n]);
  glGenFramebuffersEXT(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateFramebuffer(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenRenderbuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetRenderbuffer(client_ids[ii])) {
      return false;
    }
  }
  scoped_array<GLuint> service_ids(new GLuint[n]);
  glGenRenderbuffersEXT(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateRenderbuffer(client_ids[ii], service_ids[ii]);
  }
  return true;
}

bool GLES2DecoderImpl::GenTexturesHelper(GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetTexture(client_ids[ii])) {
      return false;
    }
  }
  scoped_array<GLuint> service_ids(new GLuint[n]);
  glGenTextures(n, service_ids.get());
  for (GLsizei ii = 0; ii < n; ++ii) {
    CreateTexture(client_ids[ii], service_ids[ii]);
  }
  return true;
}

void GLES2DecoderImpl::DeleteBuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    Buffer* buffer = GetBuffer(client_ids[ii]);
    if (buffer && !buffer->IsDeleted()) {
      state_.vertex_attrib_manager->Unbind(buffer);
      if (state_.bound_array_buffer == buffer) {
        state_.bound_array_buffer = NULL;
      }
      RemoveBuffer(client_ids[ii]);
    }
  }
}

void GLES2DecoderImpl::DeleteFramebuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  bool supports_separate_framebuffer_binds =
     features().chromium_framebuffer_multisample;

  for (GLsizei ii = 0; ii < n; ++ii) {
    Framebuffer* framebuffer =
        GetFramebuffer(client_ids[ii]);
    if (framebuffer && !framebuffer->IsDeleted()) {
      if (framebuffer == state_.bound_draw_framebuffer) {
        state_.bound_draw_framebuffer = NULL;
        clear_state_dirty_ = true;
        GLenum target = supports_separate_framebuffer_binds ?
            GL_DRAW_FRAMEBUFFER_EXT : GL_FRAMEBUFFER;
        glBindFramebufferEXT(target, GetBackbufferServiceId());
      }
      if (framebuffer == state_.bound_read_framebuffer) {
        state_.bound_read_framebuffer = NULL;
        GLenum target = supports_separate_framebuffer_binds ?
            GL_READ_FRAMEBUFFER_EXT : GL_FRAMEBUFFER;
        glBindFramebufferEXT(target, GetBackbufferServiceId());
      }
      OnFboChanged();
      RemoveFramebuffer(client_ids[ii]);
    }
  }
}

void GLES2DecoderImpl::DeleteRenderbuffersHelper(
    GLsizei n, const GLuint* client_ids) {
  bool supports_separate_framebuffer_binds =
     features().chromium_framebuffer_multisample;
  for (GLsizei ii = 0; ii < n; ++ii) {
    Renderbuffer* renderbuffer =
        GetRenderbuffer(client_ids[ii]);
    if (renderbuffer && !renderbuffer->IsDeleted()) {
      if (state_.bound_renderbuffer == renderbuffer) {
        state_.bound_renderbuffer = NULL;
      }
      // Unbind from current framebuffers.
      if (supports_separate_framebuffer_binds) {
        if (state_.bound_read_framebuffer) {
          state_.bound_read_framebuffer->UnbindRenderbuffer(
              GL_READ_FRAMEBUFFER_EXT, renderbuffer);
        }
        if (state_.bound_draw_framebuffer) {
          state_.bound_draw_framebuffer->UnbindRenderbuffer(
              GL_DRAW_FRAMEBUFFER_EXT, renderbuffer);
        }
      } else {
        if (state_.bound_draw_framebuffer) {
          state_.bound_draw_framebuffer->UnbindRenderbuffer(
              GL_FRAMEBUFFER, renderbuffer);
        }
      }
      clear_state_dirty_ = true;
      RemoveRenderbuffer(client_ids[ii]);
    }
  }
}

void GLES2DecoderImpl::DeleteTexturesHelper(
    GLsizei n, const GLuint* client_ids) {
  bool supports_separate_framebuffer_binds =
     features().chromium_framebuffer_multisample;
  for (GLsizei ii = 0; ii < n; ++ii) {
    Texture* texture = GetTexture(client_ids[ii]);
    if (texture && !texture->IsDeleted()) {
      if (texture->IsAttachedToFramebuffer()) {
        clear_state_dirty_ = true;
      }
      // Unbind texture from texture units.
      for (size_t jj = 0; jj < state_.texture_units.size(); ++jj) {
        state_.texture_units[jj].Unbind(texture);
      }
      // Unbind from current framebuffers.
      if (supports_separate_framebuffer_binds) {
        if (state_.bound_read_framebuffer) {
          state_.bound_read_framebuffer->UnbindTexture(
              GL_READ_FRAMEBUFFER_EXT, texture);
        }
        if (state_.bound_draw_framebuffer) {
          state_.bound_draw_framebuffer->UnbindTexture(
              GL_DRAW_FRAMEBUFFER_EXT, texture);
        }
      } else {
        if (state_.bound_draw_framebuffer) {
          state_.bound_draw_framebuffer->UnbindTexture(GL_FRAMEBUFFER, texture);
        }
      }
      GLuint service_id = texture->service_id();
      if (texture->IsStreamTexture() && stream_texture_manager_) {
        stream_texture_manager_->DestroyStreamTexture(service_id);
      }
#if defined(OS_MACOSX)
      if (texture->target() == GL_TEXTURE_RECTANGLE_ARB) {
        ReleaseIOSurfaceForTexture(service_id);
      }
#endif
      RemoveTexture(client_ids[ii]);
    }
  }
}

// }  // anonymous namespace

bool GLES2DecoderImpl::MakeCurrent() {
  if (!context_.get() || !context_->MakeCurrent(surface_.get()))
    return false;

  if (WasContextLost()) {
    LOG(ERROR) << "  GLES2DecoderImpl: Context lost during MakeCurrent.";
    return false;
  }

  if (engine() && query_manager_.get())
    query_manager_->ProcessPendingTransferQueries();

  // TODO(epenner): Is there a better place to do this? Transfers
  // can complete any time we yield the main thread. So we *must*
  // process transfers after any such yield, before resuming.
  bool frame_buffer_dirty = false;
  bool texture_dirty = false;
  texture_manager()->BindFinishedAsyncPixelTransfers(
      &texture_dirty, &frame_buffer_dirty);
  // Texture unit zero might be stomped.
  if (texture_dirty)
    RestoreCurrentTexture2DBindings();
  // A texture attached to frame-buffer might have changed size.
  if (frame_buffer_dirty) {
    clear_state_dirty_ = true;
    // TODO(gman): If textures tracked which framebuffers they were attached to
    // we could just mark those framebuffers as not complete.
    framebuffer_manager()->IncFramebufferStateChangeCount();
  }

  if (workarounds().flush_on_context_switch)
    glFlush();

  return true;
}

void GLES2DecoderImpl::ReleaseCurrent() {
  if (context_.get())
    context_->ReleaseCurrent(surface_.get());
}

void GLES2DecoderImpl::RestoreCurrentRenderbufferBindings() {
  Renderbuffer* renderbuffer =
      GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
  glBindRenderbufferEXT(
      GL_RENDERBUFFER, renderbuffer ? renderbuffer->service_id() : 0);
}

static void RebindCurrentFramebuffer(
    GLenum target,
    Framebuffer* info,
    GLuint back_buffer_service_id) {
  GLuint framebuffer_id = info ? info->service_id() : 0;

  if (framebuffer_id == 0) {
    framebuffer_id = back_buffer_service_id;
  }

  glBindFramebufferEXT(target, framebuffer_id);
}

void GLES2DecoderImpl::RestoreCurrentFramebufferBindings() {
  clear_state_dirty_ = true;

  if (!features().chromium_framebuffer_multisample) {
    RebindCurrentFramebuffer(
        GL_FRAMEBUFFER,
        state_.bound_draw_framebuffer.get(),
        GetBackbufferServiceId());
  } else {
    RebindCurrentFramebuffer(
        GL_READ_FRAMEBUFFER_EXT,
        state_.bound_read_framebuffer.get(),
        GetBackbufferServiceId());
    RebindCurrentFramebuffer(
        GL_DRAW_FRAMEBUFFER_EXT,
        state_.bound_draw_framebuffer.get(),
        GetBackbufferServiceId());
  }
  OnFboChanged();
}

void GLES2DecoderImpl::RestoreCurrentTexture2DBindings() {
  TextureUnit& info = state_.texture_units[0];
  GLuint last_id;
  if (info.bound_texture_2d) {
    last_id = info.bound_texture_2d->service_id();
  } else {
    last_id = 0;
  }

  glBindTexture(GL_TEXTURE_2D, last_id);
  glActiveTexture(GL_TEXTURE0 + state_.active_texture_unit);
}

bool GLES2DecoderImpl::CheckFramebufferValid(
    Framebuffer* framebuffer,
    GLenum target, const char* func_name) {
  if (!framebuffer) {
    if (backbuffer_needs_clear_bits_) {
      glClearColor(0, 0, 0, (GLES2Util::GetChannelsForFormat(
          offscreen_target_color_format_) & 0x0008) != 0 ? 0 : 1);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glClearStencil(0);
      glStencilMask(-1);
      glClearDepth(1.0f);
      glDepthMask(true);
      glDisable(GL_SCISSOR_TEST);
      glClear(backbuffer_needs_clear_bits_);
      backbuffer_needs_clear_bits_ = 0;
      RestoreClearState();
    }
    return true;
  }

  if (framebuffer_manager()->IsComplete(framebuffer)) {
    return true;
  }

  GLenum completeness = framebuffer->IsPossiblyComplete();
  if (completeness != GL_FRAMEBUFFER_COMPLETE) {
    SetGLError(
        GL_INVALID_FRAMEBUFFER_OPERATION, func_name, "framebuffer incomplete");
    return false;
  }

  // Are all the attachments cleared?
  if (renderbuffer_manager()->HaveUnclearedRenderbuffers() ||
      texture_manager()->HaveUnclearedMips()) {
    if (!framebuffer->IsCleared()) {
      // Can we clear them?
      if (framebuffer->GetStatus(texture_manager(), target) !=
          GL_FRAMEBUFFER_COMPLETE) {
        SetGLError(
            GL_INVALID_FRAMEBUFFER_OPERATION, func_name,
            "framebuffer incomplete (clear)");
        return false;
      }
      ClearUnclearedAttachments(target, framebuffer);
    }
  }

  if (!framebuffer_manager()->IsComplete(framebuffer)) {
    if (framebuffer->GetStatus(texture_manager(), target) !=
        GL_FRAMEBUFFER_COMPLETE) {
      SetGLError(
          GL_INVALID_FRAMEBUFFER_OPERATION, func_name,
          "framebuffer incomplete (check)");
      return false;
    }
    framebuffer_manager()->MarkAsComplete(framebuffer);
  }

  // NOTE: At this point we don't know if the framebuffer is complete but
  // we DO know that everything that needs to be cleared has been cleared.
  return true;
}

bool GLES2DecoderImpl::CheckBoundFramebuffersValid(const char* func_name) {
  if (!features().chromium_framebuffer_multisample) {
    return CheckFramebufferValid(
        state_.bound_draw_framebuffer, GL_FRAMEBUFFER_EXT, func_name);
  }
  return CheckFramebufferValid(
             state_.bound_draw_framebuffer,
             GL_DRAW_FRAMEBUFFER_EXT, func_name) &&
         CheckFramebufferValid(
             state_.bound_read_framebuffer,
             GL_READ_FRAMEBUFFER_EXT, func_name);
}

gfx::Size GLES2DecoderImpl::GetBoundReadFrameBufferSize() {
  Framebuffer* framebuffer =
      GetFramebufferInfoForTarget(GL_READ_FRAMEBUFFER_EXT);
  if (framebuffer != NULL) {
    const Framebuffer::Attachment* attachment =
        framebuffer->GetAttachment(GL_COLOR_ATTACHMENT0);
    if (attachment) {
      return gfx::Size(attachment->width(), attachment->height());
    }
    return gfx::Size(0, 0);
  } else if (offscreen_target_frame_buffer_.get()) {
    return offscreen_size_;
  } else {
    return surface_->GetSize();
  }
}

GLenum GLES2DecoderImpl::GetBoundReadFrameBufferInternalFormat() {
  Framebuffer* framebuffer =
      GetFramebufferInfoForTarget(GL_READ_FRAMEBUFFER_EXT);
  if (framebuffer != NULL) {
    return framebuffer->GetColorAttachmentFormat();
  } else if (offscreen_target_frame_buffer_.get()) {
    return offscreen_target_color_format_;
  } else {
    return back_buffer_color_format_;
  }
}

GLenum GLES2DecoderImpl::GetBoundDrawFrameBufferInternalFormat() {
  Framebuffer* framebuffer =
      GetFramebufferInfoForTarget(GL_DRAW_FRAMEBUFFER_EXT);
  if (framebuffer != NULL) {
    return framebuffer->GetColorAttachmentFormat();
  } else if (offscreen_target_frame_buffer_.get()) {
    return offscreen_target_color_format_;
  } else {
    return back_buffer_color_format_;
  }
}

void GLES2DecoderImpl::UpdateParentTextureInfo() {
  if (parent_) {
    // Update the info about the offscreen saved color texture in the parent.
    // The reference to the parent is a weak pointer and will become null if the
    // parent is later destroyed.
    TextureManager* parent_texture_manager = parent_->texture_manager();
    parent_texture_manager->SetLevelInfo(
        offscreen_saved_color_texture_info_,
        GL_TEXTURE_2D,
        0,  // level
        GL_RGBA,
        offscreen_size_.width(),
        offscreen_size_.height(),
        1,  // depth
        0,  // border
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        true);
    parent_texture_manager->SetParameter(
        offscreen_saved_color_texture_info_,
        GL_TEXTURE_MAG_FILTER,
        GL_NEAREST);
    parent_texture_manager->SetParameter(
        offscreen_saved_color_texture_info_,
        GL_TEXTURE_MIN_FILTER,
        GL_NEAREST);
    parent_texture_manager->SetParameter(
        offscreen_saved_color_texture_info_,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE);
    parent_texture_manager->SetParameter(
        offscreen_saved_color_texture_info_,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE);
  } else {
    offscreen_saved_color_texture_info_ = NULL;
  }
}

void GLES2DecoderImpl::SetResizeCallback(
    const base::Callback<void(gfx::Size)>& callback) {
  resize_callback_ = callback;
}

void GLES2DecoderImpl::SetMsgCallback(const MsgCallback& callback) {
  msg_callback_ = callback;
}

void GLES2DecoderImpl::SetWaitSyncPointCallback(
    const WaitSyncPointCallback& callback) {
  wait_sync_point_callback_ = callback;
}

void GLES2DecoderImpl::SetStreamTextureManager(StreamTextureManager* manager) {
  stream_texture_manager_ = manager;
}

gfx::AsyncPixelTransferDelegate*
    GLES2DecoderImpl::GetAsyncPixelTransferDelegate() {
  return async_pixel_transfer_delegate_.get();
}

void GLES2DecoderImpl::SetAsyncPixelTransferDelegate(
    gfx::AsyncPixelTransferDelegate* delegate) {
  async_pixel_transfer_delegate_ = make_scoped_ptr(delegate);
}

bool GLES2DecoderImpl::GetServiceTextureId(uint32 client_texture_id,
                                           uint32* service_texture_id) {
  Texture* texture =
      texture_manager()->GetTexture(client_texture_id);
  if (texture) {
    *service_texture_id = texture->service_id();
    return true;
  }
  return false;
}

uint32 GLES2DecoderImpl::GetTextureUploadCount() {
  return texture_upload_count_ +
      async_pixel_transfer_delegate_->GetTextureUploadCount();
}

base::TimeDelta GLES2DecoderImpl::GetTotalTextureUploadTime() {
  return total_texture_upload_time_ +
      async_pixel_transfer_delegate_->GetTotalTextureUploadTime();
}

base::TimeDelta GLES2DecoderImpl::GetTotalProcessingCommandsTime() {
  return total_processing_commands_time_;
}

void GLES2DecoderImpl::AddProcessingCommandsTime(base::TimeDelta time) {
  total_processing_commands_time_ += time;
}

void GLES2DecoderImpl::Destroy(bool have_context) {
  DCHECK(!have_context || context_->IsCurrent(NULL));

  ChildList children = children_;
  for (ChildList::iterator it = children.begin(); it != children.end(); ++it)
    (*it)->SetParent(NULL, 0);
  DCHECK(children_.empty());
  SetParent(NULL, 0);

  // Unbind everything.
  state_.vertex_attrib_manager = NULL;
  default_vertex_attrib_manager_ = NULL;
  state_.texture_units.clear();
  state_.bound_array_buffer = NULL;
  state_.current_query = NULL;
  state_.current_program = NULL;
  state_.bound_read_framebuffer = NULL;
  state_.bound_draw_framebuffer = NULL;
  state_.bound_renderbuffer = NULL;

  if (have_context) {
    if (copy_texture_CHROMIUM_.get()) {
      copy_texture_CHROMIUM_->Destroy();
      copy_texture_CHROMIUM_.reset();
    }

    if (state_.current_program) {
      program_manager()->UnuseProgram(shader_manager(), state_.current_program);
      state_.current_program = NULL;
    }

    if (attrib_0_buffer_id_) {
      glDeleteBuffersARB(1, &attrib_0_buffer_id_);
    }
    if (fixed_attrib_buffer_id_) {
      glDeleteBuffersARB(1, &fixed_attrib_buffer_id_);
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
    if (offscreen_resolved_frame_buffer_.get())
      offscreen_resolved_frame_buffer_->Destroy();
    if (offscreen_resolved_color_texture_.get())
      offscreen_resolved_color_texture_->Destroy();
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
    if (offscreen_resolved_frame_buffer_.get())
      offscreen_resolved_frame_buffer_->Invalidate();
    if (offscreen_resolved_color_texture_.get())
      offscreen_resolved_color_texture_->Invalidate();
  }
  copy_texture_CHROMIUM_.reset();

  if (query_manager_.get()) {
    query_manager_->Destroy(have_context);
    query_manager_.reset();
  }

  if (vertex_array_manager_ .get()) {
    vertex_array_manager_->Destroy(have_context);
    vertex_array_manager_.reset();
  }

  offscreen_target_frame_buffer_.reset();
  offscreen_target_color_texture_.reset();
  offscreen_target_color_render_buffer_.reset();
  offscreen_target_depth_render_buffer_.reset();
  offscreen_target_stencil_render_buffer_.reset();
  offscreen_saved_frame_buffer_.reset();
  offscreen_saved_color_texture_.reset();
  offscreen_resolved_frame_buffer_.reset();
  offscreen_resolved_color_texture_.reset();

  if (group_) {
    group_->Destroy(this, have_context);
    group_ = NULL;
  }

  if (context_.get()) {
    context_->ReleaseCurrent(NULL);
    context_ = NULL;
  }

#if defined(OS_MACOSX)
  for (TextureToIOSurfaceMap::iterator it = texture_to_io_surface_map_.begin();
       it != texture_to_io_surface_map_.end(); ++it) {
    CFRelease(it->second);
  }
  texture_to_io_surface_map_.clear();
#endif
}

void GLES2DecoderImpl::SetSurface(
    const scoped_refptr<gfx::GLSurface>& surface) {
  DCHECK(context_->IsCurrent(NULL));
  DCHECK(surface_.get());
  surface_ = surface;
  RestoreCurrentFramebufferBindings();
}

bool GLES2DecoderImpl::SetParent(GLES2Decoder* new_parent,
                                 uint32 new_parent_texture_id) {
  if (!offscreen_saved_color_texture_.get())
    return false;

  // Remove the saved frame buffer mapping from the parent decoder. The
  // parent pointer is a weak pointer so it will be null if the parent has
  // already been destroyed.
  if (parent_) {
    ChildList::iterator it = std::find(
        parent_->children_.begin(),
        parent_->children_.end(),
        this);
    DCHECK(it != parent_->children_.end());
    parent_->children_.erase(it);
    // First check the texture has been mapped into the parent. This might not
    // be the case if initialization failed midway through.
    GLuint service_id = offscreen_saved_color_texture_->id();
    GLuint client_id = 0;
    if (parent_->texture_manager()->GetClientId(service_id, &client_id)) {
      parent_->texture_manager()->RemoveTexture(client_id);
    }
  }

  GLES2DecoderImpl* new_parent_impl = static_cast<GLES2DecoderImpl*>(
      new_parent);
  if (new_parent_impl) {
#ifndef NDEBUG
    ChildList::iterator it = std::find(
        new_parent_impl->children_.begin(),
        new_parent_impl->children_.end(),
        this);
    DCHECK(it == new_parent_impl->children_.end());
#endif
    new_parent_impl->children_.push_back(this);
    // Map the ID of the saved offscreen texture into the parent so that
    // it can reference it.
    GLuint service_id = offscreen_saved_color_texture_->id();

    // Replace texture info when ID is already in use by parent.
    if (new_parent_impl->texture_manager()->GetTexture(
        new_parent_texture_id))
      new_parent_impl->texture_manager()->RemoveTexture(
          new_parent_texture_id);

    offscreen_saved_color_texture_info_ =
        new_parent_impl->CreateTexture(new_parent_texture_id, service_id);
    offscreen_saved_color_texture_info_->SetNotOwned();
    new_parent_impl->texture_manager()->
       SetInfoTarget(offscreen_saved_color_texture_info_, GL_TEXTURE_2D);

    parent_ = base::AsWeakPtr<GLES2DecoderImpl>(new_parent_impl);

    UpdateParentTextureInfo();
  } else {
    parent_.reset();
    offscreen_saved_color_texture_info_ = NULL;
  }

  return true;
}

size_t GLES2DecoderImpl::GetBackbufferMemoryTotal() {
  size_t total = 0;
  if (offscreen_target_frame_buffer_.get()) {
    if (offscreen_target_color_texture_.get()) {
        total += offscreen_target_color_texture_->estimated_size();
    }
    if (offscreen_target_color_render_buffer_.get()) {
        total += offscreen_target_color_render_buffer_->estimated_size();
    }
    if (offscreen_target_depth_render_buffer_.get()) {
        total += offscreen_target_depth_render_buffer_->estimated_size();
    }
    if (offscreen_target_stencil_render_buffer_.get()) {
        total += offscreen_target_stencil_render_buffer_->estimated_size();
    }
    if (offscreen_saved_color_texture_.get()) {
        total += offscreen_saved_color_texture_->estimated_size();
    }
    if (offscreen_resolved_color_texture_.get()) {
        total += offscreen_resolved_color_texture_->estimated_size();
    }
  } else {
    gfx::Size size = surface_->GetSize();
    total += size.width() * size.height() *
        GLES2Util::RenderbufferBytesPerPixel(back_buffer_color_format_);
  }
  return total;
}

bool GLES2DecoderImpl::ResizeOffscreenFrameBuffer(const gfx::Size& size) {
  bool is_offscreen = !!offscreen_target_frame_buffer_.get();
  if (!is_offscreen) {
    LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFrameBuffer called "
               << " with an onscreen framebuffer.";
    return false;
  }

  if (offscreen_size_ == size)
    return true;

  offscreen_size_ = size;
  int w = offscreen_size_.width();
  int h = offscreen_size_.height();
  if (w < 0 || h < 0 || h >= (INT_MAX / 4) / (w ? w : 1)) {
    LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFrameBuffer failed "
               << "to allocate storage due to excessive dimensions.";
    return false;
  }

  // Reallocate the offscreen target buffers.
  DCHECK(offscreen_target_color_format_);
  if (IsOffscreenBufferMultisampled()) {
    if (!offscreen_target_color_render_buffer_->AllocateStorage(
        offscreen_size_, offscreen_target_color_format_,
        offscreen_target_samples_)) {
      LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFrameBuffer failed "
                 << "to allocate storage for offscreen target color buffer.";
      return false;
    }
  } else {
    if (!offscreen_target_color_texture_->AllocateStorage(
        offscreen_size_, offscreen_target_color_format_, false)) {
      LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFrameBuffer failed "
                 << "to allocate storage for offscreen target color texture.";
      return false;
    }
  }
  if (offscreen_target_depth_format_ &&
      !offscreen_target_depth_render_buffer_->AllocateStorage(
      offscreen_size_, offscreen_target_depth_format_,
      offscreen_target_samples_)) {
    LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFrameBuffer failed "
               << "to allocate storage for offscreen target depth buffer.";
    return false;
  }
  if (offscreen_target_stencil_format_ &&
      !offscreen_target_stencil_render_buffer_->AllocateStorage(
      offscreen_size_, offscreen_target_stencil_format_,
      offscreen_target_samples_)) {
    LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFrameBuffer failed "
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
      LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFrameBuffer failed "
                 << "because offscreen FBO was incomplete.";
    return false;
  }

  // Clear the target frame buffer.
  {
    ScopedFrameBufferBinder binder(this, offscreen_target_frame_buffer_->id());
    glClearColor(0, 0, 0, (GLES2Util::GetChannelsForFormat(
        offscreen_target_color_format_) & 0x0008) != 0 ? 0 : 1);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearStencil(0);
    glStencilMaskSeparate(GL_FRONT, -1);
    glStencilMaskSeparate(GL_BACK, -1);
    glClearDepth(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    RestoreClearState();
  }

  // Destroy the offscreen resolved framebuffers.
  if (offscreen_resolved_frame_buffer_.get())
    offscreen_resolved_frame_buffer_->Destroy();
  if (offscreen_resolved_color_texture_.get())
    offscreen_resolved_color_texture_->Destroy();
  offscreen_resolved_color_texture_.reset();
  offscreen_resolved_frame_buffer_.reset();

  return true;
}

error::Error GLES2DecoderImpl::HandleResizeCHROMIUM(
    uint32 immediate_data_size, const cmds::ResizeCHROMIUM& c) {
  if (!offscreen_target_frame_buffer_.get() && surface_->DeferDraws())
    return error::kDeferCommandUntilLater;

  GLuint width = static_cast<GLuint>(c.width);
  GLuint height = static_cast<GLuint>(c.height);
  TRACE_EVENT2("gpu", "glResizeChromium", "width", width, "height", height);

  width = std::max(1U, width);
  height = std::max(1U, height);

#if defined(OS_POSIX) && !defined(OS_MACOSX) && \
    !defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  // Make sure that we are done drawing to the back buffer before resizing.
  glFinish();
#endif
  bool is_offscreen = !!offscreen_target_frame_buffer_.get();
  if (is_offscreen) {
    if (!ResizeOffscreenFrameBuffer(gfx::Size(width, height))) {
      LOG(ERROR) << "GLES2DecoderImpl: Context lost because "
                 << "ResizeOffscreenFrameBuffer failed.";
      return error::kLostContext;
    }
  }

  if (!resize_callback_.is_null()) {
    resize_callback_.Run(gfx::Size(width, height));
    CHECK(context_->IsCurrent(surface_.get()));
    if (!context_->IsCurrent(surface_.get())) {
      LOG(ERROR) << "GLES2DecoderImpl: Context lost because context no longer "
                 << "current after resize callback.";
      return error::kLostContext;
    }
  }

  return error::kNoError;
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
  if (log_commands()) {
    // TODO(notme): Change this to a LOG/VLOG that works in release. Tried
    // LOG(INFO), tried VLOG(1), no luck.
    LOG(ERROR) << "[" << GetLogPrefix() << "]" << "cmd: "
               << GetCommandName(command);
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
          case cmds::name::kCmdId:                                 \
            result = Handle ## name(                               \
                immediate_data_size,                               \
                *static_cast<const gles2::cmds::name*>(cmd_data)); \
            break;                                                 \

        GLES2_COMMAND_LIST(GLES2_CMD_OP)
        #undef GLES2_CMD_OP
      }
      if (debug()) {
        GLenum error;
        while ((error = glGetError()) != GL_NO_ERROR) {
          LOG(ERROR) << "[" << GetLogPrefix() << "] "
                     << "GL ERROR: " << GLES2Util::GetStringEnum(error) << " : "
                     << GetCommandName(command);
          SetGLError(error, "DoCommand", "GL error from driver");
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

void GLES2DecoderImpl::RemoveBuffer(GLuint client_id) {
  buffer_manager()->RemoveBuffer(client_id);
}

bool GLES2DecoderImpl::CreateProgramHelper(GLuint client_id) {
  if (GetProgram(client_id)) {
    return false;
  }
  GLuint service_id = glCreateProgram();
  if (service_id != 0) {
    CreateProgram(client_id, service_id);
  }
  return true;
}

bool GLES2DecoderImpl::CreateShaderHelper(GLenum type, GLuint client_id) {
  if (GetShader(client_id)) {
    return false;
  }
  GLuint service_id = glCreateShader(type);
  if (service_id != 0) {
    CreateShader(client_id, service_id, type);
  }
  return true;
}

void GLES2DecoderImpl::DoFinish() {
  glFinish();
  ProcessPendingQueries();
}

void GLES2DecoderImpl::DoFlush() {
  glFlush();
  ProcessPendingQueries();
}

void GLES2DecoderImpl::DoActiveTexture(GLenum texture_unit) {
  GLuint texture_index = texture_unit - GL_TEXTURE0;
  if (texture_index >= state_.texture_units.size()) {
    SetGLErrorInvalidEnum(
        "glActiveTexture", texture_unit, "texture_unit");
    return;
  }
  state_.active_texture_unit = texture_index;
  glActiveTexture(texture_unit);
}

void GLES2DecoderImpl::DoBindBuffer(GLenum target, GLuint client_id) {
  Buffer* info = NULL;
  GLuint service_id = 0;
  if (client_id != 0) {
    info = GetBuffer(client_id);
    if (!info) {
      if (!group_->bind_generates_resource()) {
        LOG(ERROR) << "glBindBuffer: id not generated by glGenBuffers";
        current_decoder_error_ = error::kGenericError;
        return;
      }

      // It's a new id so make a buffer info for it.
      glGenBuffersARB(1, &service_id);
      CreateBuffer(client_id, service_id);
      info = GetBuffer(client_id);
      IdAllocatorInterface* id_allocator =
          group_->GetIdAllocator(id_namespaces::kBuffers);
      id_allocator->MarkAsUsed(client_id);
    }
  }
  LogClientServiceForInfo(info, client_id, "glBindBuffer");
  if (info) {
    if (!buffer_manager()->SetTarget(info, target)) {
      SetGLError(GL_INVALID_OPERATION,
                 "glBindBuffer", "buffer bound to more than 1 target");
      return;
    }
    service_id = info->service_id();
  }
  switch (target) {
    case GL_ARRAY_BUFFER:
      state_.bound_array_buffer = info;
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      state_.vertex_attrib_manager->SetElementArrayBuffer(info);
      break;
    default:
      NOTREACHED();  // Validation should prevent us getting here.
      break;
  }
  glBindBuffer(target, service_id);
}

bool GLES2DecoderImpl::BoundFramebufferHasColorAttachmentWithAlpha() {
  return (GLES2Util::GetChannelsForFormat(
      GetBoundDrawFrameBufferInternalFormat()) & 0x0008) != 0;
}

bool GLES2DecoderImpl::BoundFramebufferHasDepthAttachment() {
  Framebuffer* framebuffer =
      GetFramebufferInfoForTarget(GL_DRAW_FRAMEBUFFER_EXT);
  if (framebuffer) {
    return framebuffer->HasDepthAttachment();
  }
  if (offscreen_target_frame_buffer_.get()) {
    return offscreen_target_depth_format_ != 0;
  }
  return back_buffer_has_depth_;
}

bool GLES2DecoderImpl::BoundFramebufferHasStencilAttachment() {
  Framebuffer* framebuffer =
      GetFramebufferInfoForTarget(GL_DRAW_FRAMEBUFFER_EXT);
  if (framebuffer) {
    return framebuffer->HasStencilAttachment();
  }
  if (offscreen_target_frame_buffer_.get()) {
    return offscreen_target_stencil_format_ != 0 ||
           offscreen_target_depth_format_ == GL_DEPTH24_STENCIL8;
  }
  return back_buffer_has_stencil_;
}

void GLES2DecoderImpl::ApplyDirtyState() {
  if (clear_state_dirty_) {
    glColorMask(
        state_.color_mask_red, state_.color_mask_green, state_.color_mask_blue,
        state_.color_mask_alpha &&
            BoundFramebufferHasColorAttachmentWithAlpha());
    bool have_depth = BoundFramebufferHasDepthAttachment();
    glDepthMask(state_.depth_mask && have_depth);
    EnableDisable(GL_DEPTH_TEST, state_.enable_flags.depth_test && have_depth);
    bool have_stencil = BoundFramebufferHasStencilAttachment();
    glStencilMaskSeparate(
        GL_FRONT, have_stencil ? state_.stencil_front_writemask : 0);
    glStencilMaskSeparate(
        GL_BACK, have_stencil ? state_.stencil_back_writemask : 0);
    EnableDisable(
        GL_STENCIL_TEST, state_.enable_flags.stencil_test && have_stencil);
    EnableDisable(GL_CULL_FACE, state_.enable_flags.cull_face);
    EnableDisable(GL_SCISSOR_TEST, state_.enable_flags.scissor_test);
    EnableDisable(GL_BLEND, state_.enable_flags.blend);
    clear_state_dirty_ = false;
  }
}

GLuint GLES2DecoderImpl::GetBackbufferServiceId() const {
  return (offscreen_target_frame_buffer_.get()) ?
      offscreen_target_frame_buffer_->id() :
      (surface_ ? surface_->GetBackingFrameBufferObject() : 0);
}

void GLES2DecoderImpl::RestoreState() const {
  // Restore the Framebuffer first because of bugs in Intel drivers.
  // Intel drivers incorrectly clip the viewport settings to
  // the size of the current framebuffer object.
  RestoreFramebufferBindings();
  state_.RestoreState();
}

void GLES2DecoderImpl::RestoreFramebufferBindings() const {
  GLuint service_id = state_.bound_draw_framebuffer ?
      state_.bound_draw_framebuffer->service_id() :
      GetBackbufferServiceId();
  if (!features().chromium_framebuffer_multisample) {
    glBindFramebufferEXT(GL_FRAMEBUFFER, service_id);
  } else {
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, service_id);
    service_id = state_.bound_read_framebuffer ?
        state_.bound_read_framebuffer->service_id() :
        GetBackbufferServiceId();
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, service_id);
  }
  OnFboChanged();

}

void GLES2DecoderImpl::RestoreTextureState(unsigned service_id) const {
  GLuint client_id = 0;
  if (texture_manager()->GetClientId(service_id, &client_id)) {
    Texture* texture = GetTexture(client_id);
    glBindTexture(GL_TEXTURE_2D, service_id);
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture->wrap_s());
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture->wrap_t());
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture->min_filter());
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture->mag_filter());
    RestoreTextureUnitBindings(state_.active_texture_unit);
  }
}

void GLES2DecoderImpl::OnFboChanged() const {
  if (workarounds().restore_scissor_on_fbo_change)
    glScissor(state_.scissor_x, state_.scissor_y,
              state_.scissor_width, state_.scissor_height);
}

void GLES2DecoderImpl::DoBindFramebuffer(GLenum target, GLuint client_id) {
  Framebuffer* info = NULL;
  GLuint service_id = 0;
  if (client_id != 0) {
    info = GetFramebuffer(client_id);
    if (!info) {
      if (!group_->bind_generates_resource()) {
         LOG(ERROR)
             << "glBindFramebuffer: id not generated by glGenFramebuffers";
         current_decoder_error_ = error::kGenericError;
         return;
      }

      // It's a new id so make a framebuffer info for it.
      glGenFramebuffersEXT(1, &service_id);
      CreateFramebuffer(client_id, service_id);
      info = GetFramebuffer(client_id);
      IdAllocatorInterface* id_allocator =
          group_->GetIdAllocator(id_namespaces::kFramebuffers);
      id_allocator->MarkAsUsed(client_id);
    } else {
      service_id = info->service_id();
    }
    info->MarkAsValid();
  }
  LogClientServiceForInfo(info, client_id, "glBindFramebuffer");

  if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER_EXT) {
    state_.bound_draw_framebuffer = info;
  }
  if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER_EXT) {
    state_.bound_read_framebuffer = info;
  }

  clear_state_dirty_ = true;

  // If we are rendering to the backbuffer get the FBO id for any simulated
  // backbuffer.
  if (info == NULL) {
    service_id = GetBackbufferServiceId();
  }

  glBindFramebufferEXT(target, service_id);
  OnFboChanged();
}

void GLES2DecoderImpl::DoBindRenderbuffer(GLenum target, GLuint client_id) {
  Renderbuffer* info = NULL;
  GLuint service_id = 0;
  if (client_id != 0) {
    info = GetRenderbuffer(client_id);
    if (!info) {
      if (!group_->bind_generates_resource()) {
        LOG(ERROR)
            << "glBindRenderbuffer: id not generated by glGenRenderbuffers";
        current_decoder_error_ = error::kGenericError;
        return;
      }

      // It's a new id so make a renderbuffer info for it.
      glGenRenderbuffersEXT(1, &service_id);
      CreateRenderbuffer(client_id, service_id);
      info = GetRenderbuffer(client_id);
      IdAllocatorInterface* id_allocator =
          group_->GetIdAllocator(id_namespaces::kRenderbuffers);
      id_allocator->MarkAsUsed(client_id);
    } else {
      service_id = info->service_id();
    }
    info->MarkAsValid();
  }
  LogClientServiceForInfo(info, client_id, "glBindRenerbuffer");
  state_.bound_renderbuffer = info;
  glBindRenderbufferEXT(target, service_id);
}

void GLES2DecoderImpl::DoBindTexture(GLenum target, GLuint client_id) {
  Texture* info = NULL;
  GLuint service_id = 0;
  if (client_id != 0) {
    info = GetTexture(client_id);
    if (!info) {
      if (!group_->bind_generates_resource()) {
         LOG(ERROR) << "glBindTexture: id not generated by glGenTextures";
         current_decoder_error_ = error::kGenericError;
         return;
      }

      // It's a new id so make a texture info for it.
      glGenTextures(1, &service_id);
      DCHECK_NE(0u, service_id);
      CreateTexture(client_id, service_id);
      info = GetTexture(client_id);
      IdAllocatorInterface* id_allocator =
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
               "glBindTexture", "texture bound to more than 1 target.");
    return;
  }
  if (info->IsStreamTexture() && target != GL_TEXTURE_EXTERNAL_OES) {
    SetGLError(GL_INVALID_OPERATION,
               "glBindTexture", "illegal target for stream texture.");
    return;
  }
  LogClientServiceForInfo(info, client_id, "glBindTexture");
  if (info->target() == 0) {
    texture_manager()->SetInfoTarget(info, target);
  }
  glBindTexture(target, info->service_id());

  TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
  unit.bind_target = target;
  switch (target) {
    case GL_TEXTURE_2D:
      unit.bound_texture_2d = info;
      break;
    case GL_TEXTURE_CUBE_MAP:
      unit.bound_texture_cube_map = info;
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      unit.bound_texture_external_oes = info;
      if (info->IsStreamTexture()) {
        DCHECK(stream_texture_manager_);
        StreamTexture* stream_tex =
            stream_texture_manager_->LookupStreamTexture(info->service_id());
        if (stream_tex)
          stream_tex->Update();
      }
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      unit.bound_texture_rectangle_arb = info;
      break;
    default:
      NOTREACHED();  // Validation should prevent us getting here.
      break;
  }
}

void GLES2DecoderImpl::DoDisableVertexAttribArray(GLuint index) {
  if (state_.vertex_attrib_manager->Enable(index, false)) {
    if (index != 0 ||
        gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2) {
      glDisableVertexAttribArray(index);
    }
  } else {
    SetGLError(GL_INVALID_VALUE,
               "glDisableVertexAttribArray", "index out of range");
  }
}

void GLES2DecoderImpl::DoDiscardFramebufferEXT(GLenum target,
                                               GLsizei numAttachments,
                                               const GLenum* attachments) {
  Framebuffer* framebuffer =
      GetFramebufferInfoForTarget(GL_FRAMEBUFFER);

  // Validates the attachments. If one of them fails
  // the whole command fails.
  for (GLsizei i = 0; i < numAttachments; ++i) {
    if ((framebuffer &&
        !validators_->attachment.IsValid(attachments[i])) ||
       (!framebuffer &&
        !validators_->backbuffer_attachment.IsValid(attachments[i]))) {
      SetGLErrorInvalidEnum("glDiscardFramebufferEXT",
                           attachments[i],
                           "attachments");
      return;
    }
  }

  // Marks each one of them as not cleared
  for (GLsizei i = 0; i < numAttachments; ++i) {
    if (framebuffer) {
      framebuffer->MarkAttachmentAsCleared(renderbuffer_manager(),
                                           texture_manager(),
                                           attachments[i],
                                           false);
    } else {
      switch (attachments[i]) {
        case GL_COLOR_EXT:
          backbuffer_needs_clear_bits_ |= GL_COLOR_BUFFER_BIT;
          break;
        case GL_DEPTH_EXT:
          backbuffer_needs_clear_bits_ |= GL_DEPTH_BUFFER_BIT;
        case GL_STENCIL_EXT:
          backbuffer_needs_clear_bits_ |= GL_STENCIL_BUFFER_BIT;
          break;
        default:
          NOTREACHED();
          break;
      }
    }
  }

  glDiscardFramebufferEXT(target, numAttachments, attachments);
}

void GLES2DecoderImpl::DoEnableVertexAttribArray(GLuint index) {
  if (state_.vertex_attrib_manager->Enable(index, true)) {
    glEnableVertexAttribArray(index);
  } else {
    SetGLError(GL_INVALID_VALUE,
               "glEnableVertexAttribArray", "index out of range");
  }
}

void GLES2DecoderImpl::DoGenerateMipmap(GLenum target) {
  Texture* info = GetTextureInfoForTarget(target);
  if (!info ||
      !texture_manager()->CanGenerateMipmaps(info)) {
    SetGLError(GL_INVALID_OPERATION,
               "glGenerateMipmaps", "Can not generate mips");
    return;
  }

  if (target == GL_TEXTURE_CUBE_MAP) {
    for (int i = 0; i < 6; ++i) {
      GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
      if (!texture_manager()->ClearTextureLevel(this, info, face, 0)) {
        SetGLError(GL_OUT_OF_MEMORY, "glGenerateMipmaps", "dimensions too big");
        return;
      }
    }
  } else {
    if (!texture_manager()->ClearTextureLevel(this, info, target, 0)) {
      SetGLError(GL_OUT_OF_MEMORY, "glGenerateMipmaps", "dimensions too big");
      return;
    }
  }

  CopyRealGLErrorsToWrapper();
  // Workaround for Mac driver bug. In the large scheme of things setting
  // glTexParamter twice for glGenerateMipmap is probably not a lage performance
  // hit so there's probably no need to make this conditional. The bug appears
  // to be that if the filtering mode is set to something that doesn't require
  // mipmaps for rendering, or is never set to something other than the default,
  // then glGenerateMipmap misbehaves.
  if (workarounds().set_texture_filter_before_generating_mipmap) {
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
  }
  glGenerateMipmapEXT(target);
  if (workarounds().set_texture_filter_before_generating_mipmap) {
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, info->min_filter());
  }
  GLenum error = PeekGLError();
  if (error == GL_NO_ERROR) {
    texture_manager()->MarkMipmapsGenerated(info);
  }
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
      }
  }
  switch (pname) {
    case GL_MAX_VIEWPORT_DIMS:
      if (offscreen_target_frame_buffer_.get()) {
        *num_written = 2;
        if (params) {
          params[0] = renderbuffer_manager()->max_renderbuffer_size();
          params[1] = renderbuffer_manager()->max_renderbuffer_size();
        }
        return true;
      }
      return false;
    case GL_MAX_SAMPLES:
      *num_written = 1;
      if (params) {
        params[0] = renderbuffer_manager()->max_samples();
      }
      return true;
    case GL_MAX_RENDERBUFFER_SIZE:
      *num_written = 1;
      if (params) {
        params[0] = renderbuffer_manager()->max_renderbuffer_size();
      }
      return true;
    case GL_MAX_TEXTURE_SIZE:
      *num_written = 1;
      if (params) {
        params[0] = texture_manager()->MaxSizeForTarget(GL_TEXTURE_2D);
      }
      return true;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      *num_written = 1;
      if (params) {
        params[0] = texture_manager()->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP);
      }
      return true;
    case GL_ALPHA_BITS:
      *num_written = 1;
      if (params) {
        GLint v = 0;
        glGetIntegerv(GL_ALPHA_BITS, &v);
        params[0] = BoundFramebufferHasColorAttachmentWithAlpha() ? v : 0;
      }
      return true;
    case GL_DEPTH_BITS:
      *num_written = 1;
      if (params) {
        GLint v = 0;
        glGetIntegerv(GL_DEPTH_BITS, &v);
        params[0] = BoundFramebufferHasDepthAttachment() ? v : 0;
      }
      return true;
    case GL_STENCIL_BITS:
      *num_written = 1;
      if (params) {
        GLint v = 0;
        glGetIntegerv(GL_STENCIL_BITS, &v);
        params[0] = BoundFramebufferHasStencilAttachment() ? v : 0;
      }
      return true;
    case GL_COMPRESSED_TEXTURE_FORMATS:
      *num_written = validators_->compressed_texture_format.GetValues().size();
      if (params) {
        for (GLint ii = 0; ii < *num_written; ++ii) {
          params[ii] = validators_->compressed_texture_format.GetValues()[ii];
        }
      }
      return true;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      *num_written = 1;
      if (params) {
        *params = validators_->compressed_texture_format.GetValues().size();
      }
      return true;
    case GL_NUM_SHADER_BINARY_FORMATS:
      *num_written = 1;
      if (params) {
        *params = validators_->shader_binary_format.GetValues().size();
      }
      return true;
    case GL_SHADER_BINARY_FORMATS:
      *num_written = validators_->shader_binary_format.GetValues().size();
      if (params) {
        for (GLint ii = 0; ii <  *num_written; ++ii) {
          params[ii] = validators_->shader_binary_format.GetValues()[ii];
        }
      }
      return true;
    case GL_SHADER_COMPILER:
      *num_written = 1;
      if (params) {
        *params = GL_TRUE;
      }
      return true;
    case GL_ARRAY_BUFFER_BINDING:
      *num_written = 1;
      if (params) {
        if (state_.bound_array_buffer) {
          GLuint client_id = 0;
          buffer_manager()->GetClientId(state_.bound_array_buffer->service_id(),
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
        if (state_.vertex_attrib_manager->element_array_buffer()) {
          GLuint client_id = 0;
          buffer_manager()->GetClientId(
              state_.vertex_attrib_manager->element_array_buffer()->
                  service_id(), &client_id);
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
        Framebuffer* framebuffer =
            GetFramebufferInfoForTarget(GL_FRAMEBUFFER);
        if (framebuffer) {
          GLuint client_id = 0;
          framebuffer_manager()->GetClientId(
              framebuffer->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_READ_FRAMEBUFFER_BINDING_EXT:
      *num_written = 1;
      if (params) {
        Framebuffer* framebuffer =
            GetFramebufferInfoForTarget(GL_READ_FRAMEBUFFER_EXT);
        if (framebuffer) {
          GLuint client_id = 0;
          framebuffer_manager()->GetClientId(
              framebuffer->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_RENDERBUFFER_BINDING:
      *num_written = 1;
      if (params) {
        Renderbuffer* renderbuffer =
            GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
        if (renderbuffer) {
          GLuint client_id = 0;
          renderbuffer_manager()->GetClientId(
              renderbuffer->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_CURRENT_PROGRAM:
      *num_written = 1;
      if (params) {
        if (state_.current_program) {
          GLuint client_id = 0;
          program_manager()->GetClientId(
              state_.current_program->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_VERTEX_ARRAY_BINDING_OES:
      *num_written = 1;
      if (params) {
        if (state_.vertex_attrib_manager != default_vertex_attrib_manager_) {
          GLuint client_id = 0;
          vertex_array_manager_->GetClientId(
              state_.vertex_attrib_manager->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_2D:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
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
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
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
    case GL_TEXTURE_BINDING_EXTERNAL_OES:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_external_oes) {
          GLuint client_id = 0;
          texture_manager()->GetClientId(
              unit.bound_texture_external_oes->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_TEXTURE_BINDING_RECTANGLE_ARB:
      *num_written = 1;
      if (params) {
        TextureUnit& unit = state_.texture_units[state_.active_texture_unit];
        if (unit.bound_texture_rectangle_arb) {
          GLuint client_id = 0;
          texture_manager()->GetClientId(
              unit.bound_texture_rectangle_arb->service_id(), &client_id);
          *params = client_id;
        } else {
          *params = 0;
        }
      }
      return true;
    case GL_UNPACK_FLIP_Y_CHROMIUM:
      *num_written = 1;
      if (params) {
        params[0] = unpack_flip_y_;
      }
      return true;
    case GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM:
      *num_written = 1;
      if (params) {
        params[0] = unpack_premultiply_alpha_;
      }
      return true;
    case GL_UNPACK_UNPREMULTIPLY_ALPHA_CHROMIUM:
      *num_written = 1;
      if (params) {
        params[0] = unpack_unpremultiply_alpha_;
      }
      return true;
    default:
      *num_written = util_.GLGetNumValuesReturned(pname);
      return false;
  }
}

bool GLES2DecoderImpl::GetNumValuesReturnedForGLGet(
    GLenum pname, GLsizei* num_values) {
  if (state_.GetStateAsGLint(pname, NULL, num_values)) {
    return true;
  }
  return GetHelper(pname, NULL, num_values);
}

void GLES2DecoderImpl::DoGetBooleanv(GLenum pname, GLboolean* params) {
  DCHECK(params);
  GLsizei num_written = 0;
  if (GetNumValuesReturnedForGLGet(pname, &num_written)) {
    scoped_array<GLint> values(new GLint[num_written]);
    if (!state_.GetStateAsGLint(pname, values.get(), &num_written)) {
      GetHelper(pname, values.get(), &num_written);
    }
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
  if (!state_.GetStateAsGLfloat(pname, params, &num_written)) {
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
}

void GLES2DecoderImpl::DoGetIntegerv(GLenum pname, GLint* params) {
  DCHECK(params);
  GLsizei num_written;
  if (!state_.GetStateAsGLint(pname, params, &num_written) &&
      !GetHelper(pname, params, &num_written)) {
    glGetIntegerv(pname, params);
  }
}

void GLES2DecoderImpl::DoGetProgramiv(
    GLuint program_id, GLenum pname, GLint* params) {
  Program* info = GetProgramInfoNotShader(
      program_id, "glGetProgramiv");
  if (!info) {
    return;
  }
  info->GetProgramiv(pname, params);
}

void GLES2DecoderImpl::DoBindAttribLocation(
    GLuint program, GLuint index, const char* name) {
  if (!StringIsValidForGLES(name)) {
    SetGLError(GL_INVALID_VALUE, "glBindAttribLocation", "Invalid character");
    return;
  }
  if (ProgramManager::IsInvalidPrefix(name, strlen(name))) {
    SetGLError(GL_INVALID_OPERATION, "glBindAttribLocation", "reserved prefix");
    return;
  }
  if (index >= group_->max_vertex_attribs()) {
    SetGLError(GL_INVALID_VALUE, "glBindAttribLocation", "index out of range");
    return;
  }
  Program* info = GetProgramInfoNotShader(
      program, "glBindAttribLocation");
  if (!info) {
    return;
  }
  info->SetAttribLocationBinding(name, static_cast<GLint>(index));
  glBindAttribLocation(info->service_id(), index, name);
}

error::Error GLES2DecoderImpl::HandleBindAttribLocation(
    uint32 immediate_data_size, const cmds::BindAttribLocation& c) {
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  uint32 name_size = c.data_size;
  const char* name = GetSharedMemoryAs<const char*>(
      c.name_shm_id, c.name_shm_offset, name_size);
  if (name == NULL) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  DoBindAttribLocation(program, index, name_str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindAttribLocationImmediate(
    uint32 immediate_data_size, const cmds::BindAttribLocationImmediate& c) {
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  uint32 name_size = c.data_size;
  const char* name = GetImmediateDataAs<const char*>(
      c, name_size, immediate_data_size);
  if (name == NULL) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  DoBindAttribLocation(program, index, name_str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindAttribLocationBucket(
    uint32 immediate_data_size, const cmds::BindAttribLocationBucket& c) {
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  DoBindAttribLocation(program, index, name_str.c_str());
  return error::kNoError;
}

void GLES2DecoderImpl::DoBindUniformLocationCHROMIUM(
    GLuint program, GLint location, const char* name) {
  if (!StringIsValidForGLES(name)) {
    SetGLError(GL_INVALID_VALUE,
               "glBindUniformLocationCHROMIUM", "Invalid character");
    return;
  }
  if (ProgramManager::IsInvalidPrefix(name, strlen(name))) {
    SetGLError(GL_INVALID_OPERATION,
               "glBindUniformLocationCHROMIUM", "reserved prefix");
    return;
  }
  if (location < 0 || static_cast<uint32>(location) >=
      (group_->max_fragment_uniform_vectors() +
       group_->max_vertex_uniform_vectors()) * 4) {
    SetGLError(GL_INVALID_VALUE,
               "glBindUniformLocationCHROMIUM", "location out of range");
    return;
  }
  Program* info = GetProgramInfoNotShader(
      program, "glBindUniformLocationCHROMIUM");
  if (!info) {
    return;
  }
  if (!info->SetUniformLocationBinding(name, location)) {
    SetGLError(GL_INVALID_VALUE,
               "glBindUniformLocationCHROMIUM", "location out of range");
  }
}

error::Error GLES2DecoderImpl::HandleBindUniformLocationCHROMIUM(
    uint32 immediate_data_size, const cmds::BindUniformLocationCHROMIUM& c) {
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  uint32 name_size = c.data_size;
  const char* name = GetSharedMemoryAs<const char*>(
      c.name_shm_id, c.name_shm_offset, name_size);
  if (name == NULL) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  DoBindUniformLocationCHROMIUM(program, location, name_str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindUniformLocationCHROMIUMImmediate(
    uint32 immediate_data_size,
    const cmds::BindUniformLocationCHROMIUMImmediate& c) {
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  uint32 name_size = c.data_size;
  const char* name = GetImmediateDataAs<const char*>(
      c, name_size, immediate_data_size);
  if (name == NULL) {
    return error::kOutOfBounds;
  }
  String name_str(name, name_size);
  DoBindUniformLocationCHROMIUM(program, location, name_str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindUniformLocationCHROMIUMBucket(
    uint32 immediate_data_size,
    const cmds::BindUniformLocationCHROMIUMBucket& c) {
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  DoBindUniformLocationCHROMIUM(program, location, name_str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteShader(
    uint32 immediate_data_size, const cmds::DeleteShader& c) {
  GLuint client_id = c.shader;
  if (client_id) {
    Shader* info = GetShader(client_id);
    if (info) {
      if (!info->IsDeleted()) {
        glDeleteShader(info->service_id());
        shader_manager()->MarkAsDeleted(info);
      }
    } else {
      SetGLError(GL_INVALID_VALUE, "glDeleteShader", "unknown shader");
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteProgram(
    uint32 immediate_data_size, const cmds::DeleteProgram& c) {
  GLuint client_id = c.program;
  if (client_id) {
    Program* info = GetProgram(client_id);
    if (info) {
      if (!info->IsDeleted()) {
        program_manager()->MarkAsDeleted(shader_manager(), info);
      }
    } else {
      SetGLError(GL_INVALID_VALUE, "glDeleteProgram", "unknown program");
    }
  }
  return error::kNoError;
}

void GLES2DecoderImpl::DoDeleteSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids) {
  IdAllocatorInterface* id_allocator = group_->GetIdAllocator(namespace_id);
  for (GLsizei ii = 0; ii < n; ++ii) {
    id_allocator->FreeID(ids[ii]);
  }
}

error::Error GLES2DecoderImpl::HandleDeleteSharedIdsCHROMIUM(
    uint32 immediate_data_size, const cmds::DeleteSharedIdsCHROMIUM& c) {
  GLuint namespace_id = static_cast<GLuint>(c.namespace_id);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* ids = GetSharedMemoryAs<const GLuint*>(
      c.ids_shm_id, c.ids_shm_offset, data_size);
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "DeleteSharedIdsCHROMIUM", "n < 0");
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
  IdAllocatorInterface* id_allocator = group_->GetIdAllocator(namespace_id);
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
    uint32 immediate_data_size, const cmds::GenSharedIdsCHROMIUM& c) {
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
    SetGLError(GL_INVALID_VALUE, "GenSharedIdsCHROMIUM", "n < 0");
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
  IdAllocatorInterface* id_allocator = group_->GetIdAllocator(namespace_id);
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (!id_allocator->MarkAsUsed(ids[ii])) {
      for (GLsizei jj = 0; jj < ii; ++jj) {
        id_allocator->FreeID(ids[jj]);
      }
      SetGLError(
          GL_INVALID_VALUE, "RegisterSharedIdsCHROMIUM",
          "attempt to register id that already exists");
      return;
    }
  }
}

error::Error GLES2DecoderImpl::HandleRegisterSharedIdsCHROMIUM(
    uint32 immediate_data_size, const cmds::RegisterSharedIdsCHROMIUM& c) {
  GLuint namespace_id = static_cast<GLuint>(c.namespace_id);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* ids = GetSharedMemoryAs<GLuint*>(
      c.ids_shm_id, c.ids_shm_offset, data_size);
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "RegisterSharedIdsCHROMIUM", "n < 0");
    return error::kNoError;
  }
  if (ids == NULL) {
    return error::kOutOfBounds;
  }
  DoRegisterSharedIdsCHROMIUM(namespace_id, n, ids);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::DoClear(GLbitfield mask) {
  DCHECK(!ShouldDeferDraws());
  if (CheckBoundFramebuffersValid("glClear")) {
    UNSHIPPED_TRACE_EVENT_INSTANT2(
        "test_gpu", "DoClear",
        "red", state_.color_clear_red,
        "green", state_.color_clear_green);
    ApplyDirtyState();
    glClear(mask);
  }
  return error::kNoError;
}

void GLES2DecoderImpl::DoFramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint client_renderbuffer_id) {
  Framebuffer* framebuffer_info =
      GetFramebufferInfoForTarget(target);
  if (!framebuffer_info) {
    SetGLError(GL_INVALID_OPERATION,
               "glFramebufferRenderbuffer", "no framebuffer bound");
    return;
  }
  GLuint service_id = 0;
  Renderbuffer* info = NULL;
  if (client_renderbuffer_id) {
    info = GetRenderbuffer(client_renderbuffer_id);
    if (!info) {
      SetGLError(GL_INVALID_OPERATION,
                 "glFramebufferRenderbuffer", "unknown renderbuffer");
      return;
    }
    service_id = info->service_id();
  }
  CopyRealGLErrorsToWrapper();
  glFramebufferRenderbufferEXT(
      target, attachment, renderbuffertarget, service_id);
  GLenum error = PeekGLError();
  if (error == GL_NO_ERROR) {
    framebuffer_info->AttachRenderbuffer(attachment, info);
  }
  if (framebuffer_info == state_.bound_draw_framebuffer) {
    clear_state_dirty_ = true;
  }
}

void GLES2DecoderImpl::DoDisable(GLenum cap) {
  if (SetCapabilityState(cap, false)) {
    glDisable(cap);
  }
}

void GLES2DecoderImpl::DoEnable(GLenum cap) {
  if (SetCapabilityState(cap, true)) {
    glEnable(cap);
  }
}

void GLES2DecoderImpl::DoDepthRangef(GLclampf znear, GLclampf zfar) {
  state_.z_near = std::min(1.0f, std::max(0.0f, znear));
  state_.z_far = std::min(1.0f, std::max(0.0f, zfar));
  glDepthRange(znear, zfar);
}

void GLES2DecoderImpl::DoHint(GLenum target, GLenum mode) {
  switch (target) {
    case GL_GENERATE_MIPMAP_HINT:
      state_.hint_generate_mipmap = mode;
      break;
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
      state_.hint_fragment_shader_derivative = mode;
      break;
    default:
      NOTREACHED();
  }
  glHint(target, mode);
}

void GLES2DecoderImpl::DoSampleCoverage(GLclampf value, GLboolean invert) {
  state_.sample_coverage_value = std::min(1.0f, std::max(0.0f, value));
  state_.sample_coverage_invert = (invert != 0);
  glSampleCoverage(state_.sample_coverage_value, invert);
}

// Assumes framebuffer is complete.
void GLES2DecoderImpl::ClearUnclearedAttachments(
    GLenum target, Framebuffer* info) {
  if (target == GL_READ_FRAMEBUFFER_EXT) {
    // bind this to the DRAW point, clear then bind back to READ
    // TODO(gman): I don't think there is any guarantee that an FBO that
    //   is complete on the READ attachment will be complete as a DRAW
    //   attachment.
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, info->service_id());
  }
  GLbitfield clear_bits = 0;
  if (info->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0)) {
    glClearColor(
        0.0f, 0.0f, 0.0f,
        (GLES2Util::GetChannelsForFormat(
             info->GetColorAttachmentFormat()) & 0x0008) != 0 ? 0.0f : 1.0f);
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

  framebuffer_manager()->MarkAttachmentsAsCleared(
      info, renderbuffer_manager(), texture_manager());

  RestoreClearState();

  if (target == GL_READ_FRAMEBUFFER_EXT) {
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, info->service_id());
    Framebuffer* framebuffer =
        GetFramebufferInfoForTarget(GL_DRAW_FRAMEBUFFER_EXT);
    GLuint service_id =
        framebuffer ? framebuffer->service_id() : GetBackbufferServiceId();
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, service_id);
  }
}

void GLES2DecoderImpl::RestoreClearState() {
  clear_state_dirty_ = true;
  glClearColor(
      state_.color_clear_red, state_.color_clear_green, state_.color_clear_blue,
      state_.color_clear_alpha);
  glClearStencil(state_.stencil_clear);
  glClearDepth(state_.depth_clear);
  if (state_.enable_flags.scissor_test) {
    glEnable(GL_SCISSOR_TEST);
  }
}

GLenum GLES2DecoderImpl::DoCheckFramebufferStatus(GLenum target) {
  Framebuffer* framebuffer =
      GetFramebufferInfoForTarget(target);
  if (!framebuffer) {
    return GL_FRAMEBUFFER_COMPLETE;
  }
  GLenum completeness = framebuffer->IsPossiblyComplete();
  if (completeness != GL_FRAMEBUFFER_COMPLETE) {
    return completeness;
  }
  return framebuffer->GetStatus(texture_manager(), target);
}

void GLES2DecoderImpl::DoFramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget,
    GLuint client_texture_id, GLint level) {
  Framebuffer* framebuffer_info =
      GetFramebufferInfoForTarget(target);
  if (!framebuffer_info) {
    SetGLError(GL_INVALID_OPERATION,
               "glFramebufferTexture2D", "no framebuffer bound.");
    return;
  }
  GLuint service_id = 0;
  Texture* info = NULL;
  if (client_texture_id) {
    info = GetTexture(client_texture_id);
    if (!info) {
      SetGLError(GL_INVALID_OPERATION,
                 "glFramebufferTexture2D", "unknown texture");
      return;
    }
    service_id = info->service_id();
  }

  if (!texture_manager()->ValidForTarget(textarget, level, 0, 0, 1)) {
    SetGLError(GL_INVALID_VALUE,
               "glFramebufferTexture2D", "level out of range");
    return;
  }

  CopyRealGLErrorsToWrapper();
  glFramebufferTexture2DEXT(target, attachment, textarget, service_id, level);
  GLenum error = PeekGLError();
  if (error == GL_NO_ERROR) {
    framebuffer_info->AttachTexture(attachment, info, textarget, level);
  }
  if (framebuffer_info == state_.bound_draw_framebuffer) {
    clear_state_dirty_ = true;
  }
}

void GLES2DecoderImpl::DoGetFramebufferAttachmentParameteriv(
    GLenum target, GLenum attachment, GLenum pname, GLint* params) {
  Framebuffer* framebuffer_info =
      GetFramebufferInfoForTarget(target);
  if (!framebuffer_info) {
    SetGLError(GL_INVALID_OPERATION,
               "glFramebufferAttachmentParameteriv", "no framebuffer bound");
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
  Renderbuffer* renderbuffer =
      GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
  if (!renderbuffer) {
    SetGLError(GL_INVALID_OPERATION,
               "glGetRenderbufferParameteriv", "no renderbuffer bound");
    return;
  }
  switch (pname) {
    case GL_RENDERBUFFER_INTERNAL_FORMAT:
      *params = renderbuffer->internal_format();
      break;
    case GL_RENDERBUFFER_WIDTH:
      *params = renderbuffer->width();
      break;
    case GL_RENDERBUFFER_HEIGHT:
      *params = renderbuffer->height();
      break;
    default:
      glGetRenderbufferParameterivEXT(target, pname, params);
      break;
  }
}

void GLES2DecoderImpl::DoBlitFramebufferEXT(
    GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
    GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
    GLbitfield mask, GLenum filter) {
  DCHECK(!ShouldDeferReads() && !ShouldDeferDraws());
  if (!features().chromium_framebuffer_multisample) {
    SetGLError(GL_INVALID_OPERATION,
               "glBlitFramebufferEXT", "function not available");
  }
  glDisable(GL_SCISSOR_TEST);
  if (IsAngle()) {
    glBlitFramebufferANGLE(
        srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
  } else {
    glBlitFramebufferEXT(
        srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
  }
  EnableDisable(GL_SCISSOR_TEST, state_.enable_flags.scissor_test);
  UNSHIPPED_TRACE_EVENT_INSTANT1("test_gpu", "DoBlit", "width", srcX1 - srcX0);
}

void GLES2DecoderImpl::DoRenderbufferStorageMultisample(
    GLenum target, GLsizei samples, GLenum internalformat,
    GLsizei width, GLsizei height) {
  if (!features().chromium_framebuffer_multisample) {
    SetGLError(GL_INVALID_OPERATION,
               "glRenderbufferStorageMultisampleEXT", "function not available");
    return;
  }

  Renderbuffer* renderbuffer =
      GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
  if (!renderbuffer) {
    SetGLError(GL_INVALID_OPERATION,
               "glRenderbufferStorageMultisampleEXT", "no renderbuffer bound");
    return;
  }

  if (samples > renderbuffer_manager()->max_samples()) {
    SetGLError(GL_INVALID_VALUE,
               "glRenderbufferStorageMultisampleEXT", "samples too large");
    return;
  }

  if (width > renderbuffer_manager()->max_renderbuffer_size() ||
      height > renderbuffer_manager()->max_renderbuffer_size()) {
    SetGLError(GL_INVALID_VALUE,
               "glRenderbufferStorageMultisample", "dimensions too large");
    return;
  }

  uint32 estimated_size = 0;
  if (!RenderbufferManager::ComputeEstimatedRenderbufferSize(
      width, height, samples, internalformat, &estimated_size)) {
    SetGLError(GL_OUT_OF_MEMORY,
               "glRenderbufferStorageMultsampleEXT", "dimensions too large");
    return;
  }

  if (!EnsureGPUMemoryAvailable(estimated_size)) {
    SetGLError(GL_OUT_OF_MEMORY,
               "glRenderbufferStorageMultsampleEXT", "out of memory");
    return;
  }

  GLenum impl_format = RenderbufferManager::
      InternalRenderbufferFormatToImplFormat(internalformat);
  CopyRealGLErrorsToWrapper();
  if (IsAngle()) {
    glRenderbufferStorageMultisampleANGLE(
        target, samples, impl_format, width, height);
  } else {
    glRenderbufferStorageMultisampleEXT(
        target, samples, impl_format, width, height);
  }
  GLenum error = PeekGLError();
  if (error == GL_NO_ERROR) {
    // TODO(gman): If renderbuffers tracked which framebuffers they were
    // attached to we could just mark those framebuffers as not complete.
    framebuffer_manager()->IncFramebufferStateChangeCount();
    renderbuffer_manager()->SetInfo(
        renderbuffer, samples, internalformat, width, height);
  }
}

void GLES2DecoderImpl::DoRenderbufferStorage(
  GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  Renderbuffer* renderbuffer =
      GetRenderbufferInfoForTarget(GL_RENDERBUFFER);
  if (!renderbuffer) {
    SetGLError(GL_INVALID_OPERATION,
               "glRenderbufferStorage", "no renderbuffer bound");
    return;
  }

  if (width > renderbuffer_manager()->max_renderbuffer_size() ||
      height > renderbuffer_manager()->max_renderbuffer_size()) {
    SetGLError(GL_INVALID_VALUE,
               "glRenderbufferStorage", "dimensions too large");
    return;
  }

  uint32 estimated_size = 0;
  if (!RenderbufferManager::ComputeEstimatedRenderbufferSize(
      width, height, 1, internalformat, &estimated_size)) {
    SetGLError(GL_OUT_OF_MEMORY, "glRenderbufferStorage",
               "dimensions too large");
    return;
  }

  if (!EnsureGPUMemoryAvailable(estimated_size)) {
    SetGLError(GL_OUT_OF_MEMORY, "glRenderbufferStorage", "out of memory");
    return;
  }

  CopyRealGLErrorsToWrapper();
  glRenderbufferStorageEXT(
      target, RenderbufferManager::
          InternalRenderbufferFormatToImplFormat(internalformat),
      width, height);
  GLenum error = PeekGLError();
  if (error == GL_NO_ERROR) {
    // TODO(gman): If tetxures tracked which framebuffers they were attached to
    // we could just mark those framebuffers as not complete.
    framebuffer_manager()->IncFramebufferStateChangeCount();
    renderbuffer_manager()->SetInfo(
        renderbuffer, 1, internalformat, width, height);
  }
}

void GLES2DecoderImpl::DoLinkProgram(GLuint program) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoLinkProgram");
  Program* info = GetProgramInfoNotShader(
      program, "glLinkProgram");
  if (!info) {
    return;
  }

  LogClientServiceForInfo(info, program, "glLinkProgram");
  ShaderTranslator* vertex_translator = NULL;
  ShaderTranslator* fragment_translator = NULL;
  if (use_shader_translator_) {
    vertex_translator = vertex_translator_;
    fragment_translator = fragment_translator_;
  }
  if (info->Link(shader_manager(),
                 vertex_translator,
                 fragment_translator,
                 feature_info_)) {
    if (info == state_.current_program.get()) {
      if (workarounds().use_current_program_after_successful_link) {
        glUseProgram(info->service_id());
      }
      program_manager()->ClearUniforms(info);
    }
  }
};

void GLES2DecoderImpl::DoTexParameterf(
    GLenum target, GLenum pname, GLfloat param) {
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glTexParameterf", "unknown texture");
    return;
  }

  GLenum error = texture_manager()->SetParameter(
      info, pname, static_cast<GLint>(param));
  if (error != GL_NO_ERROR) {
    SetGLErrorInvalidParam(
        error, "glTexParameterf", pname, static_cast<GLint>(param));
    return;
  }
  glTexParameterf(target, pname, param);
}

void GLES2DecoderImpl::DoTexParameteri(
    GLenum target, GLenum pname, GLint param) {
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glTexParameteri", "unknown texture");
    return;
  }

  GLenum error =  texture_manager()->SetParameter(info, pname, param);
  if (error != GL_NO_ERROR) {
    SetGLErrorInvalidParam(error, "glTexParameteri", pname, param);
    return;
  }
  // Texture tracking pools exist only for the command decoder, so
  // do not pass them on to the native GL implementation.
  if (pname == GL_TEXTURE_POOL_CHROMIUM) {
    return;
  }
  glTexParameteri(target, pname, param);
}

void GLES2DecoderImpl::DoTexParameterfv(
    GLenum target, GLenum pname, const GLfloat* params) {
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glTexParameterfv", "unknown texture");
    return;
  }

  GLenum error =texture_manager()->SetParameter(
      info, pname, static_cast<GLint>(params[0]));
  if (error != GL_NO_ERROR) {
    SetGLErrorInvalidParam(
        error, "glTexParameterfv", pname, static_cast<GLint>(params[0]));
    return;
  }
  glTexParameterfv(target, pname, params);
}

void GLES2DecoderImpl::DoTexParameteriv(
  GLenum target, GLenum pname, const GLint* params) {
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glTexParameteriv", "unknown texture");
    return;
  }

  GLenum error = texture_manager()->SetParameter(info, pname, *params);
  if (error != GL_NO_ERROR) {
    SetGLErrorInvalidParam(error, "glTexParameteriv", pname, *params);
    return;
  }
  glTexParameteriv(target, pname, params);
}

bool GLES2DecoderImpl::CheckCurrentProgram(const char* function_name) {
  if (!state_.current_program) {
    // The program does not exist.
    SetGLError(GL_INVALID_OPERATION, function_name, "no program in use");
    return false;
  }
  if (!state_.current_program->InUse()) {
    SetGLError(GL_INVALID_OPERATION, function_name, "program not linked");
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

namespace {

static const GLenum valid_int_vec1_types_list[] = {
  GL_INT,
  GL_BOOL,
  GL_SAMPLER_2D,
  GL_SAMPLER_2D_RECT_ARB,
  GL_SAMPLER_CUBE,
  GL_SAMPLER_EXTERNAL_OES,
};

static const GLenum valid_int_vec2_types_list[] = {
  GL_INT_VEC2,
  GL_BOOL_VEC2,
};

static const GLenum valid_int_vec3_types_list[] = {
  GL_INT_VEC3,
  GL_BOOL_VEC3,
};

static const GLenum valid_int_vec4_types_list[] = {
  GL_INT_VEC4,
  GL_BOOL_VEC4,
};

static const GLenum valid_float_vec1_types_list[] = {
  GL_FLOAT,
  GL_BOOL,
};

static const GLenum valid_float_vec2_types_list[] = {
  GL_FLOAT_VEC2,
  GL_BOOL_VEC2,
};

static const GLenum valid_float_vec3_types_list[] = {
  GL_FLOAT_VEC3,
  GL_BOOL_VEC3,
};

static const GLenum valid_float_vec4_types_list[] = {
  GL_FLOAT_VEC4,
  GL_BOOL_VEC4,
};

static const GLenum valid_float_mat2_types_list[] = {
  GL_FLOAT_MAT2,
};

static const GLenum valid_float_mat3_types_list[] = {
  GL_FLOAT_MAT3,
};

static const GLenum valid_float_mat4_types_list[] = {
  GL_FLOAT_MAT4,
};

static const GLES2DecoderImpl::BaseUniformInfo valid_int_vec1_base_info = {
  valid_int_vec1_types_list,
  arraysize(valid_int_vec1_types_list),
};

static const GLES2DecoderImpl::BaseUniformInfo valid_int_vec2_base_info = {
  valid_int_vec2_types_list,
  arraysize(valid_int_vec2_types_list),
};

static const GLES2DecoderImpl::BaseUniformInfo valid_int_vec3_base_info = {
  valid_int_vec3_types_list,
  arraysize(valid_int_vec3_types_list),
};

static const GLES2DecoderImpl::BaseUniformInfo valid_int_vec4_base_info = {
  valid_int_vec4_types_list,
  arraysize(valid_int_vec4_types_list),
};

static const GLES2DecoderImpl::BaseUniformInfo valid_float_vec1_base_info = {
  valid_float_vec1_types_list,
  arraysize(valid_float_vec1_types_list),
};

static const GLES2DecoderImpl::BaseUniformInfo valid_float_vec2_base_info = {
  valid_float_vec2_types_list,
  arraysize(valid_float_vec2_types_list),
};

static const GLES2DecoderImpl::BaseUniformInfo valid_float_vec3_base_info = {
  valid_float_vec3_types_list,
  arraysize(valid_float_vec3_types_list),
};

static const GLES2DecoderImpl::BaseUniformInfo valid_float_vec4_base_info = {
  valid_float_vec4_types_list,
  arraysize(valid_float_vec4_types_list),
};

static const GLES2DecoderImpl::BaseUniformInfo valid_float_mat2_base_info = {
  valid_float_mat2_types_list,
  arraysize(valid_float_mat2_types_list),
};

static const GLES2DecoderImpl::BaseUniformInfo valid_float_mat3_base_info = {
  valid_float_mat3_types_list,
  arraysize(valid_float_mat3_types_list),
};

static const GLES2DecoderImpl::BaseUniformInfo valid_float_mat4_base_info = {
  valid_float_mat4_types_list,
  arraysize(valid_float_mat4_types_list),
};

}  // anonymous namespace.

bool GLES2DecoderImpl::PrepForSetUniformByLocation(
    GLint fake_location, const char* function_name,
    const GLES2DecoderImpl::BaseUniformInfo& base_info,
    GLint* real_location, GLenum* type, GLsizei* count) {
  DCHECK(type);
  DCHECK(count);
  DCHECK(real_location);

  if (!CheckCurrentProgramForUniform(fake_location, function_name)) {
    return false;
  }
  GLint array_index = -1;
  const Program::UniformInfo* info =
      state_.current_program->GetUniformInfoByFakeLocation(
          fake_location, real_location, &array_index);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION, function_name, "unknown location");
    return false;
  }
  bool okay = false;
  for (size_t ii = 0; ii < base_info.num_valid_types; ++ii) {
    if (base_info.valid_types[ii] == info->type) {
      okay = true;
      break;
    }
  }
  if (!okay) {
    SetGLError(
        GL_INVALID_OPERATION, function_name, "wrong uniform function for type");
    return false;
  }
  if (*count > 1 && !info->is_array) {
    SetGLError(
        GL_INVALID_OPERATION, function_name, "count > 1 for non-array");
    return false;
  }
  *count = std::min(info->size - array_index, *count);
  if (*count <= 0) {
    return false;
  }
  *type = info->type;
  return true;
}

void GLES2DecoderImpl::DoUniform1i(GLint fake_location, GLint v0) {
  GLenum type = 0;
  GLsizei count = 1;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(
      fake_location, "glUniform1iv", valid_int_vec1_base_info,
      &real_location, &type, &count)) {
    return;
  }
  if (!state_.current_program->SetSamplers(
      state_.texture_units.size(), fake_location, 1, &v0)) {
    SetGLError(GL_INVALID_VALUE, "glUniform1i", "texture unit out of range");
    return;
  }
  glUniform1i(real_location, v0);
}

void GLES2DecoderImpl::DoUniform1iv(
    GLint fake_location, GLsizei count, const GLint *value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(
      fake_location, "glUniform1iv", valid_int_vec1_base_info,
      &real_location, &type, &count)) {
    return;
  }
  if (type == GL_SAMPLER_2D || type == GL_SAMPLER_2D_RECT_ARB ||
      type == GL_SAMPLER_CUBE || type == GL_SAMPLER_EXTERNAL_OES) {
    if (!state_.current_program->SetSamplers(
          state_.texture_units.size(), fake_location, count, value)) {
      SetGLError(GL_INVALID_VALUE, "glUniform1iv", "texture unit out of range");
      return;
    }
  }
  glUniform1iv(real_location, count, value);
}

void GLES2DecoderImpl::DoUniform1fv(
    GLint fake_location, GLsizei count, const GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(
      fake_location, "glUniform1fv", valid_float_vec1_base_info,
      &real_location, &type, &count)) {
    return;
  }
  if (type == GL_BOOL) {
    scoped_array<GLint> temp(new GLint[count]);
    for (GLsizei ii = 0; ii < count; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    DoUniform1iv(real_location, count, temp.get());
  } else {
    glUniform1fv(real_location, count, value);
  }
}

void GLES2DecoderImpl::DoUniform2fv(
    GLint fake_location, GLsizei count, const GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(
      fake_location, "glUniform2fv", valid_float_vec2_base_info,
      &real_location, &type, &count)) {
    return;
  }
  if (type == GL_BOOL_VEC2) {
    GLsizei num_values = count * 2;
    scoped_array<GLint> temp(new GLint[num_values]);
    for (GLsizei ii = 0; ii < num_values; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    glUniform2iv(real_location, count, temp.get());
  } else {
    glUniform2fv(real_location, count, value);
  }
}

void GLES2DecoderImpl::DoUniform3fv(
    GLint fake_location, GLsizei count, const GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(
      fake_location, "glUniform3fv", valid_float_vec3_base_info,
      &real_location, &type, &count)) {
    return;
  }
  if (type == GL_BOOL_VEC3) {
    GLsizei num_values = count * 3;
    scoped_array<GLint> temp(new GLint[num_values]);
    for (GLsizei ii = 0; ii < num_values; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    glUniform3iv(real_location, count, temp.get());
  } else {
    glUniform3fv(real_location, count, value);
  }
}

void GLES2DecoderImpl::DoUniform4fv(
    GLint fake_location, GLsizei count, const GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(
      fake_location, "glUniform4fv", valid_float_vec4_base_info,
      &real_location, &type, &count)) {
    return;
  }
  if (type == GL_BOOL_VEC4) {
    GLsizei num_values = count * 4;
    scoped_array<GLint> temp(new GLint[num_values]);
    for (GLsizei ii = 0; ii < num_values; ++ii) {
      temp[ii] = static_cast<GLint>(value[ii] != 0.0f);
    }
    glUniform4iv(real_location, count, temp.get());
  } else {
    glUniform4fv(real_location, count, value);
  }
}

void GLES2DecoderImpl::DoUniform2iv(
    GLint fake_location, GLsizei count, const GLint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(
      fake_location, "glUniform2iv", valid_int_vec2_base_info,
      &real_location, &type, &count)) {
    return;
  }
  glUniform2iv(real_location, count, value);
}

void GLES2DecoderImpl::DoUniform3iv(
    GLint fake_location, GLsizei count, const GLint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(
      fake_location, "glUniform3iv", valid_int_vec3_base_info,
      &real_location, &type, &count)) {
    return;
  }
  glUniform3iv(real_location, count, value);
}

void GLES2DecoderImpl::DoUniform4iv(
    GLint fake_location, GLsizei count, const GLint* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(
      fake_location, "glUniform4iv", valid_int_vec4_base_info,
      &real_location, &type, &count)) {
    return;
  }
  glUniform4iv(real_location, count, value);
}

void GLES2DecoderImpl::DoUniformMatrix2fv(
    GLint fake_location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(
      fake_location, "glUniformMatrix2fv", valid_float_mat2_base_info,
      &real_location, &type, &count)) {
    return;
  }
  glUniformMatrix2fv(real_location, count, transpose, value);
}

void GLES2DecoderImpl::DoUniformMatrix3fv(
    GLint fake_location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(
      fake_location, "glUniformMatrix3fv",  valid_float_mat3_base_info,
      &real_location, &type, &count)) {
    return;
  }
  glUniformMatrix3fv(real_location, count, transpose, value);
}

void GLES2DecoderImpl::DoUniformMatrix4fv(
    GLint fake_location, GLsizei count, GLboolean transpose,
    const GLfloat* value) {
  GLenum type = 0;
  GLint real_location = -1;
  if (!PrepForSetUniformByLocation(
      fake_location, "glUniformMatrix4fv",  valid_float_mat4_base_info,
      &real_location, &type, &count)) {
    return;
  }
  glUniformMatrix4fv(real_location, count, transpose, value);
}

void GLES2DecoderImpl::DoUseProgram(GLuint program) {
  GLuint service_id = 0;
  Program* info = NULL;
  if (program) {
    info = GetProgramInfoNotShader(program, "glUseProgram");
    if (!info) {
      return;
    }
    if (!info->IsValid()) {
      // Program was not linked successfully. (ie, glLinkProgram)
      SetGLError(GL_INVALID_OPERATION, "glUseProgram", "program not linked");
      return;
    }
    service_id = info->service_id();
  }
  if (state_.current_program) {
    program_manager()->UnuseProgram(shader_manager(), state_.current_program);
  }
  state_.current_program = info;
  LogClientServiceMapping("glUseProgram", program, service_id);
  glUseProgram(service_id);
  if (state_.current_program) {
    program_manager()->UseProgram(state_.current_program);
  }
}

uint32 GLES2DecoderImpl::GetGLError() {
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

unsigned GLES2DecoderImpl::PeekGLError() {
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    SetGLError(error, "", "");
  }
  return error;
}

void GLES2DecoderImpl::SetGLError(
    unsigned error, const char* function_name, const char* msg) {
  if (msg) {
    last_error_ = msg;
    LogMessage(GetLogPrefix() + ": " + std::string("GL ERROR :") +
               GLES2Util::GetStringEnum(error) + " : " +
               function_name + ": " + msg);
  }
  error_bits_ |= GLES2Util::GLErrorToErrorBit(error);
}

void GLES2DecoderImpl::SetGLErrorInvalidEnum(
    const char* function_name, unsigned value, const char* label) {
  SetGLError(GL_INVALID_ENUM, function_name,
             (std::string(label) + " was " +
              GLES2Util::GetStringEnum(value)).c_str());
}

void GLES2DecoderImpl::SetGLErrorInvalidParam(
    unsigned error,
    const char* function_name,
    unsigned pname,
    int param) {
  if (error == GL_INVALID_ENUM) {
    SetGLError(
        GL_INVALID_ENUM, function_name,
        (std::string("trying to set ") +
         GLES2Util::GetStringEnum(pname) + " to " +
         GLES2Util::GetStringEnum(param)).c_str());
  } else {
    SetGLError(
        error, function_name,
        (std::string("trying to set ") +
         GLES2Util::GetStringEnum(pname) + " to " +
         base::StringPrintf("%d", param)).c_str());
  }
}

const std::string& GLES2DecoderImpl::GetLogPrefix() const {
  const std::string& prefix(debug_marker_manager_.GetMarker());
  return prefix.empty() ? this_in_hex_ : prefix;
}

void GLES2DecoderImpl::LogMessage(const std::string& msg) {
  if (log_message_count_ < kMaxLogMessages ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGLErrorLimit)) {
    ++log_message_count_;
    // LOG this unless logging is turned off as any chromium code that
    // generates these errors probably has a bug.
    if (log_synthesized_gl_errors()) {
      LOG(ERROR) << msg;
    }
    if (!msg_callback_.is_null()) {
      msg_callback_.Run(0, msg);
    }
  } else {
    if (log_message_count_ == kMaxLogMessages) {
      ++log_message_count_;
      LOG(ERROR)
          << "Too many GL errors, not reporting any more for this context."
          << " use --disable-gl-error-limit to see all errors.";
    }
  }
}

void GLES2DecoderImpl::RenderWarning(const std::string& msg) {
  LogMessage(std::string("RENDER WARNING: ") + msg);
}

void GLES2DecoderImpl::PerformanceWarning(const std::string& msg) {
  LogMessage(std::string("PERFORMANCE WARNING: ") + msg);
}

void GLES2DecoderImpl::ForceCompileShaderIfPending(
    Shader* info) {
  if (info->compilation_status() ==
      Shader::PENDING_DEFERRED_COMPILE) {
    ShaderTranslator* translator = NULL;
    if (use_shader_translator_) {
      translator = info->shader_type() == GL_VERTEX_SHADER ?
          vertex_translator_.get() : fragment_translator_.get();
    }
    // We know there will be no errors, because we only defer compilation on
    // shaders that were previously compiled successfully.
    program_manager()->ForceCompileShader(info->deferred_compilation_source(),
                                          info,
                                          translator,
                                          feature_info_);
  }
}

void GLES2DecoderImpl::CopyRealGLErrorsToWrapper() {
  GLenum error;
  while ((error = glGetError()) != GL_NO_ERROR) {
    SetGLError(error, "", NULL);
  }
}

void GLES2DecoderImpl::ClearRealGLErrors() {
  GLenum error;
  while ((error = glGetError()) != GL_NO_ERROR) {
    if (error != GL_OUT_OF_MEMORY) {
      // GL_OUT_OF_MEMORY can legally happen on lost device.
      NOTREACHED() << "GL error " << error << " was unhandled.";
    }
  }
}

bool GLES2DecoderImpl::SetBlackTextureForNonRenderableTextures() {
  DCHECK(state_.current_program);
  // Only check if there are some unrenderable textures.
  if (!texture_manager()->HaveUnrenderableTextures()) {
    return false;
  }
  bool textures_set = false;
  const Program::SamplerIndices& sampler_indices =
     state_.current_program->sampler_indices();
  for (size_t ii = 0; ii < sampler_indices.size(); ++ii) {
    const Program::UniformInfo* uniform_info =
        state_.current_program->GetUniformInfo(sampler_indices[ii]);
    DCHECK(uniform_info);
    for (size_t jj = 0; jj < uniform_info->texture_units.size(); ++jj) {
      GLuint texture_unit_index = uniform_info->texture_units[jj];
      if (texture_unit_index < state_.texture_units.size()) {
        TextureUnit& texture_unit = state_.texture_units[texture_unit_index];
        Texture* texture_info =
            texture_unit.GetInfoForSamplerType(uniform_info->type);
        if (!texture_info || !texture_manager()->CanRender(texture_info)) {
          textures_set = true;
          glActiveTexture(GL_TEXTURE0 + texture_unit_index);
          glBindTexture(
              GetBindTargetForSamplerType(uniform_info->type),
              texture_manager()->black_texture_id(uniform_info->type));
          RenderWarning(
              std::string("texture bound to texture unit ") +
              base::IntToString(texture_unit_index) +
              " is not renderable. It maybe non-power-of-2 and have "
              " incompatible texture filtering or is not "
              "'texture complete'");
        }
      }
      // else: should this be an error?
    }
  }
  return textures_set;
}

void GLES2DecoderImpl::RestoreStateForNonRenderableTextures() {
  DCHECK(state_.current_program);
  const Program::SamplerIndices& sampler_indices =
     state_.current_program->sampler_indices();
  for (size_t ii = 0; ii < sampler_indices.size(); ++ii) {
    const Program::UniformInfo* uniform_info =
        state_.current_program->GetUniformInfo(sampler_indices[ii]);
    DCHECK(uniform_info);
    for (size_t jj = 0; jj < uniform_info->texture_units.size(); ++jj) {
      GLuint texture_unit_index = uniform_info->texture_units[jj];
      if (texture_unit_index < state_.texture_units.size()) {
        TextureUnit& texture_unit = state_.texture_units[texture_unit_index];
        Texture* texture_info =
            uniform_info->type == GL_SAMPLER_2D ?
                texture_unit.bound_texture_2d :
                texture_unit.bound_texture_cube_map;
        if (!texture_info || !texture_manager()->CanRender(texture_info)) {
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
  glActiveTexture(GL_TEXTURE0 + state_.active_texture_unit);
}

bool GLES2DecoderImpl::ClearUnclearedTextures() {
  // Only check if there are some uncleared textures.
  if (!texture_manager()->HaveUnsafeTextures()) {
    return true;
  }

  // 1: Check all textures we are about to render with.
  if (state_.current_program) {
    const Program::SamplerIndices& sampler_indices =
       state_.current_program->sampler_indices();
    for (size_t ii = 0; ii < sampler_indices.size(); ++ii) {
      const Program::UniformInfo* uniform_info =
          state_.current_program->GetUniformInfo(sampler_indices[ii]);
      DCHECK(uniform_info);
      for (size_t jj = 0; jj < uniform_info->texture_units.size(); ++jj) {
        GLuint texture_unit_index = uniform_info->texture_units[jj];
        if (texture_unit_index < state_.texture_units.size()) {
          TextureUnit& texture_unit = state_.texture_units[texture_unit_index];
          Texture* texture_info =
              texture_unit.GetInfoForSamplerType(uniform_info->type);
          if (texture_info && !texture_info->SafeToRenderFrom()) {
            if (!texture_manager()->ClearRenderableLevels(this, texture_info)) {
              return false;
            }
          }
        }
      }
    }
  }
  return true;
}

bool GLES2DecoderImpl::IsDrawValid(
    const char* function_name, GLuint max_vertex_accessed, GLsizei primcount) {
  // NOTE: We specifically do not check current_program->IsValid() because
  // it could never be invalid since glUseProgram would have failed. While
  // glLinkProgram could later mark the program as invalid the previous
  // valid program will still function if it is still the current program.
  if (!state_.current_program) {
    // The program does not exist.
    // But GL says no ERROR.
    RenderWarning("Drawing with no current shader program.");
    return false;
  }

  // true if any enabled, used divisor is zero
  bool divisor0 = false;
  // Validate all attribs currently enabled. If they are used by the current
  // program then check that they have enough elements to handle the draw call.
  // If they are not used by the current program check that they have a buffer
  // assigned.
  const VertexAttribManager::VertexAttribInfoList& infos =
      state_.vertex_attrib_manager->GetEnabledVertexAttribInfos();
  for (VertexAttribManager::VertexAttribInfoList::const_iterator it =
       infos.begin(); it != infos.end(); ++it) {
    const VertexAttrib* info = *it;
    const Program::VertexAttrib* attrib_info =
        state_.current_program->GetAttribInfoByLocation(info->index());
    if (attrib_info) {
      divisor0 |= (info->divisor() == 0);
      GLuint count = info->MaxVertexAccessed(primcount, max_vertex_accessed);
      // This attrib is used in the current program.
      if (!info->CanAccess(count)) {
        SetGLError(
            GL_INVALID_OPERATION, function_name,
            (std::string(
                 "attempt to access out of range vertices in attribute ") +
             base::IntToString(info->index())).c_str());
        return false;
      }
    } else {
      // This attrib is not used in the current program.
      if (!info->buffer()) {
        SetGLError(
            GL_INVALID_OPERATION, function_name,
            (std::string(
                 "attempt to render with no buffer attached to "
                 "enabled attribute ") +
                 base::IntToString(info->index())).c_str());
        return false;
      }
    }
  }

  if (primcount && !divisor0) {
    SetGLError(
        GL_INVALID_OPERATION, function_name,
        "attempt instanced render with all attributes having "
        "non-zero divisors");
    return false;
  }

  return true;
}

bool GLES2DecoderImpl::SimulateAttrib0(
    const char* function_name, GLuint max_vertex_accessed, bool* simulated) {
  DCHECK(simulated);
  *simulated = false;

  if (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2)
    return true;

  const VertexAttrib* info =
      state_.vertex_attrib_manager->GetVertexAttrib(0);
  // If it's enabled or it's not used then we don't need to do anything.
  bool attrib_0_used =
      state_.current_program->GetAttribInfoByLocation(0) != NULL;
  if (info->enabled() && attrib_0_used) {
    return true;
  }

  // Make a buffer with a single repeated vec4 value enough to
  // simulate the constant value that is supposed to be here.
  // This is required to emulate GLES2 on GL.
  GLuint num_vertices = max_vertex_accessed + 1;
  uint32 size_needed = 0;

  if (num_vertices == 0 ||
      !SafeMultiplyUint32(num_vertices, sizeof(Vec4), &size_needed) ||
      size_needed > 0x7FFFFFFFU) {
    SetGLError(GL_OUT_OF_MEMORY, function_name, "Simulating attrib 0");
    return false;
  }

  PerformanceWarning(
      "Attribute 0 is disabled. This has signficant performance penalty");

  CopyRealGLErrorsToWrapper();
  glBindBuffer(GL_ARRAY_BUFFER, attrib_0_buffer_id_);

  bool new_buffer = static_cast<GLsizei>(size_needed) > attrib_0_size_;
  if (new_buffer) {
    glBufferData(GL_ARRAY_BUFFER, size_needed, NULL, GL_DYNAMIC_DRAW);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
      SetGLError(GL_OUT_OF_MEMORY, function_name, "Simulating attrib 0");
      return false;
    }
  }

  const Vec4& value = state_.attrib_values[0];
  if (new_buffer ||
      (attrib_0_used &&
       (!attrib_0_buffer_matches_value_ ||
        (value.v[0] != attrib_0_value_.v[0] ||
         value.v[1] != attrib_0_value_.v[1] ||
         value.v[2] != attrib_0_value_.v[2] ||
         value.v[3] != attrib_0_value_.v[3])))) {
    std::vector<Vec4> temp(num_vertices, value);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size_needed, &temp[0].v[0]);
    attrib_0_buffer_matches_value_ = true;
    attrib_0_value_ = value;
    attrib_0_size_ = size_needed;
  }

  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);

  if (info->divisor())
    glVertexAttribDivisorANGLE(0, 0);

  *simulated = true;
  return true;
}

void GLES2DecoderImpl::RestoreStateForAttrib(GLuint attrib) {
  const VertexAttrib* info =
      state_.vertex_attrib_manager->GetVertexAttrib(attrib);
  const void* ptr = reinterpret_cast<const void*>(info->offset());
  Buffer* buffer_info = info->buffer();
  glBindBuffer(GL_ARRAY_BUFFER, buffer_info ? buffer_info->service_id() : 0);
  glVertexAttribPointer(
      attrib, info->size(), info->type(), info->normalized(), info->gl_stride(),
      ptr);
  if (info->divisor())
    glVertexAttribDivisorANGLE(attrib, info->divisor());
  glBindBuffer(
      GL_ARRAY_BUFFER,
      state_.bound_array_buffer ? state_.bound_array_buffer->service_id() : 0);

  // Never touch vertex attribute 0's state (in particular, never
  // disable it) when running on desktop GL because it will never be
  // re-enabled.
  if (attrib != 0 ||
      gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2) {
    if (info->enabled()) {
      glEnableVertexAttribArray(attrib);
    } else {
      glDisableVertexAttribArray(attrib);
    }
  }
}

bool GLES2DecoderImpl::SimulateFixedAttribs(
    const char* function_name,
    GLuint max_vertex_accessed, bool* simulated, GLsizei primcount) {
  DCHECK(simulated);
  *simulated = false;
  if (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2)
    return true;

  if (!state_.vertex_attrib_manager->HaveFixedAttribs()) {
    return true;
  }

  PerformanceWarning(
      "GL_FIXED attributes have a signficant performance penalty");

  // NOTE: we could be smart and try to check if a buffer is used
  // twice in 2 different attribs, find the overlapping parts and therefore
  // duplicate the minimum amount of data but this whole code path is not meant
  // to be used normally. It's just here to pass that OpenGL ES 2.0 conformance
  // tests so we just add to the buffer attrib used.

  GLuint elements_needed = 0;
  const VertexAttribManager::VertexAttribInfoList& infos =
      state_.vertex_attrib_manager->GetEnabledVertexAttribInfos();
  for (VertexAttribManager::VertexAttribInfoList::const_iterator it =
       infos.begin(); it != infos.end(); ++it) {
    const VertexAttrib* info = *it;
    const Program::VertexAttrib* attrib_info =
        state_.current_program->GetAttribInfoByLocation(info->index());
    GLuint max_accessed = info->MaxVertexAccessed(primcount,
                                                  max_vertex_accessed);
    GLuint num_vertices = max_accessed + 1;
    if (num_vertices == 0) {
      SetGLError(GL_OUT_OF_MEMORY, function_name, "Simulating attrib 0");
      return false;
    }
    if (attrib_info &&
        info->CanAccess(max_accessed) &&
        info->type() == GL_FIXED) {
      uint32 elements_used = 0;
      if (!SafeMultiplyUint32(num_vertices, info->size(), &elements_used) ||
          !SafeAddUint32(elements_needed, elements_used, &elements_needed)) {
        SetGLError(
            GL_OUT_OF_MEMORY, function_name, "simulating GL_FIXED attribs");
        return false;
      }
    }
  }

  const uint32 kSizeOfFloat = sizeof(float);  // NOLINT
  uint32 size_needed = 0;
  if (!SafeMultiplyUint32(elements_needed, kSizeOfFloat, &size_needed) ||
      size_needed > 0x7FFFFFFFU) {
    SetGLError(GL_OUT_OF_MEMORY, function_name, "simulating GL_FIXED attribs");
    return false;
  }

  CopyRealGLErrorsToWrapper();

  glBindBuffer(GL_ARRAY_BUFFER, fixed_attrib_buffer_id_);
  if (static_cast<GLsizei>(size_needed) > fixed_attrib_buffer_size_) {
    glBufferData(GL_ARRAY_BUFFER, size_needed, NULL, GL_DYNAMIC_DRAW);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
      SetGLError(
          GL_OUT_OF_MEMORY, function_name, "simulating GL_FIXED attribs");
      return false;
    }
  }

  // Copy the elements and convert to float
  GLintptr offset = 0;
  for (VertexAttribManager::VertexAttribInfoList::const_iterator it =
           infos.begin(); it != infos.end(); ++it) {
    const VertexAttrib* info = *it;
    const Program::VertexAttrib* attrib_info =
        state_.current_program->GetAttribInfoByLocation(info->index());
    GLuint max_accessed = info->MaxVertexAccessed(primcount,
                                                  max_vertex_accessed);
    GLuint num_vertices = max_accessed + 1;
    if (num_vertices == 0) {
      SetGLError(GL_OUT_OF_MEMORY, function_name, "Simulating attrib 0");
      return false;
    }
    if (attrib_info &&
        info->CanAccess(max_accessed) &&
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
  glBindBuffer(
      GL_ARRAY_BUFFER,
      state_.bound_array_buffer ? state_.bound_array_buffer->service_id() : 0);
}

error::Error GLES2DecoderImpl::DoDrawArrays(
    const char* function_name,
    bool instanced,
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount) {
  if (ShouldDeferDraws())
    return error::kDeferCommandUntilLater;
  if (!validators_->draw_mode.IsValid(mode)) {
    SetGLErrorInvalidEnum(function_name, mode, "mode");
    return error::kNoError;
  }
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, function_name, "count < 0");
    return error::kNoError;
  }
  if (primcount < 0) {
    SetGLError(GL_INVALID_VALUE, function_name, "primcount < 0");
    return error::kNoError;
  }
  if (!CheckBoundFramebuffersValid(function_name)) {
    return error::kNoError;
  }
  // We have to check this here because the prototype for glDrawArrays
  // is GLint not GLsizei.
  if (first < 0) {
    SetGLError(GL_INVALID_VALUE, function_name, "first < 0");
    return error::kNoError;
  }

  if (count == 0 || (instanced && primcount == 0)) {
    RenderWarning("Render count or primcount is 0.");
    return error::kNoError;
  }

  GLuint max_vertex_accessed = first + count - 1;
  if (IsDrawValid(function_name, max_vertex_accessed, primcount)) {
    if (!ClearUnclearedTextures()) {
      SetGLError(GL_INVALID_VALUE, function_name, "out of memory");
      return error::kNoError;
    }
    bool simulated_attrib_0 = false;
    if (!SimulateAttrib0(
        function_name, max_vertex_accessed, &simulated_attrib_0)) {
      return error::kNoError;
    }
    bool simulated_fixed_attribs = false;
    if (SimulateFixedAttribs(
        function_name, max_vertex_accessed, &simulated_fixed_attribs,
        primcount)) {
      bool textures_set = SetBlackTextureForNonRenderableTextures();
      ApplyDirtyState();
      if (!instanced) {
        glDrawArrays(mode, first, count);
      } else {
        glDrawArraysInstancedANGLE(mode, first, count, primcount);
      }
      ProcessPendingQueries();
      if (textures_set) {
        RestoreStateForNonRenderableTextures();
      }
      if (simulated_fixed_attribs) {
        RestoreStateForSimulatedFixedAttribs();
      }
    }
    if (simulated_attrib_0) {
      RestoreStateForAttrib(0);
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDrawArrays(
    uint32 immediate_data_size, const cmds::DrawArrays& c) {
  return DoDrawArrays("glDrawArrays",
                      false,
                      static_cast<GLenum>(c.mode),
                      static_cast<GLint>(c.first),
                      static_cast<GLsizei>(c.count),
                      0);
}

error::Error GLES2DecoderImpl::HandleDrawArraysInstancedANGLE(
    uint32 immediate_data_size, const cmds::DrawArraysInstancedANGLE& c) {
  if (!features().angle_instanced_arrays) {
    SetGLError(GL_INVALID_OPERATION,
               "glDrawArraysInstancedANGLE", "function not available");
    return error::kNoError;
  }
  return DoDrawArrays("glDrawArraysIntancedANGLE",
                      true,
                      static_cast<GLenum>(c.mode),
                      static_cast<GLint>(c.first),
                      static_cast<GLsizei>(c.count),
                      static_cast<GLsizei>(c.primcount));
}

error::Error GLES2DecoderImpl::DoDrawElements(
    const char* function_name,
    bool instanced,
    GLenum mode,
    GLsizei count,
    GLenum type,
    int32 offset,
    GLsizei primcount) {
  if (ShouldDeferDraws())
    return error::kDeferCommandUntilLater;
  if (!state_.vertex_attrib_manager->element_array_buffer()) {
    SetGLError(GL_INVALID_OPERATION,
               function_name, "No element array buffer bound");
    return error::kNoError;
  }

  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, function_name, "count < 0");
    return error::kNoError;
  }
  if (offset < 0) {
    SetGLError(GL_INVALID_VALUE, function_name, "offset < 0");
    return error::kNoError;
  }
  if (!validators_->draw_mode.IsValid(mode)) {
    SetGLErrorInvalidEnum(function_name, mode, "mode");
    return error::kNoError;
  }
  if (!validators_->index_type.IsValid(type)) {
    SetGLErrorInvalidEnum(function_name, type, "type");
    return error::kNoError;
  }
  if (primcount < 0) {
    SetGLError(GL_INVALID_VALUE, function_name, "primcount < 0");
    return error::kNoError;
  }

  if (!CheckBoundFramebuffersValid(function_name)) {
    return error::kNoError;
  }

  if (count == 0 || (instanced && primcount == 0)) {
    return error::kNoError;
  }

  GLuint max_vertex_accessed;
  if (!state_.vertex_attrib_manager->element_array_buffer()->
      GetMaxValueForRange(offset, count, type, &max_vertex_accessed)) {
    SetGLError(GL_INVALID_OPERATION,
               function_name, "range out of bounds for buffer");
    return error::kNoError;
  }

  if (IsDrawValid(function_name, max_vertex_accessed, primcount)) {
    if (!ClearUnclearedTextures()) {
      SetGLError(GL_INVALID_VALUE, function_name, "out of memory");
      return error::kNoError;
    }
    bool simulated_attrib_0 = false;
    if (!SimulateAttrib0(
        function_name, max_vertex_accessed, &simulated_attrib_0)) {
      return error::kNoError;
    }
    bool simulated_fixed_attribs = false;
    if (SimulateFixedAttribs(
        function_name, max_vertex_accessed, &simulated_fixed_attribs,
        primcount)) {
      bool textures_set = SetBlackTextureForNonRenderableTextures();
      ApplyDirtyState();
      const GLvoid* indices = reinterpret_cast<const GLvoid*>(offset);
      if (!instanced) {
        glDrawElements(mode, count, type, indices);
      } else {
        glDrawElementsInstancedANGLE(mode, count, type, indices, primcount);
      }
      ProcessPendingQueries();
      if (textures_set) {
        RestoreStateForNonRenderableTextures();
      }
      if (simulated_fixed_attribs) {
        RestoreStateForSimulatedFixedAttribs();
      }
    }
    if (simulated_attrib_0) {
      RestoreStateForAttrib(0);
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDrawElements(
    uint32 immediate_data_size, const cmds::DrawElements& c) {
  return DoDrawElements("glDrawElements",
                        false,
                        static_cast<GLenum>(c.mode),
                        static_cast<GLsizei>(c.count),
                        static_cast<GLenum>(c.type),
                        static_cast<int32>(c.index_offset),
                        0);
}

error::Error GLES2DecoderImpl::HandleDrawElementsInstancedANGLE(
    uint32 immediate_data_size, const cmds::DrawElementsInstancedANGLE& c) {
  if (!features().angle_instanced_arrays) {
    SetGLError(GL_INVALID_OPERATION,
               "glDrawElementsInstancedANGLE", "function not available");
    return error::kNoError;
  }
  return DoDrawElements("glDrawElementsInstancedANGLE",
                        true,
                        static_cast<GLenum>(c.mode),
                        static_cast<GLsizei>(c.count),
                        static_cast<GLenum>(c.type),
                        static_cast<int32>(c.index_offset),
                        static_cast<GLsizei>(c.primcount));
}

GLuint GLES2DecoderImpl::DoGetMaxValueInBufferCHROMIUM(
    GLuint buffer_id, GLsizei count, GLenum type, GLuint offset) {
  GLuint max_vertex_accessed = 0;
  Buffer* info = GetBuffer(buffer_id);
  if (!info) {
    // TODO(gman): Should this be a GL error or a command buffer error?
    SetGLError(GL_INVALID_VALUE,
               "GetMaxValueInBufferCHROMIUM", "unknown buffer");
  } else {
    if (!info->GetMaxValueForRange(offset, count, type, &max_vertex_accessed)) {
      // TODO(gman): Should this be a GL error or a command buffer error?
      SetGLError(
          GL_INVALID_OPERATION,
          "GetMaxValueInBufferCHROMIUM", "range out of bounds for buffer");
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
  std::string str(data, data + data_size);
  Shader* info = GetShaderInfoNotProgram(
      client_id, "glShaderSource");
  if (!info) {
    return error::kNoError;
  }
  // Note: We don't actually call glShaderSource here. We wait until
  // the call to glCompileShader.
  info->UpdateSource(str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleShaderSource(
    uint32 immediate_data_size, const cmds::ShaderSource& c) {
  uint32 data_size = c.data_size;
  const char* data = GetSharedMemoryAs<const char*>(
      c.data_shm_id, c.data_shm_offset, data_size);
  if (!data) {
    return error::kOutOfBounds;
  }
  return ShaderSourceHelper(c.shader, data, data_size);
}

error::Error GLES2DecoderImpl::HandleShaderSourceImmediate(
  uint32 immediate_data_size, const cmds::ShaderSourceImmediate& c) {
  uint32 data_size = c.data_size;
  const char* data = GetImmediateDataAs<const char*>(
      c, data_size, immediate_data_size);
  if (!data) {
    return error::kOutOfBounds;
  }
  return ShaderSourceHelper(c.shader, data, data_size);
}

error::Error GLES2DecoderImpl::HandleShaderSourceBucket(
  uint32 immediate_data_size, const cmds::ShaderSourceBucket& c) {
  Bucket* bucket = GetBucket(c.data_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  return ShaderSourceHelper(
      c.shader, bucket->GetDataAs<const char*>(0, bucket->size() - 1),
      bucket->size() - 1);
}

void GLES2DecoderImpl::DoCompileShader(GLuint client_id) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoCompileShader");
  Shader* info = GetShaderInfoNotProgram(
      client_id, "glCompileShader");
  if (!info) {
    return;
  }
  ShaderTranslator* translator = NULL;
  if (use_shader_translator_) {
    translator = info->shader_type() == GL_VERTEX_SHADER ?
        vertex_translator_.get() : fragment_translator_.get();
  }

  program_manager()->DoCompileShader(info, translator, feature_info_);
};

void GLES2DecoderImpl::DoGetShaderiv(
    GLuint shader, GLenum pname, GLint* params) {
  Shader* info = GetShaderInfoNotProgram(
      shader, "glGetShaderiv");
  if (!info) {
    return;
  }
  switch (pname) {
    case GL_SHADER_SOURCE_LENGTH:
      *params = info->source() ? info->source()->size() + 1 : 0;
      return;
    case GL_COMPILE_STATUS:
      *params = compile_shader_always_succeeds_ ? true : info->IsValid();
      return;
    case GL_INFO_LOG_LENGTH:
      *params = info->log_info() ? info->log_info()->size() + 1 : 0;
      return;
    case GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE:
      ForceCompileShaderIfPending(info);
      *params = info->translated_source() ?
          info->translated_source()->size() + 1 : 0;
      return;
    default:
      break;
  }
  glGetShaderiv(info->service_id(), pname, params);
}

error::Error GLES2DecoderImpl::HandleGetShaderSource(
    uint32 immediate_data_size, const cmds::GetShaderSource& c) {
  GLuint shader = c.shader;
  uint32 bucket_id = static_cast<uint32>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  Shader* info = GetShaderInfoNotProgram(
      shader, "glGetShaderSource");
  if (!info || !info->source()) {
    bucket->SetSize(0);
    return error::kNoError;
  }
  bucket->SetFromString(info->source()->c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetTranslatedShaderSourceANGLE(
    uint32 immediate_data_size,
    const cmds::GetTranslatedShaderSourceANGLE& c) {
  GLuint shader = c.shader;

  uint32 bucket_id = static_cast<uint32>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  Shader* info = GetShaderInfoNotProgram(
      shader, "glTranslatedGetShaderSourceANGLE");
  if (!info) {
    bucket->SetSize(0);
    return error::kNoError;
  }
  ForceCompileShaderIfPending(info);

  bucket->SetFromString(info->translated_source() ?
      info->translated_source()->c_str() : NULL);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetProgramInfoLog(
    uint32 immediate_data_size, const cmds::GetProgramInfoLog& c) {
  GLuint program = c.program;
  uint32 bucket_id = static_cast<uint32>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  Program* info = GetProgramInfoNotShader(
      program, "glGetProgramInfoLog");
  if (!info || !info->log_info()) {
    bucket->SetFromString("");
    return error::kNoError;
  }
  bucket->SetFromString(info->log_info()->c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetShaderInfoLog(
    uint32 immediate_data_size, const cmds::GetShaderInfoLog& c) {
  GLuint shader = c.shader;
  uint32 bucket_id = static_cast<uint32>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);
  Shader* info = GetShaderInfoNotProgram(
      shader, "glGetShaderInfoLog");
  if (!info || !info->log_info()) {
    bucket->SetFromString("");
    return error::kNoError;
  }
  bucket->SetFromString(info->log_info()->c_str());
  return error::kNoError;
}

bool GLES2DecoderImpl::DoIsEnabled(GLenum cap) {
  return state_.GetEnabled(cap);
}

bool GLES2DecoderImpl::DoIsBuffer(GLuint client_id) {
  const Buffer* buffer = GetBuffer(client_id);
  return buffer && buffer->IsValid() && !buffer->IsDeleted();
}

bool GLES2DecoderImpl::DoIsFramebuffer(GLuint client_id) {
  const Framebuffer* framebuffer =
      GetFramebuffer(client_id);
  return framebuffer && framebuffer->IsValid() && !framebuffer->IsDeleted();
}

bool GLES2DecoderImpl::DoIsProgram(GLuint client_id) {
  // IsProgram is true for programs as soon as they are created, until they are
  // deleted and no longer in use.
  const Program* program = GetProgram(client_id);
  return program != NULL && !program->IsDeleted();
}

bool GLES2DecoderImpl::DoIsRenderbuffer(GLuint client_id) {
  const Renderbuffer* renderbuffer =
      GetRenderbuffer(client_id);
  return renderbuffer && renderbuffer->IsValid() && !renderbuffer->IsDeleted();
}

bool GLES2DecoderImpl::DoIsShader(GLuint client_id) {
  // IsShader is true for shaders as soon as they are created, until they
  // are deleted and not attached to any programs.
  const Shader* shader = GetShader(client_id);
  return shader != NULL && !shader->IsDeleted();
}

bool GLES2DecoderImpl::DoIsTexture(GLuint client_id) {
  const Texture* texture = GetTexture(client_id);
  return texture && texture->IsValid() && !texture->IsDeleted();
}

void GLES2DecoderImpl::DoAttachShader(
    GLuint program_client_id, GLint shader_client_id) {
  Program* program_info = GetProgramInfoNotShader(
      program_client_id, "glAttachShader");
  if (!program_info) {
    return;
  }
  Shader* shader_info = GetShaderInfoNotProgram(
      shader_client_id, "glAttachShader");
  if (!shader_info) {
    return;
  }
  if (!program_info->AttachShader(shader_manager(), shader_info)) {
    SetGLError(GL_INVALID_OPERATION,
               "glAttachShader", "can not attach more than"
               " one shader of the same type.");
    return;
  }
  glAttachShader(program_info->service_id(), shader_info->service_id());
}

void GLES2DecoderImpl::DoDetachShader(
    GLuint program_client_id, GLint shader_client_id) {
  Program* program_info = GetProgramInfoNotShader(
      program_client_id, "glDetachShader");
  if (!program_info) {
    return;
  }
  Shader* shader_info = GetShaderInfoNotProgram(
      shader_client_id, "glDetachShader");
  if (!shader_info) {
    return;
  }
  if (!program_info->DetachShader(shader_manager(), shader_info)) {
    SetGLError(GL_INVALID_OPERATION,
               "glDetachShader", "shader not attached to program");
    return;
  }
  glDetachShader(program_info->service_id(), shader_info->service_id());
}

void GLES2DecoderImpl::DoValidateProgram(GLuint program_client_id) {
  Program* info = GetProgramInfoNotShader(
      program_client_id, "glValidateProgram");
  if (!info) {
    return;
  }
  info->Validate();
}

void GLES2DecoderImpl::DoGetVertexAttribfv(
    GLuint index, GLenum pname, GLfloat* params) {
  VertexAttrib* info =
      state_.vertex_attrib_manager->GetVertexAttrib(index);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetVertexAttribfv", "index out of range");
    return;
  }
  switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: {
        Buffer* buffer = info->buffer();
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
    case GL_CURRENT_VERTEX_ATTRIB: {
      const Vec4& value = state_.attrib_values[index];
      params[0] = value.v[0];
      params[1] = value.v[1];
      params[2] = value.v[2];
      params[3] = value.v[3];
      break;
    }
    case GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE:
      *params = static_cast<GLfloat>(info->divisor());
      break;
    default:
      NOTREACHED();
      break;
  }
}

void GLES2DecoderImpl::DoGetVertexAttribiv(
    GLuint index, GLenum pname, GLint* params) {
  VertexAttrib* info =
      state_.vertex_attrib_manager->GetVertexAttrib(index);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glGetVertexAttribiv", "index out of range");
    return;
  }
  switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: {
        Buffer* buffer = info->buffer();
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
    case GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE:
      *params = info->divisor();
      break;
    case GL_CURRENT_VERTEX_ATTRIB: {
      const Vec4& value = state_.attrib_values[index];
      params[0] = static_cast<GLint>(value.v[0]);
      params[1] = static_cast<GLint>(value.v[1]);
      params[2] = static_cast<GLint>(value.v[2]);
      params[3] = static_cast<GLint>(value.v[3]);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

bool GLES2DecoderImpl::SetVertexAttribValue(
    const char* function_name, GLuint index, const GLfloat* value) {
  if (index >= state_.attrib_values.size()) {
    SetGLError(GL_INVALID_VALUE, function_name, "index out of range");
    return false;
  }
  Vec4& v = state_.attrib_values[index];
  v.v[0] = value[0];
  v.v[1] = value[1];
  v.v[2] = value[2];
  v.v[3] = value[3];
  return true;
}

void GLES2DecoderImpl::DoVertexAttrib1f(GLuint index, GLfloat v0) {
  GLfloat v[4] = { v0, 0.0f, 0.0f, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib1f", index, v)) {
    glVertexAttrib1f(index, v0);
  }
}

void GLES2DecoderImpl::DoVertexAttrib2f(GLuint index, GLfloat v0, GLfloat v1) {
  GLfloat v[4] = { v0, v1, 0.0f, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib2f", index, v)) {
    glVertexAttrib2f(index, v0, v1);
  }
}

void GLES2DecoderImpl::DoVertexAttrib3f(
    GLuint index, GLfloat v0, GLfloat v1, GLfloat v2) {
  GLfloat v[4] = { v0, v1, v2, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib3f", index, v)) {
    glVertexAttrib3f(index, v0, v1, v2);
  }
}

void GLES2DecoderImpl::DoVertexAttrib4f(
    GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
  GLfloat v[4] = { v0, v1, v2, v3, };
  if (SetVertexAttribValue("glVertexAttrib4f", index, v)) {
    glVertexAttrib4f(index, v0, v1, v2, v3);
  }
}

void GLES2DecoderImpl::DoVertexAttrib1fv(GLuint index, const GLfloat* v) {
  GLfloat t[4] = { v[0], 0.0f, 0.0f, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib1fv", index, t)) {
    glVertexAttrib1fv(index, v);
  }
}

void GLES2DecoderImpl::DoVertexAttrib2fv(GLuint index, const GLfloat* v) {
  GLfloat t[4] = { v[0], v[1], 0.0f, 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib2fv", index, t)) {
    glVertexAttrib2fv(index, v);
  }
}

void GLES2DecoderImpl::DoVertexAttrib3fv(GLuint index, const GLfloat* v) {
  GLfloat t[4] = { v[0], v[1], v[2], 1.0f, };
  if (SetVertexAttribValue("glVertexAttrib3fv", index, t)) {
    glVertexAttrib3fv(index, v);
  }
}

void GLES2DecoderImpl::DoVertexAttrib4fv(GLuint index, const GLfloat* v) {
  if (SetVertexAttribValue("glVertexAttrib4fv", index, v)) {
    glVertexAttrib4fv(index, v);
  }
}

error::Error GLES2DecoderImpl::HandleVertexAttribPointer(
    uint32 immediate_data_size, const cmds::VertexAttribPointer& c) {

  if (!state_.bound_array_buffer || state_.bound_array_buffer->IsDeleted()) {
    if (state_.vertex_attrib_manager == default_vertex_attrib_manager_) {
      SetGLError(GL_INVALID_VALUE,
                "glVertexAttribPointer", "no array buffer bound");
      return error::kNoError;
    } else if (c.offset != 0) {
      SetGLError(GL_INVALID_VALUE,
                "glVertexAttribPointer", "client side arrays are not allowed");
      return error::kNoError;
    }
  }

  GLuint indx = c.indx;
  GLint size = c.size;
  GLenum type = c.type;
  GLboolean normalized = c.normalized;
  GLsizei stride = c.stride;
  GLsizei offset = c.offset;
  const void* ptr = reinterpret_cast<const void*>(offset);
  if (!validators_->vertex_attrib_type.IsValid(type)) {
    SetGLErrorInvalidEnum("glVertexAttribPointer", type, "type");
    return error::kNoError;
  }
  if (!validators_->vertex_attrib_size.IsValid(size)) {
    SetGLError(GL_INVALID_VALUE,
               "glVertexAttribPointer", "size GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (indx >= group_->max_vertex_attribs()) {
    SetGLError(GL_INVALID_VALUE, "glVertexAttribPointer", "index out of range");
    return error::kNoError;
  }
  if (stride < 0) {
    SetGLError(GL_INVALID_VALUE,
               "glVertexAttribPointer", "stride < 0");
    return error::kNoError;
  }
  if (stride > 255) {
    SetGLError(GL_INVALID_VALUE,
               "glVertexAttribPointer", "stride > 255");
    return error::kNoError;
  }
  if (offset < 0) {
    SetGLError(GL_INVALID_VALUE,
               "glVertexAttribPointer", "offset < 0");
    return error::kNoError;
  }
  GLsizei component_size =
      GLES2Util::GetGLTypeSizeForTexturesAndBuffers(type);
  if (offset % component_size > 0) {
    SetGLError(GL_INVALID_OPERATION,
               "glVertexAttribPointer", "offset not valid for type");
    return error::kNoError;
  }
  if (stride % component_size > 0) {
    SetGLError(GL_INVALID_OPERATION,
               "glVertexAttribPointer", "stride not valid for type");
    return error::kNoError;
  }
  state_.vertex_attrib_manager->SetAttribInfo(
      indx,
      state_.bound_array_buffer,
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

void GLES2DecoderImpl::DoViewport(GLint x, GLint y, GLsizei width,
                                  GLsizei height) {
  state_.viewport_x = x;
  state_.viewport_y = y;
  state_.viewport_width = std::min(width, viewport_max_width_);
  state_.viewport_height = std::min(height, viewport_max_height_);
  glViewport(x, y, width, height);
}

error::Error GLES2DecoderImpl::HandleVertexAttribDivisorANGLE(
    uint32 immediate_data_size, const cmds::VertexAttribDivisorANGLE& c) {
  if (!features().angle_instanced_arrays) {
    SetGLError(GL_INVALID_OPERATION,
               "glVertexAttribDivisorANGLE", "function not available");
  }
  GLuint index = c.index;
  GLuint divisor = c.divisor;
  if (index >= group_->max_vertex_attribs()) {
    SetGLError(GL_INVALID_VALUE,
               "glVertexAttribDivisorANGLE", "index out of range");
    return error::kNoError;
  }

  state_.vertex_attrib_manager->SetDivisor(
      index,
      divisor);
  glVertexAttribDivisorANGLE(index, divisor);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleReadPixels(
    uint32 immediate_data_size, const cmds::ReadPixels& c) {
  if (ShouldDeferReads())
    return error::kDeferCommandUntilLater;
  GLint x = c.x;
  GLint y = c.y;
  GLsizei width = c.width;
  GLsizei height = c.height;
  GLenum format = c.format;
  GLenum type = c.type;
  if (width < 0 || height < 0) {
    SetGLError(GL_INVALID_VALUE, "glReadPixels", "dimensions < 0");
    return error::kNoError;
  }
  typedef cmds::ReadPixels::Result Result;
  uint32 pixels_size;
  if (!GLES2Util::ComputeImageDataSizes(
      width, height, format, type, state_.pack_alignment, &pixels_size,
      NULL, NULL)) {
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
    SetGLErrorInvalidEnum("glReadPixels", format, "format");
    return error::kNoError;
  }
  if (!validators_->pixel_type.IsValid(type)) {
    SetGLErrorInvalidEnum("glReadPixels", type, "type");
    return error::kNoError;
  }
  if (width == 0 || height == 0) {
    return error::kNoError;
  }

  // Get the size of the current fbo or backbuffer.
  gfx::Size max_size = GetBoundReadFrameBufferSize();

  int32 max_x;
  int32 max_y;
  if (!SafeAddInt32(x, width, &max_x) || !SafeAddInt32(y, height, &max_y)) {
    SetGLError(GL_INVALID_VALUE, "glReadPixels", "dimensions out of range");
    return error::kNoError;
  }

  if (!CheckBoundFramebuffersValid("glReadPixels")) {
    return error::kNoError;
  }

  CopyRealGLErrorsToWrapper();

  ScopedResolvedFrameBufferBinder binder(this, false, true);

  if (x < 0 || y < 0 || max_x > max_size.width() || max_y > max_size.height()) {
    // The user requested an out of range area. Get the results 1 line
    // at a time.
    uint32 temp_size;
    uint32 unpadded_row_size;
    uint32 padded_row_size;
    if (!GLES2Util::ComputeImageDataSizes(
        width, 2, format, type, state_.pack_alignment, &temp_size,
        &unpadded_row_size, &padded_row_size)) {
      SetGLError(GL_INVALID_VALUE, "glReadPixels", "dimensions out of range");
      return error::kNoError;
    }

    GLint dest_x_offset = std::max(-x, 0);
    uint32 dest_row_offset;
    if (!GLES2Util::ComputeImageDataSizes(
        dest_x_offset, 1, format, type, state_.pack_alignment, &dest_row_offset,
        NULL, NULL)) {
      SetGLError(GL_INVALID_VALUE, "glReadPixels", "dimensions out of range");
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
  GLenum error = PeekGLError();
  if (error == GL_NO_ERROR) {
    *result = true;

    GLenum read_format = GetBoundReadFrameBufferInternalFormat();
    uint32 channels_exist = GLES2Util::GetChannelsForFormat(read_format);
    if ((channels_exist & 0x0008) == 0 &&
        workarounds().clear_alpha_in_readpixels) {
      // Set the alpha to 255 because some drivers are buggy in this regard.
      uint32 temp_size;

      uint32 unpadded_row_size;
      uint32 padded_row_size;
      if (!GLES2Util::ComputeImageDataSizes(
          width, 2, format, type, state_.pack_alignment, &temp_size,
          &unpadded_row_size, &padded_row_size)) {
        SetGLError(GL_INVALID_VALUE, "glReadPixels", "dimensions out of range");
        return error::kNoError;
      }
      // NOTE: Assumes the type is GL_UNSIGNED_BYTE which was true at the time
      // of this implementation.
      if (type != GL_UNSIGNED_BYTE) {
        SetGLError(
            GL_INVALID_OPERATION, "glReadPixels",
            "unsupported readPixel format");
        return error::kNoError;
      }
      switch (format) {
        case GL_RGBA:
        case GL_BGRA_EXT:
        case GL_ALPHA: {
          int offset = (format == GL_ALPHA) ? 0 : 3;
          int step = (format == GL_ALPHA) ? 1 : 4;
          uint8* dst = static_cast<uint8*>(pixels) + offset;
          for (GLint yy = 0; yy < height; ++yy) {
            uint8* end = dst + unpadded_row_size;
            for (uint8* d = dst; d < end; d += step) {
              *d = 255;
            }
            dst += padded_row_size;
          }
          break;
        }
        default:
          break;
      }
    }
  }

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePixelStorei(
    uint32 immediate_data_size, const cmds::PixelStorei& c) {
  GLenum pname = c.pname;
  GLenum param = c.param;
  if (!validators_->pixel_store.IsValid(pname)) {
    SetGLErrorInvalidEnum("glPixelStorei", pname, "pname");
    return error::kNoError;
  }
  switch (pname) {
    case GL_PACK_ALIGNMENT:
    case GL_UNPACK_ALIGNMENT:
        if (!validators_->pixel_store_alignment.IsValid(param)) {
            SetGLError(GL_INVALID_VALUE,
                       "glPixelStore", "param GL_INVALID_VALUE");
            return error::kNoError;
        }
        break;
    case GL_UNPACK_FLIP_Y_CHROMIUM:
        unpack_flip_y_ = (param != 0);
        return error::kNoError;
    case GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM:
        unpack_premultiply_alpha_ = (param != 0);
        return error::kNoError;
    case GL_UNPACK_UNPREMULTIPLY_ALPHA_CHROMIUM:
        unpack_unpremultiply_alpha_ = (param != 0);
        return error::kNoError;
    default:
        break;
  }
  glPixelStorei(pname, param);
  switch (pname) {
    case GL_PACK_ALIGNMENT:
        state_.pack_alignment = param;
        break;
    case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
        state_.pack_reverse_row_order = (param != 0);
        break;
    case GL_UNPACK_ALIGNMENT:
        state_.unpack_alignment = param;
        break;
    default:
        // Validation should have prevented us from getting here.
        NOTREACHED();
        break;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePostSubBufferCHROMIUM(
    uint32 immediate_data_size, const cmds::PostSubBufferCHROMIUM& c) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::HandlePostSubBufferCHROMIUM");
  if (!surface_->HasExtension("GL_CHROMIUM_post_sub_buffer")) {
    SetGLError(GL_INVALID_OPERATION,
               "glPostSubBufferCHROMIUM", "command not supported by surface");
    return error::kNoError;
  }
  if (surface_->PostSubBuffer(c.x, c.y, c.width, c.height)) {
    return error::kNoError;
  } else {
    LOG(ERROR) << "Context lost because PostSubBuffer failed.";
    return error::kLostContext;
  }
}

error::Error GLES2DecoderImpl::GetAttribLocationHelper(
    GLuint client_id, uint32 location_shm_id, uint32 location_shm_offset,
    const std::string& name_str) {
  if (!StringIsValidForGLES(name_str.c_str())) {
    SetGLError(GL_INVALID_VALUE, "glGetAttribLocation", "Invalid character");
    return error::kNoError;
  }
  Program* info = GetProgramInfoNotShader(
      client_id, "glGetAttribLocation");
  if (!info) {
    return error::kNoError;
  }
  if (!info->IsValid()) {
    SetGLError(
        GL_INVALID_OPERATION, "glGetAttribLocation", "program not linked");
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
    uint32 immediate_data_size, const cmds::GetAttribLocation& c) {
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
    uint32 immediate_data_size, const cmds::GetAttribLocationImmediate& c) {
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
    uint32 immediate_data_size, const cmds::GetAttribLocationBucket& c) {
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
  if (!StringIsValidForGLES(name_str.c_str())) {
    SetGLError(GL_INVALID_VALUE, "glGetUniformLocation", "Invalid character");
    return error::kNoError;
  }
  Program* info = GetProgramInfoNotShader(
      client_id, "glUniformLocation");
  if (!info) {
    return error::kNoError;
  }
  if (!info->IsValid()) {
    SetGLError(
        GL_INVALID_OPERATION, "glGetUniformLocation", "program not linked");
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
  *location = info->GetUniformFakeLocation(name_str);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetUniformLocation(
    uint32 immediate_data_size, const cmds::GetUniformLocation& c) {
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
    uint32 immediate_data_size, const cmds::GetUniformLocationImmediate& c) {
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
    uint32 immediate_data_size, const cmds::GetUniformLocationBucket& c) {
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
    uint32 immediate_data_size, const cmds::GetString& c) {
  GLenum name = static_cast<GLenum>(c.name);
  if (!validators_->string_type.IsValid(name)) {
    SetGLErrorInvalidEnum("glGetString", name, "name");
    return error::kNoError;
  }
  const char* gl_str = reinterpret_cast<const char*>(glGetString(name));
  const char* str = NULL;
  std::string extensions;
  switch (name) {
    case GL_VERSION:
      str = "OpenGL ES 2.0 Chromium";
      break;
    case GL_SHADING_LANGUAGE_VERSION:
      str = "OpenGL ES GLSL ES 1.0 Chromium";
      break;
    case GL_RENDERER:
      str = "Chromium";
      break;
    case GL_VENDOR:
      str = "Chromium";
      break;
    case GL_EXTENSIONS:
      {
        // For WebGL contexts, strip out the OES derivatives extension if it has
        // not been enabled.
        if (force_webgl_glsl_validation_ &&
            !derivatives_explicitly_enabled_) {
          extensions = feature_info_->extensions();
          size_t offset = extensions.find(kOESDerivativeExtension);
          if (std::string::npos != offset) {
            extensions.replace(offset,
                               offset + arraysize(kOESDerivativeExtension),
                               std::string());
          }
        } else {
          extensions = feature_info_->extensions().c_str();
        }
        std::string surface_extensions = surface_->GetExtensions();
        if (!surface_extensions.empty())
          extensions += " " + surface_extensions;
        str = extensions.c_str();
      }
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
    SetGLErrorInvalidEnum("glBufferData", target, "target");
    return;
  }
  if (!validators_->buffer_usage.IsValid(usage)) {
    SetGLErrorInvalidEnum("glBufferData", usage, "usage");
    return;
  }
  if (size < 0) {
    SetGLError(GL_INVALID_VALUE, "glBufferData", "size < 0");
    return;
  }
  Buffer* info = GetBufferInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glBufferData", "unknown buffer");
    return;
  }

  if (!EnsureGPUMemoryAvailable(size)) {
    SetGLError(GL_OUT_OF_MEMORY, "glBufferData", "out of memory");
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
  GLenum error = PeekGLError();
  if (error == GL_NO_ERROR) {
    buffer_manager()->SetInfo(info, size, usage);
    info->SetRange(0, size, data);
  } else {
    buffer_manager()->SetInfo(info, 0, usage);
  }
}

error::Error GLES2DecoderImpl::HandleBufferData(
    uint32 immediate_data_size, const cmds::BufferData& c) {
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
    uint32 immediate_data_size, const cmds::BufferDataImmediate& c) {
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
  Buffer* info = GetBufferInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE, "glBufferSubData", "unknown buffer");
    return;
  }
  if (!info->SetRange(offset, size, data)) {
    SetGLError(GL_INVALID_VALUE, "glBufferSubData", "out of range");
    return;
  }
  glBufferSubData(target, offset, size, data);
}

bool GLES2DecoderImpl::ClearLevel(
    unsigned service_id,
    unsigned bind_target,
    unsigned target,
    int level,
    unsigned format,
    unsigned type,
    int width,
    int height,
    bool is_texture_immutable) {
  uint32 channels = GLES2Util::GetChannelsForFormat(format);
  if (IsAngle() && (channels & GLES2Util::kDepth) != 0) {
    // It's a depth format and ANGLE doesn't allow texImage2D or texSubImage2D
    // on depth formats.
    GLuint fb = 0;
    glGenFramebuffersEXT(1, &fb);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, fb);

    bool have_stencil = (channels & GLES2Util::kStencil) != 0;
    GLenum attachment = have_stencil ? GL_DEPTH_STENCIL_ATTACHMENT :
                                       GL_DEPTH_ATTACHMENT;

    glFramebufferTexture2DEXT(
        GL_DRAW_FRAMEBUFFER_EXT, attachment, target, service_id, level);
    // ANGLE promises a depth only attachment ok.
    if (glCheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER_EXT) !=
        GL_FRAMEBUFFER_COMPLETE) {
      return false;
    }
    glClearStencil(0);
    glStencilMask(-1);
    glClearDepth(1.0f);
    glDepthMask(true);
    glDisable(GL_SCISSOR_TEST);
    glClear(GL_DEPTH_BUFFER_BIT | (have_stencil ? GL_STENCIL_BUFFER_BIT : 0));

    RestoreClearState();

    glDeleteFramebuffersEXT(1, &fb);
    Framebuffer* framebuffer =
        GetFramebufferInfoForTarget(GL_DRAW_FRAMEBUFFER_EXT);
    GLuint fb_service_id =
        framebuffer ? framebuffer->service_id() : GetBackbufferServiceId();
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, fb_service_id);
    return true;
  }

  static const uint32 kMaxZeroSize = 1024 * 1024 * 4;

  uint32 size;
  uint32 padded_row_size;
  if (!GLES2Util::ComputeImageDataSizes(
          width, height, format, type, state_.unpack_alignment, &size,
          NULL, &padded_row_size)) {
    return false;
  }

  TRACE_EVENT1("gpu", "GLES2DecoderImpl::ClearLevel", "size", size);

  int tile_height;

  if (size > kMaxZeroSize) {
    if (kMaxZeroSize < padded_row_size) {
        // That'd be an awfully large texture.
        return false;
    }
    // We should never have a large total size with a zero row size.
    DCHECK_GT(padded_row_size, 0U);
    tile_height = kMaxZeroSize / padded_row_size;
    if (!GLES2Util::ComputeImageDataSizes(
            width, tile_height, format, type, state_.unpack_alignment, &size,
            NULL, NULL)) {
      return false;
    }
  } else {
    tile_height = height;
  }

  // Assumes the size has already been checked.
  scoped_array<char> zero(new char[size]);
  memset(zero.get(), 0, size);
  glBindTexture(bind_target, service_id);

  GLint y = 0;
  while (y < height) {
    GLint h = y + tile_height > height ? height - y : tile_height;
    if (is_texture_immutable || h != height) {
      glTexSubImage2D(target, level, 0, y, width, h, format, type, zero.get());
    } else {
      WrappedTexImage2D(
          target, level, format, width, h, 0, format, type, zero.get());
    }
    y += tile_height;
  }
  Texture* info = GetTextureInfoForTarget(bind_target);
  glBindTexture(bind_target, info ? info->service_id() : 0);
  return true;
}

namespace {

const int kS3TCBlockWidth = 4;
const int kS3TCBlockHeight = 4;
const int kS3TCDXT1BlockSize = 8;
const int kS3TCDXT3AndDXT5BlockSize = 16;
const int kETC1BlockWidth = 4;
const int kETC1BlockHeight = 4;
const int kETC1BlockSize = 8;

bool IsValidDXTSize(GLint level, GLsizei size) {
  return (size == 1) ||
         (size == 2) || !(size % kS3TCBlockWidth);
}

}  // anonymous namespace.

bool GLES2DecoderImpl::ValidateCompressedTexFuncData(
    const char* function_name,
    GLsizei width, GLsizei height, GLenum format, size_t size) {
  unsigned int bytes_required = 0;

  switch (format) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: {
        int num_blocks_across =
            (width + kS3TCBlockWidth - 1) / kS3TCBlockWidth;
        int num_blocks_down =
            (height + kS3TCBlockHeight - 1) / kS3TCBlockHeight;
        int num_blocks = num_blocks_across * num_blocks_down;
        bytes_required = num_blocks * kS3TCDXT1BlockSize;
        break;
      }
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: {
        int num_blocks_across =
            (width + kS3TCBlockWidth - 1) / kS3TCBlockWidth;
        int num_blocks_down =
            (height + kS3TCBlockHeight - 1) / kS3TCBlockHeight;
        int num_blocks = num_blocks_across * num_blocks_down;
        bytes_required = num_blocks * kS3TCDXT3AndDXT5BlockSize;
        break;
      }
    case GL_ETC1_RGB8_OES: {
        int num_blocks_across =
            (width + kETC1BlockWidth - 1) / kETC1BlockWidth;
        int num_blocks_down =
            (height + kETC1BlockHeight - 1) / kETC1BlockHeight;
        int num_blocks = num_blocks_across * num_blocks_down;
        bytes_required = num_blocks * kETC1BlockSize;
        break;
      }
    default:
      SetGLErrorInvalidEnum(function_name, format, "format");
      return false;
  }

  if (size != bytes_required) {
    SetGLError(
        GL_INVALID_VALUE, function_name, "size is not correct for dimensions");
    return false;
  }

  return true;
}

bool GLES2DecoderImpl::ValidateCompressedTexDimensions(
    const char* function_name,
    GLint level, GLsizei width, GLsizei height, GLenum format) {
  switch (format) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: {
      if (!IsValidDXTSize(level, width) || !IsValidDXTSize(level, height)) {
        SetGLError(
            GL_INVALID_OPERATION, function_name,
            "width or height invalid for level");
        return false;
      }
      return true;
    }
    case GL_ETC1_RGB8_OES:
      if (width <= 0 || height <= 0) {
        SetGLError(
            GL_INVALID_OPERATION, function_name,
            "width or height invalid for level");
        return false;
      }
      return true;
    default:
      return false;
  }
}

bool GLES2DecoderImpl::ValidateCompressedTexSubDimensions(
    const char* function_name,
    GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLsizei width, GLsizei height, GLenum format,
    Texture* texture) {
  if (xoffset < 0 || yoffset < 0) {
    SetGLError(
        GL_INVALID_VALUE, function_name, "xoffset or yoffset < 0");
    return false;
  }

  switch (format) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: {
      const int kBlockWidth = 4;
      const int kBlockHeight = 4;
      if ((xoffset % kBlockWidth) || (yoffset % kBlockHeight)) {
        SetGLError(
            GL_INVALID_OPERATION, function_name,
            "xoffset or yoffset not multiple of 4");
        return false;
      }
      GLsizei tex_width = 0;
      GLsizei tex_height = 0;
      if (!texture->GetLevelSize(target, level, &tex_width, &tex_height) ||
          width - xoffset > tex_width ||
          height - yoffset > tex_height) {
        SetGLError(
            GL_INVALID_OPERATION, function_name, "dimensions out of range");
        return false;
      }
      return ValidateCompressedTexDimensions(
          function_name, level, width, height, format);
    }
    case GL_ETC1_RGB8_OES: {
      SetGLError(
          GL_INVALID_OPERATION, function_name,
          "TexsubImage2d not supported for ECT1_RGB8_OES textures");
      return false;
    }
    default:
      return false;
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
    SetGLErrorInvalidEnum("glCompressedTexImage2D", target, "target");
    return error::kNoError;
  }
  if (!validators_->compressed_texture_format.IsValid(
      internal_format)) {
    SetGLErrorInvalidEnum(
        "glCompressedTexImage2D", internal_format, "internal_format");
    return error::kNoError;
  }
  if (!texture_manager()->ValidForTarget(target, level, width, height, 1) ||
      border != 0) {
    SetGLError(GL_INVALID_VALUE,
               "glCompressedTexImage2D", "dimensions out of range");
    return error::kNoError;
  }
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_VALUE,
               "glCompressedTexImage2D", "unknown texture target");
    return error::kNoError;
  }
  if (info->IsImmutable()) {
    SetGLError(GL_INVALID_OPERATION,
               "glCompressedTexImage2D", "texture is immutable");
    return error::kNoError;
  }

  if (!ValidateCompressedTexDimensions(
      "glCompressedTexImage2D", level, width, height, internal_format) ||
      !ValidateCompressedTexFuncData(
      "glCompressedTexImage2D", width, height, internal_format, image_size)) {
    return error::kNoError;
  }

  if (!EnsureGPUMemoryAvailable(image_size)) {
    SetGLError(GL_OUT_OF_MEMORY, "glCompressedTexImage2D", "out of memory");
    return error::kNoError;
  }

  if (info->IsAttachedToFramebuffer()) {
    clear_state_dirty_ = true;
    // TODO(gman): If textures tracked which framebuffers they were attached to
    // we could just mark those framebuffers as not complete.
    framebuffer_manager()->IncFramebufferStateChangeCount();
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
  GLenum error = PeekGLError();
  if (error == GL_NO_ERROR) {
    texture_manager()->SetLevelInfo(
        info, target, level, internal_format, width, height, 1, border, 0, 0,
        true);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCompressedTexImage2D(
    uint32 immediate_data_size, const cmds::CompressedTexImage2D& c) {
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
    uint32 immediate_data_size, const cmds::CompressedTexImage2DImmediate& c) {
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
    uint32 immediate_data_size, const cmds::CompressedTexImage2DBucket& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  Bucket* bucket = GetBucket(c.bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  uint32 data_size = bucket->size();
  GLsizei imageSize = data_size;
  const void* data = bucket->GetData(0, data_size);
  if (!data) {
    return error::kInvalidArguments;
  }
  return DoCompressedTexImage2D(
      target, level, internal_format, width, height, border,
      imageSize, data);
}

error::Error GLES2DecoderImpl::HandleCompressedTexSubImage2DBucket(
    uint32 immediate_data_size,
    const cmds::CompressedTexSubImage2DBucket& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  Bucket* bucket = GetBucket(c.bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  uint32 data_size = bucket->size();
  GLsizei imageSize = data_size;
  const void* data = bucket->GetData(0, data_size);
  if (!data) {
    return error::kInvalidArguments;
  }
  if (!validators_->texture_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM, "glCompressedTexSubImage2D", "target");
    return error::kNoError;
  }
  if (!validators_->compressed_texture_format.IsValid(format)) {
    SetGLErrorInvalidEnum("glCompressedTexSubImage2D", format, "format");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D", "height < 0");
    return error::kNoError;
  }
  if (imageSize < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D", "imageSize < 0");
    return error::kNoError;
  }
  DoCompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
  return error::kNoError;
}

bool GLES2DecoderImpl::ValidateTextureParameters(
    const char* function_name,
    GLenum target, GLenum format, GLenum type, GLint level) {
  if (!feature_info_->GetTextureFormatValidator(format).IsValid(type)) {
      SetGLError(GL_INVALID_OPERATION, function_name,
                 (std::string("invalid type ") +
                  GLES2Util::GetStringEnum(type) + " for format " +
                  GLES2Util::GetStringEnum(format)).c_str());
      return false;
  }

  uint32 channels = GLES2Util::GetChannelsForFormat(format);
  if ((channels & (GLES2Util::kDepth | GLES2Util::kStencil)) != 0 && level) {
    SetGLError(GL_INVALID_OPERATION, function_name,
               (std::string("invalid type ") +
                GLES2Util::GetStringEnum(type) + " for format " +
                GLES2Util::GetStringEnum(format)).c_str());
    return false;
  }
  return true;
}

bool GLES2DecoderImpl::ValidateTexImage2D(
    const char* function_name,
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
    SetGLErrorInvalidEnum(function_name, target, "target");
    return false;
  }
  if (!validators_->texture_format.IsValid(internal_format)) {
    SetGLErrorInvalidEnum(function_name, internal_format, "internal_format");
    return false;
  }
  if (!validators_->texture_format.IsValid(format)) {
    SetGLErrorInvalidEnum(function_name, format, "format");
    return false;
  }
  if (!validators_->pixel_type.IsValid(type)) {
    SetGLErrorInvalidEnum(function_name, type, "type");
    return false;
  }
  if (format != internal_format) {
    SetGLError(GL_INVALID_OPERATION,
               function_name, "format != internalFormat");
    return false;
  }
  if (!ValidateTextureParameters(function_name, target, format, type, level)) {
    return false;
  }
  if (!texture_manager()->ValidForTarget(target, level, width, height, 1) ||
      border != 0) {
    SetGLError(GL_INVALID_VALUE, function_name, "dimensions out of range");
    return false;
  }
  if ((GLES2Util::GetChannelsForFormat(format) &
       (GLES2Util::kDepth | GLES2Util::kStencil)) != 0 && pixels) {
    SetGLError(
        GL_INVALID_OPERATION,
        function_name, "can not supply data for depth or stencil textures");
    return false;
  }
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               function_name, "unknown texture for target");
    return false;
  }
  if (info->IsImmutable()) {
    SetGLError(GL_INVALID_OPERATION,
               function_name, "texture is immutable");
    return false;
  }
  return true;
}

void GLES2DecoderImpl::DoTexImage2D(
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
  if (!ValidateTexImage2D("glTexImage2D", target, level, internal_format,
      width, height, border, format, type, pixels, pixels_size)) {
    return;
  }

  if (!EnsureGPUMemoryAvailable(pixels_size)) {
    SetGLError(GL_OUT_OF_MEMORY, "glTexImage2D", "out of memory");
    return;
  }

  Texture* info = GetTextureInfoForTarget(target);
  GLsizei tex_width = 0;
  GLsizei tex_height = 0;
  GLenum tex_type = 0;
  GLenum tex_format = 0;
  bool level_is_same =
      info->GetLevelSize(target, level, &tex_width, &tex_height) &&
      info->GetLevelType(target, level, &tex_type, &tex_format) &&
      width == tex_width && height == tex_height &&
      type == tex_type && format == tex_format;

  if (level_is_same && !pixels) {
    // Just set the level info but mark the texture as uncleared.
    texture_manager()->SetLevelInfo(
        info,
        target, level, internal_format, width, height, 1, border, format, type,
        false);
    tex_image_2d_failed_ = false;
    return;
  }

  if (info->IsAttachedToFramebuffer()) {
    clear_state_dirty_ = true;
    // TODO(gman): If textures tracked which framebuffers they were attached to
    // we could just mark those framebuffers as not complete.
    framebuffer_manager()->IncFramebufferStateChangeCount();
  }

  if (!teximage2d_faster_than_texsubimage2d_ && level_is_same && pixels) {
    glTexSubImage2D(target, level, 0, 0, width, height, format, type, pixels);
    texture_manager()->SetLevelCleared(info, target, level, true);
    tex_image_2d_failed_ = false;
    return;
  }

  CopyRealGLErrorsToWrapper();
  WrappedTexImage2D(
      target, level, internal_format, width, height, border, format, type,
      pixels);
  GLenum error = PeekGLError();
  if (error == GL_NO_ERROR) {
    texture_manager()->SetLevelInfo(
        info,
        target, level, internal_format, width, height, 1, border, format, type,
        pixels != NULL);
    tex_image_2d_failed_ = false;
  }
  return;
}

error::Error GLES2DecoderImpl::HandleTexImage2D(
    uint32 immediate_data_size, const cmds::TexImage2D& c) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::HandleTexImage2D");
  tex_image_2d_failed_ = true;
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
  if (!GLES2Util::ComputeImageDataSizes(
      width, height, format, type, state_.unpack_alignment, &pixels_size, NULL,
      NULL)) {
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

  DoTexImage2D(
      target, level, internal_format, width, height, border, format, type,
      pixels, pixels_size);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexImage2DImmediate(
    uint32 immediate_data_size, const cmds::TexImage2DImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint internal_format = static_cast<GLint>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32 size;
  if (!GLES2Util::ComputeImageDataSizes(
      width, height, format, type, state_.unpack_alignment, &size,
      NULL, NULL)) {
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
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glCompressedTexSubImage2D", "unknown texture for target");
    return;
  }
  GLenum type = 0;
  GLenum internal_format = 0;
  if (!info->GetLevelType(target, level, &type, &internal_format)) {
    SetGLError(
        GL_INVALID_OPERATION,
        "glCompressedTexSubImage2D", "level does not exist.");
    return;
  }
  if (internal_format != format) {
    SetGLError(
        GL_INVALID_OPERATION,
        "glCompressedTexSubImage2D", "format does not match internal format.");
    return;
  }
  if (!info->ValidForTexture(
      target, level, xoffset, yoffset, width, height, format, type)) {
    SetGLError(GL_INVALID_VALUE,
               "glCompressedTexSubImage2D", "bad dimensions.");
    return;
  }

  if (!ValidateCompressedTexFuncData(
      "glCompressedTexSubImage2D", width, height, format, image_size) ||
      !ValidateCompressedTexSubDimensions(
      "glCompressedTexSubImage2D",
      target, level, xoffset, yoffset, width, height, format, info)) {
    return;
  }


  // Note: There is no need to deal with texture cleared tracking here
  // because the validation above means you can only get here if the level
  // is already a matching compressed format and in that case
  // CompressedTexImage2D already cleared the texture.
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
  DCHECK(!ShouldDeferReads());
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glCopyTexImage2D", "unknown texture for target");
    return;
  }
  if (info->IsImmutable()) {
    SetGLError(GL_INVALID_OPERATION,
               "glCopyTexImage2D", "texture is immutable");
  }
  if (!texture_manager()->ValidForTarget(target, level, width, height, 1) ||
      border != 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexImage2D", "dimensions out of range");
    return;
  }
  if (!ValidateTextureParameters(
      "glCopyTexImage2D", target, internal_format, GL_UNSIGNED_BYTE, level)) {
    return;
  }

  // Check we have compatible formats.
  GLenum read_format = GetBoundReadFrameBufferInternalFormat();
  uint32 channels_exist = GLES2Util::GetChannelsForFormat(read_format);
  uint32 channels_needed = GLES2Util::GetChannelsForFormat(internal_format);

  if ((channels_needed & channels_exist) != channels_needed) {
    SetGLError(GL_INVALID_OPERATION, "glCopyTexImage2D", "incompatible format");
    return;
  }

  if ((channels_needed & (GLES2Util::kDepth | GLES2Util::kStencil)) != 0) {
    SetGLError(
        GL_INVALID_OPERATION,
        "glCopyTexImage2D", "can not be used with depth or stencil textures");
    return;
  }

  uint32 estimated_size = 0;
  if (!GLES2Util::ComputeImageDataSizes(
      width, height, internal_format, GL_UNSIGNED_BYTE, state_.unpack_alignment,
      &estimated_size, NULL, NULL)) {
    SetGLError(GL_OUT_OF_MEMORY, "glCopyTexImage2D", "dimensions too large");
    return;
  }

  if (!EnsureGPUMemoryAvailable(estimated_size)) {
    SetGLError(GL_OUT_OF_MEMORY, "glCopyTexImage2D", "out of memory");
    return;
  }

  if (!CheckBoundFramebuffersValid("glCopyTexImage2D")) {
    return;
  }

  CopyRealGLErrorsToWrapper();
  ScopedResolvedFrameBufferBinder binder(this, false, true);
  gfx::Size size = GetBoundReadFrameBufferSize();

  if (info->IsAttachedToFramebuffer()) {
    clear_state_dirty_ = true;
    // TODO(gman): If textures tracked which framebuffers they were attached to
    // we could just mark those framebuffers as not complete.
    framebuffer_manager()->IncFramebufferStateChangeCount();
  }

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
    if (!ClearLevel(
        info->service_id(), info->target(),
        target, level, internal_format, GL_UNSIGNED_BYTE, width, height,
        info->IsImmutable())) {
      SetGLError(GL_OUT_OF_MEMORY, "glCopyTexImage2D", "dimensions too big");
      return;
    }
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
  GLenum error = PeekGLError();
  if (error == GL_NO_ERROR) {
    texture_manager()->SetLevelInfo(
        info, target, level, internal_format, width, height, 1,
        border, internal_format, GL_UNSIGNED_BYTE, true);
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
  DCHECK(!ShouldDeferReads());
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glCopyTexSubImage2D", "unknown texture for target");
    return;
  }
  GLenum type = 0;
  GLenum format = 0;
  if (!info->GetLevelType(target, level, &type, &format) ||
      !info->ValidForTexture(
          target, level, xoffset, yoffset, width, height, format, type)) {
    SetGLError(GL_INVALID_VALUE,
               "glCopyTexSubImage2D", "bad dimensions.");
    return;
  }
  if (info->AsyncTransferIsInProgress()) {
    SetGLError(GL_INVALID_OPERATION,
               "glCopyTexSubImage2D", "async upload pending for texture");
    return;
  }

  // Check we have compatible formats.
  GLenum read_format = GetBoundReadFrameBufferInternalFormat();
  uint32 channels_exist = GLES2Util::GetChannelsForFormat(read_format);
  uint32 channels_needed = GLES2Util::GetChannelsForFormat(format);

  if (!channels_needed ||
      (channels_needed & channels_exist) != channels_needed) {
    SetGLError(
        GL_INVALID_OPERATION, "glCopyTexSubImage2D", "incompatible format");
    return;
  }

  if ((channels_needed & (GLES2Util::kDepth | GLES2Util::kStencil)) != 0) {
    SetGLError(
        GL_INVALID_OPERATION,
        "glCopySubImage2D", "can not be used with depth or stencil textures");
    return;
  }

  if (!CheckBoundFramebuffersValid("glCopyTexSubImage2D")) {
    return;
  }

  ScopedResolvedFrameBufferBinder binder(this, false, true);
  gfx::Size size = GetBoundReadFrameBufferSize();
  GLint copyX = 0;
  GLint copyY = 0;
  GLint copyWidth = 0;
  GLint copyHeight = 0;
  Clip(x, width, size.width(), &copyX, &copyWidth);
  Clip(y, height, size.height(), &copyY, &copyHeight);

  if (!texture_manager()->ClearTextureLevel(this, info, target, level)) {
    SetGLError(GL_OUT_OF_MEMORY, "glCopyTexSubImage2D", "dimensions too big");
    return;
  }

  if (copyX != x ||
      copyY != y ||
      copyWidth != width ||
      copyHeight != height) {
    // some part was clipped so clear the sub rect.
    uint32 pixels_size = 0;
    if (!GLES2Util::ComputeImageDataSizes(
        width, height, format, type, state_.unpack_alignment, &pixels_size,
        NULL, NULL)) {
      SetGLError(
          GL_INVALID_VALUE, "glCopyTexSubImage2D", "dimensions too large");
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

bool GLES2DecoderImpl::ValidateTexSubImage2D(
    error::Error* error,
    const char* function_name,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    const void * data) {
  (*error) = error::kNoError;
  if (!validators_->texture_target.IsValid(target)) {
    SetGLErrorInvalidEnum(function_name, target, "target");
    return false;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, function_name, "width < 0");
    return false;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, function_name, "height < 0");
    return false;
  }
  if (!validators_->texture_format.IsValid(format)) {
    SetGLErrorInvalidEnum(function_name, format, "format");
    return false;
  }
  if (!validators_->pixel_type.IsValid(type)) {
    SetGLErrorInvalidEnum(function_name, type, "type");
    return false;
  }
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               function_name, "unknown texture for target");
    return false;
  }
  GLenum current_type = 0;
  GLenum internal_format = 0;
  if (!info->GetLevelType(target, level, &current_type, &internal_format)) {
    SetGLError(
        GL_INVALID_OPERATION, function_name, "level does not exist.");
    return false;
  }
  if (format != internal_format) {
    SetGLError(GL_INVALID_OPERATION,
               function_name, "format does not match internal format.");
    return false;
  }
  if (type != current_type) {
    SetGLError(GL_INVALID_OPERATION,
               function_name, "type does not match type of texture.");
    return false;
  }
  if (info->AsyncTransferIsInProgress()) {
    SetGLError(GL_INVALID_OPERATION,
               function_name, "async upload pending for texture");
    return false;
  }
  if (!info->ValidForTexture(
          target, level, xoffset, yoffset, width, height, format, type)) {
    SetGLError(GL_INVALID_VALUE, function_name, "bad dimensions.");
    return false;
  }
  if ((GLES2Util::GetChannelsForFormat(format) &
       (GLES2Util::kDepth | GLES2Util::kStencil)) != 0) {
    SetGLError(
        GL_INVALID_OPERATION,
        function_name, "can not supply data for depth or stencil textures");
    return false;
  }
  if (data == NULL) {
    (*error) = error::kOutOfBounds;
    return false;
  }
  return true;
}

error::Error GLES2DecoderImpl::DoTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    const void * data) {
  error::Error error = error::kNoError;
  if (!ValidateTexSubImage2D(&error, "glTexSubImage2D", target, level,
      xoffset, yoffset, width, height, format, type, data)) {
    return error;
  }
  Texture* info = GetTextureInfoForTarget(target);
  GLsizei tex_width = 0;
  GLsizei tex_height = 0;
  bool ok = info->GetLevelSize(target, level, &tex_width, &tex_height);
  DCHECK(ok);
  if (xoffset != 0 || yoffset != 0 ||
      width != tex_width || height != tex_height) {
    if (!texture_manager()->ClearTextureLevel(this, info, target, level)) {
      SetGLError(GL_OUT_OF_MEMORY, "glTexSubImage2D", "dimensions too big");
      return error::kNoError;
    }
    ScopedTextureUploadTimer timer(this);
    glTexSubImage2D(
        target, level, xoffset, yoffset, width, height, format, type, data);
    return error::kNoError;
  }

  if (teximage2d_faster_than_texsubimage2d_ && !info->IsImmutable()) {
    ScopedTextureUploadTimer timer(this);
    // NOTE: In OpenGL ES 2.0 border is always zero and format is always the
    // same as internal_foramt. If that changes we'll need to look them up.
    WrappedTexImage2D(
        target, level, format, width, height, 0, format, type, data);
  } else {
    ScopedTextureUploadTimer timer(this);
    glTexSubImage2D(
        target, level, xoffset, yoffset, width, height, format, type, data);
  }
  texture_manager()->SetLevelCleared(info, target, level, true);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexSubImage2D(
    uint32 immediate_data_size, const cmds::TexSubImage2D& c) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::HandleTexSubImage2D");
  GLboolean internal = static_cast<GLboolean>(c.internal);
  if (internal == GL_TRUE && tex_image_2d_failed_)
    return error::kNoError;

  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32 data_size;
  if (!GLES2Util::ComputeImageDataSizes(
      width, height, format, type, state_.unpack_alignment, &data_size,
      NULL, NULL)) {
    return error::kOutOfBounds;
  }
  const void* pixels = GetSharedMemoryAs<const void*>(
      c.pixels_shm_id, c.pixels_shm_offset, data_size);
  return DoTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
}

error::Error GLES2DecoderImpl::HandleTexSubImage2DImmediate(
    uint32 immediate_data_size, const cmds::TexSubImage2DImmediate& c) {
  GLboolean internal = static_cast<GLboolean>(c.internal);
  if (internal == GL_TRUE && tex_image_2d_failed_)
    return error::kNoError;

  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32 data_size;
  if (!GLES2Util::ComputeImageDataSizes(
      width, height, format, type, state_.unpack_alignment, &data_size,
      NULL, NULL)) {
    return error::kOutOfBounds;
  }
  const void* pixels = GetImmediateDataAs<const void*>(
      c, data_size, immediate_data_size);
  return DoTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, type, pixels);
}

error::Error GLES2DecoderImpl::HandleGetVertexAttribPointerv(
    uint32 immediate_data_size, const cmds::GetVertexAttribPointerv& c) {
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetVertexAttribPointerv::Result Result;
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
    SetGLErrorInvalidEnum("glGetVertexAttribPointerv", pname, "pname");
    return error::kNoError;
  }
  if (index >= group_->max_vertex_attribs()) {
    SetGLError(GL_INVALID_VALUE,
               "glGetVertexAttribPointerv", "index out of range.");
    return error::kNoError;
  }
  result->SetNumResults(1);
  *result->GetData() =
      state_.vertex_attrib_manager->GetVertexAttrib(index)->offset();
  return error::kNoError;
}

bool GLES2DecoderImpl::GetUniformSetup(
    GLuint program, GLint fake_location,
    uint32 shm_id, uint32 shm_offset,
    error::Error* error, GLint* real_location,
    GLuint* service_id, void** result_pointer, GLenum* result_type) {
  DCHECK(error);
  DCHECK(service_id);
  DCHECK(result_pointer);
  DCHECK(result_type);
  DCHECK(real_location);
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
  Program* info = GetProgramInfoNotShader(
      program, "glGetUniform");
  if (!info) {
    return false;
  }
  if (!info->IsValid()) {
    // Program was not linked successfully. (ie, glLinkProgram)
    SetGLError(GL_INVALID_OPERATION, "glGetUniform", "program not linked");
    return false;
  }
  *service_id = info->service_id();
  GLint array_index = -1;
  const Program::UniformInfo* uniform_info =
      info->GetUniformInfoByFakeLocation(
          fake_location, real_location, &array_index);
  if (!uniform_info) {
    // No such location.
    SetGLError(GL_INVALID_OPERATION, "glGetUniform", "unknown location");
    return false;
  }
  GLenum type = uniform_info->type;
  GLsizei size = GLES2Util::GetGLDataTypeSizeForUniforms(type);
  if (size == 0) {
    SetGLError(GL_INVALID_OPERATION, "glGetUniform", "unknown type");
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
    uint32 immediate_data_size, const cmds::GetUniformiv& c) {
  GLuint program = c.program;
  GLint fake_location = c.location;
  GLuint service_id;
  GLenum result_type;
  GLint real_location = -1;
  Error error;
  void* result;
  if (GetUniformSetup(
      program, fake_location, c.params_shm_id, c.params_shm_offset,
      &error, &real_location, &service_id, &result, &result_type)) {
    glGetUniformiv(
        service_id, real_location,
        static_cast<cmds::GetUniformiv::Result*>(result)->GetData());
  }
  return error;
}

error::Error GLES2DecoderImpl::HandleGetUniformfv(
    uint32 immediate_data_size, const cmds::GetUniformfv& c) {
  GLuint program = c.program;
  GLint fake_location = c.location;
  GLuint service_id;
  GLint real_location = -1;
  Error error;
  typedef cmds::GetUniformfv::Result Result;
  Result* result;
  GLenum result_type;
  if (GetUniformSetup(
      program, fake_location, c.params_shm_id, c.params_shm_offset,
      &error, &real_location, &service_id,
      reinterpret_cast<void**>(&result), &result_type)) {
    if (result_type == GL_BOOL || result_type == GL_BOOL_VEC2 ||
        result_type == GL_BOOL_VEC3 || result_type == GL_BOOL_VEC4) {
      GLsizei num_values = result->GetNumResults();
      scoped_array<GLint> temp(new GLint[num_values]);
      glGetUniformiv(service_id, real_location, temp.get());
      GLfloat* dst = result->GetData();
      for (GLsizei ii = 0; ii < num_values; ++ii) {
        dst[ii] = (temp[ii] != 0);
      }
    } else {
      glGetUniformfv(service_id, real_location, result->GetData());
    }
  }
  return error;
}

error::Error GLES2DecoderImpl::HandleGetShaderPrecisionFormat(
    uint32 immediate_data_size, const cmds::GetShaderPrecisionFormat& c) {
  GLenum shader_type = static_cast<GLenum>(c.shadertype);
  GLenum precision_type = static_cast<GLenum>(c.precisiontype);
  typedef cmds::GetShaderPrecisionFormat::Result Result;
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
    SetGLErrorInvalidEnum(
        "glGetShaderPrecisionFormat", shader_type, "shader_type");
    return error::kNoError;
  }
  if (!validators_->shader_precision.IsValid(precision_type)) {
    SetGLErrorInvalidEnum(
        "glGetShaderPrecisionFormat", precision_type, "precision_type");
    return error::kNoError;
  }

  result->success = 1;  // true
  switch (precision_type) {
    case GL_LOW_INT:
    case GL_MEDIUM_INT:
    case GL_HIGH_INT:
      // These values are for a 32-bit twos-complement integer format.
      result->min_range = 31;
      result->max_range = 30;
      result->precision = 0;
      break;
    case GL_LOW_FLOAT:
    case GL_MEDIUM_FLOAT:
    case GL_HIGH_FLOAT:
      // These values are for an IEEE single-precision floating-point format.
      result->min_range = 127;
      result->max_range = 127;
      result->precision = 23;
      break;
    default:
      NOTREACHED();
      break;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetAttachedShaders(
    uint32 immediate_data_size, const cmds::GetAttachedShaders& c) {
  uint32 result_size = c.result_size;
  GLuint program = static_cast<GLuint>(c.program);
  Program* info = GetProgramInfoNotShader(
      program, "glGetAttachedShaders");
  if (!info) {
    return error::kNoError;
  }
  typedef cmds::GetAttachedShaders::Result Result;
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
    uint32 immediate_data_size, const cmds::GetActiveUniform& c) {
  GLuint program = c.program;
  GLuint index = c.index;
  uint32 name_bucket_id = c.name_bucket_id;
  typedef cmds::GetActiveUniform::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }
  Program* info = GetProgramInfoNotShader(
      program, "glGetActiveUniform");
  if (!info) {
    return error::kNoError;
  }
  const Program::UniformInfo* uniform_info =
      info->GetUniformInfo(index);
  if (!uniform_info) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveUniform", "index out of range");
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
    uint32 immediate_data_size, const cmds::GetActiveAttrib& c) {
  GLuint program = c.program;
  GLuint index = c.index;
  uint32 name_bucket_id = c.name_bucket_id;
  typedef cmds::GetActiveAttrib::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }
  Program* info = GetProgramInfoNotShader(
      program, "glGetActiveAttrib");
  if (!info) {
    return error::kNoError;
  }
  const Program::VertexAttrib* attrib_info =
      info->GetAttribInfo(index);
  if (!attrib_info) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveAttrib", "index out of range");
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
    uint32 immediate_data_size, const cmds::ShaderBinary& c) {
#if 1  // No binary shader support.
  SetGLError(GL_INVALID_OPERATION, "glShaderBinary", "not supported");
  return error::kNoError;
#else
  GLsizei n = static_cast<GLsizei>(c.n);
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glShaderBinary", "n < 0");
    return error::kNoError;
  }
  GLsizei length = static_cast<GLsizei>(c.length);
  if (length < 0) {
    SetGLError(GL_INVALID_VALUE, "glShaderBinary", "length < 0");
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
    Shader* info = GetShader(shaders[ii]);
    if (!info) {
      SetGLError(GL_INVALID_VALUE, "glShaderBinary", "unknown shader");
      return error::kNoError;
    }
    service_ids[ii] = info->service_id();
  }
  // TODO(gman): call glShaderBinary
  return error::kNoError;
#endif
}

error::Error GLES2DecoderImpl::HandleSwapBuffers(
    uint32 immediate_data_size, const cmds::SwapBuffers& c) {
  bool is_offscreen = !!offscreen_target_frame_buffer_.get();

  int this_frame_number = frame_number_++;
  // TRACE_EVENT for gpu tests:
  TRACE_EVENT_INSTANT2("test_gpu", "SwapBuffersLatency",
                       "GLImpl", static_cast<int>(gfx::GetGLImplementation()),
                       "width", (is_offscreen ? offscreen_size_.width() :
                                 surface_->GetSize().width()));
  TRACE_EVENT2("gpu", "GLES2DecoderImpl::HandleSwapBuffers",
               "offscreen", is_offscreen,
               "frame", this_frame_number);
  // If offscreen then don't actually SwapBuffers to the display. Just copy
  // the rendered frame to another frame buffer.
  if (is_offscreen) {
    TRACE_EVENT2("gpu", "Offscreen",
        "width", offscreen_size_.width(), "height", offscreen_size_.height());
    if (offscreen_size_ != offscreen_saved_color_texture_->size()) {
      // Workaround for NVIDIA driver bug on OS X; crbug.com/89557,
      // crbug.com/94163. TODO(kbr): figure out reproduction so Apple will
      // fix this.
      if (workarounds().needs_offscreen_buffer_workaround) {
        offscreen_saved_frame_buffer_->Create();
        glFinish();
      }

      // Allocate the offscreen saved color texture.
      DCHECK(offscreen_saved_color_format_);
      offscreen_saved_color_texture_->AllocateStorage(
          offscreen_size_, offscreen_saved_color_format_, false);

      offscreen_saved_frame_buffer_->AttachRenderTexture(
          offscreen_saved_color_texture_.get());
      if (offscreen_size_.width() != 0 && offscreen_size_.height() != 0) {
        if (offscreen_saved_frame_buffer_->CheckStatus() !=
            GL_FRAMEBUFFER_COMPLETE) {
          LOG(ERROR) << "GLES2DecoderImpl::ResizeOffscreenFrameBuffer failed "
                     << "because offscreen saved FBO was incomplete.";
          return error::kLostContext;
        }

        // Clear the offscreen color texture.
        // TODO(piman): Is this still necessary?
        {
          ScopedFrameBufferBinder binder(this,
                                         offscreen_saved_frame_buffer_->id());
          glClearColor(0, 0, 0, 0);
          glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
          glDisable(GL_SCISSOR_TEST);
          glClear(GL_COLOR_BUFFER_BIT);
          RestoreClearState();
        }
      }

      UpdateParentTextureInfo();
    }

    if (offscreen_size_.width() == 0 || offscreen_size_.height() == 0)
      return error::kNoError;
    ScopedGLErrorSuppressor suppressor(this);

    if (IsOffscreenBufferMultisampled()) {
      // For multisampled buffers, resolve the frame buffer.
      ScopedResolvedFrameBufferBinder binder(this, true, false);
      return error::kNoError;
    } else {
      ScopedFrameBufferBinder binder(this,
                                     offscreen_target_frame_buffer_->id());

      if (offscreen_target_buffer_preserved_) {
        // Copy the target frame buffer to the saved offscreen texture.
        offscreen_saved_color_texture_->Copy(
            offscreen_saved_color_texture_->size(),
            offscreen_saved_color_format_);
      } else {
        // Flip the textures in the parent context via the texture manager.
        if (!!offscreen_saved_color_texture_info_.get())
          offscreen_saved_color_texture_info_->
              SetServiceId(offscreen_target_color_texture_->id());

        offscreen_saved_color_texture_.swap(offscreen_target_color_texture_);
        offscreen_target_frame_buffer_->AttachRenderTexture(
            offscreen_target_color_texture_.get());
      }

      // Ensure the side effects of the copy are visible to the parent
      // context. There is no need to do this for ANGLE because it uses a
      // single D3D device for all contexts.
      if (!IsAngle())
        glFlush();
      return error::kNoError;
    }
  } else {
    TRACE_EVENT2("gpu", "Onscreen",
        "width", surface_->GetSize().width(),
        "height", surface_->GetSize().height());
    if (!surface_->SwapBuffers()) {
      LOG(ERROR) << "Context lost because SwapBuffers failed.";
      return error::kLostContext;
    }
  }

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleEnableFeatureCHROMIUM(
    uint32 immediate_data_size, const cmds::EnableFeatureCHROMIUM& c) {
  Bucket* bucket = GetBucket(c.bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  typedef cmds::EnableFeatureCHROMIUM::Result Result;
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
  if (feature_str.compare("pepper3d_allow_buffers_on_multiple_targets") == 0) {
    buffer_manager()->set_allow_buffers_on_multiple_targets(true);
  } else if (feature_str.compare("pepper3d_support_fixed_attribs") == 0) {
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
  } else if (feature_str.compare("webgl_enable_glsl_webgl_validation") == 0) {
    force_webgl_glsl_validation_ = true;
    InitializeShaderTranslator();
  } else {
    return error::kNoError;
  }

  *result = 1;  // true.
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetRequestableExtensionsCHROMIUM(
    uint32 immediate_data_size,
    const cmds::GetRequestableExtensionsCHROMIUM& c) {
  Bucket* bucket = CreateBucket(c.bucket_id);
  scoped_refptr<FeatureInfo> info(new FeatureInfo());
  info->Initialize(disallowed_features_, NULL);
  bucket->SetFromString(info->extensions().c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleRequestExtensionCHROMIUM(
    uint32 immediate_data_size, const cmds::RequestExtensionCHROMIUM& c) {
  Bucket* bucket = GetBucket(c.bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string feature_str;
  if (!bucket->GetAsString(&feature_str)) {
    return error::kInvalidArguments;
  }

  bool desire_webgl_glsl_validation =
      feature_str.find("GL_CHROMIUM_webglsl") != std::string::npos;
  bool desire_standard_derivatives = false;
  if (force_webgl_glsl_validation_) {
    desire_standard_derivatives =
        feature_str.find("GL_OES_standard_derivatives") != std::string::npos;
  }

  if (desire_webgl_glsl_validation != force_webgl_glsl_validation_ ||
      desire_standard_derivatives != derivatives_explicitly_enabled_) {
    force_webgl_glsl_validation_ = desire_webgl_glsl_validation;
    derivatives_explicitly_enabled_ = desire_standard_derivatives;
    InitializeShaderTranslator();
  }

  UpdateCapabilities();

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetMultipleIntegervCHROMIUM(
    uint32 immediate_data_size, const cmds::GetMultipleIntegervCHROMIUM& c) {
  GLuint count = c.count;
  uint32 pnames_size;
  if (!SafeMultiplyUint32(count, sizeof(GLenum), &pnames_size)) {
    return error::kOutOfBounds;
  }
  const GLenum* pnames = GetSharedMemoryAs<const GLenum*>(
      c.pnames_shm_id, c.pnames_shm_offset, pnames_size);
  if (pnames == NULL) {
    return error::kOutOfBounds;
  }

  // We have to copy them since we use them twice so the client
  // can't change them between the time we validate them and the time we use
  // them.
  scoped_array<GLenum> enums(new GLenum[count]);
  memcpy(enums.get(), pnames, pnames_size);

  // Count up the space needed for the result.
  uint32 num_results = 0;
  for (GLuint ii = 0; ii < count; ++ii) {
    uint32 num = util_.GLGetNumValuesReturned(enums[ii]);
    if (num == 0) {
      SetGLErrorInvalidEnum("glGetMulitpleCHROMIUM", enums[ii], "pname");
      return error::kNoError;
    }
    // Num will never be more than 4.
    DCHECK_LE(num, 4u);
    if (!SafeAddUint32(num_results, num, &num_results)) {
      return error::kOutOfBounds;
    }
  }

  uint32 result_size = 0;
  if (!SafeMultiplyUint32(num_results, sizeof(GLint), &result_size)) {
    return error::kOutOfBounds;
  }

  if (result_size != static_cast<uint32>(c.size)) {
    SetGLError(GL_INVALID_VALUE,
               "glGetMulitpleCHROMIUM", "bad size GL_INVALID_VALUE");
    return error::kNoError;
  }

  GLint* results = GetSharedMemoryAs<GLint*>(
      c.results_shm_id, c.results_shm_offset, result_size);
  if (results == NULL) {
    return error::kOutOfBounds;
  }

  // Check the results have been cleared in case the context was lost.
  for (uint32 ii = 0; ii < num_results; ++ii) {
    if (results[ii]) {
      return error::kInvalidArguments;
    }
  }

  // Get each result.
  GLint* start = results;
  for (GLuint ii = 0; ii < count; ++ii) {
    GLsizei num_written = 0;
    if (!state_.GetStateAsGLint(enums[ii], results, &num_written) &&
        !GetHelper(enums[ii], results, &num_written)) {
      glGetIntegerv(enums[ii], results);
    }
    results += num_written;
  }

  // Just to verify. Should this be a DCHECK?
  if (static_cast<uint32>(results - start) != num_results) {
    return error::kOutOfBounds;
  }

  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetProgramInfoCHROMIUM(
    uint32 immediate_data_size, const cmds::GetProgramInfoCHROMIUM& c) {
  GLuint program = static_cast<GLuint>(c.program);
  uint32 bucket_id = c.bucket_id;
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(ProgramInfoHeader));  // in case we fail.
  Program* info = NULL;
  info = GetProgram(program);
  if (!info || !info->IsValid()) {
    return error::kNoError;
  }
  info->GetProgram(program_manager(), bucket);
  return error::kNoError;
}

error::ContextLostReason GLES2DecoderImpl::GetContextLostReason() {
  switch (reset_status_) {
    case GL_NO_ERROR:
      // TODO(kbr): improve the precision of the error code in this case.
      // Consider delegating to context for error code if MakeCurrent fails.
      return error::kUnknown;
    case GL_GUILTY_CONTEXT_RESET_ARB:
      return error::kGuilty;
    case GL_INNOCENT_CONTEXT_RESET_ARB:
      return error::kInnocent;
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      return error::kUnknown;
  }

  NOTREACHED();
  return error::kUnknown;
}

bool GLES2DecoderImpl::WasContextLost() {
  if (reset_status_ != GL_NO_ERROR) {
    return true;
  }
  if (context_->WasAllocatedUsingRobustnessExtension()) {
    GLenum status = GL_NO_ERROR;
    if (has_robustness_extension_)
      status = glGetGraphicsResetStatusARB();
    if (status != GL_NO_ERROR) {
      // The graphics card was reset. Signal a lost context to the application.
      reset_status_ = status;
      LOG(ERROR) << (surface_->IsOffscreen() ? "Offscreen" : "Onscreen")
                 << " context lost via ARB/EXT_robustness. Reset status = "
                 << GLES2Util::GetStringEnum(status);
      return true;
    }
  }
  return false;
}

void GLES2DecoderImpl::LoseContext(uint32 reset_status) {
  // Only loses the context once.
  if (reset_status_ != GL_NO_ERROR) {
    return;
  }

  // Marks this context as lost.
  reset_status_ = reset_status;
  current_decoder_error_ = error::kLostContext;

  // Loses the parent's context.
  if (parent_) {
    parent_->LoseContext(reset_status);
  }

  // Loses any child contexts.
  for (ChildList::iterator it = children_.begin();
       it != children_.end();
       ++it) {
    (*it)->LoseContext(reset_status);
  }
}

error::Error GLES2DecoderImpl::HandleLoseContextCHROMIUM(
    uint32 immediate_data_size, const cmds::LoseContextCHROMIUM& c) {
  GLenum current = static_cast<GLenum>(c.current);
  GLenum other = static_cast<GLenum>(c.other);
  if (!validators_->reset_status.IsValid(current)) {
    SetGLErrorInvalidEnum("glLoseContextCHROMIUM", current, "current");
  }
  if (!validators_->reset_status.IsValid(other)) {
    SetGLErrorInvalidEnum("glLoseContextCHROMIUM", other, "other");
  }
  group_->LoseContexts(other);
  reset_status_ = current;
  current_decoder_error_ = error::kLostContext;
  return error::kLostContext;
}

error::Error GLES2DecoderImpl::HandleWaitSyncPointCHROMIUM(
    uint32 immediate_data_size, const cmds::WaitSyncPointCHROMIUM& c) {
  if (wait_sync_point_callback_.is_null())
    return error::kNoError;

  return wait_sync_point_callback_.Run(c.sync_point) ?
      error::kNoError : error::kDeferCommandUntilLater;
}

bool GLES2DecoderImpl::GenQueriesEXTHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (query_manager_->GetQuery(client_ids[ii])) {
      return false;
    }
  }
  // NOTE: We don't generate Query objects here. Only in BeginQueryEXT
  return true;
}

void GLES2DecoderImpl::DeleteQueriesEXTHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    QueryManager::Query* query = query_manager_->GetQuery(client_ids[ii]);
    if (query && !query->IsDeleted()) {
      if (query == state_.current_query) {
        state_.current_query = NULL;
      }
      query->Destroy(true);
      query_manager_->RemoveQuery(client_ids[ii]);
    }
  }
}

bool GLES2DecoderImpl::ProcessPendingQueries() {
  if (query_manager_.get() == NULL) {
    return false;
  }
  if (!query_manager_->ProcessPendingQueries()) {
    current_decoder_error_ = error::kOutOfBounds;
  }
  return query_manager_->HavePendingQueries();
}

error::Error GLES2DecoderImpl::HandleBeginQueryEXT(
    uint32 immediate_data_size, const cmds::BeginQueryEXT& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint client_id = static_cast<GLuint>(c.id);
  int32 sync_shm_id = static_cast<int32>(c.sync_data_shm_id);
  uint32 sync_shm_offset = static_cast<uint32>(c.sync_data_shm_offset);

  switch (target) {
    case GL_COMMANDS_ISSUED_CHROMIUM:
    case GL_LATENCY_QUERY_CHROMIUM:
    case GL_ASYNC_PIXEL_TRANSFERS_COMPLETED_CHROMIUM:
      break;
    default:
      if (!features().occlusion_query_boolean) {
        SetGLError(GL_INVALID_OPERATION, "glBeginQueryEXT", "not enabled");
        return error::kNoError;
      }
      break;
  }

  if (state_.current_query) {
    SetGLError(
        GL_INVALID_OPERATION, "glBeginQueryEXT", "query already in progress");
    return error::kNoError;
  }

  if (client_id == 0) {
    SetGLError(GL_INVALID_OPERATION, "glBeginQueryEXT", "id is 0");
    return error::kNoError;
  }

  QueryManager::Query* query = query_manager_->GetQuery(client_id);
  if (!query) {
    // TODO(gman): Decide if we need this check.
    //
    // Checks id was made by glGenQueries
    //
    // From the POV of OpenGL ES 2.0 you need to call glGenQueriesEXT
    // for all Query ids but from the POV of the command buffer service maybe
    // you don't.
    //
    // The client can enforce this. I don't think the service cares.
    //
    // IdAllocatorInterface* id_allocator =
    //     group_->GetIdAllocator(id_namespaces::kQueries);
    // if (!id_allocator->InUse(client_id)) {
    //   SetGLError(GL_INVALID_OPERATION,
    //              "glBeginQueryEXT", "id not made by glGenQueriesEXT");
    //   return error::kNoError;
    // }
    query = query_manager_->CreateQuery(
        target, client_id, sync_shm_id, sync_shm_offset);
  }

  if (query->target() != target) {
    SetGLError(
        GL_INVALID_OPERATION, "glBeginQueryEXT", "target does not match");
    return error::kNoError;
  } else if (query->shm_id() != sync_shm_id ||
             query->shm_offset() != sync_shm_offset) {
    DLOG(ERROR) << "Shared memory used by query not the same as before";
    return error::kInvalidArguments;
  }

  if (!query_manager_->BeginQuery(query)) {
    return error::kOutOfBounds;
  }

  state_.current_query = query;
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleEndQueryEXT(
    uint32 immediate_data_size, const cmds::EndQueryEXT& c) {
  GLenum target = static_cast<GLenum>(c.target);
  uint32 submit_count = static_cast<GLuint>(c.submit_count);

  if (!state_.current_query) {
    SetGLError(GL_INVALID_OPERATION, "glEndQueryEXT", "No active query");
    return error::kNoError;
  }
  if (state_.current_query->target() != target) {
    SetGLError(GL_INVALID_OPERATION,
               "glEndQueryEXT", "target does not match active query");
    return error::kNoError;
  }

  if (!query_manager_->EndQuery(state_.current_query, submit_count)) {
    return error::kOutOfBounds;
  }

  query_manager_->ProcessPendingTransferQueries();

  state_.current_query = NULL;
  return error::kNoError;
}

bool GLES2DecoderImpl::GenVertexArraysOESHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (GetVertexAttribManager(client_ids[ii])) {
      return false;
    }
  }

  if (!features().native_vertex_array_object) {
    // Emulated VAO
    for (GLsizei ii = 0; ii < n; ++ii) {
      CreateVertexAttribManager(client_ids[ii], 0);
    }
  } else {
    scoped_array<GLuint> service_ids(new GLuint[n]);

    glGenVertexArraysOES(n, service_ids.get());
    for (GLsizei ii = 0; ii < n; ++ii) {
      CreateVertexAttribManager(client_ids[ii], service_ids[ii]);
    }
  }

  return true;
}

void GLES2DecoderImpl::DeleteVertexArraysOESHelper(
    GLsizei n, const GLuint* client_ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    VertexAttribManager* vao =
        GetVertexAttribManager(client_ids[ii]);
    if (vao && !vao->IsDeleted()) {
      if (state_.vertex_attrib_manager == vao) {
        state_.vertex_attrib_manager = default_vertex_attrib_manager_;
      }
      RemoveVertexAttribManager(client_ids[ii]);
    }
  }
}

void GLES2DecoderImpl::DoBindVertexArrayOES(GLuint client_id) {
  VertexAttribManager* vao = NULL;
  GLuint service_id = 0;
  if (client_id != 0) {
    vao = GetVertexAttribManager(client_id);
    if (!vao) {
      // Unlike most Bind* methods, the spec explicitly states that VertexArray
      // only allows names that have been previously generated. As such, we do
      // not generate new names here.
      SetGLError(GL_INVALID_OPERATION,
          "glBindVertexArrayOES", ""
          "bad vertex array id.");
      current_decoder_error_ = error::kNoError;
      return;
    } else {
      service_id = vao->service_id();
    }
  } else {
    vao = default_vertex_attrib_manager_;
  }

  // Only set the VAO state if it's changed
  if (state_.vertex_attrib_manager != vao) {
    state_.vertex_attrib_manager = vao;
    if (!features().native_vertex_array_object) {
      EmulateVertexArrayState();
    } else {
      glBindVertexArrayOES(service_id);
    }
  }
}

// Used when OES_vertex_array_object isn't natively supported
void GLES2DecoderImpl::EmulateVertexArrayState() {
  // Setup the Vertex attribute state
  for (uint32 vv = 0; vv < group_->max_vertex_attribs(); ++vv) {
    RestoreStateForAttrib(vv);
  }

  // Setup the element buffer
  Buffer* element_array_buffer =
      state_.vertex_attrib_manager->element_array_buffer();
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
      element_array_buffer ? element_array_buffer->service_id() : 0);
}

bool GLES2DecoderImpl::DoIsVertexArrayOES(GLuint client_id) {
  const VertexAttribManager* vao =
      GetVertexAttribManager(client_id);
  return vao && vao->IsValid() && !vao->IsDeleted();
}

error::Error GLES2DecoderImpl::HandleCreateStreamTextureCHROMIUM(
    uint32 immediate_data_size,
    const cmds::CreateStreamTextureCHROMIUM& c) {
  if (!features().chromium_stream_texture) {
    SetGLError(GL_INVALID_OPERATION,
               "glOpenStreamTextureCHROMIUM", ""
               "not supported.");
    return error::kNoError;
  }

  uint32 client_id = c.client_id;
  typedef cmds::CreateStreamTextureCHROMIUM::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));

  if (!result)
    return error::kOutOfBounds;
  *result = GL_ZERO;
  Texture* info =
      texture_manager()->GetTexture(client_id);
  if (!info) {
    SetGLError(GL_INVALID_VALUE,
               "glCreateStreamTextureCHROMIUM", ""
               "bad texture id.");
    return error::kNoError;
  }

  if (info->IsStreamTexture()) {
    SetGLError(GL_INVALID_OPERATION,
               "glCreateStreamTextureCHROMIUM", ""
               "is already a stream texture.");
    return error::kNoError;
  }

  if (info->target() && info->target() != GL_TEXTURE_EXTERNAL_OES) {
    SetGLError(GL_INVALID_OPERATION,
               "glCreateStreamTextureCHROMIUM", ""
               "is already bound to incompatible target.");
    return error::kNoError;
  }

  if (!stream_texture_manager_)
    return error::kInvalidArguments;

  GLuint object_id = stream_texture_manager_->CreateStreamTexture(
      info->service_id(), client_id);

  if (object_id) {
    info->SetStreamTexture(true);
  } else {
    SetGLError(GL_OUT_OF_MEMORY,
               "glCreateStreamTextureCHROMIUM", ""
               "failed to create platform texture.");
  }

  *result = object_id;
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDestroyStreamTextureCHROMIUM(
    uint32 immediate_data_size,
    const cmds::DestroyStreamTextureCHROMIUM& c) {
  GLuint client_id = c.texture;
  Texture* info =
      texture_manager()->GetTexture(client_id);
  if (info && info->IsStreamTexture()) {
    if (!stream_texture_manager_)
      return error::kInvalidArguments;

    stream_texture_manager_->DestroyStreamTexture(info->service_id());
    info->SetStreamTexture(false);
  } else {
    SetGLError(GL_INVALID_VALUE,
               "glDestroyStreamTextureCHROMIUM", "bad texture id.");
  }

  return error::kNoError;
}

#if defined(OS_MACOSX)
void GLES2DecoderImpl::ReleaseIOSurfaceForTexture(GLuint texture_id) {
  TextureToIOSurfaceMap::iterator it = texture_to_io_surface_map_.find(
      texture_id);
  if (it != texture_to_io_surface_map_.end()) {
    // Found a previous IOSurface bound to this texture; release it.
    CFTypeRef surface = it->second;
    CFRelease(surface);
    texture_to_io_surface_map_.erase(it);
  }
}
#endif

void GLES2DecoderImpl::DoTexImageIOSurface2DCHROMIUM(
    GLenum target, GLsizei width, GLsizei height,
    GLuint io_surface_id, GLuint plane) {
#if defined(OS_MACOSX)
  if (gfx::GetGLImplementation() != gfx::kGLImplementationDesktopGL) {
    SetGLError(
        GL_INVALID_OPERATION,
        "glTexImageIOSurface2DCHROMIUM", "only supported on desktop GL.");
    return;
  }

  IOSurfaceSupport* surface_support = IOSurfaceSupport::Initialize();
  if (!surface_support) {
    SetGLError(GL_INVALID_OPERATION,
               "glTexImageIOSurface2DCHROMIUM", "only supported on 10.6.");
    return;
  }

  if (target != GL_TEXTURE_RECTANGLE_ARB) {
    // This might be supported in the future, and if we could require
    // support for binding an IOSurface to a NPOT TEXTURE_2D texture, we
    // could delete a lot of code. For now, perform strict validation so we
    // know what's going on.
    SetGLError(
        GL_INVALID_OPERATION,
        "glTexImageIOSurface2DCHROMIUM",
        "requires TEXTURE_RECTANGLE_ARB target");
    return;
  }

  // Default target might be conceptually valid, but disallow it to avoid
  // accidents.
  Texture* info = GetTextureInfoForTargetUnlessDefault(
      target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glTexImageIOSurface2DCHROMIUM", "no rectangle texture bound");
    return;
  }

  // Look up the new IOSurface. Note that because of asynchrony
  // between processes this might fail; during live resizing the
  // plugin process might allocate and release an IOSurface before
  // this process gets a chance to look it up. Hold on to any old
  // IOSurface in this case.
  CFTypeRef surface = surface_support->IOSurfaceLookup(io_surface_id);
  if (!surface) {
    SetGLError(
        GL_INVALID_OPERATION,
        "glTexImageIOSurface2DCHROMIUM", "no IOSurface with the given ID");
    return;
  }

  // Release any IOSurface previously bound to this texture.
  ReleaseIOSurfaceForTexture(info->service_id());

  // Make sure we release the IOSurface even if CGLTexImageIOSurface2D fails.
  texture_to_io_surface_map_.insert(
      std::make_pair(info->service_id(), surface));

  CGLContextObj context =
      static_cast<CGLContextObj>(context_->GetHandle());

  CGLError err = surface_support->CGLTexImageIOSurface2D(
      context,
      target,
      GL_RGBA,
      width,
      height,
      GL_BGRA,
      GL_UNSIGNED_INT_8_8_8_8_REV,
      surface,
      plane);

  if (err != kCGLNoError) {
    SetGLError(
        GL_INVALID_OPERATION,
        "glTexImageIOSurface2DCHROMIUM", "error in CGLTexImageIOSurface2D");
    return;
  }

  texture_manager()->SetLevelInfo(
      info, target, 0, GL_RGBA, width, height, 1, 0,
      GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, true);

#else
  SetGLError(GL_INVALID_OPERATION,
             "glTexImageIOSurface2DCHROMIUM", "not supported.");
#endif
}

static GLenum ExtractFormatFromStorageFormat(GLenum internalformat) {
  switch (internalformat) {
    case GL_RGB565:
      return GL_RGB;
    case GL_RGBA4:
      return GL_RGBA;
    case GL_RGB5_A1:
      return GL_RGBA;
    case GL_RGB8_OES:
      return GL_RGB;
    case GL_RGBA8_OES:
      return GL_RGBA;
    case GL_LUMINANCE8_ALPHA8_EXT:
      return GL_LUMINANCE_ALPHA;
    case GL_LUMINANCE8_EXT:
      return GL_LUMINANCE;
    case GL_ALPHA8_EXT:
      return GL_ALPHA;
    case GL_RGBA32F_EXT:
      return GL_RGBA;
    case GL_RGB32F_EXT:
      return GL_RGB;
    case GL_ALPHA32F_EXT:
      return GL_ALPHA;
    case GL_LUMINANCE32F_EXT:
      return GL_LUMINANCE;
    case GL_LUMINANCE_ALPHA32F_EXT:
      return GL_LUMINANCE_ALPHA;
    case GL_RGBA16F_EXT:
      return GL_RGBA;
    case GL_RGB16F_EXT:
      return GL_RGB;
    case GL_ALPHA16F_EXT:
      return GL_ALPHA;
    case GL_LUMINANCE16F_EXT:
      return GL_LUMINANCE;
    case GL_LUMINANCE_ALPHA16F_EXT:
      return GL_LUMINANCE_ALPHA;
    case GL_BGRA8_EXT:
      return GL_BGRA_EXT;
    default:
      return GL_NONE;
  }
}

void GLES2DecoderImpl::DoCopyTextureCHROMIUM(
    GLenum target, GLuint source_id, GLuint dest_id, GLint level,
    GLenum internal_format) {
  Texture* dest_info = GetTexture(dest_id);
  Texture* source_info = GetTexture(source_id);

  if (!source_info || !dest_info) {
    SetGLError(GL_INVALID_VALUE, "glCopyTextureCHROMIUM", "unknown texture id");
    return;
  }

  if (GL_TEXTURE_2D != target) {
    SetGLError(GL_INVALID_VALUE,
               "glCopyTextureCHROMIUM", "invalid texture target");
    return;
  }

  if (dest_info->target() != GL_TEXTURE_2D ||
      source_info->target() != GL_TEXTURE_2D) {
    SetGLError(GL_INVALID_VALUE,
               "glCopyTextureCHROMIUM", "invalid texture target binding");
    return;
  }

  int source_width, source_height, dest_width, dest_height;
  if (!source_info->GetLevelSize(GL_TEXTURE_2D, 0, &source_width,
                                 &source_height)) {
    SetGLError(GL_INVALID_VALUE,
               "glCopyTextureChromium", "source texture has no level 0");
    return;
  }

  // Check that this type of texture is allowed.
  if (!texture_manager()->ValidForTarget(GL_TEXTURE_2D, level, source_width,
                                         source_height, 1)) {
    SetGLError(GL_INVALID_VALUE,
             "glCopyTextureCHROMIUM", "Bad dimensions");
    return;
  }

  // Defer initializing the CopyTextureCHROMIUMResourceManager until it is
  // needed because it takes 10s of milliseconds to initialize.
  if (!copy_texture_CHROMIUM_.get()) {
    CopyRealGLErrorsToWrapper();
    copy_texture_CHROMIUM_.reset(new CopyTextureCHROMIUMResourceManager());
    copy_texture_CHROMIUM_->Initialize(this);
    RestoreCurrentFramebufferBindings();
    if (PeekGLError() != GL_NO_ERROR)
      return;
  }

  GLenum dest_type;
  GLenum dest_internal_format;
  bool dest_level_defined = dest_info->GetLevelSize(GL_TEXTURE_2D, level,
                                                    &dest_width,
                                                    &dest_height);

  if (dest_level_defined) {
    dest_info->GetLevelType(GL_TEXTURE_2D, level, &dest_type,
                            &dest_internal_format);
  } else {
    GLenum source_internal_format;
    source_info->GetLevelType(GL_TEXTURE_2D, 0, &dest_type,
                              &source_internal_format);
  }

  // Resize the destination texture to the dimensions of the source texture.
  if (!dest_level_defined || dest_width != source_width ||
      dest_height != source_height ||
      dest_internal_format != internal_format) {
    // Ensure that the glTexImage2D succeeds.
    CopyRealGLErrorsToWrapper();
    glBindTexture(GL_TEXTURE_2D, dest_info->service_id());
    WrappedTexImage2D(
        GL_TEXTURE_2D, level, internal_format, source_width, source_height,
        0, internal_format, dest_type, NULL);
    GLenum error = PeekGLError();
    if (error != GL_NO_ERROR) {
      RestoreCurrentTexture2DBindings();
      return;
    }

    texture_manager()->SetLevelInfo(
        dest_info, GL_TEXTURE_2D, level, internal_format, source_width,
        source_height, 1, 0, internal_format, dest_type, true);
  } else {
    texture_manager()->SetLevelCleared(dest_info, GL_TEXTURE_2D, level, true);
  }

  copy_texture_CHROMIUM_->DoCopyTexture(this,
                                        source_info->target(),
                                        dest_info->target(),
                                        source_info->service_id(),
                                        dest_info->service_id(), level,
                                        source_width, source_height,
                                        unpack_flip_y_,
                                        unpack_premultiply_alpha_,
                                        unpack_unpremultiply_alpha_);
}

static GLenum ExtractTypeFromStorageFormat(GLenum internalformat) {
  switch (internalformat) {
    case GL_RGB565:
      return GL_UNSIGNED_SHORT_5_6_5;
    case GL_RGBA4:
      return GL_UNSIGNED_SHORT_4_4_4_4;
    case GL_RGB5_A1:
      return GL_UNSIGNED_SHORT_5_5_5_1;
    case GL_RGB8_OES:
      return GL_UNSIGNED_BYTE;
    case GL_RGBA8_OES:
      return GL_UNSIGNED_BYTE;
    case GL_LUMINANCE8_ALPHA8_EXT:
      return GL_UNSIGNED_BYTE;
    case GL_LUMINANCE8_EXT:
      return GL_UNSIGNED_BYTE;
    case GL_ALPHA8_EXT:
      return GL_UNSIGNED_BYTE;
    case GL_RGBA32F_EXT:
      return GL_FLOAT;
    case GL_RGB32F_EXT:
      return GL_FLOAT;
    case GL_ALPHA32F_EXT:
      return GL_FLOAT;
    case GL_LUMINANCE32F_EXT:
      return GL_FLOAT;
    case GL_LUMINANCE_ALPHA32F_EXT:
      return GL_FLOAT;
    case GL_RGBA16F_EXT:
      return GL_HALF_FLOAT_OES;
    case GL_RGB16F_EXT:
      return GL_HALF_FLOAT_OES;
    case GL_ALPHA16F_EXT:
      return GL_HALF_FLOAT_OES;
    case GL_LUMINANCE16F_EXT:
      return GL_HALF_FLOAT_OES;
    case GL_LUMINANCE_ALPHA16F_EXT:
      return GL_HALF_FLOAT_OES;
    case GL_BGRA8_EXT:
      return GL_UNSIGNED_BYTE;
    default:
      return GL_NONE;
  }
}

void GLES2DecoderImpl::DoTexStorage2DEXT(
    GLenum target,
    GLint levels,
    GLenum internal_format,
    GLsizei width,
    GLsizei height) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoTexStorage2DEXT");
  if (!texture_manager()->ValidForTarget(target, 0, width, height, 1) ||
      TextureManager::ComputeMipMapCount(width, height, 1) < levels) {
    SetGLError(
        GL_INVALID_VALUE, "glTexStorage2DEXT", "dimensions out of range");
    return;
  }
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glTexStorage2DEXT", "unknown texture for target");
    return;
  }
  if (info->IsAttachedToFramebuffer()) {
    clear_state_dirty_ = true;
  }
  if (info->IsImmutable()) {
    SetGLError(GL_INVALID_OPERATION,
               "glTexStorage2DEXT", "texture is immutable");
    return;
  }

  GLenum format = ExtractFormatFromStorageFormat(internal_format);
  GLenum type = ExtractTypeFromStorageFormat(internal_format);

  {
    GLsizei level_width = width;
    GLsizei level_height = height;
    uint32 estimated_size = 0;
    for (int ii = 0; ii < levels; ++ii) {
      uint32 level_size = 0;
      if (!GLES2Util::ComputeImageDataSizes(
          level_width, level_height, format, type, state_.unpack_alignment,
          &estimated_size, NULL, NULL) ||
          !SafeAddUint32(estimated_size, level_size, &estimated_size)) {
        SetGLError(GL_OUT_OF_MEMORY,
                   "glTexStorage2DEXT", "dimensions too large");
        return;
      }
      level_width = std::max(1, level_width >> 1);
      level_height = std::max(1, level_height >> 1);
    }
    if (!EnsureGPUMemoryAvailable(estimated_size)) {
      SetGLError(GL_OUT_OF_MEMORY, "glTexStorage2DEXT", "out of memory");
      return;
    }
  }

  CopyRealGLErrorsToWrapper();
  glTexStorage2DEXT(target, levels, GetTexInternalFormat(internal_format),
                    width, height);
  GLenum error = PeekGLError();
  if (error == GL_NO_ERROR) {
    GLsizei level_width = width;
    GLsizei level_height = height;
    for (int ii = 0; ii < levels; ++ii) {
      texture_manager()->SetLevelInfo(
          info, target, ii, format, level_width, level_height, 1, 0, format,
          type, false);
      level_width = std::max(1, level_width >> 1);
      level_height = std::max(1, level_height >> 1);
    }
    info->SetImmutable(true);
  }
}

error::Error GLES2DecoderImpl::HandleGenMailboxCHROMIUM(
    uint32 immediate_data_size, const cmds::GenMailboxCHROMIUM& c) {
  MailboxName name;
  mailbox_manager()->GenerateMailboxName(&name);
  uint32 bucket_id = static_cast<uint32>(c.bucket_id);
  Bucket* bucket = CreateBucket(bucket_id);

  bucket->SetSize(GL_MAILBOX_SIZE_CHROMIUM);
  bucket->SetData(&name, 0, GL_MAILBOX_SIZE_CHROMIUM);

  return error::kNoError;
}

void GLES2DecoderImpl::DoProduceTextureCHROMIUM(GLenum target,
                                                const GLbyte* mailbox) {
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glProduceTextureCHROMIUM", "unknown texture for target");
    return;
  }

  TextureDefinition* definition = texture_manager()->Save(info);
  if (!definition) {
    SetGLError(GL_INVALID_OPERATION,
               "glProduceTextureCHROMIUM", "invalid texture");
    return;
  }

  if (!group_->mailbox_manager()->ProduceTexture(
      target,
      *reinterpret_cast<const MailboxName*>(mailbox),
      definition,
      texture_manager())) {
    bool success = texture_manager()->Restore(info, definition);
    DCHECK(success);
    SetGLError(GL_INVALID_OPERATION,
               "glProduceTextureCHROMIUM", "invalid mailbox name");
    return;
  }

  glBindTexture(info->target(), info->service_id());
}

void GLES2DecoderImpl::DoConsumeTextureCHROMIUM(GLenum target,
                                                const GLbyte* mailbox) {
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glConsumeTextureCHROMIUM", "unknown texture for target");
    return;
  }

  scoped_ptr<TextureDefinition> definition(
      group_->mailbox_manager()->ConsumeTexture(
      target,
      *reinterpret_cast<const MailboxName*>(mailbox)));
  if (!definition.get()) {
    SetGLError(GL_INVALID_OPERATION,
               "glConsumeTextureCHROMIUM", "invalid mailbox name");
    return;
  }

  if (!texture_manager()->Restore(info, definition.release())) {
    SetGLError(GL_INVALID_OPERATION,
               "glConsumeTextureCHROMIUM", "invalid texture");
    return;
  }

  glBindTexture(info->target(), info->service_id());
}

void GLES2DecoderImpl::DoInsertEventMarkerEXT(
    GLsizei length, const GLchar* marker) {
  if (!marker) {
    marker = "";
  }
  debug_marker_manager_.SetMarker(
      length ? std::string(marker, length) : std::string(marker));
}

void GLES2DecoderImpl::DoPushGroupMarkerEXT(
    GLsizei length, const GLchar* marker) {
  if (!marker) {
    marker = "";
  }
  debug_marker_manager_.PushGroup(
      length ? std::string(marker, length) : std::string(marker));
}

void GLES2DecoderImpl::DoPopGroupMarkerEXT(void) {
  debug_marker_manager_.PopGroup();
}

void GLES2DecoderImpl::DoBindTexImage2DCHROMIUM(
    GLenum target, GLint image_id) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoBindTexImage2DCHROMIUM");
  if (target != GL_TEXTURE_2D) {
    // This might be supported in the future.
    SetGLError(
        GL_INVALID_OPERATION,
        "glBindTexImage2DCHROMIUM", "requires TEXTURE_2D target");
    return;
  }

  // Default target might be conceptually valid, but disallow it to avoid
  // accidents.
  Texture* info = GetTextureInfoForTargetUnlessDefault(
      target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glBindTexImage2DCHROMIUM", "no texture bound");
    return;
  }

  gfx::GLImage* gl_image = image_manager()->LookupImage(image_id);
  if (!gl_image) {
    SetGLError(GL_INVALID_OPERATION,
               "glBindTexImage2DCHROMIUM",
               "no image found with the given ID");
    return;
  }

  if (!gl_image->BindTexImage()) {
    SetGLError(GL_INVALID_OPERATION,
               "glBindTexImage2DCHROMIUM",
               "fail to bind image with the given ID");
    return;
  }

  gfx::Size size = gl_image->GetSize();
  texture_manager()->SetLevelInfo(
      info, target, 0, GL_RGBA, size.width(), size.height(), 1, 0,
      GL_RGBA, GL_UNSIGNED_BYTE, true);
  texture_manager()->SetLevelImage(info, target, 0, gl_image);
}

void GLES2DecoderImpl::DoReleaseTexImage2DCHROMIUM(
    GLenum target, GLint image_id) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::DoReleaseTexImage2DCHROMIUM");
  if (target != GL_TEXTURE_2D) {
    // This might be supported in the future.
    SetGLError(
        GL_INVALID_OPERATION,
        "glReleaseTexImage2DCHROMIUM", "requires TEXTURE_2D target");
    return;
  }

  // Default target might be conceptually valid, but disallow it to avoid
  // accidents.
  Texture* info = GetTextureInfoForTargetUnlessDefault(
      target);
  if (!info) {
    SetGLError(GL_INVALID_OPERATION,
               "glReleaseTexImage2DCHROMIUM", "no texture bound");
    return;
  }

  gfx::GLImage* gl_image = image_manager()->LookupImage(image_id);
  if (!gl_image) {
    SetGLError(GL_INVALID_OPERATION,
               "glReleaseTexImage2DCHROMIUM",
               "no image found with the given ID");
    return;
  }

  // Do nothing when image is not currently bound.
  if (info->GetLevelImage(target, 0) != gl_image)
    return;

  gl_image->ReleaseTexImage();

  texture_manager()->SetLevelInfo(
      info, target, 0, GL_RGBA, 0, 0, 1, 0,
      GL_RGBA, GL_UNSIGNED_BYTE, false);
}

error::Error GLES2DecoderImpl::HandleTraceBeginCHROMIUM(
    uint32 immediate_data_size, const cmds::TraceBeginCHROMIUM& c) {
  Bucket* bucket = GetBucket(c.bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string command_name;
  if (!bucket->GetAsString(&command_name)) {
    return error::kInvalidArguments;
  }
  TRACE_EVENT_COPY_ASYNC_BEGIN0("gpu", command_name.c_str(), this);
  if (!gpu_tracer_->Begin(command_name)) {
    SetGLError(GL_INVALID_OPERATION,
               "glTraceBeginCHROMIUM", "unable to create begin trace");
    return error::kNoError;
  }
  return error::kNoError;
}

void GLES2DecoderImpl::DoTraceEndCHROMIUM() {
  if (gpu_tracer_->CurrentName().empty()) {
    SetGLError(GL_INVALID_OPERATION,
               "glTraceEndCHROMIUM", "no trace begin found");
    return;
  }
  TRACE_EVENT_COPY_ASYNC_END0("gpu", gpu_tracer_->CurrentName().c_str(), this);
  gpu_tracer_->End();
}

bool GLES2DecoderImpl::ValidateAsyncTransfer(
    const char* function_name,
    Texture* info,
    GLenum target,
    GLint level,
    const void * data) {
  // We only support async uploads to 2D textures for now.
  if (GL_TEXTURE_2D != target) {
    SetGLErrorInvalidEnum(function_name, target, "target");
    return false;
  }
  // We only support uploads to level zero for now.
  if (level != 0) {
    SetGLError(GL_INVALID_VALUE, function_name, "level != 0");
    return false;
  }
  // A transfer buffer must be bound, even for asyncTexImage2D.
  if (data == NULL) {
    SetGLError(GL_INVALID_OPERATION, function_name, "buffer == 0");
    return false;
  }
  // We only support one async transfer in progress.
  if (!info || info->AsyncTransferIsInProgress()) {
    SetGLError(GL_INVALID_OPERATION,
        function_name, "transfer already in progress");
    return false;
  }
  return true;
}

error::Error GLES2DecoderImpl::HandleAsyncTexImage2DCHROMIUM(
    uint32 immediate_data_size, const cmds::AsyncTexImage2DCHROMIUM& c) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::HandleAsyncTexImage2DCHROMIUM");
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

  // TODO(epenner): Move this and copies of this memory validation
  // into ValidateTexImage2D step.
  if (!GLES2Util::ComputeImageDataSizes(
      width, height, format, type, state_.unpack_alignment, &pixels_size, NULL,
      NULL)) {
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

  // All the normal glTexSubImage2D validation.
  if (!ValidateTexImage2D(
      "glAsyncTexImage2DCHROMIUM", target, level, internal_format,
      width, height, border, format, type, pixels, pixels_size)) {
    return error::kNoError;
  }

  // Extra async validation.
  Texture* info = GetTextureInfoForTarget(target);
  if (!ValidateAsyncTransfer(
      "glAsyncTexImage2DCHROMIUM", info, target, level, pixels))
    return error::kNoError;

  // Don't allow async redefinition of a textures.
  if (info->IsDefined()) {
    SetGLError(GL_INVALID_OPERATION,
        "glAsyncTexImage2DCHROMIUM", "already defined");
    return error::kNoError;
  }

  if (!EnsureGPUMemoryAvailable(pixels_size)) {
    SetGLError(GL_OUT_OF_MEMORY, "glAsyncTexImage2DCHROMIUM", "out of memory");
    return error::kNoError;
  }

  // We know the memory/size is safe, so get the real shared memory since
  // it might need to be duped to prevent use-after-free of the memory.
  gpu::Buffer buffer = GetSharedMemoryBuffer(c.pixels_shm_id);
  base::SharedMemory* shared_memory = buffer.shared_memory;
  uint32 shm_size = buffer.size;
  uint32 shm_data_offset = c.pixels_shm_offset;
  uint32 shm_data_size = pixels_size;

  // Set up the async state if needed, and make the texture
  // immutable so the async state stays valid. The level info
  // is set up lazily when the transfer completes.
  DCHECK(!info->GetAsyncTransferState());
  info->SetAsyncTransferState(
      async_pixel_transfer_delegate_->
          CreatePixelTransferState(info->service_id()));
  info->SetImmutable(true);

  // Issue the async call and set up the texture.
  GLenum gl_internal_format =
      GetTexInternalFormat(internal_format, format, type);
  gfx::AsyncTexImage2DParams tex_params = {target, level, gl_internal_format,
                                           width, height, border, format, type};
  gfx::AsyncMemoryParams mem_params = {shared_memory, shm_size,
                                       shm_data_offset, shm_data_size};

  // Add a pending transfer to the texture manager, which will bind the
  // transfer data to the texture and set the level info at the same time,
  // after the the transfer is complete.
  texture_manager()->AddPendingAsyncPixelTransfer(
      info->GetAsyncTransferState()->AsWeakPtr(), info);

  async_pixel_transfer_delegate_->AsyncTexImage2D(
      info->GetAsyncTransferState(), tex_params, mem_params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleAsyncTexSubImage2DCHROMIUM(
    uint32 immediate_data_size, const cmds::AsyncTexSubImage2DCHROMIUM& c) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::HandleAsyncTexSubImage2DCHROMIUM");
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);

  // TODO(epenner): Move this and copies of this memory validation
  // into ValidateTexSubImage2D step.
  uint32 data_size;
  if (!GLES2Util::ComputeImageDataSizes(
      width, height, format, type, state_.unpack_alignment, &data_size,
      NULL, NULL)) {
    return error::kOutOfBounds;
  }
  const void* pixels = GetSharedMemoryAs<const void*>(
      c.data_shm_id, c.data_shm_offset, data_size);

  // All the normal glTexSubImage2D validation.
  error::Error error = error::kNoError;
  if (!ValidateTexSubImage2D(&error, "glAsyncTexSubImage2DCHROMIUM",
      target, level, xoffset, yoffset, width, height, format, type, pixels)) {
    return error;
  }

  // Extra async validation.
  Texture* info = GetTextureInfoForTarget(target);
  if (!ValidateAsyncTransfer(
         "glAsyncTexSubImage2DCHROMIUM", info, target, level, pixels))
    return error::kNoError;

  // Guarantee async textures are always 'cleared' as follows:
  // - AsyncTexImage2D can not redefine an existing texture
  // - AsyncTexImage2D must initialize the entire image via non-null buffer.
  // - AsyncTexSubImage2D clears synchronously if not already cleared.
  // - Textures become immutable after an async call.
  // This way we know in all cases that an async texture is always clear.
  if (!info->SafeToRenderFrom()) {
    if (!texture_manager()->ClearTextureLevel(this, info, target, level)) {
      SetGLError(GL_OUT_OF_MEMORY,
          "glAsyncTexSubImage2DCHROMIUM", "dimensions too big");
      return error::kNoError;
    }
  }

  // We know the memory/size is safe, so get the real shared memory since
  // it might need to be duped to prevent use-after-free of the memory.
  gpu::Buffer buffer = GetSharedMemoryBuffer(c.data_shm_id);
  base::SharedMemory* shared_memory = buffer.shared_memory;
  uint32 shm_size = buffer.size;
  uint32 shm_data_offset = c.data_shm_offset;
  uint32 shm_data_size = data_size;

  if (!info->GetAsyncTransferState()) {
    // Set up the async state if needed, and make the texture
    // immutable so the async state stays valid.
    info->SetAsyncTransferState(
        async_pixel_transfer_delegate_->
            CreatePixelTransferState(info->service_id()));
    info->SetImmutable(true);
  }

  gfx::AsyncTexSubImage2DParams tex_params = {target, level, xoffset, yoffset,
                                              width, height, format, type};
  gfx::AsyncMemoryParams mem_params = {shared_memory, shm_size,
                                       shm_data_offset, shm_data_size};
  async_pixel_transfer_delegate_->AsyncTexSubImage2D(
      info->GetAsyncTransferState(), tex_params, mem_params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleWaitAsyncTexImage2DCHROMIUM(
    uint32 immediate_data_size, const cmds::WaitAsyncTexImage2DCHROMIUM& c) {
  TRACE_EVENT0("gpu", "GLES2DecoderImpl::HandleWaitAsyncTexImage2DCHROMIUM");
  GLenum target = static_cast<GLenum>(c.target);

  if (GL_TEXTURE_2D != target) {
    SetGLError(GL_INVALID_ENUM, "glWaitAsyncTexImage2DCHROMIUM", "target");
    return error::kNoError;
  }
  Texture* info = GetTextureInfoForTarget(target);
  if (!info) {
      SetGLError(GL_INVALID_OPERATION,
                 "glWaitAsyncTexImage2DCHROMIUM", "unknown texture");
    return error::kNoError;
  }
  async_pixel_transfer_delegate_->WaitForTransferCompletion(
      info->GetAsyncTransferState());
  return error::kNoError;
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/gles2_cmd_decoder_autogen.h"

}  // namespace gles2
}  // namespace gpu
