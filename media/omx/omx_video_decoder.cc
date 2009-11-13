// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "media/omx/input_buffer.h"
#include "media/omx/omx_video_decoder.h"

namespace media {

OmxVideoDecoder::OmxVideoDecoder(MessageLoop* message_loop)
    : input_buffer_count_(0),
      input_buffer_size_(0),
      input_port_(0),
      input_eos_(false),
      output_buffer_count_(0),
      output_buffer_size_(0),
      output_port_(0),
      output_eos_(false),
      decoder_handle_(NULL),
      state_(kEmpty),
      next_state_(kEmpty),
      codec_(kCodecNone),
      message_loop_(message_loop) {
}

OmxVideoDecoder::~OmxVideoDecoder() {
  DCHECK(state_ == kError || state_ == kEmpty);
  DCHECK_EQ(0u, input_buffers_.size());
  DCHECK_EQ(0u, output_buffers_.size());
  DCHECK(available_input_buffers_.empty());
  DCHECK(available_output_buffers_.empty());
  DCHECK(input_queue_.empty());
  DCHECK(output_queue_.empty());
}

void OmxVideoDecoder::Setup(const char* component, Codec codec) {
  DCHECK_EQ(kEmpty, state_);
  component_ = component;
  codec_ = codec;
}

void OmxVideoDecoder::SetErrorCallback(Callback* callback) {
  DCHECK_EQ(kEmpty, state_);
  error_callback_.reset(callback);
}

void OmxVideoDecoder::Start() {
  DCHECK_NE(kCodecNone, codec_);

  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &OmxVideoDecoder::StartTask));
}

void OmxVideoDecoder::Stop(Callback* callback) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &OmxVideoDecoder::StopTask, callback));
}

void OmxVideoDecoder::Read(ReadCallback* callback) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &OmxVideoDecoder::ReadTask, callback));
}

void OmxVideoDecoder::Feed(InputBuffer* buffer, FeedCallback* callback) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &OmxVideoDecoder::FeedTask, buffer, callback));
}

void OmxVideoDecoder::Flush(Callback* callback) {
  // TODO(hclam): implement.
}

OmxVideoDecoder::State OmxVideoDecoder::GetState() const {
  return state_;
}

void OmxVideoDecoder::SetState(State state) {
  state_ = state;
}

OmxVideoDecoder::State OmxVideoDecoder::GetNextState() const {
  return next_state_;
}

void OmxVideoDecoder::SetNextState(State state) {
  next_state_ = state;
}

void OmxVideoDecoder::StartTask() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  StateTransitionTask(kLoaded);
}

void OmxVideoDecoder::StopTask(Callback* callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  stop_callback_.reset(callback);

  if (GetState() == kError) {
    DoneStop();
    return;
  }

  FreeInputQueue();
  FreeOutputQueue();

  if (GetState() == kExecuting)
    StateTransitionTask(kIdle);
  // TODO(hclam): The following two transitions may not be correct.
  else if (GetState() == kPortSettingDisable)
    StateTransitionTask(kIdle);
  else if (GetState() == kPortSettingEnable)
    StateTransitionTask(kIdle);
  else if (GetState() == kIdle)
    StateTransitionTask(kLoaded);
  else if (GetState() == kLoaded)
    StateTransitionTask(kEmpty);
}

void OmxVideoDecoder::ReadTask(ReadCallback* callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Don't accept read request on error state.
  if (!CanAcceptOutput()) {
    callback->RunWithParams(MakeTuple(static_cast<uint8*>(NULL), 0));
    delete callback;
    return;
  }

  // Queue this request.
  output_queue_.push(callback);

  // Make our best effort to serve the request and read
  // from the decoder.
  FillBufferTask();
}

void OmxVideoDecoder::FeedTask(InputBuffer* buffer, FeedCallback* callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanAcceptInput()) {
    callback->Run(buffer);
    delete callback;
    return;
  }

  // Queue this input buffer.
  input_queue_.push(std::make_pair(buffer, callback));

  // Try to feed buffers into the decoder.
  EmptyBufferTask();
}

// This method assumes OMX_AllocateBuffer() will allocate
// buffer internally. If this is not the case we need to
// call OMX_UseBuffer() to allocate buffer manually and
// assign to the headers.
bool OmxVideoDecoder::AllocateInputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  for(int i = 0; i < input_buffer_count_; ++i) {
    OMX_BUFFERHEADERTYPE* buffer;
    OMX_ERRORTYPE error =
        OMX_AllocateBuffer(decoder_handle_, &buffer, input_port_,
                           NULL, input_buffer_size_);
    if (error != OMX_ErrorNone)
      return false;
    input_buffers_.push_back(buffer);
    available_input_buffers_.push(buffer);
  }
  return true;
}

