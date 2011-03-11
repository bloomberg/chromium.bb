// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class interacts with OmxCodec and the VideoDecoderImpl
// in the media pipeline.
//
// THREADING SEMANTICS
//
// This class is created by OmxVideoDecoder and lives on the thread
// that it lives. This class is given the message loop
// for the above thread. The OMX callbacks are guaranteed to be
// executed on the hosting message loop. Because of that there's no need
// for locking anywhere.

#include "media/video/omx_video_decode_engine.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "media/base/buffers.h"
#include "media/base/pipeline.h"

namespace media {

OmxVideoDecodeEngine::OmxVideoDecodeEngine()
    : width_(16),
      height_(16),
      message_loop_(NULL),
      input_buffer_count_(0),
      input_buffer_size_(0),
      input_port_(0),
      input_buffers_at_component_(0),
      input_pending_request_(0),
      input_queue_has_eos_(false),
      input_has_fed_eos_(false),
      input_port_flushed_(false),
      output_buffer_count_(0),
      output_buffer_size_(0),
      output_port_(0),
      output_buffers_at_component_(0),
      output_pending_request_(0),
      output_eos_(false),
      output_port_flushed_(false),
      il_state_(kIlNone),
      expected_il_state_(kIlNone),
      client_state_(kClientNotInitialized),
      component_handle_(NULL),
      need_free_input_buffers_(false),
      need_free_output_buffers_(false),
      flush_pending_(false),
      output_frames_allocated_(false),
      need_setup_output_port_(false) {
  // TODO(wjia): change uses_egl_image_ to runtime setup
#if ENABLE_EGLIMAGE == 1
  uses_egl_image_ = true;
  DLOG(INFO) << "Uses egl image for output";
#else
  uses_egl_image_ = false;
  DLOG(INFO) << "Uses system memory for output";
#endif
}

OmxVideoDecodeEngine::~OmxVideoDecodeEngine() {
  DCHECK(client_state_ == kClientNotInitialized ||
         client_state_ == kClientStopped);
  DCHECK_EQ(il_state_, kIlNone);
  DCHECK_EQ(0u, input_buffers_.size());
  DCHECK(free_input_buffers_.empty());
  DCHECK(available_input_buffers_.empty());
  DCHECK_EQ(0, input_buffers_at_component_);
  DCHECK_EQ(0, output_buffers_at_component_);
  DCHECK(output_frames_.empty());
}

template <typename T>
static void ResetParamHeader(const OmxVideoDecodeEngine& dec, T* param) {
  memset(param, 0, sizeof(T));
  param->nVersion.nVersion = dec.current_omx_spec_version();
  param->nSize = sizeof(T);
}

void OmxVideoDecodeEngine::Initialize(
    MessageLoop* message_loop,
    VideoDecodeEngine::EventHandler* event_handler,
    VideoDecodeContext* context,
    const VideoCodecConfig& config) {
  DCHECK_EQ(message_loop, MessageLoop::current());

  message_loop_ = message_loop;
  event_handler_ = event_handler;

  width_ = config.width();
  height_ = config.height();

  // TODO(wjia): Find the right way to determine the codec type.
  OmxConfigurator::MediaFormat input_format, output_format;
  memset(&input_format, 0, sizeof(input_format));
  memset(&output_format, 0, sizeof(output_format));
  input_format.codec = OmxConfigurator::kCodecH264;
  output_format.codec = OmxConfigurator::kCodecRaw;
  configurator_.reset(
      new OmxDecoderConfigurator(input_format, output_format));

  // TODO(jiesun): We already ensure Initialize() is called in thread context,
  // We should try to merge the following function into this function.
  client_state_ = kClientInitializing;
  InitializeTask();

  VideoCodecInfo info;
  // TODO(jiesun): ridiculous, we never fail initialization?
  info.success = true;
  info.provides_buffers = !uses_egl_image_;
  info.stream_info.surface_type =
      uses_egl_image_ ? VideoFrame::TYPE_GL_TEXTURE
                      : VideoFrame::TYPE_SYSTEM_MEMORY;
  info.stream_info.surface_format = GetSurfaceFormat();
  info.stream_info.surface_width = config.width();
  info.stream_info.surface_height = config.height();
  event_handler_->OnInitializeComplete(info);
}

// This method handles only input buffer, without coupling with output
void OmxVideoDecodeEngine::ConsumeVideoSample(scoped_refptr<Buffer> buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK(!free_input_buffers_.empty());
  DCHECK_GT(input_pending_request_, 0);

  --input_pending_request_;

  if (!CanAcceptInput()) {
    FinishEmptyBuffer(buffer);
    return;
  }

  if (buffer->IsEndOfStream()) {
    DLOG(INFO) << "Input queue has EOS";
    input_queue_has_eos_ = true;
  }

  OMX_BUFFERHEADERTYPE* omx_buffer = free_input_buffers_.front();
  free_input_buffers_.pop();

  // setup |omx_buffer|.
  omx_buffer->pBuffer = const_cast<OMX_U8*>(buffer->GetData());
  omx_buffer->nFilledLen = buffer->GetDataSize();
  omx_buffer->nAllocLen = omx_buffer->nFilledLen;
  if (input_queue_has_eos_)
    omx_buffer->nFlags |= OMX_BUFFERFLAG_EOS;
  else
    omx_buffer->nFlags &= ~OMX_BUFFERFLAG_EOS;
  omx_buffer->nTimeStamp = buffer->GetTimestamp().InMicroseconds();
  omx_buffer->pAppPrivate = buffer.get();
  buffer->AddRef();
  available_input_buffers_.push(omx_buffer);

  // Try to feed buffers into the decoder.
  EmptyBufferTask();

  if (flush_pending_ && input_pending_request_ == 0)
    StartFlush();
}

void OmxVideoDecodeEngine::Flush() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(il_state_, kIlExecuting);

