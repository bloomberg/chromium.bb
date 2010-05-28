// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class interacts with OmxCodec and the VideoDecoderImpl
// in the media pipeline.
//
// THREADING SEMANTICS
//
// This class is created by VideoDecoderImpl and lives on the thread
// that VideoDecoderImpl lives. This class is given the message loop
// for the above thread. The OMX callbacks are guaranteed to be
// executed on the hosting message loop. This essentially means that
// all methods in this class are executed on the same thread as
// VideoDecoderImpl. Because of that there's no need for locking anywhere.

#include "media/filters/omx_video_decode_engine.h"

#include <algorithm>
#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "media/base/buffers.h"
#include "media/base/callback.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

OmxVideoDecodeEngine::OmxVideoDecodeEngine()
    : width_(16),
      height_(16),
      message_loop_(NULL),
      input_buffer_count_(0),
      input_buffer_size_(0),
      input_port_(0),
      input_queue_has_eos_(false),
      input_has_fed_eos_(false),
      output_buffer_count_(0),
      output_buffer_size_(0),
      output_port_(0),
      output_eos_(false),
      il_state_(kIlNone),
      expected_il_state_(kIlNone),
      client_state_(kClientNotInitialized),
      component_handle_(NULL),
      output_frames_allocated_(false),
      need_setup_output_port_(false) {
#if ENABLE_EGLIMAGE == 1
  uses_egl_image_ = true;
#else
  uses_egl_image_ = false;
#endif
}

OmxVideoDecodeEngine::~OmxVideoDecodeEngine() {
  DCHECK_EQ(client_state_, kClientNotInitialized);
  DCHECK_EQ(il_state_, kIlNone);
  DCHECK_EQ(0u, input_buffers_.size());
  DCHECK_EQ(0u, output_buffers_.size());
  DCHECK(available_input_buffers_.empty());
  DCHECK(pending_input_queue_.empty());
  DCHECK(output_frames_.empty());
  DCHECK(available_output_frames_.empty());
  DCHECK(output_frames_ready_.empty());
}

#if !defined(COMPILER_MSVC)
int const OmxVideoDecodeEngine::kEosBuffer;
#endif

template <typename T>
static void ResetParamHeader(const OmxVideoDecodeEngine& dec, T* param) {
  memset(param, 0, sizeof(T));
  param->nVersion.nVersion = dec.current_omx_spec_version();
  param->nSize = sizeof(T);
}

void OmxVideoDecodeEngine::Initialize(
    MessageLoop* message_loop,
    AVStream* av_stream,
    EmptyThisBufferCallback* empty_buffer_callback,
    FillThisBufferCallback* fill_buffer_callback,
    Task* done_cb) {
  message_loop_ = message_loop;
  fill_this_buffer_callback_.reset(fill_buffer_callback);
  empty_this_buffer_callback_.reset(empty_buffer_callback);

  AutoTaskRunner done_runner(done_cb);

  width_ = av_stream->codec->width;
  height_ = av_stream->codec->height;

  // TODO(wjia): Find the right way to determine the codec type.
  OmxConfigurator::MediaFormat input_format, output_format;
  memset(&input_format, 0, sizeof(input_format));
  memset(&output_format, 0, sizeof(output_format));
  input_format.codec = OmxConfigurator::kCodecH264;
  output_format.codec = OmxConfigurator::kCodecRaw;
  configurator_.reset(
      new OmxDecoderConfigurator(input_format, output_format));

  message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &OmxVideoDecodeEngine::InitializeTask));
  client_state_ = kClientInitializing;
}

// This method handles only input buffer, without coupling with output
void OmxVideoDecodeEngine::EmptyThisBuffer(scoped_refptr<Buffer> buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanAcceptInput()) {
    OnFeedDone(buffer);
    return;
  }

  if (buffer->IsEndOfStream()) {
      input_queue_has_eos_ = true;
  }

  // Queue this input buffer.
  pending_input_queue_.push(buffer);

  // Try to feed buffers into the decoder.
  EmptyBufferTask();
}

void OmxVideoDecodeEngine::Flush(Task* done_cb) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  AutoTaskRunner done_cb_runner(done_cb);

  // TODO(wjia): to flush component
}

VideoFrame::Format OmxVideoDecodeEngine::GetSurfaceFormat() const {
  return VideoFrame::YV12;
}

VideoDecodeEngine::State OmxVideoDecodeEngine::state() const {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  switch (client_state_) {
    case kClientNotInitialized:
      return kCreated;
      break;
    case kClientInitializing:
    case kClientRunning:
    case kClientPausing:
    case kClientFlushing:
      return kNormal;
      break;
    case kClientStopping:
      return kStopped;
      break;
    case kClientError:
      return kError;
      break;
  }
  return kError;
}

void OmxVideoDecodeEngine::Stop(Callback0::Type* done_cb) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &OmxVideoDecodeEngine::StopTask, done_cb));

  // TODO(wjia): When FillThisBuffer() is added, engine state should be
  // kStopping here. engine state should be set to kStopped in OnStopDone();
  // client_state_ = kClientStopping;
}