// This method assumes OMX_AllocateBuffer() will allocate
// buffer internally. If this is not the case we need to
// call OMX_UseBuffer() to allocate buffer manually and
// assign to the headers.
bool OmxVideoDecoder::AllocateOutputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  for(int i = 0; i < output_buffer_count_; ++i) {
    OMX_BUFFERHEADERTYPE* buffer;
    OMX_ERRORTYPE error =
        OMX_AllocateBuffer(decoder_handle_, &buffer, output_port_,
                           NULL, output_buffer_size_);
    if (error != OMX_ErrorNone)
      return false;
    output_buffers_.push_back(buffer);
  }
  return true;
}

void OmxVideoDecoder::FreeInputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Calls to OMX to free buffers.
  for(size_t i = 0; i < input_buffers_.size(); ++i)
    OMX_FreeBuffer(decoder_handle_, input_port_, input_buffers_[i]);
  input_buffers_.clear();

  // Empty available buffer queue.
  while (!available_input_buffers_.empty()) {
    available_input_buffers_.pop();
  }
}

void OmxVideoDecoder::FreeOutputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Calls to OMX to free buffers.
  for(size_t i = 0; i < output_buffers_.size(); ++i)
    OMX_FreeBuffer(decoder_handle_, output_port_, output_buffers_[i]);
  output_buffers_.clear();

  // Empty available buffer queue.
  while (!available_output_buffers_.empty()) {
    available_output_buffers_.pop();
  }
}

void OmxVideoDecoder::FreeInputQueue() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  while (!input_queue_.empty()) {
    InputBuffer* buffer = input_queue_.front().first;
    FeedCallback* callback = input_queue_.front().second;
    callback->Run(buffer);
    delete callback;
    input_queue_.pop();
  }
}

void OmxVideoDecoder::FreeOutputQueue() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  while (!output_queue_.empty()) {
    ReadCallback* callback = output_queue_.front();
    callback->Run(static_cast<uint8*>(NULL), 0);
    delete callback;
    output_queue_.pop();
  }
}

