// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/omx_video_decode_accelerator.h"

#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "content/common/gpu/gles2_texture_to_egl_image_translator.h"
#include "content/common/gpu/gpu_channel.h"
#include "media/base/bitstream_buffer.h"
#include "media/video/picture.h"

// Helper typedef for input buffers.  This is used as the pAppPrivate field of
// OMX_BUFFERHEADERTYPEs of input buffers, to point to the data associated with
// them.
typedef std::pair<scoped_ptr<base::SharedMemory>, int32> SharedMemoryAndId;

enum { kNumPictureBuffers = 4 };

// Open the libnvomx here for now.
void* omx_handle = dlopen("libnvomx.so", RTLD_NOW);

typedef OMX_ERRORTYPE (*OMXInit)();
typedef OMX_ERRORTYPE (*OMXGetHandle)(
    OMX_HANDLETYPE*, OMX_STRING, OMX_PTR, OMX_CALLBACKTYPE*);
typedef OMX_ERRORTYPE (*OMXGetComponentsOfRole)(OMX_STRING, OMX_U32*, OMX_U8**);
typedef OMX_ERRORTYPE (*OMXFreeHandle)(OMX_HANDLETYPE);
typedef OMX_ERRORTYPE (*OMXDeinit)();

OMXInit omx_init = reinterpret_cast<OMXInit>(dlsym(omx_handle, "OMX_Init"));
OMXGetHandle omx_gethandle =
    reinterpret_cast<OMXGetHandle>(dlsym(omx_handle, "OMX_GetHandle"));
OMXGetComponentsOfRole omx_get_components_of_role =
    reinterpret_cast<OMXGetComponentsOfRole>(
        dlsym(omx_handle, "OMX_GetComponentsOfRole"));
OMXFreeHandle omx_free_handle =
    reinterpret_cast<OMXFreeHandle>(dlsym(omx_handle, "OMX_FreeHandle"));
OMXDeinit omx_deinit =
    reinterpret_cast<OMXDeinit>(dlsym(omx_handle, "OMX_Deinit"));

static bool AreOMXFunctionPointersInitialized() {
  return (omx_init && omx_gethandle && omx_get_components_of_role &&
          omx_free_handle && omx_deinit);
}

OmxVideoDecodeAccelerator::OmxVideoDecodeAccelerator(
    media::VideoDecodeAccelerator::Client* client,
    MessageLoop* message_loop)
    : message_loop_(message_loop),
      component_handle_(NULL),
      width_(-1),
      height_(-1),
      input_buffer_count_(0),
      input_buffer_size_(0),
      input_port_(0),
      input_buffers_at_component_(0),
      output_port_(0),
      output_buffers_at_component_(0),
      client_(client),
      on_port_disable_event_func_(NULL),
      on_port_enable_event_func_(NULL),
      on_state_event_func_(NULL),
      on_flush_event_func_(NULL),
      on_buffer_flag_event_func_(NULL) {
  if (!AreOMXFunctionPointersInitialized()) {
    LOG(ERROR) << "Failed to load openmax library";
    return;
  }
  OMX_ERRORTYPE result = omx_init();
  if (result != OMX_ErrorNone)
    LOG(ERROR) << "Failed to init OpenMAX core";
}

OmxVideoDecodeAccelerator::~OmxVideoDecodeAccelerator() {
  DCHECK(free_input_buffers_.empty());
  DCHECK_EQ(0, input_buffers_at_component_);
  DCHECK_EQ(0, output_buffers_at_component_);
  DCHECK(pictures_.empty());
}

void OmxVideoDecodeAccelerator::SetEglState(
    EGLDisplay egl_display, EGLContext egl_context) {
  egl_display_ = egl_display;
  egl_context_ = egl_context;
}

void OmxVideoDecodeAccelerator::GetConfigs(
    const std::vector<uint32>& requested_configs,
    std::vector<uint32>* matched_configs) {
  // TODO(vhiremath@nvidia.com) use this properly
  NOTIMPLEMENTED();
}

// This is to initialize the OMX data structures to default values.
template <typename T>
static void InitParam(const OmxVideoDecodeAccelerator& dec, T* param) {
  memset(param, 0, sizeof(T));
  param->nVersion.nVersion = 0x00000101;
  param->nSize = sizeof(T);
}