void OmxVideoDecodeEngine::OnFormatChange(
    const OmxConfigurator::MediaFormat& input_format,
    const OmxConfigurator::MediaFormat& output_format) {
  // TODO(jiesun): We should not need this for here, because width and height
  // are already known from upper layer of the stack.
}

void OmxVideoDecodeEngine::OnFeedDone(scoped_refptr<Buffer> buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  empty_this_buffer_callback_->Run(buffer);
}

void OmxVideoDecodeEngine::OnReadComplete(OMX_BUFFERHEADERTYPE* buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // EOF
  if (!buffer) {
    scoped_refptr<VideoFrame> frame = NULL;
    fill_this_buffer_callback_->Run(frame);
    return;
  }

  scoped_refptr<VideoFrame> frame;
  if (uses_egl_image_) {
    frame =
        *static_cast<scoped_refptr<VideoFrame>* >(buffer->pAppPrivate);
    fill_this_buffer_callback_->Run(frame);
    return;
  }

  VideoFrame::CreateFrame(GetSurfaceFormat(),
                          width_, height_,
                          StreamSample::kInvalidTimestamp,
                          StreamSample::kInvalidTimestamp,
                          &frame);
  if (!frame.get()) {
    // TODO(jiesun): this is also an error case handled as normal.
    return;
  }

  // TODO(jiesun): Assume YUV 420 format.
  const int pixels = width_ * height_;
  memcpy(frame->data(VideoFrame::kYPlane), buffer->pBuffer, pixels);
  memcpy(frame->data(VideoFrame::kUPlane), buffer->pBuffer + pixels,
         pixels / 4);
  memcpy(frame->data(VideoFrame::kVPlane),
         buffer->pBuffer + pixels + pixels /4,
         pixels / 4);

  fill_this_buffer_callback_->Run(frame);
}

void OmxVideoDecodeEngine::OnStopDone() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!stop_callback_.get())
    return;
  stop_callback_->Run();
  stop_callback_.reset();
}

// Function sequence for initializing
void OmxVideoDecodeEngine::InitializeTask() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(client_state_, kClientInitializing);
  DCHECK_EQ(il_state_, kIlNone);

  il_state_ = kIlNone;
  expected_il_state_ = kIlLoaded;
  input_port_enabled_ = true;
  output_port_state_ = kPortEnabled;
  if (!CreateComponent()) {
    StopOnError();
    return;
  }
  il_state_ = kIlLoaded;

  // TODO(wjia): Disabling output port is to work around racing condition
  // due to bug in some vendor's driver. But it hits another bug.
  // So temporarily fall back to enabling output port. Still keep the code
  // disabling output port here.
  // No need to respond to this PortDisable event
  // OnPortDisableEventFunc = NULL;
  // ChangePort(OMX_CommandPortDisable, output_port_);
  // if (kClientError == client_state_) {
  //   StopOnError();
  //   return;
  // }
  // output_port_state_ = kPortDisabled;

  // Transition component to Idle state
  OnStateSetEventFunc = &OmxVideoDecodeEngine::DoneSetStateIdle;
  if (!TransitionToState(OMX_StateIdle)) {
    StopOnError();
    return;
  }
  expected_il_state_ = kIlIdle;

  if (!AllocateInputBuffers()) {
    LOG(ERROR) << "OMX_AllocateBuffer() Input buffer error";
    client_state_ = kClientError;
    StopOnError();
    return;
  }
  if (!AllocateOutputBuffers()) {
    LOG(ERROR) << "OMX_AllocateBuffer() Output buffer error";
    client_state_ = kClientError;
    return;
  }
}

