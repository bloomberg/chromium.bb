// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The bulk of this file is support code; sorry about that.  Here's an overview
// to hopefully help readers of this code:
// - RenderingHelper is charged with interacting with X11, EGL, and GLES2.
// - ClientState is an enum for the state of the decode client used by the test.
// - ClientStateNotification is a barrier abstraction that allows the test code
//   to be written sequentially and wait for the decode client to see certain
//   state transitions.
// - EglRenderingVDAClient is a VideoDecodeAccelerator::Client implementation
// - Finally actual TEST cases are at the bottom of this file, using the above
//   infrastructure.

#include <fcntl.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>

// Include gtest.h out of order because <X11/X.h> #define's Bool & None, which
// gtest uses as struct names (inside a namespace).  This means that
// #include'ing gtest after anything that pulls in X.h fails to compile.
// This is http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringize_macros.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"

#if (!defined(OS_CHROMEOS) || !defined(ARCH_CPU_ARMEL)) && !defined(OS_WIN)
#error The VideoAccelerator tests are only supported on cros/ARM/Windows.
#endif

#if defined(OS_WIN)
#include "content/common/gpu/media/dxva_video_decode_accelerator.h"
#else  // OS_WIN
#include "content/common/gpu/media/omx_video_decode_accelerator.h"
#endif  // defined(OS_WIN)

#include "third_party/angle/include/EGL/egl.h"

#if defined(OS_WIN)
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#else  // OS_WIN
#include "third_party/angle/include/GLES2/gl2.h"
#endif  // OS_WIN

using media::VideoDecodeAccelerator;

namespace {

// Values optionally filled in from flags; see main() below.
// The syntax of this variable is:
//   filename:width:height:numframes:numNALUs:minFPSwithRender:minFPSnoRender
// where only the first field is required.  Value details:
// - |filename| must be an h264 Annex B (NAL) stream.
// - |width| and |height| are in pixels.
// - |numframes| is the number of picture frames in the file.
// - |numNALUs| is the number of NAL units in the stream.
// - |minFPSwithRender| and |minFPSnoRender| are minimum frames/second speeds
//   expected to be achieved with and without rendering to the screen, resp.
//   (the latter tests just decode speed).
// - |profile| is the media::H264Profile set during Initialization.
// An empty value for a numeric field means "ignore".
const FilePath::CharType* test_video_data =
    FILE_PATH_LITERAL("test-25fps.h264:320:240:250:258:50:175:1");

// Parse |data| into its constituent parts and set the various output fields
// accordingly.  CHECK-fails on unexpected or missing required data.
// Unspecified optional fields are set to -1.
void ParseTestVideoData(FilePath::StringType data,
                        FilePath::StringType* file_name,
                        int* width, int* height,
                        int* num_frames,
                        int* num_NALUs,
                        int* min_fps_render,
                        int* min_fps_no_render,
                        int* profile) {
  std::vector<FilePath::StringType> elements;
  base::SplitString(data, ':', &elements);
  CHECK_GE(elements.size(), 1U) << data;
  CHECK_LE(elements.size(), 8U) << data;
  *file_name = elements[0];
  *width = *height = *num_frames = *num_NALUs = -1;
  *min_fps_render = *min_fps_no_render = -1;
  *profile = -1;
  if (!elements[1].empty())
    CHECK(base::StringToInt(elements[1], width));
  if (!elements[2].empty())
    CHECK(base::StringToInt(elements[2], height));
  if (!elements[3].empty())
    CHECK(base::StringToInt(elements[3], num_frames));
  if (!elements[4].empty())
    CHECK(base::StringToInt(elements[4], num_NALUs));
  if (!elements[5].empty())
    CHECK(base::StringToInt(elements[5], min_fps_render));
  if (!elements[6].empty())
    CHECK(base::StringToInt(elements[6], min_fps_no_render));
  if (!elements[7].empty())
    CHECK(base::StringToInt(elements[7], profile));
}

// Provides functionality for managing EGL, GLES2 and UI resources.
// This class is not thread safe and thus all the methods of this class
// (except for ctor/dtor) ensure they're being run on a single thread.
class RenderingHelper {
 public:
  RenderingHelper();
  ~RenderingHelper();

  // Initialize all structures to prepare to render to one or more windows of
  // the specified dimensions.  CHECK-fails if any initialization step fails.
  // After this returns, texture creation and rendering can be requested.  This
  // method can be called multiple times, in which case all previously-acquired
  // resources and initializations are discarded.  If |suppress_swap_to_display|
  // then all the usual work is done, except for the final swap of the EGL
  // surface to the display.  This cuts test times over 50% so is worth doing
  // when testing non-rendering-related aspects.
  void Initialize(bool suppress_swap_to_display, int num_windows, int width,
                  int height, base::WaitableEvent* done);

  // Undo the effects of Initialize() and signal |*done|.
  void UnInitialize(base::WaitableEvent* done);

  // Return a newly-created GLES2 texture id rendering to a specific window, and
  // signal |*done|.
  void CreateTexture(int window_id, GLuint* texture_id,
                     base::WaitableEvent* done);

  // Render |texture_id| to the screen (unless |suppress_swap_to_display_|).
  void RenderTexture(GLuint texture_id);

  // Delete |texture_id|.
  void DeleteTexture(GLuint texture_id);

  // Platform specific Init/Uninit.
  void PlatformInitialize();
  void PlatformUnInitialize();

