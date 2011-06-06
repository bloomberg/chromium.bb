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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
#include "content/common/gpu/omx_video_decode_accelerator.h"
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

// Helper for managing X11, EGL, and GLES2 resources.  Because GL state is
// thread-specific, all the methods of this class (except for ctor/dtor) CHECK
// for being run on a single thread.
//
// TODO(fischman): consider moving this into media/ if we can de-dup some of the
// code that ends up getting copy/pasted all over the place (esp. the GL setup
// code).
class RenderingHelper {
 public:
  explicit RenderingHelper();
  ~RenderingHelper();

  // Initialize all structures to prepare to render to a window of the specified
  // dimensions.  CHECK-fails if any initialization step fails.  After this
  // returns, texture creation and rendering (swaps) can be requested.
  // This method can be called multiple times, in which case all
  // previously-acquired resources and initializations are discarded.
  void Initialize(int width, int height, base::WaitableEvent* done);

  // Undo the effects of Initialize() and signal |*done|.
  void UnInitialize(base::WaitableEvent* done);

  // Return a newly-created GLES2 texture id.
  GLuint CreateTexture();

  // Render |texture_id| to the screen.
  void RenderTexture(GLuint texture_id);

  EGLDisplay egl_display() { return egl_display_; }
  EGLContext egl_context() { return egl_context_; }

 private:
  int width_;
  int height_;
  Display* x_display_;
  Window x_window_;
  EGLDisplay egl_display_;
  EGLContext egl_context_;
  EGLSurface egl_surface_;
  // Since GL carries per-thread state, we ensure all operations are carried out
  // on the same thread by remembering where we were Initialized.
  MessageLoop* message_loop_;
};

RenderingHelper::RenderingHelper() {
  memset(this, 0, sizeof(this));
}

RenderingHelper::~RenderingHelper() {
  CHECK_EQ(width_, 0) << "Must call UnInitialize before dtor.";
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
    int width, int height, base::WaitableEvent* done) {
  // Use width_ != 0 as a proxy for the class having already been
  // Initialize()'d, and UnInitialize() before continuing.
  if (width_) {
    base::WaitableEvent done(false, false);
    UnInitialize(&done);
    done.Wait();
  }

  CHECK_GT(width, 0);
  CHECK_GT(height, 0);
  width_ = width;
  height_ = height;
  message_loop_ = MessageLoop::current();

  // X11 initialization.
  CHECK(x_display_ = XOpenDisplay(NULL));
  int depth = DefaultDepth(x_display_, DefaultScreen(x_display_));
  XSetWindowAttributes window_attributes;
  window_attributes.background_pixel =
      BlackPixel(x_display_, DefaultScreen(x_display_));
  window_attributes.override_redirect = true;
  x_window_ = XCreateWindow(
      x_display_, DefaultRootWindow(x_display_),
      100, 100, /* x/y of top-left corner */
      width_, height_,
      0 /* border width */,
      depth, CopyFromParent /* class */, CopyFromParent /* visual */,
      (CWBackPixel | CWOverrideRedirect), &window_attributes);
  XStoreName(x_display_, x_window_, "OmxVideoDecodeAcceleratorTest");
  XSelectInput(x_display_, x_window_, ExposureMask);
  XMapWindow(x_display_, x_window_);

  // EGL initialization.
  egl_display_ = eglGetDisplay(x_display_);
  EGLint major;
  EGLint minor;
  CHECK(eglInitialize(egl_display_, &major, &minor)) << eglGetError();
  EGLint rgba8888[] = {
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
  egl_surface_ =
      eglCreateWindowSurface(egl_display_, egl_config, x_window_, NULL);
  CHECK_NE(egl_surface_, EGL_NO_SURFACE);
  EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  egl_context_ = eglCreateContext(
      egl_display_, egl_config, EGL_NO_CONTEXT, context_attribs);
  CHECK(eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_))
      << eglGetError();

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
  CreateShader(program, GL_VERTEX_SHADER, kVertexShader, sizeof(kVertexShader));
  CreateShader(program, GL_FRAGMENT_SHADER,
               kFragmentShaderEgl, sizeof(kFragmentShaderEgl));
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
  CHECK(eglMakeCurrent(
      egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
  CHECK(eglDestroyContext(egl_display_, egl_context_));
  CHECK(eglDestroySurface(egl_display_, egl_surface_));
  CHECK(eglTerminate(egl_display_));
  CHECK(XUnmapWindow(x_display_, x_window_));
  CHECK(XDestroyWindow(x_display_, x_window_));
  // Mimic newly-created object.
  memset(this, 0, sizeof(this));
  done->Signal();
}

GLuint RenderingHelper::CreateTexture() {
  CHECK_EQ(MessageLoop::current(), message_loop_);
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  return texture_id;
}

void RenderingHelper::RenderTexture(GLuint texture_id) {
  CHECK_EQ(MessageLoop::current(), message_loop_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  DCHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  eglSwapBuffers(egl_display_, egl_surface_);
  DCHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
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
  EglRenderingVDAClient(ClientStateNotification* note,
                        std::string* encoded_data);
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
  MessageLoop* message_loop() { return thread_.message_loop(); }
  EGLDisplay egl_display() { return rendering_helper_.egl_display(); }
  EGLContext egl_context() { return rendering_helper_.egl_context(); }

 private:
  void SetState(ClientState new_state) {
    note_->Notify(new_state);
    state_ = new_state;
  }

  // Request decode of the next NALU in the encoded data.
  void DecodeNextNALU();

  const std::string* encoded_data_;
  size_t encoded_data_next_pos_to_decode_;
  int next_bitstream_buffer_id_;
  ClientStateNotification* note_;
  VideoDecodeAccelerator* decoder_;
  ClientState state_;
  VideoDecodeAccelerator::Error error_;
  int num_decoded_frames_;
  int num_done_bitstream_buffers_;
  std::map<int, media::GLESBuffer*> picture_buffers_by_id_;
  // Required for Thread to work.  Not used otherwise.
  base::ShadowingAtExitManager at_exit_manager_;
  base::Thread thread_;
  RenderingHelper rendering_helper_;
};

EglRenderingVDAClient::EglRenderingVDAClient(ClientStateNotification* note,
                                             std::string* encoded_data)
    : encoded_data_(encoded_data), encoded_data_next_pos_to_decode_(0),
      next_bitstream_buffer_id_(0), note_(note), decoder_(NULL),
      state_(CS_CREATED),
      error_(VideoDecodeAccelerator::VIDEODECODERERROR_NONE),
      num_decoded_frames_(0), num_done_bitstream_buffers_(0),
      thread_("EglRenderingVDAClientThread") {
  CHECK(thread_.Start());
  base::WaitableEvent done(false, false);
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RenderingHelper::Initialize,
                 base::Unretained(&rendering_helper_),
                 static_cast<int>(kFrameWidth), static_cast<int>(kFrameHeight),
                 &done));
  done.Wait();
}

EglRenderingVDAClient::~EglRenderingVDAClient() {
  base::WaitableEvent done(false, false);
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RenderingHelper::UnInitialize,
                 base::Unretained(&rendering_helper_),
                 &done));
  done.Wait();
  thread_.Stop();
  STLDeleteValues(&picture_buffers_by_id_);
  SetState(CS_DESTROYED);
}