  if (il_state_ != kIlExecuting) {
    event_handler_->OnFlushComplete();
    return;
  }

  client_state_ = kClientFlushing;
  expected_il_state_ = kIlPause;
  OnStateSetEventFunc = &OmxVideoDecodeEngine::PauseFromExecuting;
  TransitionToState(OMX_StatePause);
}

void OmxVideoDecodeEngine::PauseFromExecuting(OMX_STATETYPE state) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  OnStateSetEventFunc = NULL;
  il_state_ = kIlPause;

  if (input_pending_request_ == 0)
    StartFlush();
  else
    flush_pending_ = true;
}

void OmxVideoDecodeEngine::StartFlush() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(input_pending_request_, 0);
  DLOG(INFO) << "StartFlush";

  while (!available_input_buffers_.empty())
    available_input_buffers_.pop();

  flush_pending_ = false;

  // Flush input port first.
  OnFlushEventFunc = &OmxVideoDecodeEngine::PortFlushDone;
  OMX_ERRORTYPE omxresult;
  omxresult = OMX_SendCommand(component_handle_,
                              OMX_CommandFlush,
                              input_port_, 0);
}

bool OmxVideoDecodeEngine::InputPortFlushed() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(client_state_, kClientFlushing);
  // Port flushed is defined by OpenMAX component had signal flush done and
  // We had all buffers returned from demuxer and OpenMAX component.
  int free_input_size = static_cast<int>(free_input_buffers_.size());
  return input_port_flushed_ && free_input_size == input_buffer_count_;
}

bool OmxVideoDecodeEngine::OutputPortFlushed() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(client_state_, kClientFlushing);
  // Port flushed is defined by OpenMAX component had signal flush done and
  // We had all buffers returned from renderer and OpenMAX component.
  return output_port_flushed_ && output_pending_request_ == 0;
}

void OmxVideoDecodeEngine::ComponentFlushDone() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DLOG(INFO) << "Component had been flushed!";

  if (input_port_flushed_ && output_port_flushed_) {
    event_handler_->OnFlushComplete();
    input_port_flushed_ = false;
    output_port_flushed_ = false;
  }
}