// Sequence of actions in this transition:
//
// 1. Initialize OMX (To be removed.)
// 2. Get handle of the OMX component
// 3. Get parameters about I/O ports.
// 4. Device specific configurations.
// 5. General configuration of input port.
// 6. General configuration of output port.
// 7. Get Parameters about input port.
// 8. Get Parameters about output port.
// 9. Codec specific configurations.
void OmxVideoDecoder::Transition_EmptyToLoaded() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kEmpty, GetState());

  const char* component;
  OMX_CALLBACKTYPE callback = { &EventHandler,
                                &EmptyBufferCallback,
                                &FillBufferCallback };

  // 1. Initialize the OpenMAX Core.
  // TODO(hclam): move this out.
  OMX_ERRORTYPE omxresult = OMX_Init();
  if (omxresult != OMX_ErrorNone) {
    printf("Error - Failed to Init OpenMAX core\n");
    StateTransitionTask(kError);
    return;
  }

  // 2. Get the handle to the component. After OMX_GetHandle(),
  //    the component is in loaded state.
  // TODO(hclam): We should have a list of componant names instead.
  component = component_;
  omxresult = OMX_GetHandle((OMX_HANDLETYPE*)(&decoder_handle_),
                            (OMX_STRING)component, this, &callback);
  if (omxresult != OMX_ErrorNone) {
    printf("Failed to Load the component: %s\n", component);
    StateTransitionTask(kError);
    return;
  }

  // 3. Get the port information. This will obtain information about the
  //    number of ports and index of the first port.
  OMX_PORT_PARAM_TYPE port_param;
  memset(&port_param, 0, sizeof(port_param));
  port_param.nVersion.nVersion = current_omx_spec_version();
  port_param.nSize = sizeof(port_param);
  omxresult = OMX_GetParameter(decoder_handle_, OMX_IndexParamVideoInit,
                               &port_param);
  if (omxresult != OMX_ErrorNone) {
    printf("ERROR - Failed to get Port Param\n");
    StateTransitionTask(kError);
    return;
  }
  input_port_ = port_param.nStartPortNumber;
  output_port_ = input_port_ + 1;

  // 4. Device specific configurations.
  if (!DeviceSpecificConfig()) {
    printf("Error - device specific configurations failed\n");
    StateTransitionTask(kError);
    return;
  }

  // 5. Configure the input port.
  // Query the decoder input port's minimum buffer requirements.
  // Note that port_param.nStartPortNumber defines the index of the
  // input port.
  OMX_PARAM_PORTDEFINITIONTYPE port_format;
  memset(&port_format, 0, sizeof(port_format));
  port_format.nVersion.nVersion = current_omx_spec_version();
  port_format.nSize = sizeof(port_format);
  port_format.nPortIndex = input_port_;
  omxresult = OMX_GetParameter(decoder_handle_,
                               OMX_IndexParamPortDefinition,
                               &port_format);
  if (omxresult != OMX_ErrorNone) {
    printf("Error - GetParameter failed\n");
    StateTransitionTask(kError);
    return;
  }
  if(OMX_DirInput != port_format.eDir) {
    printf ("Error - Expect Input Port\n");
    StateTransitionTask(kError);
    return;
  }
  if (codec_ == kCodecH264)
    port_format.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  else if (codec_ == kCodecMpeg4)
    port_format.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  else if (codec_ == kCodecH263)
    port_format.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
  else if (codec_ == kCodecVc1)
    port_format.format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;
  else
    printf("Error: Unsupported codec %d\n", codec_);
  // Assume QCIF.
  port_format.format.video.nFrameWidth  = 176;
  port_format.format.video.nFrameHeight = 144;
  omxresult = OMX_SetParameter(decoder_handle_,
                               OMX_IndexParamPortDefinition,
                               &port_format);
  if (omxresult != OMX_ErrorNone) {
    printf("Error - SetParameter failed\n");
    StateTransitionTask(kError);
    return;
  }

  // 6. Configure the output port.
  // There's nothing to be done here.

  // 7. Obtain the information about the input port.
  // This will have the new mini buffer count in port_format.nBufferCountMin.
  // Save this value to input_buf_count.
  port_format.nPortIndex = port_param.nStartPortNumber;
  omxresult = OMX_GetParameter(decoder_handle_,
                               OMX_IndexParamPortDefinition,
                               &port_format);
  if (omxresult != OMX_ErrorNone) {
    printf("Error - GetParameter failed\n");
    StateTransitionTask(kError);
    return;
  }
  if (OMX_DirInput != port_format.eDir) {
    printf("Error - Expect input port\n");
    StateTransitionTask(kError);
    return;
  }
  input_buffer_count_ = port_format.nBufferCountMin;
  input_buffer_size_ = port_format.nBufferSize;

  // 8. Obtain the information about the output port.
  memset(&port_format, 0, sizeof(port_format));
  port_format.nPortIndex = output_port_;
  omxresult = OMX_GetParameter(decoder_handle_,
                               OMX_IndexParamPortDefinition,
                               &port_format);
  if (omxresult != OMX_ErrorNone) {
    printf("Error - GetParameter failed\n");
    StateTransitionTask(kError);
    return;
  }
  if (OMX_DirOutput != port_format.eDir) {
    printf("Error - Expect Output Port\n");
    StateTransitionTask(kError);
    return;
  }
  output_buffer_count_ = port_format.nBufferCountMin;
  output_buffer_size_ = port_format.nBufferSize;

  // 9. Codec specific configurations.
  // This sets the NAL length size. 0 means we are using a 3 byte start code.
  // Other values specifies number of bytes of the NAL length.
  if (codec_ == kCodecH264) {
    OMX_VIDEO_CONFIG_NALSIZE naluSize;
    naluSize.nNaluBytes = 0;
    omxresult = OMX_SetConfig(decoder_handle_,
                              OMX_IndexConfigVideoNalSize, (OMX_PTR)&naluSize);
    if (omxresult != OMX_ErrorNone) {
      printf("Error - SetConfig failed\n");
      StateTransitionTask(kError);
      return;
    }
  }

  // After we have done all the configurations, we are considered loaded.
  DoneStateTransitionTask();
}

