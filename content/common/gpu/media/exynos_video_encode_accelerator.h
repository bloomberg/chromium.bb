// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_EXYNOS_VIDEO_ENCODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_EXYNOS_VIDEO_ENCODE_ACCELERATOR_H_

#include <list>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/size.h"

namespace base {

class MessageLoopProxy;

}  // namespace base

namespace media {

class BitstreamBuffer;

}  // namespace media

namespace content {

// This class handles Exynos video encode acceleration by interfacing with the
// V4L2 devices exported by the Multi Format Codec and GScaler hardware blocks
// on the Exynos platform.  The threading model of this class is the same as the
// ExynosVideoDecodeAccelerator (from which class this was designed).
class CONTENT_EXPORT ExynosVideoEncodeAccelerator
    : public media::VideoEncodeAccelerator {
 public:
  ExynosVideoEncodeAccelerator();
  virtual ~ExynosVideoEncodeAccelerator();

  // media::VideoEncodeAccelerator implementation.
  virtual bool Initialize(media::VideoFrame::Format format,
                          const gfx::Size& input_visible_size,
                          media::VideoCodecProfile output_profile,
                          uint32 initial_bitrate,
                          Client* client) OVERRIDE;
  virtual void Encode(const scoped_refptr<media::VideoFrame>& frame,
                      bool force_keyframe) OVERRIDE;
  virtual void UseOutputBitstreamBuffer(
      const media::BitstreamBuffer& buffer) OVERRIDE;
  virtual void RequestEncodingParametersChange(uint32 bitrate,
                                               uint32 framerate) OVERRIDE;
  virtual void Destroy() OVERRIDE;

  static std::vector<media::VideoEncodeAccelerator::SupportedProfile>
      GetSupportedProfiles();

 private:
  // Auto-destroy reference for BitstreamBuffer, for tracking buffers passed to
  // this instance.
  struct BitstreamBufferRef;

  // Record for GSC input buffers.
  struct GscInputRecord {
    GscInputRecord();
    bool at_device;
    scoped_refptr<media::VideoFrame> frame;
  };

  // Record for GSC output buffers.
  struct GscOutputRecord {
    GscOutputRecord();
    bool at_device;
    int mfc_input;
  };

  // Record for MFC input buffers.
  struct MfcInputRecord {
    MfcInputRecord();
    bool at_device;
    int fd[2];
  };

  // Record for MFC output buffers.
  struct MfcOutputRecord {
    MfcOutputRecord();
    bool at_device;
    linked_ptr<BitstreamBufferRef> buffer_ref;
    void* address;
    size_t length;
  };

  enum {
    kInitialFramerate = 30,
    // These are rather subjectively tuned.
    kGscInputBufferCount = 2,
    kGscOutputBufferCount = 2,
    kMfcOutputBufferCount = 2,
    // MFC hardware does not report required output buffer size correctly.
    // Use maximum theoretical size to avoid hanging the hardware.
    kMfcOutputBufferSize = (2 * 1024 * 1024),
  };

  // Internal state of the encoder.
  enum State {
    kUninitialized,  // Initialize() not yet called.
    kInitialized,    // Initialize() returned true; ready to start encoding.
    kEncoding,       // Encoding frames.
    kError,          // Error in encoder state.
  };

  //
  // Encoding tasks, to be run on encode_thread_.
  //

  // Encode a GSC input buffer.
  void EncodeTask(const scoped_refptr<media::VideoFrame>& frame,
                  bool force_keyframe);

  // Add a BitstreamBuffer to the queue of buffers ready to be used for encoder
  // output.
  void UseOutputBitstreamBufferTask(scoped_ptr<BitstreamBufferRef> buffer_ref);

  // Device destruction task.
  void DestroyTask();

  // Service I/O on the V4L2 devices.  This task should only be scheduled from
  // DevicePollTask().
  void ServiceDeviceTask();

  // Handle the various device queues.
  void EnqueueGsc();
  void DequeueGsc();
  void EnqueueMfc();
  void DequeueMfc();
  // Enqueue a buffer on the corresponding queue.  Returns false on fatal error.
  bool EnqueueGscInputRecord();
  bool EnqueueGscOutputRecord();
  bool EnqueueMfcInputRecord();
  bool EnqueueMfcOutputRecord();

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

  // Set the encoder_thread_ state (using PostTask to encoder thread, if
  // necessary).
  void SetEncoderState(State state);

  //
  // Other utility functions.  Called on encoder_thread_, unless
  // encoder_thread_ is not yet started, in which case the child thread can call
  // these (e.g. in Initialize() or Destroy()).
  //

  // Change the parameters of encoding.
  void RequestEncodingParametersChangeTask(uint32 bitrate, uint32 framerate);

  // Create the buffers we need.
  bool CreateGscInputBuffers();
  bool CreateGscOutputBuffers();
  bool SetMfcFormats();
  bool InitMfcControls();
  bool CreateMfcInputBuffers();
  bool CreateMfcOutputBuffers();

  // Destroy these buffers.
  void DestroyGscInputBuffers();
  void DestroyGscOutputBuffers();
  void DestroyMfcInputBuffers();
  void DestroyMfcOutputBuffers();

  // Our original calling message loop for the child thread.
  const scoped_refptr<base::MessageLoopProxy> child_message_loop_proxy_;

  // WeakPtr<> pointing to |this| for use in posting tasks from the encoder or
  // device worker threads back to the child thread.  Because the worker threads
  // are members of this class, any task running on those threads is guaranteed
  // that this object is still alive.  As a result, tasks posted from the child
  // thread to the encoder or device thread should use base::Unretained(this),
  // and tasks posted the other way should use |weak_this_|.
  base::WeakPtrFactory<ExynosVideoEncodeAccelerator> weak_this_ptr_factory_;
  base::WeakPtr<ExynosVideoEncodeAccelerator> weak_this_;

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_message_loop_proxy_.
  scoped_ptr<base::WeakPtrFactory<Client> > client_ptr_factory_;
  base::WeakPtr<Client> client_;

  //
  // Encoder state, owned and operated by encoder_thread_.
  // Before encoder_thread_ has started, the encoder state is managed by
  // the child (main) thread.  After encoder_thread_ has started, the encoder
  // thread should be the only one managing these.
  //

  // This thread services tasks posted from the VEA API entry points by the
  // child thread and device service callbacks posted from the device thread.
  base::Thread encoder_thread_;
  // Encoder state.
  State encoder_state_;
  // The visible/allocated sizes of the input frame.
  gfx::Size input_visible_size_;
  gfx::Size input_allocated_size_;
  // The visible/allocated sizes of the color-converted intermediate frame.
  gfx::Size converted_visible_size_;
  gfx::Size converted_allocated_size_;
  // The logical visible size of the output frame.
  gfx::Size output_visible_size_;
  // The required byte size of output BitstreamBuffers.
  size_t output_buffer_byte_size_;

  // We need to provide the stream header with every keyframe, to allow
  // midstream decoding restarts.  Store it here.
  scoped_ptr<uint8[]> stream_header_;
  size_t stream_header_size_;

  // V4L2 formats for input frames and the output stream.
  uint32 input_format_fourcc_;
  uint32 output_format_fourcc_;

  // Video frames ready to be encoded.
  std::list<scoped_refptr<media::VideoFrame> > encoder_input_queue_;

  // GSC color conversion device.
  int gsc_fd_;
  // GSC input queue state.
  bool gsc_input_streamon_;
  // GSC input buffers enqueued to device.
  int gsc_input_buffer_queued_count_;
  // GSC input buffers ready to use; LIFO since we don't care about ordering.
  std::vector<int> gsc_free_input_buffers_;
  // Mapping of int index to GSC input buffer record.
  std::vector<GscInputRecord> gsc_input_buffer_map_;

  // GSC output queue state.
  bool gsc_output_streamon_;
  // GSC output buffers enqueued to device.
  int gsc_output_buffer_queued_count_;
  // GSC output buffers ready to use; LIFO since we don't care about ordering.
  std::vector<int> gsc_free_output_buffers_;
  // Mapping of int index to GSC output buffer record.
  std::vector<GscOutputRecord> gsc_output_buffer_map_;

  // MFC input buffers filled by GSC, waiting to be queued to MFC.
  std::list<int> mfc_ready_input_buffers_;

  // MFC video encoding device.
  int mfc_fd_;

  // MFC input queue state.
  bool mfc_input_streamon_;
  // MFC input buffers enqueued to device.
  int mfc_input_buffer_queued_count_;
  // MFC input buffers ready to use; LIFO since we don't care about ordering.
  std::vector<int> mfc_free_input_buffers_;
  // Mapping of int index to MFC input buffer record.
  std::vector<MfcInputRecord> mfc_input_buffer_map_;

  // MFC output queue state.
  bool mfc_output_streamon_;
  // MFC output buffers enqueued to device.
  int mfc_output_buffer_queued_count_;
  // MFC output buffers ready to use; LIFO since we don't care about ordering.
  std::vector<int> mfc_free_output_buffers_;
  // Mapping of int index to MFC output buffer record.
  std::vector<MfcOutputRecord> mfc_output_buffer_map_;

  // Bitstream buffers ready to be used to return encoded output, as a LIFO
  // since we don't care about ordering.
  std::vector<linked_ptr<BitstreamBufferRef> > encoder_output_queue_;

  //
  // The device polling thread handles notifications of V4L2 device changes.
  // TODO(sheu): replace this thread with an TYPE_IO encoder_thread_.
  //

  // The thread.
  base::Thread device_poll_thread_;
  // eventfd fd to signal device poll thread when its poll() should be
  // interrupted.
  int device_poll_interrupt_fd_;

  DISALLOW_COPY_AND_ASSIGN(ExynosVideoEncodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_EXYNOS_VIDEO_ENCODE_ACCELERATOR_H_