// Sequence of actions in this transition:
//
// 1. Initialize OMX (To be removed.)
// 2. Map role name to component name.
// 3. Get handle of the OMX component
// 4. Get the port information.
// 5. Set role for the component.
// 6. Input/output ports media format configuration.
// 7. Obtain the information about the input port.
// 8. Obtain the information about the output port.
bool OmxVideoDecodeEngine::CreateComponent() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  static OMX_CALLBACKTYPE callback = {
    &OmxVideoDecodeEngine::EventHandler,
    &OmxVideoDecodeEngine::EmptyBufferCallback,
    &OmxVideoDecodeEngine::FillBufferCallback
  };

  // 1. Initialize the OpenMAX Core.
  // TODO(hclam): move this out.
  OMX_ERRORTYPE omxresult = OMX_Init();
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "Failed to init OpenMAX core";
    client_state_ = kClientError;
    return false;
  }

  // 2. Map role name to component name.
  std::string role_name = configurator_->GetRoleName();
  OMX_U32 roles = 0;
  omxresult = OMX_GetComponentsOfRole(
      const_cast<OMX_STRING>(role_name.c_str()),
      &roles, 0);
  if (omxresult != OMX_ErrorNone || roles == 0) {
    LOG(ERROR) << "Unsupported Role: " << role_name.c_str();
    client_state_ = kClientError;
    return false;
  }
  const OMX_U32 kMaxRolePerComponent = 20;
  CHECK(roles < kMaxRolePerComponent);

  OMX_U8** component_names = new OMX_U8*[roles];
  const int kMaxComponentNameLength = 256;
  for (size_t i = 0; i < roles; ++i)
    component_names[i] = new OMX_U8[kMaxComponentNameLength];

  omxresult = OMX_GetComponentsOfRole(
      const_cast<OMX_STRING>(role_name.c_str()),
      &roles, component_names);

  // Use first component only. Copy the name of the first component
  // so that we could free the memory.
  std::string component_name;
  if (omxresult == OMX_ErrorNone)
    component_name = reinterpret_cast<char*>(component_names[0]);

  for (size_t i = 0; i < roles; ++i)
    delete [] component_names[i];
  delete [] component_names;

  if (omxresult != OMX_ErrorNone || roles == 0) {
    LOG(ERROR) << "Unsupported Role: " << role_name.c_str();
    client_state_ = kClientError;
    return false;
  }

  // 3. Get the handle to the component. After OMX_GetHandle(),
  //    the component is in loaded state.
  OMX_STRING component = const_cast<OMX_STRING>(component_name.c_str());
  omxresult = OMX_GetHandle(&component_handle_, component, this, &callback);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "Failed to Load the component: " << component;
    client_state_ = kClientError;
    return false;
  }

  // 4. Get the port information. This will obtain information about the
  //    number of ports and index of the first port.
  OMX_PORT_PARAM_TYPE port_param;
  ResetParamHeader(*this, &port_param);
  omxresult = OMX_GetParameter(component_handle_, OMX_IndexParamVideoInit,
                               &port_param);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "Failed to get Port Param";
    client_state_ = kClientError;
    return false;
  }
  input_port_ = port_param.nStartPortNumber;
  output_port_ = input_port_ + 1;

  // 5. Set role for the component because our component could
  //    have multiple roles.
  OMX_PARAM_COMPONENTROLETYPE role_type;
  ResetParamHeader(*this, &role_type);
  base::strlcpy(reinterpret_cast<char*>(role_type.cRole),
                role_name.c_str(),
                OMX_MAX_STRINGNAME_SIZE);
  role_type.cRole[OMX_MAX_STRINGNAME_SIZE - 1] = '\0';
  omxresult = OMX_SetParameter(component_handle_,
                               OMX_IndexParamStandardComponentRole,
                               &role_type);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "Failed to Set Role";
    client_state_ = kClientError;
    return false;
  }

  // 6. Input/output ports media format configuration.
  if (!ConfigureIOPorts()) {
    LOG(ERROR) << "Media format configurations failed";
    client_state_ = kClientError;
    return false;
  }

  // 7. Obtain the information about the input port.
  // This will have the new mini buffer count in |port_format.nBufferCountMin|.
  // Save this value to input_buf_count.
  OMX_PARAM_PORTDEFINITIONTYPE port_format;
  ResetParamHeader(*this, &port_format);
  port_format.nPortIndex = input_port_;
  omxresult = OMX_GetParameter(component_handle_,
                               OMX_IndexParamPortDefinition,
                               &port_format);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamPortDefinition) failed";
    client_state_ = kClientError;
    return false;
  }
  if (OMX_DirInput != port_format.eDir) {
    LOG(ERROR) << "Expected input port";
    client_state_ = kClientError;
    return false;
  }
  input_buffer_count_ = port_format.nBufferCountMin;
  input_buffer_size_ = port_format.nBufferSize;

  // 8. Obtain the information about the output port.
  ResetParamHeader(*this, &port_format);
  port_format.nPortIndex = output_port_;
  omxresult = OMX_GetParameter(component_handle_,
                               OMX_IndexParamPortDefinition,
                               &port_format);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamPortDefinition) failed";
    client_state_ = kClientError;
    return false;
  }
  if (OMX_DirOutput != port_format.eDir) {
    LOG(ERROR) << "Expect Output Port";
    client_state_ = kClientError;
    return false;
  }

  // override buffer count when EGLImage is used
  if (uses_egl_image_) {
    port_format.nBufferCountActual = port_format.nBufferCountMin =
       output_buffer_count_ = 3;

    omxresult = OMX_SetParameter(component_handle_,
                                 OMX_IndexParamPortDefinition,
                                 &port_format);
    if (omxresult != OMX_ErrorNone) {
      LOG(ERROR) << "SetParameter(OMX_IndexParamPortDefinition) failed";
      client_state_ = kClientError;
      return false;
    }
  } else {
    output_buffer_count_ = port_format.nBufferCountActual;
  }
  output_buffer_size_ = port_format.nBufferSize;

  return true;
}

// Event callback during initialization to handle DoneStateSet to idle
void OmxVideoDecodeEngine::DoneSetStateIdle(OMX_STATETYPE state) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(client_state_, kClientInitializing);
  DCHECK_EQ(OMX_StateIdle, state);

  il_state_ = kIlIdle;
  OnStateSetEventFunc = &OmxVideoDecodeEngine::DoneSetStateExecuting;
  if (!TransitionToState(OMX_StateExecuting)) {
    StopOnError();
    return;
  }
  expected_il_state_ = kIlExecuting;
}