// Sequence of actions in this transition:
//
// 1. Send command to Idle state.
// 2. Allocate buffers for input port.
// 3. Allocate buffers for output port.
void OmxVideoDecoder::Transition_LoadedToIdle() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kLoaded, GetState());

  // 1. Sets decoder to idle state.
  OMX_ERRORTYPE omxresult = OMX_SendCommand(decoder_handle_,
                                            OMX_CommandStateSet,
                                            OMX_StateIdle, 0);
  if (omxresult != OMX_ErrorNone) {
    printf("Error - SendCommand failed\n");
    StateTransitionTask(kError);
    return;
  }

  // 2. Allocate buffer for the input port.
  if (!AllocateInputBuffers()) {
    printf("Error - OMX_AllocateBuffer Input buffer error\n");
    StateTransitionTask(kError);
    return;
  }

  // 3. Allocate buffer for the output port.
  if (!AllocateOutputBuffers()) {
    printf("Error - OMX_AllocateBuffer Output buffer error\n");
    StateTransitionTask(kError);
    return;
  }
}

// Sequence of actions in this transition:
//
// 1. Send command to Executing state.
void OmxVideoDecoder::Transition_IdleToExecuting() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kIdle, GetState());

  // Transist to executing state.
  OMX_ERRORTYPE omxresult = OMX_SendCommand(decoder_handle_,
                                            OMX_CommandStateSet,
                                            OMX_StateExecuting, 0);
  if (omxresult != OMX_ErrorNone) {
    printf("Error - SendCommand failed\n");
    StateTransitionTask(kError);
    return;
  }
}

// Sequence of actions in this transition:
//
// 1. Send command to disable output port.
// 2. Free buffers of the output port.
void OmxVideoDecoder::Transition_ExecutingToDisable() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kExecuting, GetState());

  // Send DISABLE command.
  OMX_ERRORTYPE omxresult = OMX_SendCommand(decoder_handle_,
                                            OMX_CommandPortDisable,
                                            output_port_, 0);
  if (omxresult != OMX_ErrorNone) {
    printf("Error - SendCommand failed\n");
    StateTransitionTask(kError);
    return;
  }

  // Free output Buffer.
  FreeOutputBuffers();
}

// Sequence of actions in this transition:
//
// 1. Send command to enable output port.
// 2. Get parameter of the output port.
// 3. Allocate buffers for the output port.
void OmxVideoDecoder::Transition_DisableToEnable() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kPortSettingDisable, GetState());

  // Send Enable command.
  OMX_ERRORTYPE omxresult = OMX_SendCommand(decoder_handle_,
                                            OMX_CommandPortEnable,
                                            output_port_, 0);
  if (omxresult != OMX_ErrorNone) {
    printf("Error - SendCommand failed\n");
    StateTransitionTask(kError);
    return;
  }

  // AllocateBuffers.
  OMX_PARAM_PORTDEFINITIONTYPE port_format;
  memset(&port_format, 0, sizeof(port_format));
  port_format.nPortIndex = output_port_;

  omxresult = OMX_GetParameter(decoder_handle_, OMX_IndexParamPortDefinition,
                               &port_format);
  if (omxresult != OMX_ErrorNone) {
    printf("Error - GetParameter failed\n");
    StateTransitionTask(kError);
    return;
  }
  if (OMX_DirOutput != port_format.eDir) {
    printf("Error - Expect Output Port\n");
    StateTransitionTask(kError);
    return;
  }

  // Update the ports in buffer.
  output_buffer_count_ = port_format.nBufferCountActual;
  output_buffer_size_ = port_format.nBufferSize;
  if (!AllocateOutputBuffers()) {
    printf("Error - OMX_AllocateBuffer Output buffer error\n");
    StateTransitionTask(kError);
    return;
  }
}

// Sequence of actions in this transition:
//
// 1. Send command to Idle state.
void OmxVideoDecoder::Transition_DisableToIdle() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kPortSettingDisable, GetState());

  OMX_ERRORTYPE omxresult = OMX_SendCommand(decoder_handle_,
                                            OMX_CommandStateSet,
                                            OMX_StateIdle, 0);
  if (omxresult != OMX_ErrorNone) {
    printf("Error - SendCommand failed\n");
    StateTransitionTask(kError);
    return;
  }
}

// Sequence of actions in this transition:
//
// This transition does nothing.
void OmxVideoDecoder::Transition_EnableToExecuting() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kPortSettingEnable, GetState());

  // This transition is fake, nothing to do here.
  DoneStateTransitionTask();
}