  // Platform specific window creation.
  EGLNativeWindowType PlatformCreateWindow(int top_left_x, int top_left_y);

  // Platform specific display surface returned here.
  EGLDisplay PlatformGetDisplay();

  EGLDisplay egl_display() { return egl_display_; }

  EGLContext egl_context() { return egl_context_; }

  MessageLoop* message_loop() { return message_loop_; }

 protected:
  void Clear();

  // We ensure all operations are carried out on the same thread by remembering
  // where we were Initialized.
  MessageLoop* message_loop_;
  int width_;
  int height_;
  bool suppress_swap_to_display_;

  EGLDisplay egl_display_;
  EGLContext egl_context_;
  std::vector<EGLSurface> egl_surfaces_;
  std::map<GLuint, int> texture_id_to_surface_index_;

#if defined(OS_WIN)
  std::vector<HWND> windows_;
#else  // OS_WIN
  Display* x_display_;
  std::vector<Window> x_windows_;
#endif  // OS_WIN
};

RenderingHelper::RenderingHelper() {
  Clear();
}

RenderingHelper::~RenderingHelper() {
  CHECK_EQ(width_, 0) << "Must call UnInitialize before dtor.";
  Clear();
}

// Helper for Shader creation.
static void CreateShader(
    GLuint program, GLenum type, const char* source, int size) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, &size);
  glCompileShader(shader);
  int result = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
  if (!result) {
    char log[4096];
    glGetShaderInfoLog(shader, arraysize(log), NULL, log);
    LOG(FATAL) << log;
  }
  glAttachShader(program, shader);
  glDeleteShader(shader);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

void RenderingHelper::Initialize(
    bool suppress_swap_to_display,
    int num_windows,
    int width,
    int height,
    base::WaitableEvent* done) {
  // Use width_ != 0 as a proxy for the class having already been
  // Initialize()'d, and UnInitialize() before continuing.
  if (width_) {
    base::WaitableEvent done(false, false);
    UnInitialize(&done);
    done.Wait();
  }

  suppress_swap_to_display_ = suppress_swap_to_display;
  CHECK_GT(width, 0);
  CHECK_GT(height, 0);
  width_ = width;
  height_ = height;
  message_loop_ = MessageLoop::current();
  CHECK_GT(num_windows, 0);

  PlatformInitialize();

  egl_display_ = PlatformGetDisplay();

  EGLint major;
  EGLint minor;
  CHECK(eglInitialize(egl_display_, &major, &minor)) << eglGetError();
  static EGLint rgba8888[] = {
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_NONE,
  };
  EGLConfig egl_config;
  int num_configs;
  CHECK(eglChooseConfig(egl_display_, rgba8888, &egl_config, 1, &num_configs))
      << eglGetError();
  CHECK_GE(num_configs, 1);
  static EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  egl_context_ = eglCreateContext(
      egl_display_, egl_config, EGL_NO_CONTEXT, context_attribs);
  CHECK_NE(egl_context_, EGL_NO_CONTEXT) << eglGetError();

  // Per-window/surface X11 & EGL initialization.
  for (int i = 0; i < num_windows; ++i) {
    // Arrange X windows whimsically, with some padding.
    int top_left_x = (width + 20) * (i % 4);
    int top_left_y = (height + 12) * (i % 3);

    EGLNativeWindowType window = PlatformCreateWindow(top_left_x, top_left_y);
    EGLSurface egl_surface =
        eglCreateWindowSurface(egl_display_, egl_config, window, NULL);
    egl_surfaces_.push_back(egl_surface);
    CHECK_NE(egl_surface, EGL_NO_SURFACE);
  }
  CHECK(eglMakeCurrent(egl_display_, egl_surfaces_[0],
                       egl_surfaces_[0], egl_context_)) << eglGetError();

  static const float kVertices[] =
      { -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f, -1.f, };
  static const float kTextureCoordsEgl[] = { 0, 1, 0, 0, 1, 1, 1, 0, };
  static const char kVertexShader[] = STRINGIZE(
      varying vec2 interp_tc;
      attribute vec4 in_pos;
      attribute vec2 in_tc;
      void main() {
        interp_tc = in_tc;
        gl_Position = in_pos;
      }
                                                );
  static const char kFragmentShaderEgl[] = STRINGIZE(
      precision mediump float;
      varying vec2 interp_tc;
      uniform sampler2D tex;
      void main() {
        gl_FragColor = texture2D(tex, interp_tc);
      }
                                                     );
  GLuint program = glCreateProgram();
  CreateShader(program, GL_VERTEX_SHADER,
               kVertexShader, arraysize(kVertexShader));
  CreateShader(program, GL_FRAGMENT_SHADER,
               kFragmentShaderEgl, arraysize(kFragmentShaderEgl));
  glLinkProgram(program);
  int result = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &result);
  if (!result) {
    char log[4096];
    glGetShaderInfoLog(program, arraysize(log), NULL, log);
    LOG(FATAL) << log;
  }
  glUseProgram(program);
  glDeleteProgram(program);

  glUniform1i(glGetUniformLocation(program, "tex"), 0);
  int pos_location = glGetAttribLocation(program, "in_pos");
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, kVertices);
  int tc_location = glGetAttribLocation(program, "in_tc");
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0,
                        kTextureCoordsEgl);
  done->Signal();
}