// Event callback during initialization to handle DoneStateSet to executing
void OmxVideoDecodeEngine::DoneSetStateExecuting(OMX_STATETYPE state) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(client_state_, kClientInitializing);
  DCHECK_EQ(OMX_StateExecuting, state);

  il_state_ = kIlExecuting;
  client_state_ = kClientRunning;
  OnStateSetEventFunc = NULL;
  // TODO(wjia): The engine should be the driving force to read input buffers.
  // After video decoder decouples input and output handling, this initial
  // filling should be changed to call empty_buffer_callback with NULL to pull
  // bitstream from decoder.
  EmptyBufferTask();
  InitialFillBuffer();
  if (kClientError == client_state_) {
    StopOnError();
    return;
  }
}

// Function for receiving output buffers. Hookup for buffer recycling
// and outside allocator.
void OmxVideoDecodeEngine::OnReceiveOneOutputBuffer(
    scoped_refptr<VideoFrame> video_frame) {
  OMX_BUFFERHEADERTYPE* omx_buffer = FindOmxBuffer(video_frame);
  if (omx_buffer) {
    if (kClientRunning == client_state_) {
      SendOutputBufferToComponent(omx_buffer);
    } else {
      available_output_frames_.push(omx_buffer);
    }
  } else {
    DCHECK(!output_frames_allocated_);
    output_frames_.push_back(std::make_pair(video_frame,
        static_cast<OMX_BUFFERHEADERTYPE*>(NULL)));
  }

  DCHECK(static_cast<int>(output_frames_.size()) <= output_buffer_count_);

  if ((!output_frames_allocated_) &&
      static_cast<int>(output_frames_.size()) == output_buffer_count_) {
    output_frames_allocated_ = true;

    if (need_setup_output_port_)
      SetupOutputPort();
  }

  if (kClientError == client_state_) {
    StopOnError();
    return;
  }
}

// Reconfigure port
void OmxVideoDecodeEngine::OnPortSettingsChangedRun(OMX_INDEXTYPE index,
                                                    int port) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(client_state_, kClientRunning);
  DCHECK_EQ(port, output_port_);

  if (index != OMX_IndexParamPortDefinition)
    return;

  OMX_PARAM_PORTDEFINITIONTYPE port_format;
  ResetParamHeader(*this, &port_format);
  port_format.nPortIndex = output_port_;
  OMX_ERRORTYPE omxresult;
  omxresult = OMX_GetParameter(component_handle_,
                               OMX_IndexParamPortDefinition,
                               &port_format);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamPortDefinition) failed";
    client_state_ = kClientError;
    StopOnError();
    return;
  }
  if (OMX_DirOutput != port_format.eDir) {
    LOG(ERROR) << "Expected Output Port";
    client_state_ = kClientError;
    StopOnError();
    return;
  }

  // Update the output format.
  OmxConfigurator::MediaFormat output_format;
  output_format.video_header.height = port_format.format.video.nFrameHeight;
  output_format.video_header.width = port_format.format.video.nFrameWidth;
  output_format.video_header.stride = port_format.format.video.nStride;
  // OnFormatChange(input_format, output_format);
  output_buffer_count_ = port_format.nBufferCountActual;
  output_buffer_size_ = port_format.nBufferSize;

  if (kPortEnabled == output_port_state_) {
    output_port_state_ = kPortDisabling;
    OnPortDisableEventFunc = &OmxVideoDecodeEngine::OnPortDisableEventRun;
    ChangePort(OMX_CommandPortDisable, output_port_);
    if (kClientError == client_state_) {
      StopOnError();
      return;
    }
    FreeOutputBuffers();
  } else {
    OnPortDisableEventRun(output_port_);
  }
}

// Post output port disabling
void OmxVideoDecodeEngine::OnPortDisableEventRun(int port) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(client_state_, kClientRunning);
  DCHECK_EQ(port, output_port_);

  output_port_state_ = kPortDisabled;

  // make sure all eglimages are available before enabling output port
  if (output_frames_allocated_ || !uses_egl_image_) {
    SetupOutputPort();
    if (kClientError == client_state_) {
      StopOnError();
      return;
    }
  } else {
    need_setup_output_port_ = true;
  }
}

// Enable output port and allocate buffers correspondingly
void OmxVideoDecodeEngine::SetupOutputPort() {
  DCHECK_EQ(client_state_, kClientRunning);

  need_setup_output_port_ = false;

  // Enable output port.
  output_port_state_ = kPortEnabling;
  OnPortEnableEventFunc = &OmxVideoDecodeEngine::OnPortEnableEventRun;
  ChangePort(OMX_CommandPortEnable, output_port_);
  if (kClientError == client_state_) {
    return;
  }

  // TODO(wjia): add state checking
  // Update the ports in buffer if necessary
  if (!AllocateOutputBuffers()) {
    LOG(ERROR) << "OMX_AllocateBuffer() Output buffer error";
    client_state_ = kClientError;
    return;
  }
}

