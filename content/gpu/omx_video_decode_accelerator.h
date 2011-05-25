// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_OMX_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_GPU_OMX_VIDEO_DECODE_ACCELERATOR_H_

#include <dlfcn.h>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "media/video/video_decode_accelerator.h"
#include "third_party/openmax/il/OMX_Component.h"
#include "third_party/openmax/il/OMX_Core.h"
#include "third_party/openmax/il/OMX_Video.h"

// Class to wrap OpenMAX IL accelerator behind VideoDecodeAccelerator interface.
class OmxVideoDecodeAccelerator : public media::VideoDecodeAccelerator {
 public:
  OmxVideoDecodeAccelerator(media::VideoDecodeAccelerator::Client* client,
                            MessageLoop* message_loop);
  virtual ~OmxVideoDecodeAccelerator();

  // media::VideoDecodeAccelerator implementation.
  const std::vector<uint32>& GetConfig(
      const std::vector<uint32>& prototype_config);
  bool Initialize(const std::vector<uint32>& config);
  bool Decode(const media::BitstreamBuffer& bitstream_buffer,
              const media::VideoDecodeAcceleratorCallback& callback);
  void AssignPictureBuffer(std::vector<PictureBuffer*> picture_buffers);
  void ReusePictureBuffer(int32 picture_buffer_id);
  bool Flush(const media::VideoDecodeAcceleratorCallback& callback);
  bool Abort(const media::VideoDecodeAcceleratorCallback& callback);

 private:
  MessageLoop* message_loop_;
  OMX_HANDLETYPE component_handle_;

  // Create the Component for OMX. Handles all OMX initialization.
  bool CreateComponent();
  // Buffer allocation/free methods for input and output buffers.
  bool AllocateInputBuffers();
  bool AllocateOutputBuffers();
  void FreeInputBuffers();
  void FreeOutputBuffers();

  // Methods to handle OMX state transitions.
  bool TransitionToState(OMX_STATETYPE new_state);
  void OnStateChangeLoadedToIdle(OMX_STATETYPE state);
  void OnStateChangeIdleToExecuting(OMX_STATETYPE state);
  void OnPortCommandFlush(OMX_STATETYPE state);
  void OnStateChangeExecutingToIdle(OMX_STATETYPE state);
  void OnStateChangeIdleToLoaded(OMX_STATETYPE state);
  // Stop the components when error is detected.
  void StopOnError();
  // Trigger the initial call to FillBuffers to start the decoding process.
  void InitialFillBuffer();
  // Methods for shutdown
  void PauseFromExecuting(OMX_STATETYPE ignored);
  void FlushIOPorts();
  void PortFlushDone(int port);
  void FlushBegin();

  // Determine whether we actually start decoding the bitstream.
  bool CanAcceptInput();
  // Determine whether we can issue fill buffer or empty buffer
  // to the decoder based on the current state and port state.
  bool CanEmptyBuffer();
  bool CanFillBuffer();
  void OnPortSettingsChangedRun(int port, OMX_INDEXTYPE index);

  // Decoded width/height from bitstream.
  int width_;
  int height_;
  std::vector<uint32> component_config_;

  // IL-client state.
  OMX_STATETYPE client_state_;

  // Following are input port related variables.
  int input_buffer_count_;
  int input_buffer_size_;
  int input_port_;
  int input_buffers_at_component_;

  // Following are output port related variables.
  int output_buffer_count_;
  int output_buffer_size_;
  int output_port_;
  int output_buffers_at_component_;

  bool uses_egl_image_;
  // Free input OpenMAX buffers that can be used to take bitstream from demuxer.
  std::queue<OMX_BUFFERHEADERTYPE*> free_input_buffers_;

  // For output buffer recycling cases.
  std::vector<media::VideoDecodeAccelerator::PictureBuffer*>
      assigned_picture_buffers_;
  typedef std::pair<PictureBuffer*,
                    OMX_BUFFERHEADERTYPE*> OutputPicture;
  std::vector<OutputPicture> output_pictures_;

  // To expose client callbacks from VideoDecodeAccelerator.
  Client* client_;

  media::VideoDecodeAcceleratorCallback flush_done_callback_;
  media::VideoDecodeAcceleratorCallback abort_done_callback_;

  std::vector<uint32> texture_ids_;
  std::vector<uint32> context_ids_;
  // Method to handle events
  void EventHandlerCompleteTask(OMX_EVENTTYPE event,
                                OMX_U32 data1,
                                OMX_U32 data2);

  // Method to receive buffers from component's input port
  void EmptyBufferDoneTask(OMX_BUFFERHEADERTYPE* buffer);

  // Method to receive buffers from component's output port
  void FillBufferDoneTask(OMX_BUFFERHEADERTYPE* buffer);
  typedef std::pair<OMX_BUFFERHEADERTYPE*, uint32> OMXbufferTexture;
  // void pointer to hold EGLImage handle.
  void* egl_image_;

  typedef std::map<
    OMX_BUFFERHEADERTYPE*,
    std::pair<base::SharedMemory*,
              media::VideoDecodeAcceleratorCallback> > OMXBufferCallbackMap;
  OMXBufferCallbackMap omx_buff_cb_;

  // Method used the change the state of the port.
  void ChangePort(OMX_COMMANDTYPE cmd, int port_index);

  // Member function pointers to respond to events
  void (OmxVideoDecodeAccelerator::*on_port_disable_event_func_)(int port);
  void (OmxVideoDecodeAccelerator::*on_port_enable_event_func_)(int port);
  void (OmxVideoDecodeAccelerator::*on_state_event_func_)(OMX_STATETYPE state);
  void (OmxVideoDecodeAccelerator::*on_flush_event_func_)(int port);
  void (OmxVideoDecodeAccelerator::*on_buffer_flag_event_func_)();

  // Callback methods for the OMX component.
  // When these callbacks are received, the
  // call is delegated to the three internal methods above.
  static OMX_ERRORTYPE EventHandler(OMX_HANDLETYPE component,
                                    OMX_PTR priv_data,
                                    OMX_EVENTTYPE event,
                                    OMX_U32 data1, OMX_U32 data2,
                                    OMX_PTR event_data);
  static OMX_ERRORTYPE EmptyBufferCallback(OMX_HANDLETYPE component,
                                           OMX_PTR priv_data,
                                           OMX_BUFFERHEADERTYPE* buffer);
  static OMX_ERRORTYPE FillBufferCallback(OMX_HANDLETYPE component,
                                          OMX_PTR priv_data,
                                          OMX_BUFFERHEADERTYPE* buffer);
};

#endif  // CONTENT_GPU_OMX_VIDEO_DECODE_ACCELERATOR_H_
