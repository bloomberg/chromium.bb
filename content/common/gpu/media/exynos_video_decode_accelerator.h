// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of VideoDecoderAccelerator
// that utilizes the hardware video decoder present on the Exynos SoC.

#ifndef CONTENT_COMMON_GPU_MEDIA_EXYNOS_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_EXYNOS_VIDEO_DECODE_ACCELERATOR_H_

#include <list>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "media/base/video_decoder_config.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_bindings.h"

namespace base {
class MessageLoopProxy;
}

namespace content {
class H264Parser;

// This class handles Exynos video acceleration directly through the V4L2
// devices exported by the Multi Format Codec and GScaler hardware blocks.
//
// The threading model of this class is driven by the fact that it needs to
// interface two fundamentally different event queues -- the one Chromium
// provides through MessageLoop, and the one driven by the V4L2 devices which
// is waited on with epoll().  There are three threads involved in this class:
//
// * The child thread, which is the main GPU process thread which calls the
//   media::VideoDecodeAccelerator entry points.  Calls from this thread
//   generally do not block (with the exception of Initialize() and Destroy()).
//   They post tasks to the decoder_thread_, which actually services the task
//   and calls back when complete through the
//   media::VideoDecodeAccelerator::Client interface.
// * The decoder_thread_, owned by this class.  It services API tasks, through
//   the *Task() routines, as well as V4L2 device events, through
//   ServiceDeviceTask().  Almost all state modification is done on this thread.
// * The device_poll_thread_, owned by this class.  All it does is epoll() on
//   the V4L2 in DevicePollTask() and schedule a ServiceDeviceTask() on the
//   decoder_thread_ when something interesting happens.
//   TODO(sheu): replace this thread with an TYPE_IO decoder_thread_.
//
// Note that this class has no locks!  Everything's serviced on the
// decoder_thread_, so there are no synchronization issues.
// ... well, there are, but it's a matter of getting messages posted in the
// right order, not fiddling with locks.
class CONTENT_EXPORT ExynosVideoDecodeAccelerator :
    public media::VideoDecodeAccelerator {
 public:
  ExynosVideoDecodeAccelerator(
      EGLDisplay egl_display,
      EGLContext egl_context,
      Client* client,
      const base::Callback<bool(void)>& make_context_current);
  virtual ~ExynosVideoDecodeAccelerator();

  // media::VideoDecodeAccelerator implementation.
  // Note: Initialize() and Destroy() are synchronous.
  virtual bool Initialize(media::VideoCodecProfile profile) OVERRIDE;
  virtual void Decode(const media::BitstreamBuffer& bitstream_buffer) OVERRIDE;
  virtual void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void Destroy() OVERRIDE;

  // Do any necessary initialization before the sandbox is enabled.
  static void PreSandboxInitialization();

  // Lazily initialize static data after sandbox is enabled.  Return false on
  // init failure.
  static bool PostSandboxInitialization();

 private:
  // These are rather subjectively tuned.
  enum {
    kMfcInputBufferCount = 8,
    kMfcOutputBufferExtraCount = 5,  // number of buffers above request by V4L2.
    kMfcInputBufferMaxSize = 512 * 1024,
    kGscInputBufferCount = 6,
    kGscOutputBufferCount = 6,
  };

  // Internal state of the decoder.
  enum State {
    kUninitialized,      // Initialize() not yet called.
    kInitialized,        // Initialize() returned true; ready to start decoding.
    kDecoding,           // DecodeBufferInitial() successful; decoding frames.
    kResetting,          // Presently resetting.
    kAfterReset,         // After Reset(), ready to start decoding again.
    kError,              // Error in kDecoding state.
  };

  enum BufferId {
    kFlushBufferId = -2  // Buffer id for flush buffer, queued by FlushTask().
  };

  // File descriptors we need to poll.
  enum PollFds {
    kPollMfc = (1 << 0),
    kPollGsc = (1 << 1),
  };

  // Auto-destruction reference for BitstreamBuffer, for message-passing from
  // Decode() to DecodeTask().
  struct BitstreamBufferRef;

  // Auto-destruction reference for an array of PictureBuffer, for
  // message-passing from AssignPictureBuffers() to AssignPictureBuffersTask().
  struct PictureBufferArrayRef;

  // Auto-destruction reference for EGLSync (for message-passing).
  struct EGLSyncKHRRef;

  // Record for MFC input buffers.
  struct MfcInputRecord {
    MfcInputRecord();
    ~MfcInputRecord();
    bool at_device;        // held by device.
    void* address;         // mmap() address.
    size_t length;         // mmap() length.
    off_t bytes_used;      // bytes filled in the mmap() segment.
    int32 input_id;        // triggering input_id as given to Decode().
  };

  // Record for MFC output buffers.
  struct MfcOutputRecord {
    MfcOutputRecord();
    ~MfcOutputRecord();
    bool at_device;        // held by device.
    size_t bytes_used[2];  // bytes used in each dmabuf.
    void* address[2];      // mmap() address for each plane.
    size_t length[2];      // mmap() length for each plane.
    int32 input_id;        // triggering input_id as given to Decode().
  };

  // Record for GSC input buffers.
  struct GscInputRecord {
    GscInputRecord();
    ~GscInputRecord();
    bool at_device;        // held by device.
    int mfc_output;        // MFC output buffer index to recycle when this input
                           // is complete
  };

  // Record for GSC output buffers.
  struct GscOutputRecord {
    GscOutputRecord();
    ~GscOutputRecord();
    bool at_device;        // held by device.
    bool at_client;        // held by client.
    int fd;                // file descriptor from backing EGLImage.
    EGLImageKHR egl_image; // backing EGLImage.
    EGLSyncKHR egl_sync;   // sync the compositor's use of the EGLImage.
    int32 picture_id;      // picture buffer id as returned to PictureReady().
  };

  //
  // Decoding tasks, to be run on decode_thread_.
  //

  // Enqueue a BitstreamBuffer to decode.  This will enqueue a buffer to the
  // decoder_input_queue_, then queue a DecodeBufferTask() to actually decode
  // the buffer.
  void DecodeTask(scoped_ptr<BitstreamBufferRef> bitstream_record);

  // Decode from the buffers queued in decoder_input_queue_.  Calls
  // DecodeBufferInitial() or DecodeBufferContinue() as appropriate.
  void DecodeBufferTask();
  // Find the extents of one frame fragment to push to HW.
  bool FindFrameFragment(const uint8* data, size_t size, size_t* endpos);
  // Schedule another DecodeBufferTask() if we're behind.
  void ScheduleDecodeBufferTaskIfNeeded();

  // Return true if we should continue to schedule DecodeBufferTask()s after
  // completion.  Store the amount of input actually consumed in |endpos|.
  bool DecodeBufferInitial(const void* data, size_t size, size_t* endpos);
  bool DecodeBufferContinue(const void* data, size_t size);

  // Accumulate data for the next frame to decode.  May return false in
  // non-error conditions; for example when pipeline is full and should be
  // retried later.
  bool AppendToInputFrame(const void* data, size_t size);
  // Flush data for one decoded frame.
  bool FlushInputFrame();

  // Process an AssignPictureBuffers() API call.  After this, the
  // device_poll_thread_ can be started safely, since we have all our
  // buffers.
  void AssignPictureBuffersTask(scoped_ptr<PictureBufferArrayRef> pic_buffers);

  // Service I/O on the V4L2 devices.  This task should only be scheduled from
  // DevicePollTask().
  void ServiceDeviceTask();
  // Handle the various device queues.
  void EnqueueMfc();
  void DequeueMfc();
  void EnqueueGsc();
  void DequeueGsc();
  // Enqueue a buffer on the corresponding queue.
  bool EnqueueMfcInputRecord();
  bool EnqueueMfcOutputRecord();
  bool EnqueueGscInputRecord();
  bool EnqueueGscOutputRecord();

  // Process a ReusePictureBuffer() API call.  The API call create an EGLSync
  // object on the main (GPU process) thread; we will record this object so we
  // can wait on it before reusing the buffer.
  void ReusePictureBufferTask(int32 picture_buffer_id,
                              scoped_ptr<EGLSyncKHRRef> egl_sync_ref);

  // Flush() task.  Child thread should not submit any more buffers until it
  // receives the NotifyFlushDone callback.  This task will schedule an empty
  // BitstreamBufferRef (with input_id == kFlushBufferId) to perform the flush.
  void FlushTask();
  // Notify the client of a flush completion, if required.  This should be
  // called any time a relevant queue could potentially be emptied: see
  // function definition.
  void NotifyFlushDoneIfNeeded();

  // Reset() task.  This task will schedule a ResetDoneTask() that will send
  // the NotifyResetDone callback, then set the decoder state to kResetting so
  // that all intervening tasks will drain.
  void ResetTask();
  // ResetDoneTask() will set the decoder state back to kAfterReset, so
  // subsequent decoding can continue.
  void ResetDoneTask();

  // Device destruction task.
  void DestroyTask();

  // Attempt to start/stop device_poll_thread_.
  bool StartDevicePoll();
  bool StopDevicePoll();
  // Set/clear the device poll interrupt (using device_poll_interrupt_fd_).
  bool SetDevicePollInterrupt();
  bool ClearDevicePollInterrupt();

  //
  // Device tasks, to be run on device_poll_thread_.
  //

  // The device task.
  void DevicePollTask(unsigned int poll_fds);

  //
  // Safe from any thread.
  //

  // Error notification (using PostTask() to child thread, if necessary).
  void NotifyError(Error error);

  // Set the decoder_thread_ state (using PostTask to decoder thread, if
  // necessary).
  void SetDecoderState(State state);

  //
  // Other utility functions.  Called on decoder_thread_, unless
  // decoder_thread_ is not yet started, in which case the child thread can call
  // these (e.g. in Initialize() or Destroy()).
  //

  // Create the buffers we need.
  bool CreateMfcInputBuffers();
  bool CreateMfcOutputBuffers();
  bool CreateGscInputBuffers();
  bool CreateGscOutputBuffers();

  // Destroy these buffers.
  void DestroyMfcInputBuffers();
  void DestroyMfcOutputBuffers();
  void DestroyGscInputBuffers();
  void DestroyGscOutputBuffers();

  // Our original calling message loop for the child thread.
  scoped_refptr<base::MessageLoopProxy> child_message_loop_proxy_;

  // WeakPtr<> pointing to |this| for use in posting tasks from the decoder or
  // device worker threads back to the child thread.  Because the worker threads
  // are members of this class, any task running on those threads is guaranteed
  // that this object is still alive.  As a result, tasks posted from the child
  // thread to the decoder or device thread should use base::Unretained(this),
  // and tasks posted the other way should use |weak_this_|.
  base::WeakPtr<ExynosVideoDecodeAccelerator> weak_this_;

  // To expose client callbacks from VideoDecodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_message_loop_proxy_.
  base::WeakPtrFactory<Client> client_ptr_factory_;
  base::WeakPtr<Client> client_;

  //
  // Decoder state, owned and operated by decoder_thread_.
  // Before decoder_thread_ has started, the decoder state is managed by
  // the child (main) thread.  After decoder_thread_ has started, the decoder
  // thread should be the only one managing these.
  //

  // This thread services tasks posted from the VDA API entry points by the
  // child thread and device service callbacks posted from the device thread.
  base::Thread decoder_thread_;
  // Decoder state machine state.
  State decoder_state_;
  // BitstreamBuffer we're presently reading.
  scoped_ptr<BitstreamBufferRef> decoder_current_bitstream_buffer_;
  // FlushTask() and ResetTask() should not affect buffers that have been
  // queued afterwards.  For flushing or resetting the pipeline then, we will
  // delay these buffers until after the flush or reset completes.
  int decoder_delay_bitstream_buffer_id_;
  // MFC input buffer we're presently filling.
  int decoder_current_input_buffer_;
  // We track the number of buffer decode tasks we have scheduled, since each
  // task execution should complete one buffer.  If we fall behind (due to
  // resource backpressure, etc.), we'll have to schedule more to catch up.
  int decoder_decode_buffer_tasks_scheduled_;
  // Picture buffers held by the client.
  int decoder_frames_at_client_;
  // Are we flushing?
  bool decoder_flushing_;
  // Input queue for decoder_thread_: BitstreamBuffers in.
  std::list<linked_ptr<BitstreamBufferRef> > decoder_input_queue_;
  // For H264 decode, hardware requires that we send it frame-sized chunks.
  // We'll need to parse the stream.
  scoped_ptr<content::H264Parser> decoder_h264_parser_;

  //
  // Hardware state and associated queues.  Since decoder_thread_ services
  // the hardware, decoder_thread_ owns these too.
  //

  // Completed decode buffers, waiting for MFC.
  std::list<int> mfc_input_ready_queue_;

  // MFC decode device.
  int mfc_fd_;

  // MFC input buffer state.
  bool mfc_input_streamon_;
  // MFC input buffers, total.
  int mfc_input_buffer_count_;
  // MFC input buffers enqueued to device.
  int mfc_input_buffer_queued_count_;
  // Input buffers ready to use, as a LIFO since we don't care about ordering.
  std::vector<int> mfc_free_input_buffers_;
  // Mapping of int index to MFC input buffer record.
  std::vector<MfcInputRecord> mfc_input_buffer_map_;

  // MFC output buffer state.
  bool mfc_output_streamon_;
  // MFC output buffers, total.
  int mfc_output_buffer_count_;
  // MFC output buffers enqueued to device.
  int mfc_output_buffer_queued_count_;
  // Output buffers ready to use, as a LIFO since we don't care about ordering.
  std::vector<int> mfc_free_output_buffers_;
  // Mapping of int index to MFC output buffer record.
  std::vector<MfcOutputRecord> mfc_output_buffer_map_;
  // Required size of MFC output buffers.  Two sizes for two planes.
  size_t mfc_output_buffer_size_[2];
  uint32 mfc_output_buffer_pixelformat_;

  // Completed MFC outputs, waiting for GSC.
  std::list<int> mfc_output_gsc_input_queue_;

  // GSC decode device.
  int gsc_fd_;

  // GSC input buffer state.
  bool gsc_input_streamon_;
  // GSC input buffers, total.
  int gsc_input_buffer_count_;
  // GSC input buffers enqueued to device.
  int gsc_input_buffer_queued_count_;
  // Input buffers ready to use, as a LIFO since we don't care about ordering.
  std::vector<int> gsc_free_input_buffers_;
  // Mapping of int index to GSC input buffer record.
  std::vector<GscInputRecord> gsc_input_buffer_map_;

  // GSC output buffer state.
  bool gsc_output_streamon_;
  // GSC output buffers, total.
  int gsc_output_buffer_count_;
  // GSC output buffers enqueued to device.
  int gsc_output_buffer_queued_count_;
  // Output buffers ready to use.  We need a FIFO here.
  std::list<int> gsc_free_output_buffers_;
  // Mapping of int index to GSC output buffer record.
  std::vector<GscOutputRecord> gsc_output_buffer_map_;

  // Output picture size.
  gfx::Size frame_buffer_size_;

  //
  // The device polling thread handles notifications of V4L2 device changes.
  //

  // The thread.
  base::Thread device_poll_thread_;
  // eventfd fd to signal device poll thread when its poll() should be
  // interrupted.
  int device_poll_interrupt_fd_;

  //
  // Other state, held by the child (main) thread.
  //

  // Make our context current before running any EGL entry points.
  base::Callback<bool(void)> make_context_current_;

  // EGL state
  EGLDisplay egl_display_;
  EGLContext egl_context_;

  // The codec we'll be decoding for.
  media::VideoCodecProfile video_profile_;

  DISALLOW_COPY_AND_ASSIGN(ExynosVideoDecodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_EXYNOS_VIDEO_DECODE_ACCELERATOR_H_
