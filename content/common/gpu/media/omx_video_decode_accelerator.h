// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_OMX_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_OMX_VIDEO_DECODE_ACCELERATOR_H_

#include <dlfcn.h>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "content/common/content_export.h"
#include "media/video/video_decode_accelerator.h"
#include "third_party/openmax/il/OMX_Component.h"
#include "third_party/openmax/il/OMX_Core.h"
#include "third_party/openmax/il/OMX_Video.h"
#include "ui/gl/gl_bindings.h"

namespace content {
class Gles2TextureToEglImageTranslator;

// Class to wrap OpenMAX IL accelerator behind VideoDecodeAccelerator interface.
// The implementation assumes an OpenMAX IL 1.1.2 implementation conforming to
// http://www.khronos.org/registry/omxil/specs/OpenMAX_IL_1_1_2_Specification.pdf
//
// This class lives on a single thread (the GPU process ChildThread) and DCHECKs
// that it is never accessed from any other.  OMX callbacks are trampolined from
// the OMX component's thread to maintain this invariant, using |weak_this()|.
class CONTENT_EXPORT OmxVideoDecodeAccelerator :
    public media::VideoDecodeAccelerator {
 public:
  // Does not take ownership of |client| which must outlive |*this|.
  OmxVideoDecodeAccelerator(
      EGLDisplay egl_display, EGLContext egl_context,
      media::VideoDecodeAccelerator::Client* client,
      const base::Callback<bool(void)>& make_context_current);
  virtual ~OmxVideoDecodeAccelerator();

  // media::VideoDecodeAccelerator implementation.
  bool Initialize(media::VideoCodecProfile profile) OVERRIDE;
  void Decode(const media::BitstreamBuffer& bitstream_buffer) OVERRIDE;
  virtual void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) OVERRIDE;
  void ReusePictureBuffer(int32 picture_buffer_id) OVERRIDE;
  void Flush() OVERRIDE;
  void Reset() OVERRIDE;
  void Destroy() OVERRIDE;

  base::WeakPtr<OmxVideoDecodeAccelerator> weak_this() { return weak_this_; }

  // Do any necessary initialization before the sandbox is enabled.
  static void PreSandboxInitialization();

 private:
  // Because OMX state-transitions are described solely by the "state reached"
  // (3.1.2.9.1, table 3-7 of the spec), we track what transition was requested
  // using this enum.  Note that it is an error to request a transition while
  // |*this| is in any state other than NO_TRANSITION, unless requesting
  // DESTROYING or ERRORING.
  enum CurrentStateChange {
    NO_TRANSITION,  // Not in the middle of a transition.
    INITIALIZING,
    FLUSHING,
    RESETTING,
    DESTROYING,
    ERRORING,  // Trumps all other transitions; no recovery is possible.
  };

  // Add codecs as we get HW that supports them (and which are supported by SW
  // decode!).
  enum Codec {
    UNKNOWN,
    H264,
    VP8
  };

  // Helper struct for keeping track of the relationship between an OMX output
  // buffer and the PictureBuffer it points to.
  struct OutputPicture {
    OutputPicture(media::PictureBuffer p_b, OMX_BUFFERHEADERTYPE* o_b_h,
                  EGLImageKHR e_i)
        : picture_buffer(p_b), omx_buffer_header(o_b_h),  egl_image(e_i) {}
    media::PictureBuffer picture_buffer;
    OMX_BUFFERHEADERTYPE* omx_buffer_header;
    EGLImageKHR egl_image;
  };
  typedef std::map<int32, OutputPicture> OutputPictureById;

  MessageLoop* message_loop_;
  OMX_HANDLETYPE component_handle_;

  // Create the Component for OMX. Handles all OMX initialization.
  bool CreateComponent();
  // Buffer allocation/free methods for input and output buffers.
  bool AllocateInputBuffers();
  bool AllocateFakeOutputBuffers();
  bool AllocateOutputBuffers();
  void FreeOMXBuffers();

  // Methods to handle OMX state transitions.  See section 3.1.1.2 of the spec.
  // Request transitioning OMX component to some other state.
  void BeginTransitionToState(OMX_STATETYPE new_state);
  // The callback received when the OMX component has transitioned.
  void DispatchStateReached(OMX_STATETYPE reached);
  // Callbacks handling transitioning to specific states during state changes.
  // These follow a convention of OnReached<STATE>In<CurrentStateChange>(),
  // requiring that each pair of <reached-state>/CurrentStateChange is unique
  // (i.e. the source state is uniquely defined by the pair).
  void OnReachedIdleInInitializing();
  void OnReachedExecutingInInitializing();
  void OnReachedPauseInResetting();
  void OnReachedExecutingInResetting();
  void OnReachedIdleInDestroying();
  void OnReachedLoadedInDestroying();
  void OnReachedEOSInFlushing();
  void OnReachedInvalidInErroring();
  void ShutdownComponent();
  void BusyLoopInDestroying(scoped_ptr<OmxVideoDecodeAccelerator> self);

  // Port-flushing helpers.
  void FlushIOPorts();
  void InputPortFlushDone();
  void OutputPortFlushDone();

  // Stop the component when any error is detected.
  void StopOnError(media::VideoDecodeAccelerator::Error error);