// Post output port enabling
void OmxVideoDecodeEngine::OnPortEnableEventRun(int port) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(client_state_, kClientRunning);
  DCHECK_EQ(port, output_port_);

  output_port_state_ = kPortEnabled;
  OnPortEnableEventFunc = NULL;
  InitialFillBuffer();
  if (kClientError == client_state_) {
    StopOnError();
    return;
  }
}

// Functions for stopping
void OmxVideoDecodeEngine::StopTask(Callback* callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  stop_callback_.reset(callback);

  if (client_state_ == kClientError) {
    OnStopDone();
    return;
  }

  FreeInputQueue();

  // TODO(wjia): add more state checking
  if (kClientRunning == client_state_) {
    client_state_ = kClientStopping;
    DeinitFromExecuting(OMX_StateExecuting);
  }
}

void OmxVideoDecodeEngine::DeinitFromExecuting(OMX_STATETYPE state) {
  DCHECK_EQ(state, OMX_StateExecuting);

  OnStateSetEventFunc = &OmxVideoDecodeEngine::DeinitFromIdle;
  TransitionToState(OMX_StateIdle);
  expected_il_state_ = kIlIdle;
}

void OmxVideoDecodeEngine::DeinitFromIdle(OMX_STATETYPE state) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(state, OMX_StateIdle);

  il_state_ = kIlIdle;
  OnStateSetEventFunc = &OmxVideoDecodeEngine::DeinitFromLoaded;
  TransitionToState(OMX_StateLoaded);
  expected_il_state_ = kIlLoaded;

  FreeInputBuffers();
  FreeOutputBuffers();
}

void OmxVideoDecodeEngine::DeinitFromLoaded(OMX_STATETYPE state) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(state, OMX_StateLoaded);

  il_state_ = kIlLoaded;
  if (component_handle_) {
    OMX_ERRORTYPE result = OMX_FreeHandle(component_handle_);
    if (result != OMX_ErrorNone)
      LOG(ERROR) << "OMX_FreeHandle() error. Error code: " << result;
    component_handle_ = NULL;
  }
  il_state_ = expected_il_state_ = kIlNone;
  client_state_ = kClientNotInitialized;

  OMX_Deinit();

  OnStopDone();
}

void OmxVideoDecodeEngine::StopOnError() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  client_state_ = kClientStopping;
  FreeInputQueue();

  if (kIlExecuting == expected_il_state_) {
    DeinitFromExecuting(OMX_StateExecuting);
  } else if (kIlIdle == expected_il_state_) {
    DeinitFromIdle(OMX_StateIdle);
  } else if (kIlLoaded == expected_il_state_) {
    DeinitFromLoaded(OMX_StateLoaded);
  }
}

// Call OMX_UseBuffer() to avoid buffer copying when
// OMX_EmptyThisBuffer() is called
bool OmxVideoDecodeEngine::AllocateInputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  uint8* data = new uint8[input_buffer_size_];
  scoped_array<uint8> data_deleter(data);

  for (int i = 0; i < input_buffer_count_; ++i) {
    OMX_BUFFERHEADERTYPE* buffer;
    OMX_ERRORTYPE error =
        OMX_UseBuffer(component_handle_, &buffer, input_port_,
                      this, input_buffer_size_, data);
    if (error != OMX_ErrorNone)
      return false;
    buffer->nInputPortIndex = input_port_;
    buffer->nOffset = 0;
    buffer->nFlags = 0;
    input_buffers_.push_back(buffer);
    available_input_buffers_.push(buffer);
  }
  return true;
}

// This method handles EGLImage and internal buffer cases. Any external
// allocation case is similar to EGLImage
bool OmxVideoDecodeEngine::AllocateOutputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  // DCHECK_EQ(output_buffer_count_, static_cast<int>(output_frames_.size()));

  for (int i = 0; i < output_buffer_count_; ++i) {
    OMX_BUFFERHEADERTYPE* buffer;
    OMX_ERRORTYPE error;
    if (uses_egl_image_) {
      OutputFrame output_frame = output_frames_[i];
      scoped_refptr<VideoFrame> video_frame = output_frame.first;
      DCHECK(!output_frame.second);
      error = OMX_UseEGLImage(component_handle_, &buffer, output_port_,
                              &video_frame, video_frame->private_buffer());
      if (error != OMX_ErrorNone)
        return false;
      output_frames_[i].second = buffer;
      available_output_frames_.push(buffer);
    } else {
      error =
        OMX_AllocateBuffer(component_handle_, &buffer, output_port_,
                           NULL, output_buffer_size_);
      if (error != OMX_ErrorNone)
        return false;
      output_buffers_.push_back(buffer);
    }
  }

  return true;
}

void OmxVideoDecodeEngine::FreeInputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Calls to OMX to free buffers.
  for (size_t i = 0; i < input_buffers_.size(); ++i)
    OMX_FreeBuffer(component_handle_, input_port_, input_buffers_[i]);
  input_buffers_.clear();

  // Empty available buffer queue.
  while (!available_input_buffers_.empty()) {
    available_input_buffers_.pop();
  }
}

