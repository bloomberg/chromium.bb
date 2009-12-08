// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include <string>
#include <map>
#include <build/build_config.h>
#include "base/scoped_ptr.h"
#define GLES2_GPU_SERVICE 1
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

namespace command_buffer {
namespace gles2 {

namespace {

// Returns the address of the first byte after a struct.
template <typename T>
const void* AddressAfterStruct(const T& pod) {
  return reinterpret_cast<const uint8*>(&pod) + sizeof(pod);
}

// Returns the address of the frst byte after the struct.
template <typename RETURN_TYPE, typename COMMAND_TYPE>
RETURN_TYPE GetImmediateDataAs(const COMMAND_TYPE& pod) {
  return static_cast<RETURN_TYPE>(const_cast<void*>(AddressAfterStruct(pod)));
}

// Returns the size in bytes of the data of an Immediate command, a command with
// its data inline in the command buffer.
template <typename T>
unsigned int ImmediateDataSize(uint32 arg_count) {
  return static_cast<unsigned int>(
      (arg_count + 1 - ComputeNumEntries(sizeof(T))) *
      sizeof(CommandBufferEntry));  // NOLINT
}

// Checks if there is enough immediate data.
template<typename T>
bool CheckImmediateDataSize(
    uint32 immediate_data_size,
    GLuint count,
    size_t size,
    unsigned int elements_per_unit) {
  return immediate_data_size == count * size * elements_per_unit;
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

// These commands convert from c calls to local os calls.
void GLGenBuffersHelper(GLsizei n, GLuint* ids) {
  glGenBuffersARB(n, ids);
}

void GLGenFramebuffersHelper(GLsizei n, GLuint* ids) {
  glGenFramebuffersEXT(n, ids);
}

void GLGenRenderbuffersHelper(GLsizei n, GLuint* ids) {
  glGenRenderbuffersEXT(n, ids);
}

void GLGenTexturesHelper(GLsizei n, GLuint* ids) {
  glGenTextures(n, ids);
}

void GLDeleteBuffersHelper(GLsizei n, GLuint* ids) {
  glDeleteBuffersARB(n, ids);
}

void GLDeleteFramebuffersHelper(GLsizei n, GLuint* ids) {
  glDeleteFramebuffersEXT(n, ids);
}

void GLDeleteRenderbuffersHelper(GLsizei n, GLuint* ids) {
  glDeleteRenderbuffersEXT(n, ids);
}

void GLDeleteTexturesHelper(GLsizei n, GLuint* ids) {
  glDeleteTextures(n, ids);
}

namespace GLErrorBit {
enum GLErrorBit {
  kNoError = 0,
  kInvalidEnum,
  kInvalidValue,
  kInvalidOperation,
  kOutOfMemory,
  kInvalidFrameBufferOperation,
};
}

uint32 GLErrorToErrorBit(GLenum error) {
  switch(error) {
    case GL_INVALID_ENUM:
      return GLErrorBit::kInvalidEnum;
    case GL_INVALID_VALUE:
      return GLErrorBit::kInvalidValue;
    case GL_INVALID_OPERATION:
      return GLErrorBit::kInvalidOperation;
    case GL_OUT_OF_MEMORY:
      return GLErrorBit::kOutOfMemory;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return GLErrorBit::kInvalidFrameBufferOperation;
    default:
      DCHECK(false);
      return GLErrorBit::kNoError;
  }
}

GLenum GLErrorBitToGLError(uint32 error_bit) {
  switch(error_bit) {
    case GLErrorBit::kInvalidEnum:
      return GL_INVALID_ENUM;
    case GLErrorBit::kInvalidValue:
      return GL_INVALID_VALUE;
    case GLErrorBit::kInvalidOperation:
      return GL_INVALID_OPERATION;
    case GLErrorBit::kOutOfMemory:
      return GL_OUT_OF_MEMORY;
    case GLErrorBit::kInvalidFrameBufferOperation:
      return GL_INVALID_FRAMEBUFFER_OPERATION;
    default:
      DCHECK(false);
      return GL_NO_ERROR;
  }
}

}  // anonymous namespace.

#if defined(OS_LINUX)
GLES2Decoder::GLES2Decoder()
    : debug_(false),
      window_(NULL) {
#elif defined(OS_WIN)
GLES2Decoder::GLES2Decoder()
    : debug_(false),
      hwnd_(NULL) {
#else
GLES2Decoder::GLES2Decoder() {
#endif
}

// This class maps one set of ids to another.
class IdMap {
 public:
  // Maps a client_id to a service_id. Return false if the client_id or
  // service_id are already mapped to something else.
  bool AddMapping(GLuint client_id, GLuint service_id);

  // Unmaps a pair of ids. Returns false if the pair were not previously mapped.
  bool RemoveMapping(GLuint client_id, GLuint service_id);

  // Gets the corresponding service_id for the given client_id.
  // Returns false if there is no corresponding service_id.
  bool GetServiceId(GLuint client_id, GLuint* service_id);

  // Gets the corresponding client_id for the given service_id.
  // Returns false if there is no corresponding client_id.
  bool GetClientId(GLuint service_id, GLuint* client_id);

 private:
  // TODO(gman): Replace with faster implementation.
  typedef std::map<GLuint, GLuint> MapType;
  MapType id_map_;
};

bool IdMap::AddMapping(GLuint client_id, GLuint service_id) {
  std::pair<MapType::iterator, bool> result = id_map_.insert(
      std::make_pair(client_id, service_id));
  return result.second;
}

bool IdMap::RemoveMapping(GLuint client_id, GLuint service_id) {
  MapType::iterator iter = id_map_.find(client_id);
  if (iter != id_map_.end() && iter->second == service_id) {
    id_map_.erase(iter);
    return true;
  }
  return false;
}

bool IdMap::GetServiceId(GLuint client_id, GLuint* service_id) {
  DCHECK(service_id);
  MapType::iterator iter = id_map_.find(client_id);
  if (iter != id_map_.end()) {
    *service_id = iter->second;
    return true;
  }
  return false;
}

bool IdMap::GetClientId(GLuint service_id, GLuint* client_id) {
  DCHECK(client_id);
  MapType::iterator end(id_map_.end());
  for (MapType::iterator iter(id_map_.begin());
       iter != end;
       ++iter) {
    if (iter->second == service_id) {
      *client_id = iter->first;
      return true;
    }
  }
  return false;
}

// This class implements GLES2Decoder so we don't have to expose all the GLES2
// cmd stuff to outside this class.
class GLES2DecoderImpl : public GLES2Decoder {
 public:
  GLES2DecoderImpl();

  // Overridden from AsyncAPIInterface.
  virtual ParseError DoCommand(unsigned int command,
                               unsigned int arg_count,
                               const void* args);

  // Overridden from AsyncAPIInterface.
  virtual const char* GetCommandName(unsigned int command_id) const;

  // Overridden from GLES2Decoder.
  virtual bool Initialize();

  // Overridden from GLES2Decoder.
  virtual void Destroy();

 private:
  bool InitPlatformSpecific();
  bool InitGlew();

  // Typed version of GetAddressAndCheckSize.
  template <typename T>
  T GetSharedMemoryAs(unsigned int shm_id, unsigned int offset,
                      unsigned int size) {
    return static_cast<T>(GetAddressAndCheckSize(shm_id, offset, size));
  }

  // Template to help call glGenXXX functions.
  template <void gl_gen_function(GLsizei, GLuint*)>
  bool GenGLObjects(GLsizei n, const GLuint* client_ids) {
    // TODO(gman): Verify client ids are unused.
    scoped_array<GLuint>temp(new GLuint[n]);
    gl_gen_function(n, temp.get());
    // TODO(gman): check for success before copying results.
    for (GLsizei ii = 0; ii < n; ++ii) {
      if (!id_map_.AddMapping(client_ids[ii], temp[ii])) {
        // TODO(gman): fail.
      }
    }
    return true;
  }

  // Template to help call glDeleteXXX functions.
  template <void gl_delete_function(GLsizei, GLuint*)>
  bool DeleteGLObjects(GLsizei n, const GLuint* client_ids) {
    scoped_array<GLuint>temp(new GLuint[n]);
    // TODO(gman): check for success before copying results.
    for (GLsizei ii = 0; ii < n; ++ii) {
      if (id_map_.GetServiceId(client_ids[ii], &temp[ii])) {
        id_map_.RemoveMapping(client_ids[ii], temp[ii]);
      } else {
        temp[ii] = 0;
      }
    }
    gl_delete_function(n, temp.get());
    return true;
  }

  // Wrapper for glCreateProgram
  void CreateProgramHelper(GLuint client_id);

  // Wrapper for glCreateShader
  void CreateShaderHelper(GLenum type, GLuint client_id);

  // Wrapper for glBindBuffer since we need to track the current targets.
  void DoBindBuffer(GLenum target, GLuint buffer);

  // Wrapper for glDeleteProgram.
  void DoDeleteProgram(GLuint program);

  // Wrapper for glDeleteShader.
  void DoDeleteShader(GLuint shader);

  // Swaps the buffers (copies/renders to the current window).
  void DoSwapBuffers();

  // Gets the GLError through our wrapper.
  GLenum GetGLError();

  // Sets our wrapper for the GLError.
  void SetGLError(GLenum error);

  // Generate a member function prototype for each command in an automated and
  // typesafe way.
  #define GLES2_CMD_OP(name) \
     ParseError Handle ## name(             \
       uint32 immediate_data_size,          \
       const gles2::name& args);            \

  GLES2_COMMAND_LIST(GLES2_CMD_OP)

  #undef GLES2_CMD_OP

  // Current GL error bits.
  uint32 error_bits_;

  // Map of client ids to GL ids.
  IdMap id_map_;

  // Util to help with GL.
  GLES2Util util_;

  // pack alignment as last set by glPixelStorei
  GLint pack_alignment_;

  // unpack alignment as last set by glPixelStorei
  GLint unpack_alignment_;

  // The currently bound array buffer. If this is 0 it is illegal to call
  // glVertexAttribPointer.
  GLuint bound_array_buffer_;

  // The currently bound element array buffer. If this is 0 it is illegal
  // to call glDrawElements.
  GLuint bound_element_array_buffer_;

#if defined(OS_WIN)
  HDC device_context_;
  HGLRC gl_context_;
#endif

  bool anti_aliased_;

  DISALLOW_COPY_AND_ASSIGN(GLES2DecoderImpl);
};

GLES2Decoder* GLES2Decoder::Create() {
  return new GLES2DecoderImpl();
}

GLES2DecoderImpl::GLES2DecoderImpl()
    : GLES2Decoder(),
      error_bits_(0),
      util_(0),  // TODO(gman): Set to actual num compress texture formats.
      pack_alignment_(4),
      unpack_alignment_(4),
      bound_array_buffer_(0),
      bound_element_array_buffer_(0),
#ifdef OS_WIN
      device_context_(NULL),
      gl_context_(NULL),
#endif
      anti_aliased_(false) {
}

bool GLES2DecoderImpl::Initialize() {
  if (!InitPlatformSpecific())
    return false;
  if (!InitGlew())
    return false;
  CHECK_GL_ERROR();

  //glBindFramebuffer(0, 0);
  return true;
}

#if defined(OS_WIN)
namespace {

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

// Helper routine that returns the highest quality pixel format supported on
// the current platform.  Returns true upon success.
bool GetWindowsPixelFormat(HWND window,
                           bool anti_aliased,
                           int* pixel_format) {
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
  int format_index = ::ChoosePixelFormat(intermediate_dc,
                                         &kPixelFormatDescriptor);
  if (format_index == 0) {
    DLOG(ERROR) << "Unable to get the pixel format for GL context.";
    ::ReleaseDC(intermediate_window, intermediate_dc);
    ::DestroyWindow(intermediate_window);
    ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                      module_handle);
    return false;
  }
  if (!::SetPixelFormat(intermediate_dc, format_index,
                        &kPixelFormatDescriptor)) {
    DLOG(ERROR) << "Unable to set the pixel format for GL context.";
    ::ReleaseDC(intermediate_window, intermediate_dc);
    ::DestroyWindow(intermediate_window);
    ::UnregisterClass(reinterpret_cast<wchar_t*>(class_registration),
                      module_handle);
    return false;
  }

  // Store the pixel format without multisampling.
  *pixel_format = format_index;
  HGLRC gl_context = ::wglCreateContext(intermediate_dc);
  if (::wglMakeCurrent(intermediate_dc, gl_context)) {
    // GL context was successfully created and applied to the window's DC.
    // Startup GLEW, the GL extensions wrangler.
    GLenum glew_error = ::glewInit();
    if (glew_error == GLEW_OK) {
      DLOG(INFO) << "Initialized GLEW " << ::glewGetString(GLEW_VERSION);
    } else {
      DLOG(ERROR) << "Unable to initialise GLEW : "
                  << ::glewGetErrorString(glew_error);
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
          *pixel_format = msaa_pixel_format;
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

}  // anonymous namespace
#endif

bool GLES2DecoderImpl::InitPlatformSpecific() {
#if defined(OS_WIN)
  device_context_ = ::GetDC(hwnd());

  int pixel_format;

  if (!GetWindowsPixelFormat(hwnd(),
                             anti_aliased_,
                             &pixel_format)) {
      DLOG(ERROR) << "Unable to determine optimal pixel format for GL context.";
      return false;
  }

  if (!::SetPixelFormat(device_context_, pixel_format,
                        &kPixelFormatDescriptor)) {
    DLOG(ERROR) << "Unable to set the pixel format for GL context.";
    return false;
  }

  gl_context_ = ::wglCreateContext(device_context_);
  if (!gl_context_) {
    DLOG(ERROR) << "Failed to create GL context.";
    return false;
  }

  if (!::wglMakeCurrent(device_context_, gl_context_)) {
    DLOG(ERROR) << "Unable to make gl context current.";
    return false;
  }
#elif defined(OS_LINUX)
  DCHECK(window());
  if (!window()->Initialize())
    return false;
  if (!window()->MakeCurrent())
    return false;
#endif

  return true;
}

bool GLES2DecoderImpl::InitGlew() {
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

  return true;
}

void GLES2DecoderImpl::Destroy() {
#ifdef OS_LINUX
  DCHECK(window());
  window()->Destroy();
#endif
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
parse_error::ParseError GLES2DecoderImpl::DoCommand(
    unsigned int command,
    unsigned int arg_count,
    const void* cmd_data) {
  parse_error::ParseError result;
  if (debug()) {
    // TODO(gman): Change output to something useful for NaCl.
    const char* f = GetCommandName(command);
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
        if (debug()) {
          if (glGetError() != 0) {
            // TODO(gman): Change output to something useful for NaCl.
            printf("GL ERROR b4: %s\n", GetCommandName(command));
          }
        }
      }
    } else {
      result = parse_error::kParseInvalidArguments;
    }
  } else {
    result = DoCommonCommand(command, arg_count, cmd_data);
  }
  return result;
}

}  // namespace gles2
}  // namespace command_buffer

// This is included so the compiler will make these inline.
#include "gpu/command_buffer/service/gles2_cmd_decoder_validate.h"

namespace command_buffer {
namespace gles2 {

void GLES2DecoderImpl::CreateProgramHelper(GLuint client_id) {
  // TODO(gman): verify client_id is unused.
  GLuint service_id = glCreateProgram();
  if (service_id) {
    id_map_.AddMapping(client_id, service_id);
  }
}

void GLES2DecoderImpl::CreateShaderHelper(GLenum type, GLuint client_id) {
  // TODO(gman): verify client_id is unused.
  GLuint service_id = glCreateShader(type);
  if (service_id) {
    id_map_.AddMapping(client_id, service_id);
  }
}

void GLES2DecoderImpl::DoBindBuffer(GLenum target, GLuint buffer) {
  switch (target) {
    case GL_ARRAY_BUFFER:
      bound_array_buffer_ = buffer;
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      bound_element_array_buffer_ = buffer;
      break;
    default:
      break;
  }
  glBindBuffer(target, buffer);
}

void GLES2DecoderImpl::DoDeleteProgram(GLuint program) {
  GLuint service_id;
  if (id_map_.GetServiceId(program, &service_id)) {
    glDeleteProgram(service_id);
    id_map_.RemoveMapping(program, service_id);
  }
}

void GLES2DecoderImpl::DoDeleteShader(GLuint shader) {
  GLuint service_id;
  if (id_map_.GetServiceId(shader, &service_id)) {
    glDeleteProgram(service_id);
    id_map_.RemoveMapping(shader, service_id);
  }
}

// NOTE: If you need to know the results of SwapBuffers (like losing
//    the context) then add a new command. Do NOT make SwapBuffers synchronous.
void GLES2DecoderImpl::DoSwapBuffers() {
#ifdef OS_WIN
  ::SwapBuffers(device_context_);
#endif

#ifdef OS_LINUX
  DCHECK(window());
  window()->SwapBuffers();
#endif
}

GLenum GLES2DecoderImpl::GetGLError() {
  // Check the GL error first, then our wrapped error.
  GLenum error = glGetError();
  if (error == GL_NO_ERROR && error_bits_ != 0) {
    uint32 mask = 1;
    while (mask) {
      if ((error_bits_ & mask) != 0) {
        error = GLErrorBitToGLError(mask);
        break;
      }
    }
  }

  if (error != GL_NO_ERROR) {
    // There was an error, clear the corresponding wrapped error.
    error_bits_ &= ~GLErrorToErrorBit(error);
  }
  return error;
}

void GLES2DecoderImpl::SetGLError(GLenum error) {
  error_bits_ |= GLErrorToErrorBit(error);
}

parse_error::ParseError GLES2DecoderImpl::HandleDrawElements(
    uint32 immediate_data_size, const gles2::DrawElements& c) {
  if (bound_element_array_buffer_ != 0) {
    GLenum mode = c.mode;
    GLsizei count = c.count;
    GLenum type = c.type;
    const GLvoid* indices = reinterpret_cast<const GLvoid*>(c.index_offset);
    glDrawElements(mode, count, type, indices);
  } else {
    SetGLError(GL_INVALID_VALUE);
  }
  return parse_error::kParseNoError;
}

namespace {

// Calls glShaderSource for the various versions of the ShaderSource command.
// Assumes that data / data_size points to a piece of memory that is in range
// of whatever context it came from (shared memory, immediate memory, bucket
// memory.)
parse_error::ParseError ShaderSourceHelper(
    GLuint shader, GLsizei count, const char* data, uint32 data_size) {
  std::vector<std::string> strings(count);
  scoped_array<const char*> string_pointers(new const char* [count]);

  const uint32* ends = reinterpret_cast<const uint32*>(data);
  uint32 start_offset = count * sizeof(*ends);
  if (start_offset > data_size) {
    return parse_error::kParseOutOfBounds;
  }
  for (GLsizei ii = 0; ii < count; ++ii) {
    uint32 end_offset = ends[ii];
    if (end_offset > data_size || end_offset < start_offset) {
      return parse_error::kParseOutOfBounds;
    }
    strings[ii] = std::string(data + start_offset, end_offset - start_offset);
    string_pointers[ii] = strings[ii].c_str();
  }

  glShaderSource(shader, count, string_pointers.get(), NULL);
  return parse_error::kParseNoError;
}

}  // anonymous namespace.

parse_error::ParseError GLES2DecoderImpl::HandleShaderSource(
    uint32 immediate_data_size, const gles2::ShaderSource& c) {
  GLuint shader;
  if (!id_map_.GetServiceId(c.shader, &shader)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLsizei count = c.count;
  uint32 data_size = c.data_size;
  const char** data = GetSharedMemoryAs<const char**>(
      c.data_shm_id, c.data_shm_offset, data_size);
  parse_error::ParseError result =
      // TODO(gman): Manually implement validation.
      ValidateShaderSource(
          this, immediate_data_size, shader, count, data,
          reinterpret_cast<const GLint*>(1));
  if (result != parse_error::kParseNoError) {
    return result;
  }
  return ShaderSourceHelper(
      shader, count, reinterpret_cast<const char*>(data), data_size);
}

parse_error::ParseError GLES2DecoderImpl::HandleShaderSourceImmediate(
  uint32 immediate_data_size, const gles2::ShaderSourceImmediate& c) {
  GLuint shader;
  if (!id_map_.GetServiceId(c.shader, &shader)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  GLsizei count = c.count;
  uint32 data_size = c.data_size;
  // TODO(gman): need to check that data_size is in range for arg_count.
  const char** data = GetImmediateDataAs<const char**>(c);
  parse_error::ParseError result =
      ValidateShaderSourceImmediate(
          this, immediate_data_size, shader, count, data, NULL);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  return ShaderSourceHelper(
      shader, count, reinterpret_cast<const char*>(data), data_size);
}

parse_error::ParseError GLES2DecoderImpl::HandleVertexAttribPointer(
    uint32 immediate_data_size, const gles2::VertexAttribPointer& c) {
  if (bound_array_buffer_ != 0) {
    GLuint indx = c.indx;
    GLint size = c.size;
    GLenum type = c.type;
    GLboolean normalized = c.normalized;
    GLsizei stride = c.stride;
    GLuint offset = c.offset;
    const void* ptr = reinterpret_cast<const void*>(c.offset);
    // TODO(gman): Do manual validation.
    parse_error::ParseError result =
        ValidateVertexAttribPointer(
            this, immediate_data_size, indx, size, type, normalized, stride,
            reinterpret_cast<const void*>(1));
    if (result != parse_error::kParseNoError) {
      return result;
    }
    glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
  } else {
    SetGLError(GL_INVALID_VALUE);
  }
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleReadPixels(
    uint32 immediate_data_size, const gles2::ReadPixels& c) {
  GLint x = c.x;
  GLint y = c.y;
  GLsizei width = c.width;
  GLsizei height = c.height;
  GLenum format = c.format;
  GLenum type = c.type;
  uint32 pixels_size = GLES2Util::ComputeImageDataSize(
      width, height, format, type, pack_alignment_);
  void* pixels = GetSharedMemoryAs<void*>(
      c.pixels_shm_id, c.pixels_shm_offset, pixels_size);
  parse_error::ParseError result = ValidateReadPixels(
      this, immediate_data_size, x, y, width, height, format, type, pixels);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glReadPixels(x, y, width, height, format, type, pixels);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandlePixelStorei(
    uint32 immediate_data_size, const gles2::PixelStorei& c) {
  GLenum pname = c.pname;
  GLenum param = c.param;
  parse_error::ParseError result =
    ValidatePixelStorei(this, immediate_data_size, pname, param);
  if (result != parse_error::kParseNoError) {
    return result;
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
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetAttribLocation(
    uint32 immediate_data_size, const gles2::GetAttribLocation& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  uint32 name_size = c.data_size;
  const char* name = GetSharedMemoryAs<const char*>(
      c.name_shm_id, c.name_shm_offset, name_size);
  GLint* location = GetSharedMemoryAs<GLint*>(
      c.location_shm_id, c.location_shm_offset, sizeof(GLint));
  if (!location || !name) {
    return parse_error::kParseOutOfBounds;
  }
  String name_str(name, name_size);
  *location = glGetAttribLocation(program, name_str.c_str());
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetAttribLocationImmediate(
    uint32 immediate_data_size, const gles2::GetAttribLocationImmediate& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  uint32 name_size = c.data_size;
  const char* name = GetImmediateDataAs<const char*>(c);
  // TODO(gman): Make sure validate checks arg_count
  //     covers data_size.
  GLint* location = GetSharedMemoryAs<GLint*>(
      c.location_shm_id, c.location_shm_offset, sizeof(GLint));
  if (!location || !name) {
    return parse_error::kParseOutOfBounds;
  }
  String name_str(name, name_size);
  *location = glGetAttribLocation(program, name_str.c_str());
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetUniformLocation(
    uint32 immediate_data_size, const gles2::GetUniformLocation& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  uint32 name_size = c.data_size;
  const char* name = GetSharedMemoryAs<const char*>(
      c.name_shm_id, c.name_shm_offset, name_size);
  GLint* location = GetSharedMemoryAs<GLint*>(
      c.location_shm_id, c.location_shm_offset, sizeof(GLint));
  if (!location || !name) {
    return parse_error::kParseOutOfBounds;
  }
  String name_str(name, name_size);
  *location = glGetUniformLocation(program, name_str.c_str());
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetUniformLocationImmediate(
    uint32 immediate_data_size, const gles2::GetUniformLocationImmediate& c) {
  GLuint program;
  if (!id_map_.GetServiceId(c.program, &program)) {
    SetGLError(GL_INVALID_VALUE);
    return parse_error::kParseNoError;
  }
  uint32 name_size = c.data_size;
  const char* name = GetImmediateDataAs<const char*>(c);
  // TODO(gman): Make sure validate checks arg_count
  //     covers data_size.
  GLint* location = GetSharedMemoryAs<GLint*>(
      c.location_shm_id, c.location_shm_offset, sizeof(GLint));
  if (!location || !name) {
    return parse_error::kParseOutOfBounds;
  }
  String name_str(name, name_size);
  *location = glGetUniformLocation(program, name_str.c_str());
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBufferData(
    uint32 immediate_data_size, const gles2::BufferData& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32 data_shm_id = static_cast<uint32>(c.data_shm_id);
  uint32 data_shm_offset = static_cast<uint32>(c.data_shm_offset);
  GLenum usage = static_cast<GLenum>(c.usage);
  const void* data = NULL;
  if (data_shm_id != 0 || data_shm_offset != 0) {
    data = GetSharedMemoryAs<const void*>(data_shm_id, data_shm_offset, size);
    parse_error::ParseError result =
        ValidateBufferData(this, immediate_data_size, target, size, data,
                           usage);
    if (result != parse_error::kParseNoError) {
      return result;
    }
  }
  // TODO(gman): Validate case where data is NULL.
  glBufferData(target, size, data, usage);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleBufferDataImmediate(
    uint32 immediate_data_size, const gles2::BufferDataImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  const void* data = GetImmediateDataAs<const void*>(c);
  GLenum usage = static_cast<GLenum>(c.usage);
  // Immediate version.
  // TODO(gman): Handle case where data is NULL.
  parse_error::ParseError result =
      ValidateBufferDataImmediate(this, immediate_data_size, target, size, data,
                                  usage);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glBufferData(target, size, data, usage);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleCompressedTexImage2D(
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
    parse_error::ParseError result =
        ValidateCompressedTexImage2D(
            this, immediate_data_size, target, level, internal_format, width,
            height, border, image_size, data);
    if (result != parse_error::kParseNoError) {
      return result;
    }
  }
  // TODO(gman): Validate case where data is NULL.
  glCompressedTexImage2D(
      target, level, internal_format, width, height, border, image_size, data);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleCompressedTexImage2DImmediate(
    uint32 immediate_data_size, const gles2::CompressedTexImage2DImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  const void* data = GetImmediateDataAs<const void*>(c);
  // Immediate version.
  // TODO(gman): Handle case where data is NULL.
  parse_error::ParseError result =
      ValidateCompressedTexImage2DImmediate(
          this, immediate_data_size, target, level, internal_format, width,
          height, border, image_size, data);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glCompressedTexImage2D(
      target, level, internal_format, width, height, border, image_size, data);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleTexImage2D(
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
  uint32 pixels_size = GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_);
  const void* pixels = NULL;
  if (pixels_shm_id != 0 || pixels_shm_offset != 0) {
    pixels = GetSharedMemoryAs<const void*>(
        pixels_shm_id, pixels_shm_offset, pixels_size);
    parse_error::ParseError result =
        ValidateTexImage2D(
            this, immediate_data_size, target, level, internal_format, width,
            height, border, format, type, pixels);
    if (result != parse_error::kParseNoError) {
      return result;
    }
  }
  // TODO(gman): Validate case where data is NULL.
  glTexImage2D(
      target, level, internal_format, width, height, border, format, type,
      pixels);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleTexImage2DImmediate(
    uint32 immediate_data_size, const gles2::TexImage2DImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint internalformat = static_cast<GLint>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  const void* pixels = GetImmediateDataAs<const void*>(c);
  // Immediate version.
  // TODO(gman): Handle case where data is NULL.
  parse_error::ParseError result =
      ValidateTexImage2DImmediate(
          this, immediate_data_size, target, level, internalformat, width,
          height, border, format, type, pixels);
  if (result != parse_error::kParseNoError) {
    return result;
  }
  glTexImage2D(
      target, level, internalformat, width, height, border, format, type,
      pixels);
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetVertexAttribPointerv(
    uint32 immediate_data_size, const gles2::GetVertexAttribPointerv& c) {
  // TODO(gman): Implement.
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetUniformiv(
    uint32 immediate_data_size, const gles2::GetUniformiv& c) {
  // TODO(gman): Implement.
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetUniformfv(
    uint32 immediate_data_size, const gles2::GetUniformfv& c) {
  // TODO(gman): Implement.
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetShaderPrecisionFormat(
    uint32 immediate_data_size, const gles2::GetShaderPrecisionFormat& c) {
  // TODO(gman): Implement.
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetAttachedShaders(
    uint32 immediate_data_size, const gles2::GetAttachedShaders& c) {
  // TODO(gman): Implement.
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetActiveUniform(
    uint32 immediate_data_size, const gles2::GetActiveUniform& c) {
  // TODO(gman): Implement.
  return parse_error::kParseNoError;
}

parse_error::ParseError GLES2DecoderImpl::HandleGetActiveAttrib(
    uint32 immediate_data_size, const gles2::GetActiveAttrib& c) {
  // TODO(gman): Implement.
  return parse_error::kParseNoError;
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/gles2_cmd_decoder_autogen.h"

}  // namespace gles2
}  // namespace command_buffer