void OmxVideoDecodeEngine::PortFlushDone(int port) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_NE(port, static_cast<int>(OMX_ALL));

  if (port == input_port_) {
    DLOG(INFO) << "Input Port had been flushed";
    DCHECK_EQ(input_buffers_at_component_, 0);
    input_port_flushed_ = true;
    // Flush output port next.
    OMX_ERRORTYPE omxresult;
    omxresult = OMX_SendCommand(component_handle_,
                                OMX_CommandFlush,
                                output_port_, 0);
    return;
  }

  if (port == output_port_) {
    DLOG(INFO) << "Output Port had been flushed";
    DCHECK_EQ(output_buffers_at_component_, 0);

    output_port_flushed_ = true;
  }

  if (kClientFlushing == client_state_ &&
      InputPortFlushed() && OutputPortFlushed())
    ComponentFlushDone();
}

void OmxVideoDecodeEngine::Seek() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  DCHECK(client_state_ == kClientFlushing ||     // After a flush
         client_state_ == kClientInitializing);  // After an initialize.

  if (client_state_ == kClientFlushing) {
    InitialReadBuffer();
    OnStateSetEventFunc = &OmxVideoDecodeEngine::DoneSetStateExecuting;
    TransitionToState(OMX_StateExecuting);
  }

  event_handler_->OnSeekComplete();
}

int OmxVideoDecodeEngine::current_omx_spec_version() const {
  return 0x00000101;
}

VideoFrame::Format OmxVideoDecodeEngine::GetSurfaceFormat() const {
  // TODO(jiesun): Both OmxHeaderType and EGLImage surface type could have
  // different surface formats.
  return uses_egl_image_ ? VideoFrame::RGBA : VideoFrame::YV12;
}

void OmxVideoDecodeEngine::Uninitialize() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (client_state_ == kClientError) {
    OnStopDone();
    return;
  }

  // TODO(wjia): add more state checking
  if (kClientRunning == client_state_ || kClientFlushing == client_state_) {
    client_state_ = kClientStopping;
    DeinitFromExecuting(OMX_StateExecuting);
  }

  // TODO(wjia): When FillThisBuffer() is added, engine state should be
  // kStopping here. engine state should be set to kStopped in OnStopDone();
  // client_state_ = kClientStopping;
}

void OmxVideoDecodeEngine::FinishEmptyBuffer(scoped_refptr<Buffer> buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!input_queue_has_eos_) {
    event_handler_->ProduceVideoSample(buffer);
    ++input_pending_request_;
  }
}

void OmxVideoDecodeEngine::FinishFillBuffer(OMX_BUFFERHEADERTYPE* buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK(buffer);

  scoped_refptr<VideoFrame> frame;
  frame = static_cast<VideoFrame*>(buffer->pAppPrivate);

  // We should not flush buffer to renderer during decoder flushing if decoder
  // provides the buffer allocator.
  if (kClientFlushing == client_state_ && !uses_egl_image_) return;

  PipelineStatistics statistics;
  statistics.video_bytes_decoded = buffer->nFilledLen;

  frame->SetTimestamp(base::TimeDelta::FromMicroseconds(buffer->nTimeStamp));
  frame->SetDuration(frame->GetTimestamp() - last_pts_);
  last_pts_ = frame->GetTimestamp();
  event_handler_->ConsumeVideoFrame(frame, statistics);
  output_pending_request_--;
}

void OmxVideoDecodeEngine::OnStopDone() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  event_handler_->OnUninitializeComplete();
}

