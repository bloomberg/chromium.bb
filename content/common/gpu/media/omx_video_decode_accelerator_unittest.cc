// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "testing/gtest/include/gtest/gtest.h"

#include "base/at_exit.h"
#include "base/file_util.h"
#include "base/stl_util-inl.h"
#include "base/stringize_macros.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/common/gpu/media/omx_video_decode_accelerator.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/GLES2/gl2.h"

#if !defined(OS_CHROMEOS) || !defined(ARCH_CPU_ARMEL)
#error This test (and OmxVideoDecodeAccelerator) are only supported on cros/ARM!
#endif

using media::VideoDecodeAccelerator;

namespace {

// General-purpose constants for this test.
enum {
  kFrameWidth = 320,
  kFrameHeight = 240,
};

// Helper for managing X11, EGL, and GLES2 resources.  Xlib is not thread-safe,
// and GL state is thread-specific, so all the methods of this class (except for
// ctor/dtor) ensure they're being run on a single thread.
//
// TODO(fischman): consider moving this into media/ if we can de-dup some of the
// code that ends up getting copy/pasted all over the place (esp. the GL setup
// code).
class RenderingHelper {
 public:
  explicit RenderingHelper();
  ~RenderingHelper();

  // Initialize all structures to prepare to render to one or more windows of
  // the specified dimensions.  CHECK-fails if any initialization step fails.
  // After this returns, texture creation and rendering can be requested.  This
  // method can be called multiple times, in which case all previously-acquired
  // resources and initializations are discarded.  If |suppress_swap_to_display|
  // then all the usual work is done, except for the final swap of the EGL
  // surface to the display.  This cuts test times over 50% so is worth doing
  // when testing non-rendering-related aspects.
  void Initialize(bool suppress_swap_to_display, int num_windows,
                  int width, int height, base::WaitableEvent* done);

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

  EGLDisplay egl_display() { return egl_display_; }
  EGLContext egl_context() { return egl_context_; }

 private:
  // Zero-out internal state.  Helper for ctor & UnInitialize().
  void Clear();

  bool suppress_swap_to_display_;
  int width_;
  int height_;
  Display* x_display_;
  std::vector<Window> x_windows_;
  EGLDisplay egl_display_;
  EGLContext egl_context_;
  std::vector<EGLSurface> egl_surfaces_;
  std::map<GLuint, int> texture_id_to_surface_index_;
  // We ensure all operations are carried out on the same thread by remembering
  // where we were Initialized.
  MessageLoop* message_loop_;
};

RenderingHelper::RenderingHelper() {
  Clear();
}

RenderingHelper::~RenderingHelper() {
  CHECK_EQ(width_, 0) << "Must call UnInitialize before dtor.";
}

void RenderingHelper::Clear() {
  suppress_swap_to_display_ = false;
  width_ = 0;
  height_ = 0;
  x_display_ = NULL;
  x_windows_.clear();
  egl_display_ = EGL_NO_DISPLAY;
  egl_context_ = EGL_NO_CONTEXT;
  egl_surfaces_.clear();
  texture_id_to_surface_index_.clear();
  message_loop_ = NULL;
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
    int width, int height,
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

  // Per-display X11 & EGL initialization.
  CHECK(x_display_ = XOpenDisplay(NULL));
  int depth = DefaultDepth(x_display_, DefaultScreen(x_display_));
  XSetWindowAttributes window_attributes;
  window_attributes.background_pixel =
      BlackPixel(x_display_, DefaultScreen(x_display_));
  window_attributes.override_redirect = true;

  egl_display_ = eglGetDisplay(x_display_);
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
    // Arrange X windows side by side whimsically.
    int top_left_x = (width + 50) * i;
    int top_left_y = 100 + (i % 2) * 250;
    Window x_window = XCreateWindow(
      x_display_, DefaultRootWindow(x_display_),
      top_left_x, top_left_y, width_, height_,
      0 /* border width */,
      depth, CopyFromParent /* class */, CopyFromParent /* visual */,
      (CWBackPixel | CWOverrideRedirect), &window_attributes);
    x_windows_.push_back(x_window);
    XStoreName(x_display_, x_window, "OmxVideoDecodeAcceleratorTest");
    XSelectInput(x_display_, x_window, ExposureMask);
    XMapWindow(x_display_, x_window);

    EGLSurface egl_surface =
        eglCreateWindowSurface(egl_display_, egl_config, x_window, NULL);
    egl_surfaces_.push_back(egl_surface);
    CHECK_NE(egl_surface, EGL_NO_SURFACE);
  }
  CHECK(eglMakeCurrent(egl_display_, egl_surfaces_[0],
                       egl_surfaces_[0], egl_context_)) << eglGetError();