bool OmxVideoDecodeAccelerator::Initialize(const std::vector<uint32>& config) {
  // TODO(vhiremath@nvidia.com) get these actual values from config
  // Assume qvga for now
  width_ = 320;
  height_ = 240;

  client_state_ = OMX_StateLoaded;
  if (!CreateComponent()) {
    StopOnError();
    return false;
  }

  // Transition component to Idle state
  DCHECK(!on_state_event_func_);
  on_state_event_func_ =
      &OmxVideoDecodeAccelerator::OnStateChangeLoadedToIdle;
  if (!TransitionToState(OMX_StateIdle)) {
    LOG(ERROR) << "TransitionToState(OMX_StateIdle) error";
    StopOnError();
    return false;
  }

  if (!AllocateInputBuffers()) {
    LOG(ERROR) << "OMX_AllocateBuffer() Input buffer error";
    StopOnError();
    return false;
  }

  // In order to allocate output buffers we need to have textures in hand, so we
  // call ProvidePictureBuffers on the client and complete initialization in
  // AssignGLESBuffers.
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&Client::ProvidePictureBuffers, base::Unretained(client_),
                 static_cast<int32>(kNumPictureBuffers),
                 gfx::Size(width_, height_),
                 PICTUREBUFFER_MEMORYTYPE_GL_TEXTURE));

  return true;
}

bool OmxVideoDecodeAccelerator::CreateComponent() {
  OMX_CALLBACKTYPE omx_accelerator_callbacks = {
    &OmxVideoDecodeAccelerator::EventHandler,
    &OmxVideoDecodeAccelerator::EmptyBufferCallback,
    &OmxVideoDecodeAccelerator::FillBufferCallback
  };
  OMX_ERRORTYPE result = OMX_ErrorNone;

  // 1. Set the role and get all components of this role.
  // TODO(vhiremath@nvidia.com) Get this role_name from the configs
  // For now hard coding to avc.
  const char* role_name = "video_decoder.avc";
  OMX_U32 num_roles = 0;
  // Get all the components with this role.
  result = (*omx_get_components_of_role)(
      const_cast<OMX_STRING>(role_name), &num_roles, 0);
  if (result != OMX_ErrorNone || num_roles == 0) {
    LOG(ERROR) << "Unsupported Role: " << role_name << ", " << result;
    StopOnError();
    return false;
  }

  // We haven't seen HW that needs more yet,
  // but there is no reason not to raise.
  const OMX_U32 kMaxRolePerComponent = 3;
  CHECK_LT(num_roles, kMaxRolePerComponent);

  scoped_array<scoped_array<OMX_U8> > component_names(
      new scoped_array<OMX_U8>[num_roles]);
  for (size_t i = 0; i < num_roles; ++i)
    component_names[i].reset(new OMX_U8[OMX_MAX_STRINGNAME_SIZE]);
  result = (*omx_get_components_of_role)(
      const_cast<OMX_STRING>(role_name),
      &num_roles, reinterpret_cast<OMX_U8**>(component_names.get()));

  // Use first component only. Copy the name of the first component
  // so that we could free the memory.
  std::string component_name;
  if (result == OMX_ErrorNone)
    component_name = reinterpret_cast<char*>(component_names[0].get());

  if (result != OMX_ErrorNone || num_roles == 0) {
    LOG(ERROR) << "Unsupported Role: " << component_name.c_str();
    StopOnError();
    return false;
  }

  // 3. Get the handle to the component.
  //    After OMX_GetHandle(), the component is in loaded state.
  OMX_STRING component = const_cast<OMX_STRING>(component_name.c_str());
  result = omx_gethandle(&component_handle_, component, this,
                         &omx_accelerator_callbacks);
  if (result != OMX_ErrorNone) {
    LOG(ERROR) << "Failed to Load the component: " << component;
    StopOnError();
    return false;
  }

  // 4. Get the port information. This will obtain information about the
  //    number of ports and index of the first port.
  OMX_PORT_PARAM_TYPE port_param;
  InitParam(*this, &port_param);
  result = OMX_GetParameter(component_handle_, OMX_IndexParamVideoInit,
                            &port_param);
  if ((result != OMX_ErrorNone) || (port_param.nPorts != 2)) {
    LOG(ERROR) << "Failed to get Port Param: "
               << result << ", " << port_param.nPorts;
    StopOnError();
    return false;
  }
  input_port_ = port_param.nStartPortNumber;
  output_port_ = input_port_ + 1;
  // 5. Set role for the component because components can have multiple roles.
  OMX_PARAM_COMPONENTROLETYPE role_type;
  InitParam(*this, &role_type);
  base::strlcpy(reinterpret_cast<char*>(role_type.cRole),
                role_name,
                OMX_MAX_STRINGNAME_SIZE);

  result = OMX_SetParameter(component_handle_,
                            OMX_IndexParamStandardComponentRole,
                            &role_type);
  if (result != OMX_ErrorNone) {
    LOG(ERROR) << "Failed to Set Role";
    StopOnError();
    return false;
  }

  // 7. Populate input-buffer-related members based on input port data.
  OMX_PARAM_PORTDEFINITIONTYPE port_format;
  InitParam(*this, &port_format);
  port_format.nPortIndex = input_port_;
  result = OMX_GetParameter(component_handle_,
                            OMX_IndexParamPortDefinition,
                            &port_format);
  if (result != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamPortDefinition) failed";
    StopOnError();
    return false;
  }
  if (OMX_DirInput != port_format.eDir) {
    LOG(ERROR) << "Expected input port";
    StopOnError();
    return false;
  }
  input_buffer_count_ = port_format.nBufferCountActual;
  input_buffer_size_ = port_format.nBufferSize;

  // OMX_IndexParamPortDefinition on output port to be done in
  // AllocateOutputBuffers.
  // Since at this point we dont know if we will be using system memory
  // or egl-image for decoding.
  // We get this info in AssignPictureBuffers() from plugin.

  return true;
}