void RenderingHelper::UnInitialize(base::WaitableEvent* done) {
  CHECK_EQ(MessageLoop::current(), message_loop_);
  CHECK(eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT)) << eglGetError();
  CHECK(eglDestroyContext(egl_display_, egl_context_));
  for (size_t i = 0; i < egl_surfaces_.size(); ++i)
    CHECK(eglDestroySurface(egl_display_, egl_surfaces_[i]));
  CHECK(eglTerminate(egl_display_));
  Clear();
  done->Signal();
}

void RenderingHelper::Clear() {
  suppress_swap_to_display_ = false;
  width_ = 0;
  height_ = 0;
  texture_id_to_surface_index_.clear();
  message_loop_ = NULL;
  egl_display_ = EGL_NO_DISPLAY;
  egl_context_ = EGL_NO_CONTEXT;
  egl_surfaces_.clear();
  PlatformUnInitialize();
}

void RenderingHelper::CreateTexture(int window_id, GLuint* texture_id,
                                    base::WaitableEvent* done) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RenderingHelper::CreateTexture, base::Unretained(this),
                   window_id, texture_id, done));
    return;
  }
  CHECK(eglMakeCurrent(egl_display_, egl_surfaces_[window_id],
                       egl_surfaces_[window_id], egl_context_))
      << eglGetError();
  glGenTextures(1, texture_id);
  glBindTexture(GL_TEXTURE_2D, *texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // OpenGLES2.0.25 section 3.8.2 requires CLAMP_TO_EDGE for NPOT textures.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  CHECK(texture_id_to_surface_index_.insert(
      std::make_pair(*texture_id, window_id)).second);
  done->Signal();
}