  // GLES2 initialization.  Note: This is pretty much copy/pasted from
  // media/tools/player_x11/gles_video_renderer.cc, with some simplification
  // applied.
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
  // Destroy resources acquired in Initialize, in reverse-acquisition order.
  CHECK(eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT)) << eglGetError();
  CHECK(eglDestroyContext(egl_display_, egl_context_));
  for (size_t i = 0; i < egl_surfaces_.size(); ++i)
    CHECK(eglDestroySurface(egl_display_, egl_surfaces_[i]));
  CHECK(eglTerminate(egl_display_));
  for (size_t i = 0; i < x_windows_.size(); ++i) {
    CHECK(XUnmapWindow(x_display_, x_windows_[i]));
    CHECK(XDestroyWindow(x_display_, x_windows_[i]));
  }
  // Mimic newly-created object.
  Clear();
  done->Signal();
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
  DCHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  DCHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
  if (!suppress_swap_to_display_) {
    int window_id = texture_id_to_surface_index_[texture_id];
    CHECK(eglMakeCurrent(egl_display_, egl_surfaces_[window_id],
                         egl_surfaces_[window_id], egl_context_))
        << eglGetError();
    eglSwapBuffers(egl_display_, egl_surfaces_[window_id]);
  }
  DCHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
}

void RenderingHelper::DeleteTexture(GLuint texture_id) {
  glDeleteTextures(1, &texture_id);
  DCHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

// State of the EglRenderingVDAClient below.
enum ClientState {
  CS_CREATED,
  CS_DECODER_SET,
  CS_INITIALIZED,
  CS_FLUSHED,
  CS_DONE,
  CS_ABORTED,
  CS_ERROR,
  CS_DESTROYED,
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

// Client that can accept callbacks from a VideoDecodeAccelerator and is used by
// the TESTs below.
class EglRenderingVDAClient : public VideoDecodeAccelerator::Client {
 public:
  // Doesn't take ownership of |note| or |encoded_data|, which must outlive
  // |*this|.
  EglRenderingVDAClient(RenderingHelper* rendering_helper,
                        int rendering_window_id,
                        ClientStateNotification* note,
                        std::string* encoded_data,
                        int num_NALUs_per_decode,
                        bool suppress_swap_to_display);
  virtual ~EglRenderingVDAClient();

  // VideoDecodeAccelerator::Client implementation.
  // The heart of the Client.
  virtual void ProvidePictureBuffers(
      uint32 requested_num_of_buffers,
      const gfx::Size& dimensions,
      VideoDecodeAccelerator::MemoryType type);
  virtual void DismissPictureBuffer(int32 picture_buffer_id);
  virtual void PictureReady(const media::Picture& picture);
  // Simple state changes.
  virtual void NotifyInitializeDone();
  virtual void NotifyEndOfStream();
  virtual void NotifyEndOfBitstreamBuffer(int32 bitstream_buffer_id);
  virtual void NotifyFlushDone();
  virtual void NotifyAbortDone();
  virtual void NotifyError(VideoDecodeAccelerator::Error error);

  // Doesn't take ownership of |decoder|, which must outlive |*this|.
  void SetDecoder(VideoDecodeAccelerator* decoder);

  // Simple getters for inspecting the state of the Client.
  ClientState state() { return state_; }
  VideoDecodeAccelerator::Error error() { return error_; }
  int num_done_bitstream_buffers() { return num_done_bitstream_buffers_; }
  int num_decoded_frames() { return num_decoded_frames_; }
  EGLDisplay egl_display() { return rendering_helper_->egl_display(); }
  EGLContext egl_context() { return rendering_helper_->egl_context(); }
  double frames_per_second();

 private:
  typedef std::map<int, media::GLESBuffer*> PictureBufferById;

  void SetState(ClientState new_state) {
    note_->Notify(new_state);
    state_ = new_state;
  }

  // Compute & return in |*end_pos| the end position for the next batch of NALUs
  // to ship to the decoder (based on |start_pos| & |num_NALUs_per_decode_|).
  void GetRangeForNextNALUs(size_t start_pos, size_t* end_pos);

  // Request decode of the next batch of NALUs in the encoded data.
  void DecodeNextNALUs();

  RenderingHelper* rendering_helper_;
  int rendering_window_id_;
  const std::string* encoded_data_;
  const int num_NALUs_per_decode_;
  size_t encoded_data_next_pos_to_decode_;
  int next_bitstream_buffer_id_;
  ClientStateNotification* note_;
  VideoDecodeAccelerator* decoder_;
  ClientState state_;
  VideoDecodeAccelerator::Error error_;
  int num_decoded_frames_;
  int num_done_bitstream_buffers_;
  PictureBufferById picture_buffers_by_id_;
  base::TimeTicks initialize_done_ticks_;
  base::TimeTicks last_frame_delivered_ticks_;
};

EglRenderingVDAClient::EglRenderingVDAClient(RenderingHelper* rendering_helper,
                                             int rendering_window_id,
                                             ClientStateNotification* note,
                                             std::string* encoded_data,
                                             int num_NALUs_per_decode,
                                             bool suppress_swap_to_display)
    : rendering_helper_(rendering_helper),
      rendering_window_id_(rendering_window_id),
      encoded_data_(encoded_data), num_NALUs_per_decode_(num_NALUs_per_decode),
      encoded_data_next_pos_to_decode_(0), next_bitstream_buffer_id_(0),
      note_(note), decoder_(NULL), state_(CS_CREATED),
      error_(VideoDecodeAccelerator::VIDEODECODERERROR_NONE),
      num_decoded_frames_(0), num_done_bitstream_buffers_(0) {
  CHECK_GT(num_NALUs_per_decode, 0);
}

EglRenderingVDAClient::~EglRenderingVDAClient() {
  STLDeleteValues(&picture_buffers_by_id_);
  SetState(CS_DESTROYED);
}

void EglRenderingVDAClient::ProvidePictureBuffers(
    uint32 requested_num_of_buffers,
    const gfx::Size& dimensions,
    VideoDecodeAccelerator::MemoryType type) {
  CHECK_EQ(type, VideoDecodeAccelerator::PICTUREBUFFER_MEMORYTYPE_GL_TEXTURE);
  std::vector<media::GLESBuffer> buffers;
  CHECK_EQ(dimensions.width(), kFrameWidth);
  CHECK_EQ(dimensions.height(), kFrameHeight);

  for (uint32 i = 0; i < requested_num_of_buffers; ++i) {
    uint32 id = picture_buffers_by_id_.size();
    GLuint texture_id;
    base::WaitableEvent done(false, false);
    rendering_helper_->CreateTexture(rendering_window_id_, &texture_id, &done);
    done.Wait();
    media::GLESBuffer* buffer =
        new media::GLESBuffer(id, dimensions, texture_id);
    CHECK(picture_buffers_by_id_.insert(std::make_pair(id, buffer)).second);
    buffers.push_back(*buffer);
  }
  decoder_->AssignGLESBuffers(buffers);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  CHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
}

void EglRenderingVDAClient::DismissPictureBuffer(int32 picture_buffer_id) {
  PictureBufferById::iterator it =
      picture_buffers_by_id_.find(picture_buffer_id);
  DCHECK(it != picture_buffers_by_id_.end());
  rendering_helper_->DeleteTexture(it->second->texture_id());
  delete it->second;
  picture_buffers_by_id_.erase(it);
}

void EglRenderingVDAClient::PictureReady(const media::Picture& picture) {
  last_frame_delivered_ticks_ = base::TimeTicks::Now();

  // Because we feed the decoder a limited number of NALUs at a time, we can be
  // sure each the bitstream buffer from which a frame comes has a limited
  // range.  Assert that.
  CHECK_GE((picture.bitstream_buffer_id() + 1) * num_NALUs_per_decode_,
           num_decoded_frames_);
  CHECK_LE(picture.bitstream_buffer_id(), next_bitstream_buffer_id_);
  ++num_decoded_frames_;

  media::GLESBuffer* gles_buffer =
      picture_buffers_by_id_[picture.picture_buffer_id()];
  CHECK(gles_buffer);
  rendering_helper_->RenderTexture(gles_buffer->texture_id());

  decoder_->ReusePictureBuffer(picture.picture_buffer_id());
}

void EglRenderingVDAClient::NotifyInitializeDone() {
  SetState(CS_INITIALIZED);
  initialize_done_ticks_ = base::TimeTicks::Now();
  DecodeNextNALUs();
}

void EglRenderingVDAClient::NotifyEndOfStream() {
  SetState(CS_DONE);
}

void EglRenderingVDAClient::NotifyEndOfBitstreamBuffer(
    int32 bitstream_buffer_id) {
  ++num_done_bitstream_buffers_;
  DecodeNextNALUs();
}

void EglRenderingVDAClient::NotifyFlushDone() {
  SetState(CS_FLUSHED);
  decoder_->Abort();
}

void EglRenderingVDAClient::NotifyAbortDone() {
  SetState(CS_ABORTED);
}

void EglRenderingVDAClient::NotifyError(VideoDecodeAccelerator::Error error) {
  SetState(CS_ERROR);
  error_ = error;
}

void EglRenderingVDAClient::SetDecoder(VideoDecodeAccelerator* decoder) {
  decoder_ = decoder;
  SetState(CS_DECODER_SET);
}

static bool LookingAtNAL(const std::string& encoded, size_t pos) {
  return pos + 3 < encoded.size() &&
      encoded[pos] == 0 && encoded[pos + 1] == 0 &&
      encoded[pos + 2] == 0 && encoded[pos + 3] == 1;
}

void EglRenderingVDAClient::GetRangeForNextNALUs(
    size_t start_pos, size_t* end_pos) {
  *end_pos = start_pos;
  CHECK(LookingAtNAL(*encoded_data_, start_pos));
  for (int i = 0; i < num_NALUs_per_decode_; ++i) {
    *end_pos += 4;
    while (*end_pos + 3 < encoded_data_->size() &&
           !LookingAtNAL(*encoded_data_, *end_pos)) {
      ++*end_pos;
    }
    if (*end_pos + 3 >= encoded_data_->size()) {
      *end_pos = encoded_data_->size();
      return;
    }
  }
}

void EglRenderingVDAClient::DecodeNextNALUs() {
  if (encoded_data_next_pos_to_decode_ == encoded_data_->size()) {
    decoder_->Flush();
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
  memcpy(shm.memory(), encoded_data_->data() + start_pos, end_pos - start_pos);
  base::SharedMemoryHandle dup_handle;
  CHECK(shm.ShareToProcess(base::Process::Current().handle(), &dup_handle));
  media::BitstreamBuffer bitstream_buffer(
      next_bitstream_buffer_id_++, dup_handle, end_pos - start_pos);
  CHECK(decoder_->Decode(bitstream_buffer));
  encoded_data_next_pos_to_decode_ = end_pos;
}

double EglRenderingVDAClient::frames_per_second() {
  base::TimeDelta delta = last_frame_delivered_ticks_ - initialize_done_ticks_;
  CHECK_GT(delta.InSecondsF(), 0);
  return num_decoded_frames_ / delta.InSecondsF();
}

// Test parameters:
// - Number of NALUs per Decode() call.
// - Number of concurrent decoders.
class OmxVideoDecodeAcceleratorTest
    : public ::testing::TestWithParam<std::pair<int, int> > {
};

// Test the most straightforward case possible: data is decoded from a single
// chunk and rendered to the screen.
TEST_P(OmxVideoDecodeAcceleratorTest, TestSimpleDecode) {
  // Can be useful for debugging VLOGs from OVDA.
  // logging::SetMinLogLevel(-1);
  std::vector<uint32> matched_configs;

  // Required for Thread to work.  Not used otherwise.
  base::ShadowingAtExitManager at_exit_manager;

  const int num_NALUs_per_decode = GetParam().first;
  const size_t num_concurrent_decoders = GetParam().second;

  // Suppress EGL surface swapping in all but a few tests, to cut down overall
  // test runtime.
  const bool suppress_swap_to_display = num_NALUs_per_decode > 1;

  std::vector<ClientStateNotification*> notes(num_concurrent_decoders, NULL);
  std::vector<EglRenderingVDAClient*> clients(num_concurrent_decoders, NULL);
  std::vector<OmxVideoDecodeAccelerator*> decoders(
      num_concurrent_decoders, NULL);
  // Read in the video data.
  std::string data_str;
  CHECK(file_util::ReadFileToString(FilePath(std::string("test-25fps.h264")),
                                    &data_str));

  // Initialize the rendering helper.
  base::Thread rendering_thread("EglRenderingVDAClientThread");
  rendering_thread.Start();
  RenderingHelper rendering_helper;

  base::WaitableEvent done(false, false);
  rendering_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RenderingHelper::Initialize,
                 base::Unretained(&rendering_helper),
                 suppress_swap_to_display, num_concurrent_decoders,
                 static_cast<int>(kFrameWidth), static_cast<int>(kFrameHeight),
                 &done));
  done.Wait();

  // First kick off all the decoders.
  for (size_t index = 0; index < num_concurrent_decoders; ++index) {
    ClientStateNotification* note = new ClientStateNotification();
    notes[index] = note;
    EglRenderingVDAClient* client = new EglRenderingVDAClient(
        &rendering_helper, index,
        note, &data_str, num_NALUs_per_decode,
        suppress_swap_to_display);
    clients[index] = client;
    OmxVideoDecodeAccelerator* decoder =
        new OmxVideoDecodeAccelerator(client, rendering_thread.message_loop());
    decoders[index] = decoder;
    client->SetDecoder(decoder);
    decoder->SetEglState(client->egl_display(), client->egl_context());
    ASSERT_EQ(note->Wait(), CS_DECODER_SET);

    // Configure the decoder.
    int32 config_array[] = {
      media::VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_FOURCC,
      media::VIDEOCODECFOURCC_H264,
      media::VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_WIDTH, kFrameWidth,
      media::VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_HEIGHT, kFrameHeight,
      media::VIDEOATTRIBUTEKEY_VIDEOCOLORFORMAT, media::VIDEOCOLORFORMAT_RGBA,
    };
    std::vector<uint32> config(
        config_array, config_array + arraysize(config_array));
    decoder->GetConfigs(config, &matched_configs);
    CHECK(decoder->Initialize(matched_configs));
  }
  // Then wait for all the decodes to finish.
  for (size_t i = 0; i < num_concurrent_decoders; ++i) {
    ClientStateNotification* note = notes[i];
    ASSERT_EQ(note->Wait(), CS_INITIALIZED);
    // InitializeDone kicks off decoding inside the client, so we just need to
    // wait for Flush.
    ASSERT_EQ(note->Wait(), CS_FLUSHED);
    // FlushDone requests Abort().
    ASSERT_EQ(note->Wait(), CS_ABORTED);
  }
  // Finally assert that decoding went as expected.
  for (size_t i = 0; i < num_concurrent_decoders; ++i) {
    EglRenderingVDAClient* client = clients[i];
    EXPECT_EQ(client->num_decoded_frames(), 25 /* fps */ * 10 /* seconds */);
    // Hard-coded the number of NALUs in the stream.  Icky.
    EXPECT_EQ(client->num_done_bitstream_buffers(),
              ceil(258.0 / num_NALUs_per_decode));
    // These numbers are pulled out of a hat, but seem to be safe with current
    // hardware.
    double min_expected_fps = suppress_swap_to_display ? 175 : 50;
    min_expected_fps /= num_concurrent_decoders;
    LOG(INFO) << "Decoder " << i << " fps: " << client->frames_per_second();
    EXPECT_GT(client->frames_per_second(), min_expected_fps);
  }
  STLDeleteElements(&decoders);
  STLDeleteElements(&clients);
  STLDeleteElements(&notes);

  rendering_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RenderingHelper::UnInitialize,
                 base::Unretained(&rendering_helper),
                 &done));
  done.Wait();
  rendering_thread.Stop();
};

// TODO(fischman): using 16 and higher below breaks decode - visual artifacts
// are introduced as well as spurious frames are delivered (more pictures are
// returned than NALUs are fed to the decoder).  Increase the "15" below when
// http://code.google.com/p/chrome-os-partner/issues/detail?id=4378 is fixed.
INSTANTIATE_TEST_CASE_P(
    DecodeVariations, OmxVideoDecodeAcceleratorTest,
    ::testing::Values(
        std::make_pair(1, 1), std::make_pair(1, 3), std::make_pair(2, 1),
        std::make_pair(3, 1), std::make_pair(5, 1), std::make_pair(8, 1),
        std::make_pair(15, 1)));

// TODO(fischman, vrk): add more tests!  In particular:
// - Test life-cycle: Seek/Stop/Pause/Play/RePlay for a single decoder.
// - Test for memory leaks (valgrind)
// - Test alternate configurations
// - Test failure conditions.
// - Test frame size changes mid-stream

}  // namespace