// Function sequence for initializing
void OmxVideoDecodeEngine::InitializeTask() {
  DCHECK_EQ(il_state_, kIlNone);

  il_state_ = kIlNone;
  expected_il_state_ = kIlLoaded;
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
  input_buffer_count_ = port_format.nBufferCountActual;
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

  // TODO(wjia): use same buffer recycling for EGLImage and system memory.
  // Override buffer count when EGLImage is used.
  if (uses_egl_image_) {
    // TODO(wjia): remove hard-coded value
    port_format.nBufferCountActual = port_format.nBufferCountMin =
       output_buffer_count_ = 4;

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
  DLOG(INFO) << "OMX video decode engine is in Idle";

  il_state_ = kIlIdle;

  // start reading bit stream
  InitialReadBuffer();
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
  DCHECK(client_state_ == kClientInitializing ||
         client_state_ == kClientFlushing);
  DCHECK_EQ(OMX_StateExecuting, state);
  DLOG(INFO) << "OMX video decode engine is in Executing";

  il_state_ = kIlExecuting;
  client_state_ = kClientRunning;
  OnStateSetEventFunc = NULL;
  EmptyBufferTask();
  InitialFillBuffer();
  if (kClientError == client_state_) {
    StopOnError();
    return;
  }
}

// Function for receiving output buffers. Hookup for buffer recycling
// and outside allocator.
void OmxVideoDecodeEngine::ProduceVideoFrame(
    scoped_refptr<VideoFrame> video_frame) {
  DCHECK(video_frame.get() && !video_frame->IsEndOfStream());
  output_pending_request_++;

  PipelineStatistics statistics;

  if (!CanAcceptOutput()) {
    if (uses_egl_image_) {  // return it to owner.
      output_pending_request_--;
      event_handler_->ConsumeVideoFrame(video_frame, statistics);
    }
    return;
  }

  OMX_BUFFERHEADERTYPE* omx_buffer = FindOmxBuffer(video_frame);
  if (omx_buffer) {
    statistics.video_bytes_decoded = omx_buffer->nFilledLen;

    if (kClientRunning == client_state_) {
      SendOutputBufferToComponent(omx_buffer);
    } else if (kClientFlushing == client_state_) {
      if (uses_egl_image_) {  // return it to owner.
        output_pending_request_--;
        event_handler_->ConsumeVideoFrame(video_frame, statistics);
      }
      if (InputPortFlushed() && OutputPortFlushed())
        ComponentFlushDone();
    }
  } else {
    DCHECK(!output_frames_allocated_);
    DCHECK(uses_egl_image_);
    output_frames_.push_back(std::make_pair(video_frame,
        static_cast<OMX_BUFFERHEADERTYPE*>(NULL)));
  }

  DCHECK(static_cast<int>(output_frames_.size()) <= output_buffer_count_);

  if ((!output_frames_allocated_) &&
      static_cast<int>(output_frames_.size()) == output_buffer_count_) {
    output_frames_allocated_ = true;

    if (need_setup_output_port_) {
      SetupOutputPort();
    }
  }

  if (kClientError == client_state_) {
    StopOnError();
    return;
  }
}

// Reconfigure port
void OmxVideoDecodeEngine::OnPortSettingsChangedRun(int port,
                                                    OMX_INDEXTYPE index) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(client_state_, kClientRunning);
  DCHECK_EQ(port, output_port_);

  // TODO(wjia): add buffer negotiation between decoder and renderer.
  if (uses_egl_image_) {
    DLOG(INFO) << "Port settings are changed";
    return;
  }

  // TODO(wjia): remove this checking when all vendors observe same spec.
  if (index > OMX_IndexComponentStartUnused) {
    if (index != OMX_IndexParamPortDefinition)
      return;
  }

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
  DCHECK_EQ(message_loop_, MessageLoop::current());

  need_setup_output_port_ = false;

  // Enable output port when necessary since the port could be waiting for
  // buffers, instead of port reconfiguration.
  if (kPortEnabled != output_port_state_) {
    output_port_state_ = kPortEnabling;
    OnPortEnableEventFunc = &OmxVideoDecodeEngine::OnPortEnableEventRun;
    ChangePort(OMX_CommandPortEnable, output_port_);
    if (kClientError == client_state_) {
      return;
    }
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
  DCHECK_EQ(port, output_port_);
  DCHECK_EQ(client_state_, kClientRunning);

  output_port_state_ = kPortEnabled;
  last_pts_ = base::TimeDelta::FromMilliseconds(0);
  OnPortEnableEventFunc = NULL;
  InitialFillBuffer();
  if (kClientError == client_state_) {
    StopOnError();
    return;
  }
}

void OmxVideoDecodeEngine::DeinitFromExecuting(OMX_STATETYPE state) {
  DCHECK_EQ(state, OMX_StateExecuting);

  DLOG(INFO) << "Deinit from Executing";
  OnStateSetEventFunc = &OmxVideoDecodeEngine::DeinitFromIdle;
  TransitionToState(OMX_StateIdle);
  expected_il_state_ = kIlIdle;
}

void OmxVideoDecodeEngine::DeinitFromIdle(OMX_STATETYPE state) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(state, OMX_StateIdle);

  DLOG(INFO) << "Deinit from Idle";
  il_state_ = kIlIdle;
  OnStateSetEventFunc = &OmxVideoDecodeEngine::DeinitFromLoaded;
  TransitionToState(OMX_StateLoaded);
  expected_il_state_ = kIlLoaded;

  if (!input_buffers_at_component_)
    FreeInputBuffers();
  else
    need_free_input_buffers_ = true;

  if (!output_buffers_at_component_)
    FreeOutputBuffers();
  else
    need_free_output_buffers_ = true;
}