void RenderingHelper::RenderTexture(GLuint texture_id) {
  CHECK_EQ(MessageLoop::current(), message_loop_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  CHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
  if (!suppress_swap_to_display_) {
    int window_id = texture_id_to_surface_index_[texture_id];
    CHECK(eglMakeCurrent(egl_display_, egl_surfaces_[window_id],
                         egl_surfaces_[window_id], egl_context_))
        << eglGetError();
    eglSwapBuffers(egl_display_, egl_surfaces_[window_id]);
  }
  CHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
}

void RenderingHelper::DeleteTexture(GLuint texture_id) {
  glDeleteTextures(1, &texture_id);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

#if defined(OS_WIN)
void RenderingHelper::PlatformInitialize() {}

void RenderingHelper::PlatformUnInitialize() {
  for (size_t i = 0; i < windows_.size(); ++i) {
    DestroyWindow(windows_[i]);
  }
  windows_.clear();
}

EGLNativeWindowType RenderingHelper::PlatformCreateWindow(
    int top_left_x, int top_left_y) {
  HWND window = CreateWindowEx(0, L"Static", L"VideoDecodeAcceleratorTest",
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE, top_left_x,
                               top_left_y, width_, height_, NULL, NULL, NULL,
                               NULL);
  CHECK(window != NULL);
  windows_.push_back(window);
  return window;
}

EGLDisplay RenderingHelper::PlatformGetDisplay() {
  return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

#else  // OS_WIN

void RenderingHelper::PlatformInitialize() {
  CHECK(x_display_ = base::MessagePumpForUI::GetDefaultXDisplay());
}

void RenderingHelper::PlatformUnInitialize() {
  // Destroy resources acquired in Initialize, in reverse-acquisition order.
  for (size_t i = 0; i < x_windows_.size(); ++i) {
    CHECK(XUnmapWindow(x_display_, x_windows_[i]));
    CHECK(XDestroyWindow(x_display_, x_windows_[i]));
  }
  // Mimic newly created object.
  x_display_ = NULL;
  x_windows_.clear();
}

EGLDisplay RenderingHelper::PlatformGetDisplay() {
  return eglGetDisplay(x_display_);
}

EGLNativeWindowType RenderingHelper::PlatformCreateWindow(int top_left_x,
                                                          int top_left_y) {
  int depth = DefaultDepth(x_display_, DefaultScreen(x_display_));

  XSetWindowAttributes window_attributes;
  window_attributes.background_pixel =
      BlackPixel(x_display_, DefaultScreen(x_display_));
  window_attributes.override_redirect = true;

  Window x_window = XCreateWindow(
      x_display_, DefaultRootWindow(x_display_),
      top_left_x, top_left_y, width_, height_,
      0 /* border width */,
      depth, CopyFromParent /* class */, CopyFromParent /* visual */,
      (CWBackPixel | CWOverrideRedirect), &window_attributes);
  x_windows_.push_back(x_window);
  XStoreName(x_display_, x_window, "VideoDecodeAcceleratorTest");
  XSelectInput(x_display_, x_window, ExposureMask);
  XMapWindow(x_display_, x_window);
  return x_window;
}

#endif  // OS_WIN

// State of the EglRenderingVDAClient below.  Order matters here as the test
// makes assumptions about it.
enum ClientState {
  CS_CREATED,
  CS_DECODER_SET,
  CS_INITIALIZED,
  CS_FLUSHING,
  CS_FLUSHED,
  CS_DONE,
  CS_RESETTING,
  CS_RESET,
  CS_ERROR,
  CS_DESTROYED,
  CS_MAX,  // Must be last entry.
};

// Helper class allowing one thread to wait on a notification from another.
// If notifications come in faster than they are Wait()'d for, they are
// accumulated (so exactly as many Wait() calls will unblock as Notify() calls
// were made, regardless of order).
class ClientStateNotification {
 public:
  ClientStateNotification();
  ~ClientStateNotification();

  // Used to notify a single waiter of a ClientState.
  void Notify(ClientState state);
  // Used by waiters to wait for the next ClientState Notification.
  ClientState Wait();
 private:
  base::Lock lock_;
  base::ConditionVariable cv_;
  std::queue<ClientState> pending_states_for_notification_;
};

ClientStateNotification::ClientStateNotification() : cv_(&lock_) {}

ClientStateNotification::~ClientStateNotification() {}

void ClientStateNotification::Notify(ClientState state) {
  base::AutoLock auto_lock(lock_);
  pending_states_for_notification_.push(state);
  cv_.Signal();
}

ClientState ClientStateNotification::Wait() {
  base::AutoLock auto_lock(lock_);
  while (pending_states_for_notification_.empty())
    cv_.Wait();
  ClientState ret = pending_states_for_notification_.front();
  pending_states_for_notification_.pop();
  return ret;
}

// Magic constants for differentiating the reasons for NotifyResetDone being
// called.
enum ResetPoint {
  MID_STREAM_RESET = -2,
  END_OF_STREAM_RESET = -1
};

// Client that can accept callbacks from a VideoDecodeAccelerator and is used by
// the TESTs below.
class EglRenderingVDAClient : public VideoDecodeAccelerator::Client {
 public:
  // Doesn't take ownership of |rendering_helper| or |note|, which must outlive
  // |*this|.
  // |num_play_throughs| indicates how many times to play through the video.
  // |reset_after_frame_num| can be a frame number >=0 indicating a mid-stream
  // Reset() should be done after that frame number is delivered, or
  // END_OF_STREAM_RESET to indicate no mid-stream Reset().
  // |delete_decoder_state| indicates when the underlying decoder should be
  // Destroy()'d and deleted and can take values: N<0: delete after -N Decode()
  // calls have been made, N>=0 means interpret as ClientState.
  // Both |reset_after_frame_num| & |delete_decoder_state| apply only to the
  // last play-through (governed by |num_play_throughs|).
  EglRenderingVDAClient(RenderingHelper* rendering_helper,
                        int rendering_window_id,
                        ClientStateNotification* note,
                        const std::string& encoded_data,
                        int num_NALUs_per_decode,
                        int num_in_flight_decodes,
                        int num_play_throughs,
                        int reset_after_frame_num,
                        int delete_decoder_state,
                        int profile);
  virtual ~EglRenderingVDAClient();
  void CreateDecoder();

  // VideoDecodeAccelerator::Client implementation.
  // The heart of the Client.
  virtual void ProvidePictureBuffers(
      uint32 requested_num_of_buffers,
      const gfx::Size& dimensions);
  virtual void DismissPictureBuffer(int32 picture_buffer_id);
  virtual void PictureReady(const media::Picture& picture);
  // Simple state changes.
  virtual void NotifyInitializeDone();
  virtual void NotifyEndOfBitstreamBuffer(int32 bitstream_buffer_id);
  virtual void NotifyFlushDone();
  virtual void NotifyResetDone();
  virtual void NotifyError(VideoDecodeAccelerator::Error error);

  // Simple getters for inspecting the state of the Client.
  ClientState state() { return state_; }
  int num_done_bitstream_buffers() { return num_done_bitstream_buffers_; }
  int num_decoded_frames() { return num_decoded_frames_; }
  EGLDisplay egl_display() { return rendering_helper_->egl_display(); }
  EGLContext egl_context() { return rendering_helper_->egl_context(); }
  double frames_per_second();
  bool decoder_deleted() { return !decoder_; }

 private:
  typedef std::map<int, media::PictureBuffer*> PictureBufferById;

  void SetState(ClientState new_state);

  // Delete the associated OMX decoder helper.
  void DeleteDecoder();

  // Compute & return in |*end_pos| the end position for the next batch of NALUs
  // to ship to the decoder (based on |start_pos| & |num_NALUs_per_decode_|).
  void GetRangeForNextNALUs(size_t start_pos, size_t* end_pos);

  // Request decode of the next batch of NALUs in the encoded data.
  void DecodeNextNALUs();

  RenderingHelper* rendering_helper_;
  int rendering_window_id_;
  std::string encoded_data_;
  const int num_NALUs_per_decode_;
  const int num_in_flight_decodes_;
  int outstanding_decodes_;
  size_t encoded_data_next_pos_to_decode_;
  int next_bitstream_buffer_id_;
  ClientStateNotification* note_;
  scoped_refptr<VideoDecodeAccelerator> decoder_;
  std::set<int> outstanding_texture_ids_;
  int remaining_play_throughs_;
  int reset_after_frame_num_;
  int delete_decoder_state_;
  ClientState state_;
  int num_decoded_frames_;
  int num_done_bitstream_buffers_;
  PictureBufferById picture_buffers_by_id_;
  base::TimeTicks initialize_done_ticks_;
  base::TimeTicks last_frame_delivered_ticks_;
  int profile_;
};

EglRenderingVDAClient::EglRenderingVDAClient(
    RenderingHelper* rendering_helper,
    int rendering_window_id,
    ClientStateNotification* note,
    const std::string& encoded_data,
    int num_NALUs_per_decode,
    int num_in_flight_decodes,
    int num_play_throughs,
    int reset_after_frame_num,
    int delete_decoder_state,
    int profile)
    : rendering_helper_(rendering_helper),
      rendering_window_id_(rendering_window_id),
      encoded_data_(encoded_data), num_NALUs_per_decode_(num_NALUs_per_decode),
      num_in_flight_decodes_(num_in_flight_decodes), outstanding_decodes_(0),
      encoded_data_next_pos_to_decode_(0), next_bitstream_buffer_id_(0),
      note_(note),
      remaining_play_throughs_(num_play_throughs),
      reset_after_frame_num_(reset_after_frame_num),
      delete_decoder_state_(delete_decoder_state),
      state_(CS_CREATED),
      num_decoded_frames_(0), num_done_bitstream_buffers_(0),
      profile_(profile) {
  CHECK_GT(num_NALUs_per_decode, 0);
  CHECK_GT(num_in_flight_decodes, 0);
  CHECK_GT(num_play_throughs, 0);
}

EglRenderingVDAClient::~EglRenderingVDAClient() {
  DeleteDecoder();  // Clean up in case of expected error.
  CHECK(decoder_deleted());
  STLDeleteValues(&picture_buffers_by_id_);
  SetState(CS_DESTROYED);
}

void EglRenderingVDAClient::CreateDecoder() {
  CHECK(decoder_deleted());
#if defined(OS_WIN)
  scoped_refptr<DXVAVideoDecodeAccelerator> decoder =
      new DXVAVideoDecodeAccelerator(this);
#else  // OS_WIN
  scoped_refptr<OmxVideoDecodeAccelerator> decoder =
      new OmxVideoDecodeAccelerator(this);
  decoder->SetEglState(egl_display(), egl_context());
#endif  // OS_WIN
  decoder_ = decoder.release();
  SetState(CS_DECODER_SET);
  if (decoder_deleted())
    return;

  // Configure the decoder.
  media::VideoCodecProfile profile = media::H264PROFILE_BASELINE;
  if (profile_ != -1)
    profile = static_cast<media::VideoCodecProfile>(profile_);
  CHECK(decoder_->Initialize(profile));
}

void EglRenderingVDAClient::ProvidePictureBuffers(
    uint32 requested_num_of_buffers,
    const gfx::Size& dimensions) {
  if (decoder_deleted())
    return;
  std::vector<media::PictureBuffer> buffers;

  for (uint32 i = 0; i < requested_num_of_buffers; ++i) {
    uint32 id = picture_buffers_by_id_.size();
    GLuint texture_id;
    base::WaitableEvent done(false, false);
    rendering_helper_->CreateTexture(rendering_window_id_, &texture_id, &done);
    done.Wait();
    CHECK(outstanding_texture_ids_.insert(texture_id).second);
    media::PictureBuffer* buffer =
        new media::PictureBuffer(id, dimensions, texture_id);
    CHECK(picture_buffers_by_id_.insert(std::make_pair(id, buffer)).second);
    buffers.push_back(*buffer);
  }
  decoder_->AssignPictureBuffers(buffers);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  CHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
}

void EglRenderingVDAClient::DismissPictureBuffer(int32 picture_buffer_id) {
  PictureBufferById::iterator it =
      picture_buffers_by_id_.find(picture_buffer_id);
  CHECK(it != picture_buffers_by_id_.end());
  CHECK_EQ(outstanding_texture_ids_.erase(it->second->texture_id()), 1U);
  rendering_helper_->DeleteTexture(it->second->texture_id());
  delete it->second;
  picture_buffers_by_id_.erase(it);
}

void EglRenderingVDAClient::PictureReady(const media::Picture& picture) {
  // We shouldn't be getting pictures delivered after Reset has completed.
  CHECK_LT(state_, CS_RESET);

  if (decoder_deleted())
    return;
  last_frame_delivered_ticks_ = base::TimeTicks::Now();

  // Because we feed the decoder a limited number of NALUs at a time, we can be
  // sure that the bitstream buffer from which a frame comes has a limited
  // range.  Assert that.
  CHECK_GE((picture.bitstream_buffer_id() + 1) * num_NALUs_per_decode_,
           num_decoded_frames_);
  CHECK_LE(picture.bitstream_buffer_id(), next_bitstream_buffer_id_);
  ++num_decoded_frames_;

  // Mid-stream reset applies only to the last play-through per constructor
  // comment.
  if (remaining_play_throughs_ == 1 &&
      reset_after_frame_num_ == num_decoded_frames_) {
    reset_after_frame_num_ = MID_STREAM_RESET;
    decoder_->Reset();
    // Re-start decoding from the beginning of the stream to avoid needing to
    // know how to find I-frames and so on in this test.
    encoded_data_next_pos_to_decode_ = 0;
  }

  media::PictureBuffer* picture_buffer =
      picture_buffers_by_id_[picture.picture_buffer_id()];
  CHECK(picture_buffer);
  rendering_helper_->RenderTexture(picture_buffer->texture_id());

  decoder_->ReusePictureBuffer(picture.picture_buffer_id());
}

void EglRenderingVDAClient::NotifyInitializeDone() {
  SetState(CS_INITIALIZED);
  initialize_done_ticks_ = base::TimeTicks::Now();
  for (int i = 0; i < num_in_flight_decodes_; ++i)
    DecodeNextNALUs();
}

void EglRenderingVDAClient::NotifyEndOfBitstreamBuffer(
    int32 bitstream_buffer_id) {
  ++num_done_bitstream_buffers_;
  --outstanding_decodes_;
  DecodeNextNALUs();
}

void EglRenderingVDAClient::NotifyFlushDone() {
  if (decoder_deleted())
    return;
  SetState(CS_FLUSHED);
  --remaining_play_throughs_;
  DCHECK_GE(remaining_play_throughs_, 0);
  if (decoder_deleted())
    return;
  decoder_->Reset();
  SetState(CS_RESETTING);
}

void EglRenderingVDAClient::NotifyResetDone() {
  if (decoder_deleted())
    return;

  if (reset_after_frame_num_ == MID_STREAM_RESET) {
    reset_after_frame_num_ = END_OF_STREAM_RESET;
    return;
  }

  if (remaining_play_throughs_) {
    encoded_data_next_pos_to_decode_ = 0;
    NotifyInitializeDone();
    return;
  }

  SetState(CS_RESET);
  if (!decoder_deleted())
    DeleteDecoder();
}

void EglRenderingVDAClient::NotifyError(VideoDecodeAccelerator::Error error) {
  SetState(CS_ERROR);
}

static bool LookingAtNAL(const std::string& encoded, size_t pos) {
  return pos + 3 < encoded.size() &&
      encoded[pos] == 0 && encoded[pos + 1] == 0 &&
      encoded[pos + 2] == 0 && encoded[pos + 3] == 1;
}

void EglRenderingVDAClient::SetState(ClientState new_state) {
  note_->Notify(new_state);
  state_ = new_state;
  if (!remaining_play_throughs_ && new_state == delete_decoder_state_) {
    CHECK(!decoder_deleted());
    DeleteDecoder();
  }
}

void EglRenderingVDAClient::DeleteDecoder() {
  if (decoder_deleted())
    return;
  decoder_->Destroy();
  decoder_ = NULL;
  STLClearObject(&encoded_data_);
  for (std::set<int>::iterator it = outstanding_texture_ids_.begin();
       it != outstanding_texture_ids_.end(); ++it) {
    rendering_helper_->DeleteTexture(*it);
  }
  outstanding_texture_ids_.clear();
  // Cascade through the rest of the states to simplify test code below.
  for (int i = state_ + 1; i < CS_MAX; ++i)
    SetState(static_cast<ClientState>(i));
}

void EglRenderingVDAClient::GetRangeForNextNALUs(
    size_t start_pos, size_t* end_pos) {
  *end_pos = start_pos;
  CHECK(LookingAtNAL(encoded_data_, start_pos));
  for (int i = 0; i < num_NALUs_per_decode_; ++i) {
    *end_pos += 4;
    while (*end_pos + 3 < encoded_data_.size() &&
           !LookingAtNAL(encoded_data_, *end_pos)) {
      ++*end_pos;
    }
    if (*end_pos + 3 >= encoded_data_.size()) {
      *end_pos = encoded_data_.size();
      return;
    }
  }
}

void EglRenderingVDAClient::DecodeNextNALUs() {
  if (decoder_deleted())
    return;
  if (encoded_data_next_pos_to_decode_ == encoded_data_.size()) {
    if (outstanding_decodes_ == 0) {
      decoder_->Flush();
      SetState(CS_FLUSHING);
    }
    return;
  }
  size_t start_pos = encoded_data_next_pos_to_decode_;
  size_t end_pos;
  GetRangeForNextNALUs(start_pos, &end_pos);

  // Populate the shared memory buffer w/ the NALU, duplicate its handle, and
  // hand it off to the decoder.
  base::SharedMemory shm;
  CHECK(shm.CreateAndMapAnonymous(end_pos - start_pos))
      << start_pos << ", " << end_pos;
  memcpy(shm.memory(), encoded_data_.data() + start_pos, end_pos - start_pos);
  base::SharedMemoryHandle dup_handle;
  CHECK(shm.ShareToProcess(base::Process::Current().handle(), &dup_handle));
  media::BitstreamBuffer bitstream_buffer(
      next_bitstream_buffer_id_++, dup_handle, end_pos - start_pos);
  decoder_->Decode(bitstream_buffer);
  ++outstanding_decodes_;
  encoded_data_next_pos_to_decode_ = end_pos;

  if (!remaining_play_throughs_ &&
      -delete_decoder_state_ == next_bitstream_buffer_id_) {
    DeleteDecoder();
  }
}

double EglRenderingVDAClient::frames_per_second() {
  base::TimeDelta delta = last_frame_delivered_ticks_ - initialize_done_ticks_;
  if (delta.InSecondsF() == 0)
    return 0;
  return num_decoded_frames_ / delta.InSecondsF();
}

// Test parameters:
// - Number of NALUs per Decode() call.
// - Number of concurrent decoders.
// - Number of concurrent in-flight Decode() calls per decoder.
// - Number of play-throughs.
// - reset_after_frame_num: see EglRenderingVDAClient ctor.
// - delete_decoder_phase: see EglRenderingVDAClient ctor.
class VideoDecodeAcceleratorTest
    : public ::testing::TestWithParam<
  Tuple6<int, int, int, int, ResetPoint, ClientState> > {
};

// Wait for |note| to report a state and if it's not |expected_state| then
// assert |client| has deleted its decoder.
static void AssertWaitForStateOrDeleted(ClientStateNotification* note,
                                        EglRenderingVDAClient* client,
                                        ClientState expected_state) {
  ClientState state = note->Wait();
  if (state == expected_state) return;
  ASSERT_TRUE(client->decoder_deleted())
      << "Decoder not deleted but Wait() returned " << state
      << ", instead of " << expected_state;
}

// We assert a minimal number of concurrent decoders we expect to succeed.
// Different platforms can support more concurrent decoders, so we don't assert
// failure above this.
enum { kMinSupportedNumConcurrentDecoders = 3 };

// Test the most straightforward case possible: data is decoded from a single
// chunk and rendered to the screen.
TEST_P(VideoDecodeAcceleratorTest, TestSimpleDecode) {
  // Can be useful for debugging VLOGs from OVDA.
  // logging::SetMinLogLevel(-1);

  // Required for Thread to work.  Not used otherwise.
  base::ShadowingAtExitManager at_exit_manager;

  const int num_NALUs_per_decode = GetParam().a;
  const size_t num_concurrent_decoders = GetParam().b;
  const size_t num_in_flight_decodes = GetParam().c;
  const int num_play_throughs = GetParam().d;
  const int reset_after_frame_num = GetParam().e;
  const int delete_decoder_state = GetParam().f;

  FilePath::StringType test_video_file;
  int frame_width, frame_height;
  int num_frames, num_NALUs, min_fps_render, min_fps_no_render, profile;
  ParseTestVideoData(test_video_data, &test_video_file, &frame_width,
                     &frame_height, &num_frames, &num_NALUs,
                     &min_fps_render, &min_fps_no_render, &profile);
  min_fps_render /= num_concurrent_decoders;
  min_fps_no_render /= num_concurrent_decoders;

  // If we reset mid-stream and start playback over, account for frames that are
  // decoded twice in our expectations.
  if (num_frames > 0 && reset_after_frame_num >= 0)
    num_frames += reset_after_frame_num;

  // Suppress EGL surface swapping in all but a few tests, to cut down overall
  // test runtime.
  const bool suppress_swap_to_display = num_NALUs_per_decode > 1;

  std::vector<ClientStateNotification*> notes(num_concurrent_decoders, NULL);
  std::vector<EglRenderingVDAClient*> clients(num_concurrent_decoders, NULL);

  // Read in the video data.
  std::string data_str;
  CHECK(file_util::ReadFileToString(FilePath(test_video_file), &data_str))
      << "test_video_file: " << test_video_file;

  // Initialize the rendering helper.
  base::Thread rendering_thread("EglRenderingVDAClientThread");
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_DEFAULT;
#if defined(OS_WIN)
  // For windows the decoding thread initializes the media foundation decoder
  // which uses COM. We need the thread to be a UI thread.
  options.message_loop_type = MessageLoop::TYPE_UI;
#endif  // OS_WIN

  rendering_thread.StartWithOptions(options);
  RenderingHelper rendering_helper;

  base::WaitableEvent done(false, false);
  rendering_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RenderingHelper::Initialize,
                 base::Unretained(&rendering_helper),
                 suppress_swap_to_display, num_concurrent_decoders,
                 frame_width, frame_height, &done));
  done.Wait();

  // First kick off all the decoders.
  for (size_t index = 0; index < num_concurrent_decoders; ++index) {
    ClientStateNotification* note = new ClientStateNotification();
    notes[index] = note;
    EglRenderingVDAClient* client = new EglRenderingVDAClient(
        &rendering_helper, index,
        note, data_str, num_NALUs_per_decode,
        num_in_flight_decodes, num_play_throughs,
        reset_after_frame_num, delete_decoder_state, profile);
    clients[index] = client;

    rendering_thread.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&EglRenderingVDAClient::CreateDecoder,
                   base::Unretained(client)));

    ASSERT_EQ(note->Wait(), CS_DECODER_SET);
  }
  // Then wait for all the decodes to finish.
  // Only check performance & correctness later if we play through only once.
  bool skip_performance_and_correctness_checks = num_play_throughs > 1;
  for (size_t i = 0; i < num_concurrent_decoders; ++i) {
    ClientStateNotification* note = notes[i];
    ClientState state = note->Wait();
    if (state != CS_INITIALIZED) {
      skip_performance_and_correctness_checks = true;
      // We expect initialization to fail only when more than the supported
      // number of decoders is instantiated.  Assert here that something else
      // didn't trigger failure.
      ASSERT_GT(num_concurrent_decoders,
                static_cast<size_t>(kMinSupportedNumConcurrentDecoders));
      continue;
    }
    ASSERT_EQ(state, CS_INITIALIZED);
    for (int n = 0; n < num_play_throughs; ++n) {
      // For play-throughs other than the first, we expect initialization to
      // succeed unconditionally.
      if (n > 0) {
        ASSERT_NO_FATAL_FAILURE(
            AssertWaitForStateOrDeleted(note, clients[i], CS_INITIALIZED));
      }
      // InitializeDone kicks off decoding inside the client, so we just need to
      // wait for Flush.
      ASSERT_NO_FATAL_FAILURE(
          AssertWaitForStateOrDeleted(note, clients[i], CS_FLUSHING));
      ASSERT_NO_FATAL_FAILURE(
          AssertWaitForStateOrDeleted(note, clients[i], CS_FLUSHED));
      // FlushDone requests Reset().
      ASSERT_NO_FATAL_FAILURE(
          AssertWaitForStateOrDeleted(note, clients[i], CS_RESETTING));
    }
    ASSERT_NO_FATAL_FAILURE(
        AssertWaitForStateOrDeleted(note, clients[i], CS_RESET));
    // ResetDone requests Destroy().
    ASSERT_NO_FATAL_FAILURE(
        AssertWaitForStateOrDeleted(note, clients[i], CS_DESTROYED));
  }
  // Finally assert that decoding went as expected.
  for (size_t i = 0; i < num_concurrent_decoders &&
           !skip_performance_and_correctness_checks; ++i) {
    // We can only make performance/correctness assertions if the decoder was
    // allowed to finish.
    if (delete_decoder_state < CS_FLUSHED)
      continue;
    EglRenderingVDAClient* client = clients[i];
    if (num_frames > 0)
      EXPECT_EQ(client->num_decoded_frames(), num_frames);
    if (num_NALUs > 0 && reset_after_frame_num < 0) {
      EXPECT_EQ(client->num_done_bitstream_buffers(),
                ceil(static_cast<double>(num_NALUs) / num_NALUs_per_decode));
    }
    LOG(INFO) << "Decoder " << i << " fps: " << client->frames_per_second();
    int min_fps = suppress_swap_to_display ? min_fps_no_render : min_fps_render;
    if (min_fps > 0)
      EXPECT_GT(client->frames_per_second(), min_fps);
  }

  rendering_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&STLDeleteElements<std::vector<EglRenderingVDAClient*> >,
                 &clients));
  rendering_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&STLDeleteElements<std::vector<ClientStateNotification*> >,
                 &notes));
  rendering_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RenderingHelper::UnInitialize,
                 base::Unretained(&rendering_helper),
                 &done));
  done.Wait();
  rendering_thread.Stop();
};