  // Determine whether we can issue fill buffer to the decoder based on the
  // current state (and outstanding state change) of the component.
  bool CanFillBuffer();

  // Whenever port settings change, the first thing we must do is disable the
  // port (see Figure 3-18 of the OpenMAX IL spec linked to above).  When the
  // port is disabled, the component will call us back here.  We then re-enable
  // the port once we have textures, and that's the second method below.
  void OnOutputPortDisabled();
  void OnOutputPortEnabled();

  // Decode bitstream buffers that were queued (see queued_bitstream_buffers_).
  void DecodeQueuedBitstreamBuffers();

  // Lazily initialize static data after sandbox is enabled.  Return false on
  // init failure.
  static bool PostSandboxInitialization();

  // Weak pointer to |this|; used to safely trampoline calls from the OMX thread
  // to the ChildThread.  Since |this| is kept alive until OMX is fully shut
  // down, only the OMX->Child thread direction needs to be guarded this way.
  base::WeakPtr<OmxVideoDecodeAccelerator> weak_this_;

  // True once Initialize() has returned true; before this point there's never a
  // point in calling client_->NotifyError().
  bool init_begun_;

  // IL-client state.
  OMX_STATETYPE client_state_;
  // See comment on CurrentStateChange above.
  CurrentStateChange current_state_change_;

  // Following are input port related variables.
  int input_buffer_count_;
  int input_buffer_size_;
  OMX_U32 input_port_;
  int input_buffers_at_component_;

  // Following are output port related variables.
  OMX_U32 output_port_;
  int output_buffers_at_component_;

  gfx::Size last_requested_picture_buffer_dimensions_;

  // NOTE: someday there may be multiple contexts for a single decoder.  But not
  // today.
  // TODO(fischman,vrk): handle lost contexts?
  EGLDisplay egl_display_;
  EGLContext egl_context_;
  base::Callback<bool(void)> make_context_current_;

  // Free input OpenMAX buffers that can be used to take bitstream from demuxer.
  std::queue<OMX_BUFFERHEADERTYPE*> free_input_buffers_;

  // For output buffer recycling cases.
  OutputPictureById pictures_;

  // To kick the component from Loaded to Idle before we know the real size of
  // the video (so can't yet ask for textures) we populate it with fake output
  // buffers.  Keep track of them here.
  // TODO(fischman): do away with this madness.
  std::set<OMX_BUFFERHEADERTYPE*> fake_output_buffers_;

  // Encoded bitstream buffers awaiting decode, queued while the decoder was
  // unable to accept them.
  typedef std::vector<media::BitstreamBuffer> BitstreamBufferList;
  BitstreamBufferList queued_bitstream_buffers_;
  // Available output picture buffers released during Reset() and awaiting
  // re-use once Reset is done.  Is empty most of the time and drained right
  // before NotifyResetDone is sent.
  std::vector<int> queued_picture_buffer_ids_;

  // To expose client callbacks from VideoDecodeAccelerator.
  // NOTE: all calls to this object *MUST* be executed in message_loop_.
  Client* client_;

  scoped_ptr<Gles2TextureToEglImageTranslator> texture_to_egl_image_translator_;

  // These members are only used during Initialization.
  Codec codec_;
  uint32 h264_profile_;  // OMX_AVCProfile requested during Initialization.
  bool component_name_is_nvidia_;

  // Has static initialization of pre-sandbox components completed successfully?
  static bool pre_sandbox_init_done_;

  // Method to handle events
  void EventHandlerCompleteTask(OMX_EVENTTYPE event,
                                OMX_U32 data1,
                                OMX_U32 data2);

  // Method to receive buffers from component's input port
  void EmptyBufferDoneTask(OMX_BUFFERHEADERTYPE* buffer);

  // Method to receive buffers from component's output port
  void FillBufferDoneTask(OMX_BUFFERHEADERTYPE* buffer);

  // Send a command to an OMX port.  Return false on failure (after logging and
  // setting |this| to ERRORING state).
  bool SendCommandToPort(OMX_COMMANDTYPE cmd, int port_index);

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

  // When we get a texture back via ReusePictureBuffer(), we want to ensure
  // that its contents have been read out by rendering layer, before we start
  // overwriting it with the decoder. This class is used to wait for a sync
  // object inserted into the GPU command stream at the time of
  // ReusePictureBuffer. This guarantees that the object gets into the stream
  // after the corresponding texture commands have been inserted into it. Once
  // the sync object is signalled, we are sure that the stream reached the sync
  // object, which ensures that all commands related to the texture we are
  // getting back have been finished as well.
  class PictureSyncObject;

  // Check if the client is done reading out from the texture. If yes, queue
  // it for reuse by the decoder. Otherwise post self as a delayed task
  // to check later.
  void CheckPictureStatus(int32 picture_buffer_id,
                          scoped_ptr<PictureSyncObject> egl_sync_obj);

  // Queue a picture for use by the decoder, either by sending it directly to it
  // via OMX_FillThisBuffer, or by queueing it for later if we are RESETTING.
  void QueuePictureBuffer(int32 picture_buffer_id);

};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_OMX_VIDEO_DECODE_ACCELERATOR_H_