void OmxVideoDecodeEngine::DeinitFromLoaded(OMX_STATETYPE state) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(state, OMX_StateLoaded);

  DLOG(INFO) << "Deinit from Loaded";
  il_state_ = kIlLoaded;
  if (component_handle_) {
    OMX_ERRORTYPE result = OMX_FreeHandle(component_handle_);
    if (result != OMX_ErrorNone)
      LOG(ERROR) << "OMX_FreeHandle() error. Error code: " << result;
    component_handle_ = NULL;
  }
  il_state_ = expected_il_state_ = kIlNone;

  // kClientStopped is different from kClientNotInitialized. The former can't
  // accept output buffers, while the latter can.
  client_state_ = kClientStopped;

  OMX_Deinit();

  OnStopDone();
}

void OmxVideoDecodeEngine::StopOnError() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  client_state_ = kClientStopping;

  if (kIlExecuting == expected_il_state_) {
    DeinitFromExecuting(OMX_StateExecuting);
  } else if (kIlIdle == expected_il_state_) {
    DeinitFromIdle(OMX_StateIdle);
  } else if (kIlLoaded == expected_il_state_) {
    DeinitFromLoaded(OMX_StateLoaded);
  } else if (kIlPause == expected_il_state_) {
    // TODO(jiesun): Make sure this works.
    DeinitFromExecuting(OMX_StateExecuting);
  } else {
    NOTREACHED();
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
    free_input_buffers_.push(buffer);
  }
  return true;
}

// This method handles EGLImage and internal buffer cases. Any external
// allocation case is similar to EGLImage
bool OmxVideoDecodeEngine::AllocateOutputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (uses_egl_image_ && !output_frames_allocated_) {
    DLOG(INFO) << "Output frames are not allocated yet";
    need_setup_output_port_ = true;
    return true;
  }

  for (int i = 0; i < output_buffer_count_; ++i) {
    OMX_BUFFERHEADERTYPE* buffer;
    scoped_refptr<VideoFrame> video_frame;
    OMX_ERRORTYPE error;
    if (uses_egl_image_) {
      OutputFrame output_frame = output_frames_[i];
      video_frame = output_frame.first;
      DCHECK(!output_frame.second);
      error = OMX_UseEGLImage(component_handle_, &buffer, output_port_,
                              video_frame.get(), video_frame->private_buffer());
      if (error != OMX_ErrorNone)
        return false;
      output_frames_[i].second = buffer;
    } else {
      error = OMX_AllocateBuffer(component_handle_, &buffer, output_port_,
                                 NULL, output_buffer_size_);
      if (error != OMX_ErrorNone)
        return false;
      video_frame = CreateOmxBufferVideoFrame(buffer);
      output_frames_.push_back(std::make_pair(video_frame, buffer));
      buffer->pAppPrivate = video_frame.get();
    }
  }

  return true;
}