void EglRenderingVDAClient::ProvidePictureBuffers(
    uint32 requested_num_of_buffers,
    const gfx::Size& dimensions,
    VideoDecodeAccelerator::MemoryType type) {
  CHECK_EQ(message_loop(), MessageLoop::current());
  CHECK_EQ(type, VideoDecodeAccelerator::PICTUREBUFFER_MEMORYTYPE_GL_TEXTURE);
  std::vector<media::GLESBuffer> buffers;
  CHECK_EQ(dimensions.width(), kFrameWidth);
  CHECK_EQ(dimensions.height(), kFrameHeight);

  for (uint32 i = 0; i < requested_num_of_buffers; ++i) {
    uint32 id = picture_buffers_by_id_.size();
    GLuint texture_id = rendering_helper_.CreateTexture();
    // TODO(fischman): context_id is always 0.  Can it be removed from the API?
    // (since it's always inferrable from context).
    media::GLESBuffer* buffer =
        new media::GLESBuffer(id, dimensions, texture_id, 0 /* context_id */);
    CHECK(picture_buffers_by_id_.insert(std::make_pair(id, buffer)).second);
    buffers.push_back(*buffer);
  }
  decoder_->AssignGLESBuffers(buffers);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  CHECK_EQ(static_cast<int>(eglGetError()), EGL_SUCCESS);
}

void EglRenderingVDAClient::DismissPictureBuffer(int32 picture_buffer_id) {
  CHECK_EQ(message_loop(), MessageLoop::current());
  delete picture_buffers_by_id_[picture_buffer_id];
  CHECK_EQ(1U, picture_buffers_by_id_.erase(picture_buffer_id));
}