bool OmxVideoDecodeAccelerator::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK(!free_input_buffers_.empty());

  if (!CanAcceptInput()) {
    return false;
  }

  OMX_BUFFERHEADERTYPE* omx_buffer = free_input_buffers_.front();
  free_input_buffers_.pop();

  // Setup |omx_buffer|.
  scoped_ptr<base::SharedMemory> shm(
      new base::SharedMemory(bitstream_buffer.handle(), true));
  if (!shm->Map(bitstream_buffer.size())) {
    LOG(ERROR) << "Failed to SharedMemory::Map().";
    return false;
  }
  SharedMemoryAndId* input_buffer_details = new SharedMemoryAndId();
  input_buffer_details->first.reset(shm.release());
  input_buffer_details->second = bitstream_buffer.id();
  DCHECK(!omx_buffer->pAppPrivate);
  omx_buffer->pAppPrivate = input_buffer_details;
  omx_buffer->pBuffer =
      static_cast<OMX_U8*>(input_buffer_details->first->memory());
  omx_buffer->nFilledLen = bitstream_buffer.size();
  omx_buffer->nAllocLen = omx_buffer->nFilledLen;
  omx_buffer->nFlags &= ~OMX_BUFFERFLAG_EOS;
  // Abuse the header's nTimeStamp field to propagate the bitstream buffer ID to
  // the output buffer's nTimeStamp field, so we can report it back to the
  // client in PictureReady().
  omx_buffer->nTimeStamp = bitstream_buffer.id();

  // Give this buffer to OMX.
  OMX_ERRORTYPE result = OMX_ErrorNone;
  result = OMX_EmptyThisBuffer(component_handle_, omx_buffer);
  if (result != OMX_ErrorNone) {
    LOG(ERROR) << "OMX_EmptyThisBuffer() failed with result " << result;
    StopOnError();
    return false;
  }
  input_buffers_at_component_++;
  return true;
}

void OmxVideoDecodeAccelerator::AssignGLESBuffers(
    const std::vector<media::GLESBuffer>& buffers) {
  CHECK_EQ(message_loop_, MessageLoop::current());
  for (size_t i = 0; i < buffers.size(); ++i) {
    CHECK(pictures_.insert(std::make_pair(
        buffers[i].id(), OutputPicture(buffers[i], NULL))).second);
  }

  if (pictures_.size() < kNumPictureBuffers)
    return;  // get all the buffers first.
  DCHECK_EQ(pictures_.size(), kNumPictureBuffers);

  // Obtain the information about the output port.
  OMX_PARAM_PORTDEFINITIONTYPE port_format;
  InitParam(*this, &port_format);
  port_format.nPortIndex = output_port_;
  OMX_ERRORTYPE result = OMX_GetParameter(component_handle_,
                                          OMX_IndexParamPortDefinition,
                                          &port_format);
  if (result != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamPortDefinition) failed";
    StopOnError();
    return;
  }
  if (OMX_DirOutput != port_format.eDir) {
    LOG(ERROR) << "Expect Output Port";
    StopOnError();
    return;
  }

  port_format.nBufferCountActual = kNumPictureBuffers;
  port_format.nBufferCountMin = kNumPictureBuffers;

  result = OMX_SetParameter(component_handle_,
                            OMX_IndexParamPortDefinition,
                            &port_format);
  if (result != OMX_ErrorNone) {
    LOG(ERROR) << "SetParameter(OMX_IndexParamPortDefinition) failed";
    StopOnError();
    return;
  }

  if (!AllocateOutputBuffers()) {
    LOG(ERROR) << "OMX_AllocateBuffer() Output buffer error";
    StopOnError();
    return;
  }
}