scoped_refptr<VideoFrame> OmxVideoDecodeEngine::CreateOmxBufferVideoFrame(
    OMX_BUFFERHEADERTYPE* omx_buffer) {
  scoped_refptr<VideoFrame> video_frame;
  uint8* data[VideoFrame::kMaxPlanes];
  int32 strides[VideoFrame::kMaxPlanes];

  memset(data, 0, sizeof(data));
  memset(strides, 0, sizeof(strides));
  // TODO(jiesun): chroma format 4:2:0 only and 3 planes.
  data[0] = omx_buffer->pBuffer;
  data[1] = data[0] + width_ * height_;
  data[2] = data[1] + width_ * height_ / 4;
  strides[0] = width_;
  strides[1] = strides[2] = width_ >> 1;

  VideoFrame::CreateFrameExternal(
      VideoFrame::TYPE_SYSTEM_MEMORY,
      VideoFrame::YV12,
      width_, height_, 3,
      data, strides,
      kNoTimestamp,
      kNoTimestamp,
      omx_buffer,
      &video_frame);

  return video_frame;
}

void OmxVideoDecodeEngine::FreeInputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Empty available buffer queue.
  while (!free_input_buffers_.empty()) {
    free_input_buffers_.pop();
  }

  while (!available_input_buffers_.empty()) {
    OMX_BUFFERHEADERTYPE* omx_buffer = available_input_buffers_.front();
    available_input_buffers_.pop();
    Buffer* stored_buffer = static_cast<Buffer*>(omx_buffer->pAppPrivate);
    FinishEmptyBuffer(stored_buffer);
    stored_buffer->Release();
  }

  // Calls to OMX to free buffers.
  for (size_t i = 0; i < input_buffers_.size(); ++i)
    OMX_FreeBuffer(component_handle_, input_port_, input_buffers_[i]);
  input_buffers_.clear();

  need_free_input_buffers_ = false;
}

void OmxVideoDecodeEngine::FreeOutputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Calls to OMX to free buffers.
  for (size_t i = 0; i < output_frames_.size(); ++i) {
    OMX_BUFFERHEADERTYPE* omx_buffer = output_frames_[i].second;
    CHECK(omx_buffer);
    OMX_FreeBuffer(component_handle_, output_port_, omx_buffer);
  }
  output_frames_.clear();
  output_frames_allocated_ = false;

  need_free_output_buffers_ = false;
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
          !input_has_fed_eos_);
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
  return (kClientError != client_state_ &&
          kClientStopping != client_state_ &&
          kClientStopped != client_state_ &&
          !input_queue_has_eos_);
}

bool OmxVideoDecodeEngine::CanAcceptOutput() {
  return (kClientError != client_state_ &&
          kClientStopping != client_state_ &&
          kClientStopped != client_state_ &&
          output_port_state_ == kPortEnabled &&
          !output_eos_);
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
  while (!available_input_buffers_.empty() &&
         !input_has_fed_eos_) {
    OMX_BUFFERHEADERTYPE* omx_buffer = available_input_buffers_.front();
    available_input_buffers_.pop();

    input_has_fed_eos_ = omx_buffer->nFlags & OMX_BUFFERFLAG_EOS;
    if (input_has_fed_eos_) {
      DLOG(INFO) << "Input has fed EOS";
    }

    // Give this buffer to OMX.
    input_buffers_at_component_++;
    OMX_ERRORTYPE ret = OMX_EmptyThisBuffer(component_handle_, omx_buffer);
    if (ret != OMX_ErrorNone) {
      LOG(ERROR) << "OMX_EmptyThisBuffer() failed with result " << ret;
      client_state_ = kClientError;
      return;
    }
  }
}

