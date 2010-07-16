// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_OMX_VIDEO_DECODE_ENGINE_H_
#define MEDIA_FILTERS_OMX_VIDEO_DECODE_ENGINE_H_

#include <functional>
#include <list>
#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "media/filters/video_decode_engine.h"
#include "media/omx/omx_configurator.h"
#include "third_party/openmax/il/OMX_Component.h"
#include "third_party/openmax/il/OMX_Core.h"
#include "third_party/openmax/il/OMX_Video.h"

class MessageLoop;

// FFmpeg types.
struct AVStream;

namespace media {

class Buffer;

class OmxVideoDecodeEngine :
    public VideoDecodeEngine,
    public base::RefCountedThreadSafe<OmxVideoDecodeEngine> {
 public:
  OmxVideoDecodeEngine();
  virtual ~OmxVideoDecodeEngine();

  // Implementation of the VideoDecodeEngine Interface.
  virtual void Initialize(MessageLoop* message_loop,
                          AVStream* av_stream,
                          EmptyThisBufferCallback* empty_buffer_callback,
                          FillThisBufferCallback* fill_buffer_callback,
                          Task* done_cb);
  virtual void EmptyThisBuffer(scoped_refptr<Buffer> buffer);
  virtual void FillThisBuffer(scoped_refptr<VideoFrame> video_frame);
  virtual void Stop(Task* done_cb);
  virtual void Pause(Task* done_cb);
  virtual void Flush(Task* done_cb);
  virtual VideoFrame::Format GetSurfaceFormat() const;

  virtual State state() const;

  // Subclass can provide a different value.
  virtual int current_omx_spec_version() const { return 0x00000101; }

 private:
  enum OmxIlState {
    kIlNone,
    kIlLoaded,
    kIlIdle,
    kIlExecuting,
    kIlPause,
    kIlInvalid,
    kIlUnknown,
  };

  enum OmxIlClientState {
    kClientNotInitialized,
    kClientInitializing,
    kClientRunning,
    kClientStopping,
    kClientStopped,
    kClientPausing,
    kClientFlushing,
    kClientError,
  };

  enum OmxIlPortState {
    kPortDisabled,
    kPortEnabling,
    kPortEnabled,
    kPortDisabling,
  };

  typedef Callback0::Type Callback;

  // calls into other classes
  void FinishEmptyBuffer(scoped_refptr<Buffer> buffer);
  void OnFormatChange(
      const OmxConfigurator::MediaFormat& input_format,
      const OmxConfigurator::MediaFormat& output_format);
  void FinishFillBuffer(OMX_BUFFERHEADERTYPE* buffer);
  // Helper method to perform tasks when this object is stopped.
  void OnStopDone();

  // Methods to be executed in |message_loop_|, they correspond to the
  // public methods.
  void InitializeTask();
  void StopTask(Task* task);
  void PauseTask(Task* task);
  void FlushTask(Task* task);

  // Transition method sequence for initialization
  bool CreateComponent();
  void DoneSetStateIdle(OMX_STATETYPE state);
  void DoneSetStateExecuting(OMX_STATETYPE state);
  void OnPortSettingsChangedRun(int port, OMX_INDEXTYPE index);
  void OnPortDisableEventRun(int port);
  void SetupOutputPort();
  void OnPortEnableEventRun(int port);

  // Transition methods for shutdown
  void DeinitFromExecuting(OMX_STATETYPE state);
  void DeinitFromIdle(OMX_STATETYPE state);
  void DeinitFromLoaded(OMX_STATETYPE state);
  void PauseFromExecuting(OMX_STATETYPE state);
  void PortFlushDone(int port);
  void ComponentFlushDone();

  void StopOnError();

  // Methods to free input and output buffers.
  bool AllocateInputBuffers();
  bool AllocateOutputBuffers();
  void FreeInputBuffers();
  void FreeOutputBuffers();
  void FreeInputQueue();

  // Helper method to configure port format at LOADED state.
  bool ConfigureIOPorts();

  // Determine whether we can issue fill buffer or empty buffer
  // to the decoder based on the current state and port state.
  bool CanEmptyBuffer();
  bool CanFillBuffer();

  // Determine whether we can use |input_queue_| and |output_queue_|
  // based on the current state.
  bool CanAcceptInput();
  bool CanAcceptOutput();

  bool InputPortFlushed();
  bool OutputPortFlushed();

  // Method to send input buffers to component
  void EmptyBufferTask();

  // Take one decoded buffer to fulfill one read request.
  void FulfillOneRead();

  // Method doing initial reads to get bit stream from demuxer.
  void InitialReadBuffer();

  // Method doing initial fills to kick start the decoding process.
  void InitialFillBuffer();

  // helper functions
  void ChangePort(OMX_COMMANDTYPE cmd, int port_index);
  OMX_BUFFERHEADERTYPE* FindOmxBuffer(scoped_refptr<VideoFrame> video_frame);
  OMX_STATETYPE GetComponentState();
  void SendOutputBufferToComponent(OMX_BUFFERHEADERTYPE *omx_buffer);
  bool TransitionToState(OMX_STATETYPE new_state);

  // Method to handle events
  void EventHandlerCompleteTask(OMX_EVENTTYPE event,
                                OMX_U32 data1,
                                OMX_U32 data2);

  // Method to receive buffers from component's input port
  void EmptyBufferDoneTask(OMX_BUFFERHEADERTYPE* buffer);

  // Method to receive buffers from component's output port
  void FillBufferDoneTask(OMX_BUFFERHEADERTYPE* buffer);

  // The following three methods are static callback methods
  // for the OMX component. When these callbacks are received, the
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

  // Member function pointers to respond to events
  void (OmxVideoDecodeEngine::*OnPortDisableEventFunc)(int port);
  void (OmxVideoDecodeEngine::*OnPortEnableEventFunc)(int port);
  void (OmxVideoDecodeEngine::*OnStateSetEventFunc)(OMX_STATETYPE state);
  void (OmxVideoDecodeEngine::*OnFlushEventFunc)(int port);

  size_t width_;
  size_t height_;

  MessageLoop* message_loop_;

  std::vector<OMX_BUFFERHEADERTYPE*> input_buffers_;
  int input_buffer_count_;
  int input_buffer_size_;
  int input_port_;
  int input_buffers_at_component_;
  int input_pending_request_;
  bool input_queue_has_eos_;
  bool input_has_fed_eos_;
  bool input_port_flushed_;

  std::vector<OMX_BUFFERHEADERTYPE*> output_buffers_;
  int output_buffer_count_;
  int output_buffer_size_;
  int output_port_;
  bool output_eos_;
  bool output_port_flushed_;
  bool uses_egl_image_;
  base::TimeDelta last_pts_;

  // |il_state_| records the current component state. During state transition
  // |expected_il_state_| is the next state that the component will transition
  // to. After a state transition is completed, |il_state_| equals
  // |expected_il_state_|. Inequality can be used to detect a state transition.
  // These two members are read and written only on |message_loop_|.
  OmxIlState il_state_;
  OmxIlState expected_il_state_;
  OmxIlClientState client_state_;

  OMX_HANDLETYPE component_handle_;
  scoped_ptr<media::OmxConfigurator> configurator_;
  scoped_ptr<EmptyThisBufferCallback> empty_this_buffer_callback_;
  scoped_ptr<FillThisBufferCallback> fill_this_buffer_callback_;

  scoped_ptr<Task> stop_callback_;
  scoped_ptr<Task> flush_callback_;
  scoped_ptr<Task> pause_callback_;

  // Free input OpenMAX buffers that can be used to take input bitstream from
  // demuxer.
  std::queue<OMX_BUFFERHEADERTYPE*> free_input_buffers_;

  // Available input OpenMAX buffers that we can use to issue
  // OMX_EmptyThisBuffer() call.
  std::queue<OMX_BUFFERHEADERTYPE*> available_input_buffers_;

  // flag for freeing input buffers
  bool need_free_input_buffers_;

  // For output buffer recycling cases.
  typedef std::pair<scoped_refptr<VideoFrame>,
                    OMX_BUFFERHEADERTYPE*> OutputFrame;
  std::vector<OutputFrame> output_frames_;
  std::queue<OMX_BUFFERHEADERTYPE*> available_output_frames_;
  std::queue<OMX_BUFFERHEADERTYPE*> output_frames_ready_;
  bool output_frames_allocated_;

  // port related
  bool input_port_enabled_;
  bool need_setup_output_port_;
  OmxIlPortState output_port_state_;

  DISALLOW_COPY_AND_ASSIGN(OmxVideoDecodeEngine);
};

}  // namespace media

#endif  // MEDIA_FILTERS_OMX_VIDEO_DECODE_ENGINE_H_