void OmxVideoDecodeEngine::FreeOutputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Calls to OMX to free buffers.
  if (uses_egl_image_) {
    for (size_t i = 0; i < output_frames_.size(); ++i) {
      OMX_BUFFERHEADERTYPE* omx_buffer = output_frames_[i].second;
      if (!omx_buffer)
        OMX_FreeBuffer(component_handle_, output_port_, omx_buffer);
    }
    output_frames_.clear();
  } else {
    for (size_t i = 0; i < output_buffers_.size(); ++i)
      OMX_FreeBuffer(component_handle_, output_port_, output_buffers_[i]);
    output_buffers_.clear();
  }
}

void OmxVideoDecodeEngine::FreeInputQueue() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  while (!pending_input_queue_.empty()) {
    scoped_refptr<Buffer> buffer = pending_input_queue_.front();
    OnFeedDone(buffer);
    pending_input_queue_.pop();
  }
}

bool OmxVideoDecodeEngine::ConfigureIOPorts() {
  OMX_PARAM_PORTDEFINITIONTYPE input_port_def, output_port_def;
  OMX_ERRORTYPE omxresult = OMX_ErrorNone;
  // Get default input port definition.
  ResetParamHeader(*this, &input_port_def);
  input_port_def.nPortIndex = input_port_;
  omxresult = OMX_GetParameter(component_handle_,
                               OMX_IndexParamPortDefinition,
                               &input_port_def);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamPortDefinition) "
               << "for input port failed";
    return false;
  }
  if (OMX_DirInput != input_port_def.eDir) {
    LOG(ERROR) << "Expected Input Port";
    return false;
  }

  // Get default output port definition.
  ResetParamHeader(*this, &output_port_def);
  output_port_def.nPortIndex = output_port_;
  omxresult = OMX_GetParameter(component_handle_,
                               OMX_IndexParamPortDefinition,
                               &output_port_def);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamPortDefinition) "
               << "for output port failed";
    return false;
  }
  if (OMX_DirOutput != output_port_def.eDir) {
    LOG(ERROR) << "Expected Output Port";
    return false;
  }

  return configurator_->ConfigureIOPorts(
      static_cast<OMX_COMPONENTTYPE*>(component_handle_),
      &input_port_def, &output_port_def);
}

bool OmxVideoDecodeEngine::CanEmptyBuffer() {
  // We can call empty buffer while we are in executing and EOS has
  // not been sent
  return (il_state_ == kIlExecuting &&
          !input_has_fed_eos_ &&
          input_port_enabled_);
}

bool OmxVideoDecodeEngine::CanFillBuffer() {
  // Make sure component is in the executing state and end-of-stream
  // has not been reached.
  return (il_state_ == kIlExecuting &&
          !output_eos_ &&
          (output_port_state_ == kPortEnabled ||
           output_port_state_ == kPortEnabling));
}

bool OmxVideoDecodeEngine::CanAcceptInput() {
  // We can't take input buffer when in error state.
  return (client_state_ != kClientError &&
          !input_queue_has_eos_ &&
          kClientStopping != client_state_);
}

bool OmxVideoDecodeEngine::CanAcceptOutput() {
  // TODO(hclam): Reject when in stopped state.
  return client_state_ != kClientError && output_port_state_ == kPortEnabled;
}

// TODO(wjia): There are several things need to be done here:
// 1. Merge this method into EmptyThisBuffer();
// 2. Get rid of the while loop, this is not needed because when we call
// OMX_EmptyThisBuffer we assume we *always* have an input buffer.
void OmxVideoDecodeEngine::EmptyBufferTask() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanEmptyBuffer())
    return;

  // Loop for all available input data and input buffer for the
  // decoder. When input has reached EOS  we need to stop.
  while (!pending_input_queue_.empty() &&
         !available_input_buffers_.empty() &&
         !input_has_fed_eos_) {
    scoped_refptr<Buffer> buffer = pending_input_queue_.front();
    pending_input_queue_.pop();
    buffer->AddRef();

    OMX_BUFFERHEADERTYPE* omx_buffer = available_input_buffers_.front();
    available_input_buffers_.pop();

    input_has_fed_eos_ = buffer->IsEndOfStream();

    // setup |omx_buffer|.
    omx_buffer->pBuffer = const_cast<OMX_U8*>(buffer.get()->GetData());
    omx_buffer->nFilledLen = buffer.get()->GetDataSize();
    omx_buffer->nAllocLen = omx_buffer->nFilledLen;
    omx_buffer->nFlags |= input_has_fed_eos_ ? OMX_BUFFERFLAG_EOS : 0;
    omx_buffer->nTimeStamp = buffer->GetTimestamp().InMilliseconds();
    omx_buffer->pAppPrivate = buffer.get();

    // Give this buffer to OMX.
    OMX_ERRORTYPE ret = OMX_EmptyThisBuffer(component_handle_, omx_buffer);
    if (ret != OMX_ErrorNone) {
      LOG(ERROR) << "OMX_EmptyThisBuffer() failed with result " << ret;
      client_state_ = kClientError;
      return;
    }
  }
}