void EglRenderingVDAClient::PictureReady(const media::Picture& picture) {
  CHECK_EQ(message_loop(), MessageLoop::current());

  // Because we feed the decoder one NALU at a time, we can be sure each frame
  // comes from a bitstream buffer numbered at least as high as our current
  // decoded frame's index, and less than the id of the next bitstream buffer
  // we'll send for decoding.  Assert that.
  CHECK_GE(picture.bitstream_buffer_id(), num_decoded_frames_);
  CHECK_LE(picture.bitstream_buffer_id(), next_bitstream_buffer_id_);
  ++num_decoded_frames_;

  media::GLESBuffer* gles_buffer =
      picture_buffers_by_id_[picture.picture_buffer_id()];
  CHECK(gles_buffer);
  rendering_helper_.RenderTexture(gles_buffer->texture_id());

  decoder_->ReusePictureBuffer(picture.picture_buffer_id());
}

void EglRenderingVDAClient::NotifyInitializeDone() {
  CHECK_EQ(message_loop(), MessageLoop::current());
  SetState(CS_INITIALIZED);
  DecodeNextNALU();
}

void EglRenderingVDAClient::NotifyEndOfStream() {
  CHECK_EQ(message_loop(), MessageLoop::current());
  SetState(CS_DONE);
}

void EglRenderingVDAClient::NotifyEndOfBitstreamBuffer(
    int32 bitstream_buffer_id) {
  CHECK_EQ(message_loop(), MessageLoop::current());
  ++num_done_bitstream_buffers_;
  DecodeNextNALU();
}

void EglRenderingVDAClient::NotifyFlushDone() {
  CHECK_EQ(message_loop(), MessageLoop::current());
  SetState(CS_FLUSHED);
}

void EglRenderingVDAClient::NotifyAbortDone() {
  CHECK_EQ(message_loop(), MessageLoop::current());
  SetState(CS_ABORTED);
}

void EglRenderingVDAClient::NotifyError(VideoDecodeAccelerator::Error error) {
  CHECK_EQ(message_loop(), MessageLoop::current());
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

void EglRenderingVDAClient::DecodeNextNALU() {
  if (encoded_data_next_pos_to_decode_ == encoded_data_->size()) {
    decoder_->Flush();
    return;
  }
  size_t start_pos = encoded_data_next_pos_to_decode_;
  CHECK(LookingAtNAL(*encoded_data_, start_pos));
  size_t end_pos = encoded_data_next_pos_to_decode_ + 4;
  while (end_pos + 3 < encoded_data_->size() &&
         !LookingAtNAL(*encoded_data_, end_pos)) {
    ++end_pos;
  }
  if (end_pos + 3 >= encoded_data_->size())
    end_pos = encoded_data_->size();

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
  decoder_->Decode(bitstream_buffer);
  encoded_data_next_pos_to_decode_ = end_pos;
}

// Test the most straightforward case possible: data is decoded from a single
// chunk and rendered to the screen.
TEST(OmxVideoDecodeAcceleratorTest, TestSimpleDecode) {
  logging::SetMinLogLevel(-1);
  ClientStateNotification note;
  ClientState state;
  // Read in the video data and hand it off to the client for later decoding.
  std::string data_str;
  CHECK(file_util::ReadFileToString(FilePath(std::string("test-25fps.h264")),
                                    &data_str));
  EglRenderingVDAClient client(&note, &data_str);
  OmxVideoDecodeAccelerator decoder(&client, client.message_loop());
  client.SetDecoder(&decoder);
  decoder.SetEglState(client.egl_display(), client.egl_context());
  ASSERT_EQ((state = note.Wait()), CS_DECODER_SET);


  // Configure the decoder.
  int32 config_array[] = {
    media::VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_FOURCC,
    media::VIDEOCODECFOURCC_H264,
    media::VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_WIDTH, kFrameWidth,
    media::VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_HEIGHT, kFrameHeight,
    media::VIDEOATTRIBUTEKEY_VIDEOCOLORFORMAT, media::VIDEOCOLORFORMAT_RGBA,
    media::VIDEOATTRIBUTEKEY_TERMINATOR
  };
  std::vector<uint32> config(
      config_array, config_array + arraysize(config_array));
  CHECK(decoder.Initialize(config));
  ASSERT_EQ((state = note.Wait()), CS_INITIALIZED);
  // InitializeDone kicks off decoding inside the client, so we just need to
  // wait for Flush.
  ASSERT_EQ((state = note.Wait()), CS_FLUSHED);

  EXPECT_EQ(client.num_decoded_frames(), 25 /* fps */ * 10 /* seconds */);
  // Hard-coded the number of NALUs in the stream.  Icky.
  EXPECT_EQ(client.num_done_bitstream_buffers(), 258);
}

// TODO(fischman, vrk): add more tests!  In particular:
// - Test that chunking Decode() calls differently works.
// - Test for memory leaks (valgrind)
// - Test decode speed.  Ideally we can beat 60fps esp on simple test.mp4.
// - Test alternate configurations
// - Test failure conditions.
// - Test multiple decodes; sequentially & concurrently.
// - Test frame size changes mid-stream

}  // namespace