void OmxVideoDecodeAccelerator::AssignSysmemBuffers(
    const std::vector<media::SysmemBuffer>& buffers) {
  // We don't support decoding to system memory because we don't have HW that
  // needs it yet.
  NOTIMPLEMENTED();
}

void OmxVideoDecodeAccelerator::ReusePictureBuffer(int32 picture_buffer_id) {
  // TODO(vhiremath@nvidia.com) Avoid leaking of the picture buffer.
  if (!CanFillBuffer())
    return;

  OutputPictureById::iterator it = pictures_.find(picture_buffer_id);
  if (it == pictures_.end()) {
    LOG(DFATAL) << "Missing picture buffer id: " << picture_buffer_id;
    return;
  }
  OutputPicture& output_picture = it->second;

  output_buffers_at_component_++;
  OMX_ERRORTYPE result =
      OMX_FillThisBuffer(component_handle_, output_picture.omx_buffer_header);
  if (result != OMX_ErrorNone) {
    LOG(ERROR) << "OMX_FillThisBuffer() failed with result " << result;
    StopOnError();
    return;
  }
}

void OmxVideoDecodeAccelerator::InitialFillBuffer() {
  if (!CanFillBuffer())
    return;

  // Ask the decoder to fill the output buffers.
  for (OutputPictureById::iterator it = pictures_.begin();
       it != pictures_.end(); ++it) {
    OMX_BUFFERHEADERTYPE* omx_buffer = it->second.omx_buffer_header;
    DCHECK(omx_buffer);
    // Clear EOS flag.
    omx_buffer->nFlags &= ~OMX_BUFFERFLAG_EOS;
    omx_buffer->nOutputPortIndex = output_port_;
    output_buffers_at_component_++;
    OMX_ERRORTYPE result = OMX_FillThisBuffer(component_handle_, omx_buffer);
    if (result != OMX_ErrorNone) {
      LOG(ERROR) << "OMX_FillThisBuffer() failed with result " << result;
      StopOnError();
      return;
    }
  }
}

bool OmxVideoDecodeAccelerator::Flush() {
  OMX_STATETYPE il_state;
  OMX_GetState(component_handle_, &il_state);
  DCHECK_EQ(il_state, OMX_StateExecuting);
  if (il_state != OMX_StateExecuting) {
    client_->NotifyFlushDone();
    return false;
  }
  on_buffer_flag_event_func_ = &OmxVideoDecodeAccelerator::FlushBegin;

  OMX_BUFFERHEADERTYPE* omx_buffer = free_input_buffers_.front();
  free_input_buffers_.pop();

  omx_buffer->nFilledLen = 0;
  omx_buffer->nAllocLen = omx_buffer->nFilledLen;
  omx_buffer->nFlags |= OMX_BUFFERFLAG_EOS;
  omx_buffer->nTimeStamp = 0;
  // Give this buffer to OMX.
  OMX_ERRORTYPE result = OMX_ErrorNone;
  result = OMX_EmptyThisBuffer(component_handle_, omx_buffer);
  if (result != OMX_ErrorNone) {
    LOG(ERROR) << "OMX_EmptyThisBuffer() failed with result " << result;
    StopOnError();
    return false;
  }
  input_buffers_at_component_++;
  return true;
}

void OmxVideoDecodeAccelerator::FlushBegin() {
  VLOG(1) << "Starting actual flush for EOS";
  DCHECK(!on_state_event_func_);
  on_state_event_func_ = &OmxVideoDecodeAccelerator::PauseFromExecuting;
  TransitionToState(OMX_StatePause);
}

void OmxVideoDecodeAccelerator::PauseFromExecuting(OMX_STATETYPE ignored) {
  on_state_event_func_ = NULL;
  FlushIOPorts();
}

void OmxVideoDecodeAccelerator::FlushIOPorts() {
  // TODO(vhiremath@nvidia.com) review again for trick modes.
  VLOG(1) << "FlushIOPorts";

  // Flush input port first.
  DCHECK(!on_flush_event_func_);
  on_flush_event_func_ = &OmxVideoDecodeAccelerator::InputPortFlushDone;
  OMX_ERRORTYPE result;
  result = OMX_SendCommand(component_handle_,
                           OMX_CommandFlush,
                           input_port_, 0);
  if (result != OMX_ErrorNone) {
    LOG(ERROR) << "OMX_SendCommand(OMX_CommandFlush) failed";
    StopOnError();
    return;
  }
}