void OmxVideoDecoder::Transition_EnableToIdle() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kPortSettingEnable, GetState());

  OMX_ERRORTYPE omxresult = OMX_SendCommand(decoder_handle_,
                                            OMX_CommandStateSet,
                                            OMX_StateIdle, 0);
  if (omxresult != OMX_ErrorNone) {
    printf("Error - SendCommand failed\n");
    StateTransitionTask(kError);
    return;
  }
}

// Sequence of actions in this transition:
//
// 1. Send command to Idle state.
void OmxVideoDecoder::Transition_ExecutingToIdle() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kExecuting, GetState());

  OMX_ERRORTYPE omxresult = OMX_SendCommand(decoder_handle_,
                                            OMX_CommandStateSet,
                                            OMX_StateIdle, 0);
  if (omxresult != OMX_ErrorNone) {
    printf("Error - SendCommand failed\n");
    StateTransitionTask(kError);
    return;
  }
}

// Sequence of actions in this transition:
//
// 1. Send command to Loaded state
// 2. Free input buffers
// 2. Free output buffers
void OmxVideoDecoder::Transition_IdleToLoaded() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kIdle, GetState());

  OMX_ERRORTYPE omxresult = OMX_SendCommand(decoder_handle_,
                                            OMX_CommandStateSet,
                                            OMX_StateLoaded, 0);
  if (omxresult != OMX_ErrorNone) {
    printf("Error - SendCommand failed\n");
    StateTransitionTask(kError);
    return;
  }

  FreeInputBuffers();
  FreeOutputBuffers();
}

// Sequence of actions in this transition:
//
// 1. Free decoder handle
// 2. Uninitialize OMX (TODO(hclam): Remove this.)
void OmxVideoDecoder::Transition_LoadedToEmpty() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kLoaded, GetState());

  // Free the decoder handle.
  OMX_ERRORTYPE result = OMX_FreeHandle(decoder_handle_);
  if (result != OMX_ErrorNone) {
    printf("Error - Terminate: OMX_FreeHandle error. Error code: %d\n", result);
  }
  decoder_handle_ = NULL;

  // Deinit OpenMAX
  // TODO(hclam): move this out.
  OMX_Deinit();

  DoneStateTransitionTask();
}

// Sequence of actions in this transition:
//
// 1. Disable input port
// 2. Disable output port
// 3. Free input buffer
// 4. Free output buffer
// 5. Free decoder handle
// 6. Uninitialize OMX (TODO(hclam): Remove this.)
void OmxVideoDecoder::Transition_Error() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_NE(kError, GetState());

  State old_state = GetState();
  SetState(kError);

  // If we are going to error state in the following states, we need to
  // send a command to disable ports for us to free buffers.
  if (old_state == kExecuting || old_state == kIdle ||
      old_state == kPortSettingEnable || old_state == kPortSettingDisable) {
    DCHECK(decoder_handle_);
    OMX_SendCommand(decoder_handle_, OMX_CommandPortDisable, input_port_, 0);
    OMX_SendCommand(decoder_handle_, OMX_CommandPortDisable, output_port_, 0);
  }

  // Free input and output buffers.
  FreeInputBuffers();
  FreeOutputBuffers();

  // Free input and output queues.
  FreeInputQueue();
  FreeOutputQueue();

  // Free decoder handle.
  if (decoder_handle_) {
    OMX_ERRORTYPE result = OMX_FreeHandle(decoder_handle_);
    if (result != OMX_ErrorNone)
      printf("Error - OMX_FreeHandle error. Error code: %d\n", result);
    decoder_handle_ = NULL;
  }

  // Deinit OpenMAX.
  OMX_Deinit();

  DoneStateTransitionTask();
}

void OmxVideoDecoder::PostStateTransitionTask(State new_state) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &OmxVideoDecoder::StateTransitionTask, new_state));
}