void OmxVideoDecodeEngine::FulfillOneRead() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!uses_egl_image_ && !output_buffers_ready_.empty()) {
    int buffer_id = output_buffers_ready_.front();
    output_buffers_ready_.pop();

    // If the buffer is real then send it to downstream.
    // Otherwise if it is an end-of-stream buffer then just drop it.
    if (buffer_id != kEosBuffer) {
      OnReadComplete(output_buffers_[buffer_id]);
      SendOutputBufferToComponent(output_buffers_[buffer_id]);
    } else {
      OnReadComplete(static_cast<OMX_BUFFERHEADERTYPE*>(NULL));
    }
  } else if (uses_egl_image_ && !output_frames_ready_.empty()) {
    OMX_BUFFERHEADERTYPE *buffer = output_frames_ready_.front();
    output_frames_ready_.pop();

    // If the buffer is real then send it to downstream.
    // Otherwise if it is an end-of-stream buffer then just drop it.
    if (buffer->nFlags & OMX_BUFFERFLAG_EOS) {
      OnReadComplete(static_cast<OMX_BUFFERHEADERTYPE*>(NULL));
    } else {
      OnReadComplete(buffer);
    }
  }
}

void OmxVideoDecodeEngine::InitialFillBuffer() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  // DCHECK(output_frames_allocated_);

  if (!CanFillBuffer())
    return;

  // Ask the decoder to fill the output buffers.
  if (uses_egl_image_) {
    for (size_t i = 0; i < available_output_frames_.size(); ++i) {
      OMX_BUFFERHEADERTYPE* omx_buffer = available_output_frames_.front();
      available_output_frames_.pop();
      SendOutputBufferToComponent(omx_buffer);
    }
  } else {
    for (size_t i = 0; i < output_buffers_.size(); ++i) {
      OMX_BUFFERHEADERTYPE* omx_buffer = output_buffers_[i];
      omx_buffer->nOutputPortIndex = output_port_;
      // Need to clear the EOS flag.
      omx_buffer->nFlags &= ~OMX_BUFFERFLAG_EOS;
      omx_buffer->pAppPrivate = this;
      OMX_ERRORTYPE ret = OMX_FillThisBuffer(component_handle_, omx_buffer);

      if (OMX_ErrorNone != ret) {
        LOG(ERROR) << "OMX_FillThisBuffer() failed with result " << ret;
        client_state_ = kClientError;
        return;
      }
    }
  }
}

// helper functions
// Send command to disable/enable port.
void OmxVideoDecodeEngine::ChangePort(OMX_COMMANDTYPE cmd, int port_index) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  OMX_ERRORTYPE omxresult = OMX_SendCommand(component_handle_,
                                            cmd, port_index, 0);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SendCommand(OMX_CommandPortDisable) failed";
    client_state_ = kClientError;
    return;
  }
}

// Find if omx_buffer exists corresponding to video_frame
OMX_BUFFERHEADERTYPE* OmxVideoDecodeEngine::FindOmxBuffer(
    scoped_refptr<VideoFrame> video_frame) {
  for (size_t i = 0; i < output_frames_.size(); ++i) {
    scoped_refptr<VideoFrame> frame = output_frames_[i].first;
    if (video_frame->private_buffer() == frame->private_buffer())
      return output_frames_[i].second;
  }
  return NULL;
}

OMX_STATETYPE OmxVideoDecodeEngine::GetComponentState() {
  OMX_STATETYPE eState;
  OMX_ERRORTYPE eError;

  eError = OMX_GetState(component_handle_, &eState);
  if (OMX_ErrorNone != eError) {
    LOG(ERROR) << "OMX_GetState failed";
    StopOnError();
  }

  return eState;
}

// send one output buffer to component
void OmxVideoDecodeEngine::SendOutputBufferToComponent(
    OMX_BUFFERHEADERTYPE *omx_buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanFillBuffer())
    return;

  // clear EOS flag.
  omx_buffer->nFlags &= ~OMX_BUFFERFLAG_EOS;
  OMX_ERRORTYPE ret = OMX_FillThisBuffer(component_handle_, omx_buffer);

  if (OMX_ErrorNone != ret) {
    LOG(ERROR) << "OMX_FillThisBuffer() failed with result " << ret;
    client_state_ = kClientError;
    return;
  }
}

// Send state transition command to component.
bool OmxVideoDecodeEngine::TransitionToState(OMX_STATETYPE new_state) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  OMX_ERRORTYPE omxresult = OMX_SendCommand(component_handle_,
                                            OMX_CommandStateSet,
                                            new_state, 0);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SendCommand(OMX_CommandStateSet) failed";
    client_state_ = kClientError;
    return false;
  }

  return true;
}