void OmxVideoDecodeAccelerator::InputPortFlushDone(int port) {
  DCHECK_EQ(port, input_port_);
  VLOG(1) << "Input Port has been flushed";
  DCHECK_EQ(input_buffers_at_component_, 0);
  // Flush output port next.
  DCHECK(!on_flush_event_func_);
  on_flush_event_func_ = &OmxVideoDecodeAccelerator::OutputPortFlushDone;
  if (OMX_ErrorNone !=
      OMX_SendCommand(component_handle_,
                      OMX_CommandFlush,
                      output_port_, 0)) {
    LOG(ERROR) << "OMX_SendCommand(OMX_CommandFlush) failed";
    StopOnError();
    return;
  }
}

void OmxVideoDecodeAccelerator::OutputPortFlushDone(int port) {
  DCHECK_EQ(port, output_port_);

  VLOG(1) << "Output Port has been flushed";
  DCHECK_EQ(output_buffers_at_component_, 0);
  client_state_ = OMX_StatePause;
  OnPortCommandFlush(OMX_StateExecuting);
}

bool OmxVideoDecodeAccelerator::Abort() {
  CHECK_EQ(message_loop_, MessageLoop::current());
  // TODO(vhiremath@nvidia.com)
  // Need more thinking on this to handle w.r.t OMX.
  // There is no explicit UnInitialize call for this.
  // Also review again for trick modes.
  client_->NotifyAbortDone();
  return true;
}

// Event callback during initialization to handle DoneStateSet to idle
void OmxVideoDecodeAccelerator::OnStateChangeLoadedToIdle(OMX_STATETYPE state) {
  CHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK(!on_state_event_func_);
  DCHECK_EQ(client_state_, OMX_StateLoaded);
  DCHECK_EQ(OMX_StateIdle, state);

  VLOG(1) << "OMX video decode engine is in Idle";

  on_state_event_func_ =
      &OmxVideoDecodeAccelerator::OnStateChangeIdleToExecuting;
  if (!TransitionToState(OMX_StateExecuting))
    return;
}

// Event callback during initialization to handle DoneStateSet to executing
void OmxVideoDecodeAccelerator::OnStateChangeIdleToExecuting(
    OMX_STATETYPE state) {
  CHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(OMX_StateExecuting, state);
  DCHECK(!on_state_event_func_);
  VLOG(1) << "OMX video decode engine is in Executing";

  client_state_ = OMX_StateExecuting;
  // This will kickoff the actual decoding
  InitialFillBuffer();

  client_->NotifyInitializeDone();
}

// Send state transition command to component.
bool OmxVideoDecodeAccelerator::TransitionToState(OMX_STATETYPE new_state) {
  DCHECK(on_state_event_func_);
  OMX_ERRORTYPE result = OMX_SendCommand(
      component_handle_, OMX_CommandStateSet, new_state, 0);
  if (result != OMX_ErrorNone) {
    LOG(ERROR) << "SendCommand(OMX_CommandStateSet) failed";
    StopOnError();
    return false;
  }
  return true;
}

void OmxVideoDecodeAccelerator::OnPortCommandFlush(OMX_STATETYPE state) {
  DCHECK_EQ(state, OMX_StateExecuting);
  CHECK_EQ(message_loop_, MessageLoop::current());

  VLOG(1) << "Deinit from Executing";
  DCHECK(!on_state_event_func_);
  on_state_event_func_ =
      &OmxVideoDecodeAccelerator::OnStateChangeExecutingToIdle;
  TransitionToState(OMX_StateIdle);
}

void OmxVideoDecodeAccelerator::OnStateChangeExecutingToIdle(
    OMX_STATETYPE state) {
  DCHECK_EQ(state, OMX_StateIdle);
  DCHECK(!on_state_event_func_);

  VLOG(1) << "Deinit from Idle";
  on_state_event_func_ =
      &OmxVideoDecodeAccelerator::OnStateChangeIdleToLoaded;
  TransitionToState(OMX_StateLoaded);

  if (!input_buffers_at_component_)
    FreeInputBuffers();

  if (!output_buffers_at_component_)
    FreeOutputBuffers();
}