// Test that replay after EOS works fine.
INSTANTIATE_TEST_CASE_P(
    ReplayAfterEOS, VideoDecodeAcceleratorTest,
    ::testing::Values(
        MakeTuple(1, 1, 1, 4, END_OF_STREAM_RESET, CS_RESET)));

// Test that Reset() mid-stream works fine and doesn't affect decoding even when
// Decode() calls are made during the reset.
INSTANTIATE_TEST_CASE_P(
    MidStreamReset, VideoDecodeAcceleratorTest,
    ::testing::Values(
        MakeTuple(1, 1, 1, 1, static_cast<ResetPoint>(100), CS_RESET)));

// Test that Destroy() mid-stream works fine (primarily this is testing that no
// crashes occur).
INSTANTIATE_TEST_CASE_P(
    TearDownTiming, VideoDecodeAcceleratorTest,
    ::testing::Values(
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_DECODER_SET),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_INITIALIZED),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_FLUSHING),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_FLUSHED),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_RESETTING),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET,
                  static_cast<ClientState>(-1)),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET,
                  static_cast<ClientState>(-10)),
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET,
                  static_cast<ClientState>(-100))));

// Test that decoding various variation works: multiple concurrent decoders and
// multiple NALUs per Decode() call.
INSTANTIATE_TEST_CASE_P(
    DecodeVariations, VideoDecodeAcceleratorTest,
    ::testing::Values(
        MakeTuple(1, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(1, 1, 10, 1, END_OF_STREAM_RESET, CS_RESET),
        // Tests queuing.
        MakeTuple(1, 1, 15, 1, END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(1, 3, 1, 1, END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(2, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(3, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(5, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(8, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET),
        // TODO(fischman): decoding more than 15 NALUs at once breaks decode -
        // visual artifacts are introduced as well as spurious frames are
        // delivered (more pictures are returned than NALUs are fed to the
        // decoder).  Increase the "15" below when
        // http://code.google.com/p/chrome-os-partner/issues/detail?id=4378 is
        // fixed.
        MakeTuple(15, 1, 1, 1, END_OF_STREAM_RESET, CS_RESET)));

// Find out how many concurrent decoders can go before we exhaust system
// resources.
INSTANTIATE_TEST_CASE_P(
    ResourceExhaustion, VideoDecodeAcceleratorTest,
    ::testing::Values(
        // +0 hack below to promote enum to int.
        MakeTuple(1, kMinSupportedNumConcurrentDecoders + 0, 1, 1,
                  END_OF_STREAM_RESET, CS_RESET),
        MakeTuple(1, kMinSupportedNumConcurrentDecoders + 1, 1, 1,
                  END_OF_STREAM_RESET, CS_RESET)));

// TODO(fischman, vrk): add more tests!  In particular:
// - Test life-cycle: Seek/Stop/Pause/Play for a single decoder.
// - Test alternate configurations
// - Test failure conditions.
// - Test frame size changes mid-stream

}  // namespace

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);  // Removes gtest-specific args.
  CommandLine::Init(argc, argv);

  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);

  CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first == "test_video_data") {
      test_video_data = it->second.c_str();
      continue;
    }
    LOG(FATAL) << "Unexpected switch: " << it->first << ":" << it->second;
  }
#if defined(OS_WIN)
  base::ShadowingAtExitManager at_exit_manager;
  gfx::InitializeGLBindings(gfx::kGLImplementationEGLGLES2);
  gfx::GLSurface::InitializeOneOff();
  {
    // Hack to ensure that EGL extension function pointers are initialized.
    scoped_refptr<gfx::GLSurface> surface(
        gfx::GLSurface::CreateOffscreenGLSurface(false, gfx::Size(1, 1)));
    scoped_refptr<gfx::GLContext> context(
        gfx::GLContext::CreateGLContext(NULL, surface.get(),
                                        gfx::PreferIntegratedGpu));
    context->MakeCurrent(surface.get());
  }
  DXVAVideoDecodeAccelerator::PreSandboxInitialization();
#endif  // OS_WIN
  return RUN_ALL_TESTS();
}
