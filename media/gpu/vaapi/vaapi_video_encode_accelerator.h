// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread.h"
#include "media/filters/h264_bitstream_buffer.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/vaapi/accelerated_video_encoder.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

class VaapiEncodeJob;

// A VideoEncodeAccelerator implementation that uses VA-API
// (https://01.org/vaapi) for HW-accelerated video encode.
class MEDIA_GPU_EXPORT VaapiVideoEncodeAccelerator
    : public VideoEncodeAccelerator {
 public:
  VaapiVideoEncodeAccelerator();
  ~VaapiVideoEncodeAccelerator() override;

  // VideoEncodeAccelerator implementation.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config, Client* client) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(uint32_t bitrate,
                                       uint32_t framerate) override;
  void RequestEncodingParametersChange(
      const VideoBitrateAllocation& bitrate_allocation,
      uint32_t framerate) override;
  void Destroy() override;
  void Flush(FlushCallback flush_callback) override;
  bool IsFlushSupported() override;

 private:
  class H264Accelerator;
  class VP8Accelerator;
  class VP9Accelerator;

  // Encoder state.
  enum State {
    kUninitialized,
    kEncoding,
    kError,
  };

  // Holds input frames coming from the client ready to be encoded.
  struct InputFrameRef;
  // Holds output buffers coming from the client ready to be filled.
  struct BitstreamBufferRef;

  // one surface for input data.
  // one surface for reconstructed picture, which is later used for reference.
  static constexpr size_t kNumSurfacesPerInputVideoFrame = 1;
  static constexpr size_t kNumSurfacesForOutputPicture = 1;

  //
  // Tasks for each of the VEA interface calls to be executed on the
  // encoder thread.
  //
  void InitializeTask(const Config& config);

  // Enqueues |frame| onto the queue of pending inputs and attempts to continue
  // encoding.
  void EncodeTask(scoped_refptr<VideoFrame> frame, bool force_keyframe);

  // Maps |buffer_ref|, push it onto the available_bitstream_buffers_, and
  // attempts to return any pending encoded data in it, if any.
  void UseOutputBitstreamBufferTask(
      std::unique_ptr<BitstreamBufferRef> buffer_ref);

  void RequestEncodingParametersChangeTask(
      VideoBitrateAllocation bitrate_allocation,
      uint32_t framerate);

  void DestroyTask();
  void FlushTask();

  // Checks if sufficient resources for a new encode job with |frame| as input
  // are available, and if so, claims them by associating them with
  // a VaapiEncodeJob, and returns the newly-created job, nullptr otherwise.
  scoped_refptr<VaapiEncodeJob> CreateEncodeJob(scoped_refptr<VideoFrame> frame,
                                                bool force_keyframe);

  // Continues encoding frames as long as input_queue_ is not empty, and we are
  // able to create new EncodeJobs.
  void EncodePendingInputs();

  // Uploads image data from |frame| to |va_surface_id|.
  void UploadFrame(scoped_refptr<VideoFrame> frame, VASurfaceID va_surface_id);

  // Executes encode in hardware. This does not block and may return before
  // the job is finished.
  void ExecuteEncode(VASurfaceID va_surface_id);

  // Callback that returns a no longer used VASurfaceID to
  // available_va_surface_ids_ for reuse.
  void RecycleVASurfaceID(VASurfaceID va_surface_id);

  // Returns a bitstream buffer to the client if both a previously executed job
  // awaits to be completed and we have bitstream buffers available to download
  // the encoded data into.
  void TryToReturnBitstreamBuffer();

  // Downloads encoded data produced as a result of running |encode_job| into
  // |buffer|, and returns it to the client.
  void ReturnBitstreamBuffer(scoped_refptr<VaapiEncodeJob> encode_job,
                             std::unique_ptr<BitstreamBufferRef> buffer);

  // Puts the encoder into en error state and notifies the client
  // about the error.
  void NotifyError(Error error);

  // Sets the encoder state to |state| on the correct thread.
  void SetState(State state);

  // Submits |buffer| of |type| to the driver.
  void SubmitBuffer(VABufferType type,
                    scoped_refptr<base::RefCountedBytes> buffer);

  // Submits a VAEncMiscParameterBuffer |buffer| of type |type| to the driver.
  void SubmitVAEncMiscParamBuffer(VAEncMiscParameterType type,
                                  scoped_refptr<base::RefCountedBytes> buffer);

  // Submits a H264BitstreamBuffer |buffer| to the driver.
  void SubmitH264BitstreamBuffer(scoped_refptr<H264BitstreamBuffer> buffer);

  // VaapiWrapper is the owner of all HW resources (surfaces and buffers)
  // and will free them on destruction.
  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  // The coded size of an input buffer.
  gfx::Size coded_size_;

  // Size in bytes required for output bitstream buffers.
  size_t output_buffer_byte_size_;

  // This flag signals when the client is sending NV12 + DmaBuf-backed
  // VideoFrames to encode, which allows for skipping a copy-adaptation on
  // input.
  bool native_input_mode_ = false;

  // The number of va surfaces required for one video frame on Encode().
  // In |native_input_mode_|, one surface for input data is created from DmaBufs
  // of incoming VideoFrame. One surface for reconstructed picture is always
  // needed, which is later used for reference.
  // Therefore, |va_surfaces_per_video_frame| is one in |native_input_mode_|,
  // and two otherwise.
  size_t va_surfaces_per_video_frame_;

  // All of the members below must be accessed on the encoder_thread_,
  // while it is running.

  // Encoder state. Encode tasks will only run in kEncoding state.
  State state_;

  // Encoder instance managing video codec state and preparing encode jobs.
  std::unique_ptr<AcceleratedVideoEncoder> encoder_;

  // VA surfaces available for reuse.
  std::vector<VASurfaceID> available_va_surface_ids_;

  // VASurfaceIDs internal format.
  static constexpr unsigned int kVaSurfaceFormat = VA_RT_FORMAT_YUV420;

  // VA buffers for coded frames.
  std::vector<VABufferID> available_va_buffer_ids_;

  // Callback via which finished VA surfaces are returned to us.
  base::RepeatingCallback<void(VASurfaceID)> va_surface_release_cb_;

  // Queue of input frames to be encoded.
  base::queue<std::unique_ptr<InputFrameRef>> input_queue_;

  // BitstreamBuffers mapped, ready to be filled with encoded stream data.
  base::queue<std::unique_ptr<BitstreamBufferRef>> available_bitstream_buffers_;

  // Jobs submitted to driver for encode, awaiting bitstream buffers to become
  // available.
  base::queue<scoped_refptr<VaapiEncodeJob>> submitted_encode_jobs_;

  // Encoder thread. All tasks are executed on it.
  base::Thread encoder_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> encoder_thread_task_runner_;

  const scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_task_runner_.
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;
  base::WeakPtr<Client> client_;

  // WeakPtr to post from the encoder thread back to the ChildThread, as it may
  // outlive this. Posting from the ChildThread using base::Unretained(this)
  // to the encoder thread is safe, because |this| always outlives the encoder
  // thread (it's a member of this class).
  base::WeakPtr<VaapiVideoEncodeAccelerator> weak_this_;

  // The completion callback of the Flush() function.
  FlushCallback flush_callback_;

  base::WeakPtrFactory<VaapiVideoEncodeAccelerator> weak_this_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VaapiVideoEncodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODE_ACCELERATOR_H_