void OmxVideoDecodeAccelerator::OnStateChangeIdleToLoaded(OMX_STATETYPE state) {
  DCHECK_EQ(state, OMX_StateLoaded);
  CHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK(!on_state_event_func_);

  VLOG(1) << "Idle to Loaded";

  if (component_handle_) {
    OMX_ERRORTYPE result = (*omx_free_handle)(component_handle_);
    if (result != OMX_ErrorNone)
        LOG(ERROR) << "OMX_FreeHandle() error. Error code: " << result;
    component_handle_ = NULL;
  }
  client_state_ = OMX_StateLoaded;
  (*omx_deinit)();
  VLOG(1) << "OMX Deinit Clean exit done";
  client_->NotifyFlushDone();
}

void OmxVideoDecodeAccelerator::StopOnError() {
  OMX_STATETYPE il_state;
  OMX_GetState(component_handle_, &il_state);
  client_state_ = OMX_StateInvalid;
  switch (il_state) {
    case OMX_StateExecuting:
      OnPortCommandFlush(OMX_StateExecuting);
      return;
    case OMX_StateIdle:
      OnStateChangeExecutingToIdle(OMX_StateIdle);
      return;
    case OMX_StateLoaded:
      OnStateChangeIdleToLoaded(OMX_StateLoaded);
      return;
    default:
      // LOG unexpected state or just ignore?
      return;
  }
}

bool OmxVideoDecodeAccelerator::AllocateInputBuffers() {
  for (int i = 0; i < input_buffer_count_; ++i) {
    OMX_BUFFERHEADERTYPE* buffer;
    // While registering the buffer header we use fake buffer information
    // (length 0, at memory address 0x1) to fake out the "safety" check in
    // OMX_UseBuffer.  When it comes time to actually use this header in Decode
    // we set these fields to their real values (for the duration of that
    // Decode).
    OMX_ERRORTYPE result =
        OMX_UseBuffer(component_handle_, &buffer, input_port_,
                      NULL, /* pAppPrivate gets set in Decode(). */
                      0, reinterpret_cast<OMX_U8*>(0x1));
    if (result != OMX_ErrorNone)
      return false;
    buffer->nInputPortIndex = input_port_;
    buffer->nOffset = 0;
    buffer->nFlags = 0;
    free_input_buffers_.push(buffer);
  }
  return true;
}

bool OmxVideoDecodeAccelerator::AllocateOutputBuffers() {
  CHECK_EQ(message_loop_, MessageLoop::current());
  static Gles2TextureToEglImageTranslator texture2eglImage_translator;

  gfx::Size decoded_pixel_size(width_, height_);
  gfx::Size visible_pixel_size(width_, height_);
  for (OutputPictureById::iterator it = pictures_.begin();
       it != pictures_.end(); ++it) {
    media::GLESBuffer& gles_buffer = it->second.gles_buffer;
    OMX_BUFFERHEADERTYPE** omx_buffer = &it->second.omx_buffer_header;
    DCHECK(!*omx_buffer);
    void* egl = texture2eglImage_translator.TranslateToEglImage(
        egl_display_, egl_context_, gles_buffer.texture_id());
    OMX_ERRORTYPE result = OMX_UseEGLImage(
        component_handle_, omx_buffer, output_port_, &gles_buffer, egl);
    if (result != OMX_ErrorNone) {
      LOG(ERROR) << "OMX_UseEGLImage failed with: " << result;
      return false;
    }
    // Here we set a garbage bitstream buffer id, and then overwrite it before
    // passing to PictureReady.
    int garbage_bitstream_buffer_id = -1;
    (*omx_buffer)->pAppPrivate =
        new media::Picture(gles_buffer.id(), garbage_bitstream_buffer_id,
                           decoded_pixel_size, visible_pixel_size);
  }
  return true;
}

void OmxVideoDecodeAccelerator::FreeInputBuffers() {
  // Calls to OMX to free buffers.
  OMX_ERRORTYPE result;
  OMX_BUFFERHEADERTYPE* omx_buffer;
  while (!free_input_buffers_.empty()) {
    omx_buffer = free_input_buffers_.front();
    free_input_buffers_.pop();
    result = OMX_FreeBuffer(component_handle_, input_port_, omx_buffer);
    if (result != OMX_ErrorNone) {
      LOG(ERROR) << "OMX_CommandPortDisable failed with: " << result;
      StopOnError();
      return;
    }
  }
  VLOG(1) << "Input buffers freed.";
}