void OmxVideoDecoder::StateTransitionTask(State new_state) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (GetState() == kError)
    return;

  // Save the next state.
  SetNextState(new_state);

  // The following list defines all the possible state transitions
  // for this object:
  //
  // TRANSITIONS
  // 1.  Empty -> Loaded
  // 2.  Loaded -> Idle
  // 3.  Idle -> Executing
  // 4.  Executing -> Disable
  // 5.  Executing -> Idle
  // 6.  Disable -> Enable
  // 7.  Disable -> Idle
  // 8.  Enable -> Executing
  // 9.  Enable -> Idle
  // 10. Idle -> Loaded
  // 11. Loaded -> Empty  (TODO(hclam): To stopped instead.)
  // 12. *ANYTHING* -> Error
  if (GetState() == kEmpty && new_state == kLoaded)
    Transition_EmptyToLoaded();
  else if (GetState() == kLoaded && new_state == kIdle)
    Transition_LoadedToIdle();
  else if (GetState() == kIdle && new_state == kExecuting)
    Transition_IdleToExecuting();
  else if (GetState() == kExecuting && new_state == kPortSettingDisable)
    Transition_ExecutingToDisable();
  else if (GetState() == kPortSettingDisable && new_state == kPortSettingEnable)
    Transition_DisableToEnable();
  else if (GetState() == kPortSettingDisable && new_state == kIdle)
    Transition_DisableToIdle();
  else if (GetState() == kPortSettingEnable && new_state == kExecuting)
    Transition_EnableToExecuting();
  else if (GetState() == kPortSettingEnable && new_state == kIdle)
    Transition_EnableToIdle();
  else if (GetState() == kExecuting && new_state == kIdle)
    Transition_ExecutingToIdle();
  else if (GetState() == kIdle && new_state == kLoaded)
    Transition_IdleToLoaded();
  else if (GetState() == kLoaded && new_state == kEmpty)
    Transition_LoadedToEmpty();
  else if (new_state == kError)
    Transition_Error();
}

void OmxVideoDecoder::PostDoneStateTransitionTask() {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &OmxVideoDecoder::DoneStateTransitionTask));
}

void OmxVideoDecoder::DoneStateTransitionTask() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (GetState() == kError) {
    ReportError();
    return;
  }

  // Save the current state and completes the transition.
  State old_state = GetState();
  SetState(GetNextState());

  // The following list is to perform a state transition automatically
  // based on the last transition done:
  //
  //    LAST TRANSITION       NEXT TRANSITION
  //
  // 1. Empty -> Loaded       Laoded -> Idle
  // 2. Loaded -> Idle        Idle   -> Executing
  // 3. Idle -> Executing
  //
  // Because of the above reoute, once we kick start the transition
  // from empty to loaded, this method will automatically route it
  // executing eventually.
  //
  // The following sequence is for transition to the stopped state.
  //
  //    LAST TRANSITION       NEXT TRANSITION
  //
  // 4. Executing -> Idle     Idle -> Loaded
  // 5. Idle -> Loaded        Loaded -> Empty
  // TODO(hclam): should go to Stopped instead of Empty.
  //
  // During dynamic port seeting, the route of state transition is:
  //
  //    LAST TRANSITION       NEXT TRANSITION
  //
  // 6. Executing -> Disable  Disable -> Enable
  // 7. Disable -> Enable     Enable -> Executing
  if (old_state == kEmpty && GetState() == kLoaded)
    StateTransitionTask(kIdle);
  else if (old_state == kLoaded && GetState() == kIdle)
    StateTransitionTask(kExecuting);
  else if (old_state == kIdle && GetState() == kExecuting) {
    // TODO(hclam): It is a little too late to issue read requests.
    // This seems to introduce some latencies.
    InitialEmptyBuffer();
    InitialFillBuffer();
  }
  else if (old_state == kExecuting && GetState() == kPortSettingDisable)
    StateTransitionTask(kPortSettingEnable);
  else if (old_state == kPortSettingDisable && GetState() == kPortSettingEnable)
    StateTransitionTask(kExecuting);
  else if (old_state == kPortSettingEnable && GetState() == kExecuting) {
    InitialFillBuffer();
  }
  else if (old_state == kPortSettingDisable && GetState() == kIdle)
    StateTransitionTask(kLoaded);
  else if (old_state == kPortSettingEnable && GetState() == kIdle)
    StateTransitionTask(kLoaded);
  else if (old_state == kExecuting && GetState() == kIdle)
    StateTransitionTask(kLoaded);
  else if (old_state == kIdle && GetState() == kLoaded)
    StateTransitionTask(kEmpty);
  else if (old_state == kLoaded && GetState() == kEmpty)
    DoneStop();
  else {
    NOTREACHED() << "Invalid state transition";
  }
}

void OmxVideoDecoder::DoneStop() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!stop_callback_.get())
    return;
  stop_callback_->Run();
  stop_callback_.reset();
}

void OmxVideoDecoder::ReportError() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!error_callback_.get())
    return;
  error_callback_->Run();
  error_callback_.reset();
}