void OmxVideoDecodeEngine::EmptyBufferDoneTask(OMX_BUFFERHEADERTYPE* buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  Buffer* stored_buffer = static_cast<Buffer*>(buffer->pAppPrivate);
  OnFeedDone(stored_buffer);
  stored_buffer->Release();

  // Enqueue the available buffer beacuse the decoder has consumed it.
  available_input_buffers_.push(buffer);

  // Try to feed more data into the decoder.
  EmptyBufferTask();
}

void OmxVideoDecodeEngine::FillBufferDoneTask(OMX_BUFFERHEADERTYPE* buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanAcceptOutput())
    return;

  // This buffer is received with decoded frame. Enqueue it and make it
  // ready to be consumed by reads.

  if (uses_egl_image_) {
    if (buffer->nFlags & OMX_BUFFERFLAG_EOS) {
      output_eos_ = true;
    }
    output_frames_ready_.push(buffer);
  } else {
    int buffer_id = kEosBuffer;
    for (size_t i = 0; output_buffers_.size(); ++i) {
      if (output_buffers_[i] == buffer) {
        buffer_id = i;
        break;
      }
    }

    DCHECK_NE(buffer_id, kEosBuffer);

    // Determine if the buffer received is a end-of-stream buffer. If
    // the condition is true then assign a EOS id to the buffer.
    if (buffer->nFlags & OMX_BUFFERFLAG_EOS || !buffer->nFilledLen) {
      buffer_id = kEosBuffer;
      output_eos_ = true;
    }
    output_buffers_ready_.push(buffer_id);
  }

  // Try to fulfill one read request.
  FulfillOneRead();
}

void OmxVideoDecodeEngine::EventHandlerCompleteTask(OMX_EVENTTYPE event,
                                                    OMX_U32 data1,
                                                    OMX_U32 data2) {
  switch (event) {
    case OMX_EventCmdComplete: {
      // If the last command was successful, we have completed
      // a state transition. So notify that we have done it
      // accordingly.
      OMX_COMMANDTYPE cmd = static_cast<OMX_COMMANDTYPE>(data1);
      if (cmd == OMX_CommandPortDisable) {
        if (OnPortDisableEventFunc)
          (this->*OnPortDisableEventFunc)(static_cast<int>(data2));
      } else if (cmd == OMX_CommandPortEnable) {
        if (OnPortEnableEventFunc)
          (this->*OnPortEnableEventFunc)(static_cast<int>(data2));
      } else if (cmd == OMX_CommandStateSet) {
        (this->*OnStateSetEventFunc)(static_cast<OMX_STATETYPE>(data2));
      } else if (cmd == OMX_CommandFlush) {
        (this->*OnFlushEventFunc)(data2);
      } else {
        LOG(ERROR) << "Unknown command completed\n";
      }
      break;
    }
    case OMX_EventError:
      if (OMX_ErrorInvalidState == (OMX_ERRORTYPE)data1) {
        // TODO(hclam): what to do here?
      }
      StopOnError();
      break;
    case OMX_EventPortSettingsChanged:
        OnPortSettingsChangedRun(static_cast<OMX_INDEXTYPE>(data1),
                                 static_cast<int>(data2));
      break;
    default:
      LOG(ERROR) << "Warning - Unknown event received\n";
      break;
  }
}

// static
OMX_ERRORTYPE OmxVideoDecodeEngine::EventHandler(OMX_HANDLETYPE component,
                                     OMX_PTR priv_data,
                                     OMX_EVENTTYPE event,
                                     OMX_U32 data1,
                                     OMX_U32 data2,
                                     OMX_PTR event_data) {
  OmxVideoDecodeEngine* decoder = static_cast<OmxVideoDecodeEngine*>(priv_data);
  DCHECK_EQ(component, decoder->component_handle_);
  decoder->message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(decoder,
                        &OmxVideoDecodeEngine::EventHandlerCompleteTask,
                        event, data1, data2));
  return OMX_ErrorNone;
}

// static
OMX_ERRORTYPE OmxVideoDecodeEngine::EmptyBufferCallback(
    OMX_HANDLETYPE component,
    OMX_PTR priv_data,
    OMX_BUFFERHEADERTYPE* buffer) {
  OmxVideoDecodeEngine* decoder = static_cast<OmxVideoDecodeEngine*>(priv_data);
  DCHECK_EQ(component, decoder->component_handle_);
  decoder->message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(decoder,
                        &OmxVideoDecodeEngine::EmptyBufferDoneTask, buffer));
  return OMX_ErrorNone;
}

// static
OMX_ERRORTYPE OmxVideoDecodeEngine::FillBufferCallback(
    OMX_HANDLETYPE component,
    OMX_PTR priv_data,
    OMX_BUFFERHEADERTYPE* buffer) {
  OmxVideoDecodeEngine* decoder = static_cast<OmxVideoDecodeEngine*>(priv_data);
  DCHECK_EQ(component, decoder->component_handle_);
  decoder->message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(decoder,
                        &OmxVideoDecodeEngine::FillBufferDoneTask, buffer));
  return OMX_ErrorNone;
}

}  // namespace media