void OmxVideoDecodeAccelerator::FreeOutputBuffers() {
  // Calls to OMX to free buffers.
  OMX_ERRORTYPE result;
  for (OutputPictureById::iterator it = pictures_.begin();
       it != pictures_.end(); ++it) {
    OMX_BUFFERHEADERTYPE* omx_buffer = it->second.omx_buffer_header;
    CHECK(omx_buffer);
    delete reinterpret_cast<media::Picture*>(omx_buffer->pAppPrivate);
    result = OMX_FreeBuffer(component_handle_, output_port_, omx_buffer);
    if (result != OMX_ErrorNone) {
      LOG(ERROR) << "OMX_FreeBuffer failed with: " << result;
      StopOnError();
      return;
    }
    client_->DismissPictureBuffer(it->first);
  }
  pictures_.clear();
}

void OmxVideoDecodeAccelerator::OnPortSettingsChangedRun(
        int port, OMX_INDEXTYPE index) {
  // TODO(vhiremath@nvidia.com) visit again later
  // Port settings changes can be called during run time
  // changes in the resolution of video playback.
  // In this case, the component detects PortSettingsChanged
  // and sends the particular event to the IL-client.
  // This needs to be handled in this method.
  return;
}

void OmxVideoDecodeAccelerator::FillBufferDoneTask(
    OMX_BUFFERHEADERTYPE* buffer) {
  CHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_GT(output_buffers_at_component_, 0);
  output_buffers_at_component_--;

  // During the transition from Paused to Idle (e.g. during Flush()) all
  // pictures are sent back through here.  Avoid giving them to the client.
  // TODO(fischman): this is a hokey way to detect this condition.  The state
  // transitions in this class need to be rethought, and this implemented more
  // sanely.
  if (buffer->nFlags & OMX_BUFFERFLAG_EOS ||
      on_flush_event_func_ == &OmxVideoDecodeAccelerator::InputPortFlushDone ||
      on_flush_event_func_ == &OmxVideoDecodeAccelerator::OutputPortFlushDone) {
    return;
  }

  media::Picture* picture =
      reinterpret_cast<media::Picture*>(buffer->pAppPrivate);
  // See Decode() for an explanation of this abuse of nTimeStamp.
  picture->set_bitstream_buffer_id(buffer->nTimeStamp);
  client_->PictureReady(*picture);
}

void OmxVideoDecodeAccelerator::EmptyBufferDoneTask(
    OMX_BUFFERHEADERTYPE* buffer) {
  DCHECK_GT(input_buffers_at_component_, 0);
  CHECK_EQ(message_loop_, MessageLoop::current());
  free_input_buffers_.push(buffer);
  input_buffers_at_component_--;
  if (buffer->nFlags & OMX_BUFFERFLAG_EOS)
    return;

  // Retrieve the corresponding BitstreamBuffer's id and notify the client of
  // its completion.
  SharedMemoryAndId* input_buffer_details =
      reinterpret_cast<SharedMemoryAndId*>(buffer->pAppPrivate);
  DCHECK(input_buffer_details);
  buffer->pAppPrivate = NULL;
  client_->NotifyEndOfBitstreamBuffer(input_buffer_details->second);
  delete input_buffer_details;
}

void OmxVideoDecodeAccelerator::EventHandlerCompleteTask(OMX_EVENTTYPE event,
                                                         OMX_U32 data1,
                                                         OMX_U32 data2) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  switch (event) {
    case OMX_EventCmdComplete: {
      // If the last command was successful, we have completed
      // a state transition. So notify that we have done it
      // accordingly.
      OMX_COMMANDTYPE cmd = static_cast<OMX_COMMANDTYPE>(data1);
      switch (cmd) {
        case OMX_CommandPortDisable:
          if (on_port_disable_event_func_) {
            void (OmxVideoDecodeAccelerator::*func)(int) = NULL;
            std::swap(func, on_port_disable_event_func_);
            (this->*func)(static_cast<int>(data2));
          }
          break;
        case OMX_CommandPortEnable:
          if (on_port_enable_event_func_) {
            void (OmxVideoDecodeAccelerator::*func)(int) = NULL;
            std::swap(func, on_port_enable_event_func_);
            (this->*func)(static_cast<int>(data2));
          }
          break;
        case OMX_CommandStateSet:
          {
            void (OmxVideoDecodeAccelerator::*func)(OMX_STATETYPE state) = NULL;
            std::swap(func, on_state_event_func_);
            (this->*func)(static_cast<OMX_STATETYPE>(data2));
          }
          break;
        case OMX_CommandFlush:
          {
            void (OmxVideoDecodeAccelerator::*func)(int) = NULL;
            std::swap(func, on_flush_event_func_);
            (this->*func)(data2);
          }
          break;
        default:
          LOG(ERROR) << "Unknown command completed\n" << data1;
          break;
      }
      break;
    }
    case OMX_EventError:
      if (static_cast<OMX_ERRORTYPE>(data1) == OMX_ErrorInvalidState)
        StopOnError();
      break;
    case OMX_EventPortSettingsChanged:
      // TODO(vhiremath@nvidia.com) remove this hack
      // when all vendors observe same spec.
      if (data1 < OMX_IndexComponentStartUnused) {
        OnPortSettingsChangedRun(static_cast<int>(data1),
                                 static_cast<OMX_INDEXTYPE>(data2));
      } else {
        OnPortSettingsChangedRun(static_cast<int>(data2),
                                 static_cast<OMX_INDEXTYPE>(data1));
      }
      break;
    case OMX_EventBufferFlag:
      if (data1 == static_cast<OMX_U32>(output_port_)) {
        void (OmxVideoDecodeAccelerator::*func)() = NULL;
        std::swap(func, on_buffer_flag_event_func_);
        (this->*func)();
      }
      break;
    default:
      LOG(ERROR) << "Warning - Unknown event received\n";
      break;
  }
}