void OmxVideoDecodeEngine::InitialReadBuffer() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  input_queue_has_eos_ = false;
  input_has_fed_eos_ = false;
  output_eos_ = false;

  DLOG(INFO) << "OmxVideoDecodeEngine::InitialReadBuffer";
  for (size_t i = 0; i < free_input_buffers_.size(); i++)
    FinishEmptyBuffer(NULL);
}

void OmxVideoDecodeEngine::InitialFillBuffer() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  // DCHECK(output_frames_allocated_);

  if (!CanFillBuffer())
    return;

  DLOG(INFO) << "OmxVideoDecodeEngine::InitialFillBuffer";

  // Ask the decoder to fill the output buffers.
  for (uint32 i = 0; i < output_frames_.size(); ++i) {
    OMX_BUFFERHEADERTYPE* omx_buffer = output_frames_[i].second;
    SendOutputBufferToComponent(omx_buffer);
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
    if (video_frame == output_frames_[i].first)
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
  omx_buffer->nOutputPortIndex = output_port_;
  output_buffers_at_component_++;
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
  DCHECK_GT(input_buffers_at_component_, 0);

  Buffer* stored_buffer = static_cast<Buffer*>(buffer->pAppPrivate);
  buffer->pAppPrivate = NULL;
  if (client_state_ != kClientFlushing)
    FinishEmptyBuffer(stored_buffer);
  stored_buffer->Release();

  // Enqueue the available buffer because the decoder has consumed it.
  free_input_buffers_.push(buffer);
  input_buffers_at_component_--;

  if (need_free_input_buffers_ && !input_buffers_at_component_) {
    FreeInputBuffers();
    return;
  }

  // Try to feed more data into the decoder.
  EmptyBufferTask();

  if (client_state_ == kClientFlushing &&
      InputPortFlushed() && OutputPortFlushed())
    ComponentFlushDone();
}

void OmxVideoDecodeEngine::FillBufferDoneTask(OMX_BUFFERHEADERTYPE* buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_GT(output_buffers_at_component_, 0);

  output_buffers_at_component_--;

  if (need_free_output_buffers_ && !output_buffers_at_component_) {
    FreeOutputBuffers();
    return;
  }

  PipelineStatistics statistics;
  statistics.video_bytes_decoded = buffer->nFilledLen;

  if (!CanAcceptOutput()) {
    if (uses_egl_image_) {
      scoped_refptr<VideoFrame> frame;
      frame = static_cast<VideoFrame*>(buffer->pAppPrivate);
      event_handler_->ConsumeVideoFrame(frame, statistics);
      output_pending_request_--;
    }
    return;
  }

  // This buffer is received with decoded frame. Enqueue it and make it
  // ready to be consumed by reads.

  if (buffer->nFlags & OMX_BUFFERFLAG_EOS) {
    output_eos_ = true;
    DLOG(INFO) << "Output has EOS";
  }

  FinishFillBuffer(buffer);

  if (buffer->nFlags & OMX_BUFFERFLAG_EOS) {
    // Singal end of stream.
    scoped_refptr<VideoFrame> frame;
    VideoFrame::CreateEmptyFrame(&frame);
    event_handler_->ConsumeVideoFrame(frame, statistics);
  }

  if (client_state_ == kClientFlushing &&
      InputPortFlushed() && OutputPortFlushed())
    ComponentFlushDone();
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
        LOG(ERROR) << "Unknown command completed\n" << data1;
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
      // TODO(wjia): remove this hack when all vendors observe same spec.
      if (data1 < OMX_IndexComponentStartUnused)
        OnPortSettingsChangedRun(static_cast<int>(data1),
                                 static_cast<OMX_INDEXTYPE>(data2));
      else
        OnPortSettingsChangedRun(static_cast<int>(data2),
                                 static_cast<OMX_INDEXTYPE>(data1));
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

// Disable refcounting for this object because this object only lives
// on the video decoder thread and there's no need to refcount it.
DISABLE_RUNNABLE_METHOD_REFCOUNT(media::OmxVideoDecodeEngine);