bool OmxVideoDecoder::CanEmptyBuffer() {
  // We can call empty buffer while we are in executing or enabling / disabling
  // the output port.
  return (GetState() == kExecuting || GetState() == kPortSettingDisable ||
          GetState() == kPortSettingEnable) &&
      (GetNextState() == kExecuting || GetNextState() == kPortSettingDisable ||
       GetNextState() == kPortSettingEnable);
}

bool OmxVideoDecoder::CanFillBuffer() {
  // Make sure that we are staying in the executing state.
  return GetState() == kExecuting && GetState() == GetNextState();
}

bool OmxVideoDecoder::CanAcceptInput() {
  // We can't take input buffer when in error state.
  // TODO(hclam): Reject when in stopped state.
  return GetState() != kError;
}

bool OmxVideoDecoder::CanAcceptOutput() {
  // Don't output request when in error state.
  // TODO(hclam): Reject when in stopped state.
  return GetState() != kError;
}

void OmxVideoDecoder::EmptyBufferCompleteTask(OMX_BUFFERHEADERTYPE* buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanEmptyBuffer())
    return;

  // Enqueue the available buffer beacuse the decoder has consumed it.
  available_input_buffers_.push(buffer);

  // Try to feed more data into the decoder.
  EmptyBufferTask();
}

void OmxVideoDecoder::EmptyBufferTask() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanEmptyBuffer())
    return;

  // Loop for all available input data and input buffer for the
  // decoder. When input has reached EOS  we need to stop.
  while (!input_queue_.empty() &&
         !available_input_buffers_.empty() &&
         !input_eos_) {
    InputBuffer* buffer = input_queue_.front().first;
    FeedCallback* callback = input_queue_.front().second;
    OMX_BUFFERHEADERTYPE* omx_buffer = available_input_buffers_.front();
    available_input_buffers_.pop();

    // Read into |omx_buffer|.
    input_eos_ = buffer->Eos();
    int filled = buffer->Read(omx_buffer->pBuffer, input_buffer_size_);
    if (buffer->Used()) {
      input_queue_.pop();
      callback->Run(buffer);
      delete callback;
    }

    omx_buffer->nInputPortIndex = input_port_;
    omx_buffer->nOffset = 0;
    omx_buffer->nFlags = 0;
    omx_buffer->nFilledLen = filled;
    omx_buffer->pAppPrivate = this;
    omx_buffer->nFlags |= input_eos_ ? OMX_BUFFERFLAG_EOS : 0;

    // Give this buffer to OMX.
    OMX_ERRORTYPE ret = OMX_EmptyThisBuffer(decoder_handle_, omx_buffer);
    if (ret != OMX_ErrorNone) {
      printf("ERROR - OMX_EmptyThisBuffer failed with result %d\n", ret);
      StateTransitionTask(kError);
      return;
    }
  }
}

void OmxVideoDecoder::FillBufferCompleteTask(OMX_BUFFERHEADERTYPE* buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanFillBuffer())
    return;

  // Enqueue the decoded buffer.
  available_output_buffers_.push(buffer);

  // Fulfill read requests and read more from decoder.
  FillBufferTask();
}

void OmxVideoDecoder::FillBufferTask() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanFillBuffer())
    return;

  // Loop for all available output buffers and output requests. When we hit
  // EOS then stop.
  while (!output_queue_.empty() &&
         !available_output_buffers_.empty() &&
         !output_eos_) {
    ReadCallback* callback = output_queue_.front();
    output_queue_.pop();
    OMX_BUFFERHEADERTYPE* omx_buffer = available_output_buffers_.front();
    available_output_buffers_.pop();

    // Give the output data to the callback but it doesn't own this buffer.
    callback->RunWithParams(
        MakeTuple(omx_buffer->pBuffer,
                  static_cast<int>(omx_buffer->nFilledLen)));
    delete callback;

    if (omx_buffer->nFlags & OMX_BUFFERFLAG_EOS)
      output_eos_ = true;

    omx_buffer->nOutputPortIndex = output_port_;
    omx_buffer->pAppPrivate = this;
    omx_buffer->nFlags &= ~OMX_BUFFERFLAG_EOS;
    OMX_ERRORTYPE ret = OMX_FillThisBuffer(decoder_handle_, omx_buffer);
    if (OMX_ErrorNone != ret) {
      printf("Error - OMX_FillThisBuffer failed with result %d\n", ret);
      StateTransitionTask(kError);
      return;
    }
  }
}