// static
OMX_ERRORTYPE OmxVideoDecodeAccelerator::EventHandler(OMX_HANDLETYPE component,
                                                      OMX_PTR priv_data,
                                                      OMX_EVENTTYPE event,
                                                      OMX_U32 data1,
                                                      OMX_U32 data2,
                                                      OMX_PTR event_data) {
  OmxVideoDecodeAccelerator* decoder =
      static_cast<OmxVideoDecodeAccelerator*>(priv_data);
  DCHECK_EQ(component, decoder->component_handle_);

  decoder->message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(decoder,
                        &OmxVideoDecodeAccelerator::EventHandlerCompleteTask,
                        event, data1, data2));

  return OMX_ErrorNone;
}

// static
OMX_ERRORTYPE OmxVideoDecodeAccelerator::EmptyBufferCallback(
    OMX_HANDLETYPE component,
    OMX_PTR priv_data,
    OMX_BUFFERHEADERTYPE* buffer) {
  OmxVideoDecodeAccelerator* decoder =
      static_cast<OmxVideoDecodeAccelerator*>(priv_data);
  DCHECK_EQ(component, decoder->component_handle_);

  decoder->message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          decoder,
          &OmxVideoDecodeAccelerator::EmptyBufferDoneTask, buffer));
  return OMX_ErrorNone;
}

// static
OMX_ERRORTYPE OmxVideoDecodeAccelerator::FillBufferCallback(
    OMX_HANDLETYPE component,
    OMX_PTR priv_data,
    OMX_BUFFERHEADERTYPE* buffer) {
  OmxVideoDecodeAccelerator* decoder =
      static_cast<OmxVideoDecodeAccelerator*>(priv_data);
  DCHECK_EQ(component, decoder->component_handle_);

  decoder->message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          decoder,
          &OmxVideoDecodeAccelerator::FillBufferDoneTask, buffer));
  return OMX_ErrorNone;
}

bool OmxVideoDecodeAccelerator::CanAcceptInput() {
  // We can't take input buffer when in error state.
  return (client_state_ != OMX_StateInvalid &&
          client_state_ != OMX_StatePause &&
          client_state_ != OMX_StateLoaded);
}

bool OmxVideoDecodeAccelerator::CanFillBuffer() {
  // Make sure component is in the executing state and end-of-stream
  // has not been reached.
  OMX_ERRORTYPE result;
  OMX_STATETYPE il_state;
  if (client_state_ == OMX_StateLoaded)
    return false;
  result = OMX_GetState(component_handle_, &il_state);
  if (result != OMX_ErrorNone) {
    LOG(ERROR) << "SendCommand(OMX_CommandPortDisable) failed";
    StopOnError();
    return false;
  }
  return (il_state == OMX_StateExecuting);
}

// Send command to disable/enable port.
void OmxVideoDecodeAccelerator::ChangePort(
    OMX_COMMANDTYPE cmd, int port_index) {
  OMX_ERRORTYPE result = OMX_SendCommand(component_handle_,
                                         cmd, port_index, 0);
  if (result != OMX_ErrorNone) {
    LOG(ERROR) << "SendCommand(OMX_CommandPortDisable) failed";
    StopOnError();
    return;
  }
}

DISABLE_RUNNABLE_METHOD_REFCOUNT(OmxVideoDecodeAccelerator);