void OmxVideoDecoder::InitialEmptyBuffer() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanEmptyBuffer())
    return;

  // Use EmptyBuffer() to use available input buffers to feed decoder.
  EmptyBufferTask();
}

void OmxVideoDecoder::InitialFillBuffer() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanFillBuffer())
    return;

  // We'll use all the available output buffers so clear the queue
  // just to be safe.
  while (!available_output_buffers_.empty()) {
    available_output_buffers_.pop();
  }

  // Ask the decoder to fill the output buffers.
  for (size_t i = 0; i < output_buffers_.size(); ++i) {
    OMX_BUFFERHEADERTYPE* omx_buffer = output_buffers_[i];
    omx_buffer->nOutputPortIndex = output_port_;
    omx_buffer->pAppPrivate = this;
    // Need to clear the EOS flag.
    omx_buffer->nFlags &= ~OMX_BUFFERFLAG_EOS;
    OMX_ERRORTYPE ret = OMX_FillThisBuffer(decoder_handle_, omx_buffer);

    if (OMX_ErrorNone != ret) {
      printf("Error - OMX_FillThisBuffer failed with result %d\n", ret);
      StateTransitionTask(kError);
      return;
    }
  }
}

void OmxVideoDecoder::EventHandlerInternal(OMX_HANDLETYPE component,
                                           OMX_EVENTTYPE event,
                                           OMX_U32 data1,
                                           OMX_U32 data2,
                                           OMX_PTR event_data) {
  switch(event) {
    case OMX_EventCmdComplete: {
      // If the last command was successful, we have completed
      // a state transition. So notify that we have done it
      // accordingly.
      OMX_COMMANDTYPE cmd = static_cast<OMX_COMMANDTYPE>(data1);
      if (cmd == OMX_CommandPortEnable) {
        PostDoneStateTransitionTask();
      } else if (cmd == OMX_CommandPortDisable) {
        PostDoneStateTransitionTask();
      } else if (cmd == OMX_CommandStateSet) {
        PostDoneStateTransitionTask();
      } else {
        printf("Unknown command completed\n");
      }
      break;
    }
    case OMX_EventError:
      if (OMX_ErrorInvalidState == (OMX_ERRORTYPE)data1) {
        // TODO(hclam): what to do here?
      }
      PostStateTransitionTask(kError);
      break;
    case OMX_EventPortSettingsChanged:
      PostStateTransitionTask(kPortSettingDisable);
      break;
    default:
      printf("Warning - Unknown event received\n");
      break;
  }
}

void OmxVideoDecoder::EmptyBufferCallbackInternal(
    OMX_HANDLETYPE component,
    OMX_BUFFERHEADERTYPE* buffer) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &OmxVideoDecoder::EmptyBufferCompleteTask, buffer));
}

void OmxVideoDecoder::FillBufferCallbackInternal(
    OMX_HANDLETYPE component,
    OMX_BUFFERHEADERTYPE* buffer) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &OmxVideoDecoder::FillBufferCompleteTask, buffer));
}

// static
OMX_ERRORTYPE OmxVideoDecoder::EventHandler(OMX_HANDLETYPE component,
                                            OMX_PTR priv_data,
                                            OMX_EVENTTYPE event,
                                            OMX_U32 data1,
                                            OMX_U32 data2,
                                            OMX_PTR event_data) {
  OmxVideoDecoder* decoder = static_cast<OmxVideoDecoder*>(priv_data);
  decoder->EventHandlerInternal(component, event, data1, data2, event_data);
  return OMX_ErrorNone;
}

// static
OMX_ERRORTYPE OmxVideoDecoder::EmptyBufferCallback(
    OMX_HANDLETYPE component,
    OMX_PTR priv_data,
    OMX_BUFFERHEADERTYPE* buffer) {
  OmxVideoDecoder* decoder = static_cast<OmxVideoDecoder*>(priv_data);
  decoder->EmptyBufferCallbackInternal(component, buffer);
  return OMX_ErrorNone;
}

// static
OMX_ERRORTYPE OmxVideoDecoder::FillBufferCallback(
    OMX_HANDLETYPE component,
    OMX_PTR priv_data,
    OMX_BUFFERHEADERTYPE* buffer) {
  OmxVideoDecoder* decoder = static_cast<OmxVideoDecoder*>(priv_data);
  decoder->FillBufferCallbackInternal(component, buffer);
  return OMX_ErrorNone;
}

}  // namespace media
