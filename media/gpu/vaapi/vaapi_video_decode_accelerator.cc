// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_decode_accelerator.h"

#include <string.h>

#include <memory>

#include <va/va.h>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/bind_to_current_loop.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/format_utils.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/vaapi/vaapi_picture.h"
#include "media/gpu/vp8_decoder.h"
#include "media/gpu/vp9_decoder.h"
#include "media/video/picture.h"
#include "ui/gl/gl_image.h"

#define DVLOGF(level) DVLOG(level) << __func__ << "(): "
#define VLOGF(level) VLOG(level) << __func__ << "(): "

namespace media {

namespace {
// UMA errors that the VaapiVideoDecodeAccelerator class reports.
enum VAVDADecoderFailure {
  VAAPI_ERROR = 0,
  VAVDA_DECODER_FAILURES_MAX,
};
// from ITU-T REC H.264 spec
// section 8.5.6
// "Inverse scanning process for 4x4 transform coefficients and scaling lists"
static const int kZigzagScan4x4[16] = {0, 1,  4,  8,  5, 2,  3,  6,
                                       9, 12, 13, 10, 7, 11, 14, 15};

// section 8.5.7
// "Inverse scanning process for 8x8 transform coefficients and scaling lists"
static const uint8_t kZigzagScan8x8[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

// Returns the preferred VA_RT_FORMAT for the given |profile|.
unsigned int GetVaFormatForVideoCodecProfile(VideoCodecProfile profile) {
  if (profile == VP9PROFILE_PROFILE2 || profile == VP9PROFILE_PROFILE3)
    return VA_RT_FORMAT_YUV420_10BPP;
  return VA_RT_FORMAT_YUV420;
}

}  // namespace

static void ReportToUMA(VAVDADecoderFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAVDA.DecoderFailure", failure,
                            VAVDA_DECODER_FAILURES_MAX + 1);
}

#define RETURN_AND_NOTIFY_ON_FAILURE(result, log, error_code, ret) \
  do {                                                             \
    if (!(result)) {                                               \
      VLOGF(1) << log;                                             \
      NotifyError(error_code);                                     \
      return ret;                                                  \
    }                                                              \
  } while (0)

class VaapiVideoDecodeAccelerator::VaapiDecodeSurface
    : public base::RefCountedThreadSafe<VaapiDecodeSurface> {
 public:
  VaapiDecodeSurface(int32_t bitstream_id,
                     const scoped_refptr<VASurface>& va_surface);

  int32_t bitstream_id() const { return bitstream_id_; }
  scoped_refptr<VASurface> va_surface() { return va_surface_; }
  gfx::Rect visible_rect() const { return visible_rect_; }

  void set_visible_rect(const gfx::Rect& visible_rect) {
    visible_rect_ = visible_rect;
  }

 private:
  friend class base::RefCountedThreadSafe<VaapiDecodeSurface>;
  ~VaapiDecodeSurface();

  const int32_t bitstream_id_;
  const scoped_refptr<VASurface> va_surface_;
  gfx::Rect visible_rect_;
};

VaapiVideoDecodeAccelerator::VaapiDecodeSurface::VaapiDecodeSurface(
    int32_t bitstream_id,
    const scoped_refptr<VASurface>& va_surface)
    : bitstream_id_(bitstream_id), va_surface_(va_surface) {}

VaapiVideoDecodeAccelerator::VaapiDecodeSurface::~VaapiDecodeSurface() {}

class VaapiH264Picture : public H264Picture {
 public:
  explicit VaapiH264Picture(
      scoped_refptr<VaapiVideoDecodeAccelerator::VaapiDecodeSurface> surface)
      : dec_surface_(surface) {}

  VaapiH264Picture* AsVaapiH264Picture() override { return this; }
  scoped_refptr<VaapiVideoDecodeAccelerator::VaapiDecodeSurface> dec_surface() {
    return dec_surface_;
  }

 private:
  ~VaapiH264Picture() override {}

  scoped_refptr<VaapiVideoDecodeAccelerator::VaapiDecodeSurface> dec_surface_;

  DISALLOW_COPY_AND_ASSIGN(VaapiH264Picture);
};

class VaapiVideoDecodeAccelerator::VaapiH264Accelerator
    : public H264Decoder::H264Accelerator {
 public:
  VaapiH264Accelerator(VaapiVideoDecodeAccelerator* vaapi_dec,
                       VaapiWrapper* vaapi_wrapper);
  ~VaapiH264Accelerator() override;

  // H264Decoder::H264Accelerator implementation.
  scoped_refptr<H264Picture> CreateH264Picture() override;

  bool SubmitFrameMetadata(const H264SPS* sps,
                           const H264PPS* pps,
                           const H264DPB& dpb,
                           const H264Picture::Vector& ref_pic_listp0,
                           const H264Picture::Vector& ref_pic_listb0,
                           const H264Picture::Vector& ref_pic_listb1,
                           const scoped_refptr<H264Picture>& pic) override;

  bool SubmitSlice(const H264PPS* pps,
                   const H264SliceHeader* slice_hdr,
                   const H264Picture::Vector& ref_pic_list0,
                   const H264Picture::Vector& ref_pic_list1,
                   const scoped_refptr<H264Picture>& pic,
                   const uint8_t* data,
                   size_t size) override;

  bool SubmitDecode(const scoped_refptr<H264Picture>& pic) override;
  bool OutputPicture(const scoped_refptr<H264Picture>& pic) override;

  void Reset() override;

 private:
  scoped_refptr<VaapiDecodeSurface> H264PictureToVaapiDecodeSurface(
      const scoped_refptr<H264Picture>& pic);

  void FillVAPicture(VAPictureH264* va_pic, scoped_refptr<H264Picture> pic);
  int FillVARefFramesFromDPB(const H264DPB& dpb,
                             VAPictureH264* va_pics,
                             int num_pics);

  VaapiWrapper* vaapi_wrapper_;
  VaapiVideoDecodeAccelerator* vaapi_dec_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VaapiH264Accelerator);
};

class VaapiVP8Picture : public VP8Picture {
 public:
  explicit VaapiVP8Picture(
      scoped_refptr<VaapiVideoDecodeAccelerator::VaapiDecodeSurface> surface)
      : dec_surface_(surface) {}

  VaapiVP8Picture* AsVaapiVP8Picture() override { return this; }
  scoped_refptr<VaapiVideoDecodeAccelerator::VaapiDecodeSurface> dec_surface() {
    return dec_surface_;
  }

 private:
  ~VaapiVP8Picture() override {}

  scoped_refptr<VaapiVideoDecodeAccelerator::VaapiDecodeSurface> dec_surface_;

  DISALLOW_COPY_AND_ASSIGN(VaapiVP8Picture);
};

class VaapiVideoDecodeAccelerator::VaapiVP8Accelerator
    : public VP8Decoder::VP8Accelerator {
 public:
  VaapiVP8Accelerator(VaapiVideoDecodeAccelerator* vaapi_dec,
                      VaapiWrapper* vaapi_wrapper);
  ~VaapiVP8Accelerator() override;

  // VP8Decoder::VP8Accelerator implementation.
  scoped_refptr<VP8Picture> CreateVP8Picture() override;

  bool SubmitDecode(const scoped_refptr<VP8Picture>& pic,
                    const Vp8FrameHeader* frame_hdr,
                    const scoped_refptr<VP8Picture>& last_frame,
                    const scoped_refptr<VP8Picture>& golden_frame,
                    const scoped_refptr<VP8Picture>& alt_frame) override;

  bool OutputPicture(const scoped_refptr<VP8Picture>& pic) override;

 private:
  scoped_refptr<VaapiDecodeSurface> VP8PictureToVaapiDecodeSurface(
      const scoped_refptr<VP8Picture>& pic);

  VaapiWrapper* vaapi_wrapper_;
  VaapiVideoDecodeAccelerator* vaapi_dec_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VaapiVP8Accelerator);
};

class VaapiVP9Picture : public VP9Picture {
 public:
  explicit VaapiVP9Picture(
      scoped_refptr<VaapiVideoDecodeAccelerator::VaapiDecodeSurface> surface)
      : dec_surface_(surface) {}

  VaapiVP9Picture* AsVaapiVP9Picture() override { return this; }
  scoped_refptr<VaapiVideoDecodeAccelerator::VaapiDecodeSurface> dec_surface() {
    return dec_surface_;
  }

 private:
  ~VaapiVP9Picture() override {}

  scoped_refptr<VaapiVideoDecodeAccelerator::VaapiDecodeSurface> dec_surface_;

  DISALLOW_COPY_AND_ASSIGN(VaapiVP9Picture);
};

class VaapiVideoDecodeAccelerator::VaapiVP9Accelerator
    : public VP9Decoder::VP9Accelerator {
 public:
  VaapiVP9Accelerator(VaapiVideoDecodeAccelerator* vaapi_dec,
                      VaapiWrapper* vaapi_wrapper);
  ~VaapiVP9Accelerator() override;

  // VP9Decoder::VP9Accelerator implementation.
  scoped_refptr<VP9Picture> CreateVP9Picture() override;

  bool SubmitDecode(const scoped_refptr<VP9Picture>& pic,
                    const Vp9SegmentationParams& seg,
                    const Vp9LoopFilterParams& lf,
                    const std::vector<scoped_refptr<VP9Picture>>& ref_pictures,
                    const base::Closure& done_cb) override;

  bool OutputPicture(const scoped_refptr<VP9Picture>& pic) override;

  bool IsFrameContextRequired() const override { return false; }

  bool GetFrameContext(const scoped_refptr<VP9Picture>& pic,
                       Vp9FrameContext* frame_ctx) override;

 private:
  scoped_refptr<VaapiDecodeSurface> VP9PictureToVaapiDecodeSurface(
      const scoped_refptr<VP9Picture>& pic);

  VaapiWrapper* vaapi_wrapper_;
  VaapiVideoDecodeAccelerator* vaapi_dec_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VaapiVP9Accelerator);
};

class VaapiVideoDecodeAccelerator::InputBuffer {
 public:
  InputBuffer() = default;
  InputBuffer(uint32_t id,
              std::unique_ptr<SharedMemoryRegion> shm,
              base::OnceCallback<void(int32_t id)> release_cb)
      : id_(id), shm_(std::move(shm)), release_cb_(std::move(release_cb)) {}
  ~InputBuffer() {
    VLOGF(4) << "id = " << id_;
    if (release_cb_)
      std::move(release_cb_).Run(id_);
  }

  // Indicates this is a dummy buffer for flush request.
  bool IsFlushRequest() const { return shm_ == nullptr; }
  int32_t id() const { return id_; }
  SharedMemoryRegion* shm() const { return shm_.get(); }

 private:
  const int32_t id_ = -1;
  const std::unique_ptr<SharedMemoryRegion> shm_;
  base::OnceCallback<void(int32_t id)> release_cb_;

  DISALLOW_COPY_AND_ASSIGN(InputBuffer);
};

void VaapiVideoDecodeAccelerator::NotifyError(Error error) {
  if (!task_runner_->BelongsToCurrentThread()) {
    DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&VaapiVideoDecodeAccelerator::NotifyError,
                                      weak_this_, error));
    return;
  }

  // Post Cleanup() as a task so we don't recursively acquire lock_.
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&VaapiVideoDecodeAccelerator::Cleanup, weak_this_));

  VLOGF(1) << "Notifying of error " << error;
  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.reset();
  }
}

VaapiPicture* VaapiVideoDecodeAccelerator::PictureById(
    int32_t picture_buffer_id) {
  Pictures::iterator it = pictures_.find(picture_buffer_id);
  if (it == pictures_.end()) {
    VLOGF(4) << "Picture id " << picture_buffer_id << " does not exist";
    return NULL;
  }

  return it->second.get();
}

VaapiVideoDecodeAccelerator::VaapiVideoDecodeAccelerator(
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb)
    : state_(kUninitialized),
      input_ready_(&lock_),
      vaapi_picture_factory_(new VaapiPictureFactory()),
      surfaces_available_(&lock_),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      decoder_thread_("VaapiDecoderThread"),
      num_frames_at_client_(0),
      finish_flush_pending_(false),
      awaiting_va_surfaces_recycle_(false),
      requested_num_pics_(0),
      profile_(VIDEO_CODEC_PROFILE_UNKNOWN),
      make_context_current_cb_(make_context_current_cb),
      bind_image_cb_(bind_image_cb),
      weak_this_factory_(this) {
  weak_this_ = weak_this_factory_.GetWeakPtr();
  va_surface_release_cb_ = BindToCurrentLoop(
      base::Bind(&VaapiVideoDecodeAccelerator::RecycleVASurfaceID, weak_this_));
}

VaapiVideoDecodeAccelerator::~VaapiVideoDecodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

bool VaapiVideoDecodeAccelerator::Initialize(const Config& config,
                                             Client* client) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (config.is_encrypted()) {
    NOTREACHED() << "Encrypted streams are not supported for this VDA";
    return false;
  }

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();

  VideoCodecProfile profile = config.profile;

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kUninitialized);
  VLOGF(2) << "Initializing VAVDA, profile: " << GetProfileName(profile);

  vaapi_wrapper_ = VaapiWrapper::CreateForVideoCodec(
      VaapiWrapper::kDecode, profile, base::Bind(&ReportToUMA, VAAPI_ERROR));

  if (!vaapi_wrapper_.get()) {
    VLOGF(1) << "Failed initializing VAAPI for profile "
             << GetProfileName(profile);
    return false;
  }

  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    h264_accelerator_.reset(
        new VaapiH264Accelerator(this, vaapi_wrapper_.get()));
    decoder_.reset(new H264Decoder(h264_accelerator_.get()));
  } else if (profile >= VP8PROFILE_MIN && profile <= VP8PROFILE_MAX) {
    vp8_accelerator_.reset(new VaapiVP8Accelerator(this, vaapi_wrapper_.get()));
    decoder_.reset(new VP8Decoder(vp8_accelerator_.get()));
  } else if (profile >= VP9PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    vp9_accelerator_.reset(new VaapiVP9Accelerator(this, vaapi_wrapper_.get()));
    decoder_.reset(new VP9Decoder(vp9_accelerator_.get()));
  } else {
    VLOGF(1) << "Unsupported profile " << GetProfileName(profile);
    return false;
  }
  profile_ = profile;

  CHECK(decoder_thread_.Start());
  decoder_thread_task_runner_ = decoder_thread_.task_runner();

  state_ = kIdle;
  output_mode_ = config.output_mode;
  return true;
}

void VaapiVideoDecodeAccelerator::OutputPicture(
    const scoped_refptr<VASurface>& va_surface,
    int32_t input_id,
    gfx::Rect visible_rect,
    VaapiPicture* picture) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  int32_t output_id = picture->picture_buffer_id();

  VLOGF(4) << "Outputting VASurface " << va_surface->id()
           << " into pixmap bound to picture buffer id " << output_id;
  {
    TRACE_EVENT2("media,gpu", "VAVDA::DownloadFromSurface", "input_id",
                 input_id, "output_id", output_id);
    RETURN_AND_NOTIFY_ON_FAILURE(picture->DownloadFromSurface(va_surface),
                                 "Failed putting surface into pixmap",
                                 PLATFORM_FAILURE, );
  }
  // Notify the client a picture is ready to be displayed.
  ++num_frames_at_client_;
  TRACE_COUNTER1("media,gpu", "Textures at client", num_frames_at_client_);
  VLOGF(4) << "Notifying output picture id " << output_id << " for input "
           << input_id
           << " is ready. visible rect: " << visible_rect.ToString();
  if (client_) {
    // TODO(hubbe): Use the correct color space.  http://crbug.com/647725
    client_->PictureReady(Picture(output_id, input_id, visible_rect,
                                  gfx::ColorSpace(), picture->AllowOverlay()));
  }
}

void VaapiVideoDecodeAccelerator::TryOutputSurface() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Handle Destroy() arriving while pictures are queued for output.
  if (!client_)
    return;

  if (pending_output_cbs_.empty() || output_buffers_.empty())
    return;

  OutputCB output_cb = pending_output_cbs_.front();
  pending_output_cbs_.pop();

  VaapiPicture* picture = PictureById(output_buffers_.front());
  DCHECK(picture);
  output_buffers_.pop();

  output_cb.Run(picture);

  if (finish_flush_pending_ && pending_output_cbs_.empty())
    FinishFlush();
}

void VaapiVideoDecodeAccelerator::QueueInputBuffer(
    const BitstreamBuffer& bitstream_buffer) {
  VLOGF(4) << "Queueing new input buffer id: " << bitstream_buffer.id()
           << " size: " << (int)bitstream_buffer.size();
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media,gpu", "QueueInputBuffer", "input_id",
               bitstream_buffer.id());

  base::AutoLock auto_lock(lock_);
  if (bitstream_buffer.size() == 0) {
    DCHECK(!base::SharedMemory::IsHandleValid(bitstream_buffer.handle()));
    // Dummy buffer for flush.
    auto flush_buffer = std::make_unique<InputBuffer>();
    DCHECK(flush_buffer->IsFlushRequest());
    input_buffers_.push(std::move(flush_buffer));
  } else {
    std::unique_ptr<SharedMemoryRegion> shm(
        new SharedMemoryRegion(bitstream_buffer, true));
    RETURN_AND_NOTIFY_ON_FAILURE(shm->Map(), "Failed to map input buffer",
                                 UNREADABLE_INPUT, );

    auto input_buffer = std::make_unique<InputBuffer>(
        bitstream_buffer.id(), std::move(shm),
        BindToCurrentLoop(
            base::Bind(&Client::NotifyEndOfBitstreamBuffer, client_)));
    input_buffers_.push(std::move(input_buffer));

    TRACE_COUNTER1("media,gpu", "Input buffers", input_buffers_.size());
  }

  input_ready_.Signal();

  switch (state_) {
    case kIdle:
      state_ = kDecoding;
      decoder_thread_task_runner_->PostTask(
          FROM_HERE, base::Bind(&VaapiVideoDecodeAccelerator::DecodeTask,
                                base::Unretained(this)));
      break;

    case kDecoding:
      // Decoder already running.
      break;

    case kResetting:
      // When resetting, allow accumulating bitstream buffers, so that
      // the client can queue after-seek-buffers while we are finishing with
      // the before-seek one.
      break;

    default:
      VLOGF(1) << "Decode/Flush request from client in invalid state: "
               << state_;
      NotifyError(PLATFORM_FAILURE);
      break;
  }
}

bool VaapiVideoDecodeAccelerator::GetInputBuffer_Locked() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (curr_input_buffer_.get())
    return true;

  // Will only wait if it is expected that in current state new buffers will
  // be queued from the client via Decode(). The state can change during wait.
  while (input_buffers_.empty() && (state_ == kDecoding || state_ == kIdle)) {
    input_ready_.Wait();
  }

  // We could have got woken up in a different state or never got to sleep
  // due to current state.
  if (state_ != kDecoding && state_ != kIdle)
    return false;

  DCHECK(!input_buffers_.empty());
  curr_input_buffer_ = std::move(input_buffers_.front());
  input_buffers_.pop();

  if (curr_input_buffer_->IsFlushRequest()) {
    VLOGF(4) << "New flush buffer";
    return true;
  }

  VLOGF(4) << "New current input buffer, id: " << curr_input_buffer_->id()
           << " size: " << curr_input_buffer_->shm()->size() << "B";
  decoder_->SetStream(
      static_cast<uint8_t*>(curr_input_buffer_->shm()->memory()),
      curr_input_buffer_->shm()->size());

  return true;
}

void VaapiVideoDecodeAccelerator::ReturnCurrInputBuffer_Locked() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();
  DCHECK(curr_input_buffer_.get());
  curr_input_buffer_.reset();

  TRACE_COUNTER1("media,gpu", "Input buffers", input_buffers_.size());
}

// TODO(posciak): refactor the whole class to remove sleeping in wait for
// surfaces, and reschedule DecodeTask instead.
bool VaapiVideoDecodeAccelerator::WaitForSurfaces_Locked() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  while (available_va_surfaces_.empty() &&
         (state_ == kDecoding || state_ == kIdle)) {
    surfaces_available_.Wait();
  }

  return state_ == kDecoding || state_ == kIdle;
}

void VaapiVideoDecodeAccelerator::DecodeTask() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  if (state_ != kDecoding)
    return;

  // Main decode task.
  VLOGF(4) << "Decode task";

  // Try to decode what stream data is (still) in the decoder until we run out
  // of it.
  while (GetInputBuffer_Locked()) {
    DCHECK(curr_input_buffer_.get());

    if (curr_input_buffer_->IsFlushRequest()) {
      FlushTask();
      break;
    }

    AcceleratedVideoDecoder::DecodeResult res;
    {
      // We are OK releasing the lock here, as decoder never calls our methods
      // directly and we will reacquire the lock before looking at state again.
      // This is the main decode function of the decoder and while keeping
      // the lock for its duration would be fine, it would defeat the purpose
      // of having a separate decoder thread.
      base::AutoUnlock auto_unlock(lock_);
      TRACE_EVENT0("media,gpu", "VAVDA::Decode");
      res = decoder_->Decode();
    }

    switch (res) {
      case AcceleratedVideoDecoder::kAllocateNewSurfaces:
        VLOGF(2) << "Decoder requesting a new set of surfaces";
        task_runner_->PostTask(
            FROM_HERE,
            base::Bind(&VaapiVideoDecodeAccelerator::InitiateSurfaceSetChange,
                       weak_this_, decoder_->GetRequiredNumOfPictures(),
                       decoder_->GetPicSize()));
        // We'll get rescheduled once ProvidePictureBuffers() finishes.
        return;

      case AcceleratedVideoDecoder::kRanOutOfStreamData:
        ReturnCurrInputBuffer_Locked();
        break;

      case AcceleratedVideoDecoder::kRanOutOfSurfaces:
        // No more output buffers in the decoder, try getting more or go to
        // sleep waiting for them.
        if (!WaitForSurfaces_Locked())
          return;

        break;

      case AcceleratedVideoDecoder::kNeedContextUpdate:
        // This should not happen as we return false from
        // IsFrameContextRequired().
        NOTREACHED() << "Context updates not supported";
        return;

      case AcceleratedVideoDecoder::kDecodeError:
        RETURN_AND_NOTIFY_ON_FAILURE(false, "Error decoding stream",
                                     PLATFORM_FAILURE, );
        return;
    }
  }
}

void VaapiVideoDecodeAccelerator::InitiateSurfaceSetChange(size_t num_pics,
                                                           gfx::Size size) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!awaiting_va_surfaces_recycle_);

  // At this point decoder has stopped running and has already posted onto our
  // loop any remaining output request callbacks, which executed before we got
  // here. Some of them might have been pended though, because we might not
  // have had enough TFPictures to output surfaces to. Initiate a wait cycle,
  // which will wait for client to return enough PictureBuffers to us, so that
  // we can finish all pending output callbacks, releasing associated surfaces.
  VLOGF(2) << "Initiating surface set change";
  awaiting_va_surfaces_recycle_ = true;

  requested_num_pics_ = num_pics;
  requested_pic_size_ = size;

  TryFinishSurfaceSetChange();
}

void VaapiVideoDecodeAccelerator::TryFinishSurfaceSetChange() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!awaiting_va_surfaces_recycle_)
    return;

  if (!pending_output_cbs_.empty() ||
      pictures_.size() != available_va_surfaces_.size()) {
    // Either:
    // 1. Not all pending pending output callbacks have been executed yet.
    // Wait for the client to return enough pictures and retry later.
    // 2. The above happened and all surface release callbacks have been posted
    // as the result, but not all have executed yet. Post ourselves after them
    // to let them release surfaces.
    DVLOGF(2) << "Awaiting pending output/surface release callbacks to finish";
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VaapiVideoDecodeAccelerator::TryFinishSurfaceSetChange,
                   weak_this_));
    return;
  }

  // All surfaces released, destroy them and dismiss all PictureBuffers.
  awaiting_va_surfaces_recycle_ = false;
  available_va_surfaces_.clear();
  vaapi_wrapper_->DestroySurfaces();

  for (Pictures::iterator iter = pictures_.begin(); iter != pictures_.end();
       ++iter) {
    VLOGF(2) << "Dismissing picture id: " << iter->first;
    if (client_)
      client_->DismissPictureBuffer(iter->first);
  }
  pictures_.clear();

  // And ask for a new set as requested.
  VLOGF(2) << "Requesting " << requested_num_pics_
           << " pictures of size: " << requested_pic_size_.ToString();

  VideoPixelFormat format = GfxBufferFormatToVideoPixelFormat(
      vaapi_picture_factory_->GetBufferFormat());
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&Client::ProvidePictureBuffers, client_,
                            requested_num_pics_, format, 1, requested_pic_size_,
                            vaapi_picture_factory_->GetGLTextureTarget()));
}

void VaapiVideoDecodeAccelerator::Decode(
    const BitstreamBuffer& bitstream_buffer) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media,gpu", "VAVDA::Decode", "Buffer id",
               bitstream_buffer.id());

  if (bitstream_buffer.id() < 0) {
    if (base::SharedMemory::IsHandleValid(bitstream_buffer.handle()))
      base::SharedMemory::CloseHandle(bitstream_buffer.handle());
    VLOGF(1) << "Invalid bitstream_buffer, id: " << bitstream_buffer.id();
    NotifyError(INVALID_ARGUMENT);
    return;
  }

  // Skip empty buffers. VaapiVDA uses empty buffer as dummy buffer for flush
  // internally.
  if (bitstream_buffer.size() == 0) {
    if (base::SharedMemory::IsHandleValid(bitstream_buffer.handle()))
      base::SharedMemory::CloseHandle(bitstream_buffer.handle());
    if (client_)
      client_->NotifyEndOfBitstreamBuffer(bitstream_buffer.id());
    return;
  }

  QueueInputBuffer(bitstream_buffer);
}

void VaapiVideoDecodeAccelerator::RecycleVASurfaceID(
    VASurfaceID va_surface_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  available_va_surfaces_.push_back(va_surface_id);
  surfaces_available_.Signal();
}

void VaapiVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& buffers) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK(pictures_.empty());

  while (!output_buffers_.empty())
    output_buffers_.pop();

  RETURN_AND_NOTIFY_ON_FAILURE(
      buffers.size() >= requested_num_pics_,
      "Got an invalid number of picture buffers. (Got " << buffers.size()
      << ", requested " << requested_num_pics_ << ")", INVALID_ARGUMENT, );
  DCHECK(requested_pic_size_ == buffers[0].size());

  const unsigned int va_format = GetVaFormatForVideoCodecProfile(profile_);
  std::vector<VASurfaceID> va_surface_ids;
  RETURN_AND_NOTIFY_ON_FAILURE(
      vaapi_wrapper_->CreateSurfaces(va_format, requested_pic_size_,
                                     buffers.size(), &va_surface_ids),
      "Failed creating VA Surfaces", PLATFORM_FAILURE, );
  DCHECK_EQ(va_surface_ids.size(), buffers.size());

  for (size_t i = 0; i < buffers.size(); ++i) {
    uint32_t client_id = !buffers[i].client_texture_ids().empty()
                             ? buffers[i].client_texture_ids()[0]
                             : 0;
    uint32_t service_id = !buffers[i].service_texture_ids().empty()
                              ? buffers[i].service_texture_ids()[0]
                              : 0;

    DCHECK_EQ(buffers[i].texture_target(),
              vaapi_picture_factory_->GetGLTextureTarget());

    std::unique_ptr<VaapiPicture> picture(vaapi_picture_factory_->Create(
        vaapi_wrapper_, make_context_current_cb_, bind_image_cb_,
        buffers[i].id(), requested_pic_size_, service_id, client_id,
        buffers[i].texture_target()));
    RETURN_AND_NOTIFY_ON_FAILURE(
        picture.get(), "Failed creating a VaapiPicture", PLATFORM_FAILURE, );

    if (output_mode_ == Config::OutputMode::ALLOCATE) {
      RETURN_AND_NOTIFY_ON_FAILURE(
          picture->Allocate(vaapi_picture_factory_->GetBufferFormat()),
          "Failed to allocate memory for a VaapiPicture", PLATFORM_FAILURE, );
      output_buffers_.push(buffers[i].id());
    }
    bool inserted =
        pictures_.insert(std::make_pair(buffers[i].id(), std::move(picture)))
            .second;
    DCHECK(inserted);

    available_va_surfaces_.push_back(va_surface_ids[i]);
    surfaces_available_.Signal();
  }

  // Resume DecodeTask if it is still in decoding state.
  if (state_ == kDecoding) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&VaapiVideoDecodeAccelerator::DecodeTask,
                              base::Unretained(this)));
  }
}

#if defined(USE_OZONE)
static void CloseGpuMemoryBufferHandle(
    const gfx::GpuMemoryBufferHandle& handle) {
  for (const auto& fd : handle.native_pixmap_handle.fds) {
    // Close the fd by wrapping it in a ScopedFD and letting
    // it fall out of scope.
    base::ScopedFD scoped_fd(fd.fd);
  }
}

void VaapiVideoDecodeAccelerator::ImportBufferForPicture(
    int32_t picture_buffer_id,
    VideoPixelFormat pixel_format,
    const gfx::GpuMemoryBufferHandle& gpu_memory_buffer_handle) {
  VLOGF(2) << "Importing picture id: " << picture_buffer_id;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (output_mode_ != Config::OutputMode::IMPORT) {
    CloseGpuMemoryBufferHandle(gpu_memory_buffer_handle);
    VLOGF(1) << "Cannot import in non-import mode";
    NotifyError(INVALID_ARGUMENT);
    return;
  }

  VaapiPicture* picture = PictureById(picture_buffer_id);
  if (!picture) {
    CloseGpuMemoryBufferHandle(gpu_memory_buffer_handle);

    // It's possible that we've already posted a DismissPictureBuffer for this
    // picture, but it has not yet executed when this ImportBufferForPicture
    // was posted to us by the client. In that case just ignore this (we've
    // already dismissed it and accounted for that).
    VLOGF(3) << "got picture id=" << picture_buffer_id
             << " not in use (anymore?).";
    return;
  }

  if (!picture->ImportGpuMemoryBufferHandle(
          VideoPixelFormatToGfxBufferFormat(pixel_format),
          gpu_memory_buffer_handle)) {
    // ImportGpuMemoryBufferHandle will close the handles even on failure, so
    // we don't need to do this ourselves.
    VLOGF(1) << "Failed to import GpuMemoryBufferHandle";
    NotifyError(PLATFORM_FAILURE);
    return;
  }

  ReusePictureBuffer(picture_buffer_id);
}
#endif

void VaapiVideoDecodeAccelerator::ReusePictureBuffer(
    int32_t picture_buffer_id) {
  VLOGF(4) << "picture id=" << picture_buffer_id;
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media,gpu", "VAVDA::ReusePictureBuffer", "Picture id",
               picture_buffer_id);

  if (!PictureById(picture_buffer_id)) {
    // It's possible that we've already posted a DismissPictureBuffer for this
    // picture, but it has not yet executed when this ReusePictureBuffer
    // was posted to us by the client. In that case just ignore this (we've
    // already dismissed it and accounted for that).
    VLOGF(3) << "got picture id=" << picture_buffer_id
             << " not in use (anymore?).";
    return;
  }

  --num_frames_at_client_;
  TRACE_COUNTER1("media,gpu", "Textures at client", num_frames_at_client_);

  output_buffers_.push(picture_buffer_id);
  TryOutputSurface();
}

void VaapiVideoDecodeAccelerator::FlushTask() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(curr_input_buffer_.get() && curr_input_buffer_->IsFlushRequest());

  curr_input_buffer_.reset();

  // First flush all the pictures that haven't been outputted, notifying the
  // client to output them.
  bool res = decoder_->Flush();
  RETURN_AND_NOTIFY_ON_FAILURE(res, "Failed flushing the decoder.",
                               PLATFORM_FAILURE, );

  // Put the decoder in idle state, ready to resume.
  decoder_->Reset();

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VaapiVideoDecodeAccelerator::FinishFlush, weak_this_));
}

void VaapiVideoDecodeAccelerator::Flush() {
  VLOGF(2) << "Got flush request";
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Queue a dummy buffer, which means flush.
  QueueInputBuffer(media::BitstreamBuffer());
}

void VaapiVideoDecodeAccelerator::FinishFlush() {
  VLOGF(2);
  DCHECK(task_runner_->BelongsToCurrentThread());

  finish_flush_pending_ = false;

  base::AutoLock auto_lock(lock_);
  if (state_ != kDecoding) {
    DCHECK(state_ == kDestroying || state_ == kResetting) << state_;
    return;
  }

  // Still waiting for textures from client to finish outputting all pending
  // frames. Try again later.
  if (!pending_output_cbs_.empty()) {
    finish_flush_pending_ = true;
    return;
  }

  // Resume decoding if necessary.
  if (input_buffers_.empty()) {
    state_ = kIdle;
  } else {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&VaapiVideoDecodeAccelerator::DecodeTask,
                              base::Unretained(this)));
  }

  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&Client::NotifyFlushDone, client_));
}

void VaapiVideoDecodeAccelerator::ResetTask() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  // All the decoding tasks from before the reset request from client are done
  // by now, as this task was scheduled after them and client is expected not
  // to call Decode() after Reset() and before NotifyResetDone.
  decoder_->Reset();

  base::AutoLock auto_lock(lock_);

  // Return current input buffer, if present.
  if (curr_input_buffer_.get())
    ReturnCurrInputBuffer_Locked();

  // And let client know that we are done with reset.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VaapiVideoDecodeAccelerator::FinishReset, weak_this_));
}

void VaapiVideoDecodeAccelerator::Reset() {
  VLOGF(2) << "Got reset request";
  DCHECK(task_runner_->BelongsToCurrentThread());

  // This will make any new decode tasks exit early.
  base::AutoLock auto_lock(lock_);
  state_ = kResetting;
  finish_flush_pending_ = false;

  // Drop all remaining input buffers, if present.
  while (!input_buffers_.empty())
    input_buffers_.pop();
  TRACE_COUNTER1("media,gpu", "Input buffers", input_buffers_.size());

  decoder_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VaapiVideoDecodeAccelerator::ResetTask,
                            base::Unretained(this)));

  input_ready_.Signal();
  surfaces_available_.Signal();
}

void VaapiVideoDecodeAccelerator::FinishReset() {
  VLOGF(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  if (state_ != kResetting) {
    DCHECK(state_ == kDestroying || state_ == kUninitialized) << state_;
    return;  // We could've gotten destroyed already.
  }

  // Drop pending outputs.
  while (!pending_output_cbs_.empty())
    pending_output_cbs_.pop();

  if (awaiting_va_surfaces_recycle_) {
    // Decoder requested a new surface set while we were waiting for it to
    // finish the last DecodeTask, running at the time of Reset().
    // Let the surface set change finish first before resetting.
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VaapiVideoDecodeAccelerator::FinishReset, weak_this_));
    return;
  }

  state_ = kIdle;

  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&Client::NotifyResetDone, client_));

  // The client might have given us new buffers via Decode() while we were
  // resetting and might be waiting for our move, and not call Decode() anymore
  // until we return something. Post a DecodeTask() so that we won't
  // sleep forever waiting for Decode() in that case. Having two of them
  // in the pipe is harmless, the additional one will return as soon as it sees
  // that we are back in kDecoding state.
  if (!input_buffers_.empty()) {
    state_ = kDecoding;
    decoder_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&VaapiVideoDecodeAccelerator::DecodeTask,
                              base::Unretained(this)));
  }
}

void VaapiVideoDecodeAccelerator::Cleanup() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  if (state_ == kUninitialized || state_ == kDestroying)
    return;

  VLOGF(2) << "Destroying VAVDA";
  state_ = kDestroying;

  client_ptr_factory_.reset();
  weak_this_factory_.InvalidateWeakPtrs();

  // TODO(mcasas): consider deleting |decoder_| and |*_accelerator_| on
  // |decoder_thread_task_runner_|, https://crbug.com/789160.

  // Signal all potential waiters on the decoder_thread_, let them early-exit,
  // as we've just moved to the kDestroying state, and wait for all tasks
  // to finish.
  input_ready_.Signal();
  surfaces_available_.Signal();
  {
    base::AutoUnlock auto_unlock(lock_);
    decoder_thread_.Stop();
  }

  state_ = kUninitialized;
}

void VaapiVideoDecodeAccelerator::Destroy() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  Cleanup();
  delete this;
}

bool VaapiVideoDecodeAccelerator::TryToSetupDecodeOnSeparateThread(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner) {
  return false;
}

bool VaapiVideoDecodeAccelerator::DecodeSurface(
    const scoped_refptr<VaapiDecodeSurface>& dec_surface) {
  const bool result = vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(
      dec_surface->va_surface()->id());
  if (!result)
    VLOGF(1) << "Failed decoding picture";
  return result;
}

void VaapiVideoDecodeAccelerator::SurfaceReady(
    const scoped_refptr<VaapiDecodeSurface>& dec_surface) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&VaapiVideoDecodeAccelerator::SurfaceReady,
                              weak_this_, dec_surface));
    return;
  }

  DCHECK(!awaiting_va_surfaces_recycle_);

  {
    base::AutoLock auto_lock(lock_);
    // Drop any requests to output if we are resetting or being destroyed.
    if (state_ == kResetting || state_ == kDestroying)
      return;
  }

  pending_output_cbs_.push(
      base::Bind(&VaapiVideoDecodeAccelerator::OutputPicture, weak_this_,
                 dec_surface->va_surface(), dec_surface->bitstream_id(),
                 dec_surface->visible_rect()));

  TryOutputSurface();
}

scoped_refptr<VaapiVideoDecodeAccelerator::VaapiDecodeSurface>
VaapiVideoDecodeAccelerator::CreateSurface() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  if (available_va_surfaces_.empty())
    return nullptr;

  DCHECK(!awaiting_va_surfaces_recycle_);
  scoped_refptr<VASurface> va_surface(new VASurface(
      available_va_surfaces_.front(), requested_pic_size_,
      vaapi_wrapper_->va_surface_format(), va_surface_release_cb_));
  available_va_surfaces_.pop_front();

  return new VaapiDecodeSurface(curr_input_buffer_->id(), va_surface);
}

VaapiVideoDecodeAccelerator::VaapiH264Accelerator::VaapiH264Accelerator(
    VaapiVideoDecodeAccelerator* vaapi_dec,
    VaapiWrapper* vaapi_wrapper)
    : vaapi_wrapper_(vaapi_wrapper), vaapi_dec_(vaapi_dec) {
  DCHECK(vaapi_wrapper_);
  DCHECK(vaapi_dec_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VaapiVideoDecodeAccelerator::VaapiH264Accelerator::~VaapiH264Accelerator() {
  // TODO(mcasas): consider enabling the checker, https://crbug.com/789160
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<H264Picture>
VaapiVideoDecodeAccelerator::VaapiH264Accelerator::CreateH264Picture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<VaapiDecodeSurface> va_surface = vaapi_dec_->CreateSurface();
  if (!va_surface)
    return nullptr;

  return new VaapiH264Picture(std::move(va_surface));
}

// Fill |va_pic| with default/neutral values.
static void InitVAPicture(VAPictureH264* va_pic) {
  memset(va_pic, 0, sizeof(*va_pic));
  va_pic->picture_id = VA_INVALID_ID;
  va_pic->flags = VA_PICTURE_H264_INVALID;
}

bool VaapiVideoDecodeAccelerator::VaapiH264Accelerator::SubmitFrameMetadata(
    const H264SPS* sps,
    const H264PPS* pps,
    const H264DPB& dpb,
    const H264Picture::Vector& ref_pic_listp0,
    const H264Picture::Vector& ref_pic_listb0,
    const H264Picture::Vector& ref_pic_listb1,
    const scoped_refptr<H264Picture>& pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VAPictureParameterBufferH264 pic_param;
  memset(&pic_param, 0, sizeof(pic_param));

#define FROM_SPS_TO_PP(a) pic_param.a = sps->a
#define FROM_SPS_TO_PP2(a, b) pic_param.b = sps->a
  FROM_SPS_TO_PP2(pic_width_in_mbs_minus1, picture_width_in_mbs_minus1);
  // This assumes non-interlaced video
  FROM_SPS_TO_PP2(pic_height_in_map_units_minus1, picture_height_in_mbs_minus1);
  FROM_SPS_TO_PP(bit_depth_luma_minus8);
  FROM_SPS_TO_PP(bit_depth_chroma_minus8);
#undef FROM_SPS_TO_PP
#undef FROM_SPS_TO_PP2

#define FROM_SPS_TO_PP_SF(a) pic_param.seq_fields.bits.a = sps->a
#define FROM_SPS_TO_PP_SF2(a, b) pic_param.seq_fields.bits.b = sps->a
  FROM_SPS_TO_PP_SF(chroma_format_idc);
  FROM_SPS_TO_PP_SF2(separate_colour_plane_flag,
                     residual_colour_transform_flag);
  FROM_SPS_TO_PP_SF(gaps_in_frame_num_value_allowed_flag);
  FROM_SPS_TO_PP_SF(frame_mbs_only_flag);
  FROM_SPS_TO_PP_SF(mb_adaptive_frame_field_flag);
  FROM_SPS_TO_PP_SF(direct_8x8_inference_flag);
  pic_param.seq_fields.bits.MinLumaBiPredSize8x8 = (sps->level_idc >= 31);
  FROM_SPS_TO_PP_SF(log2_max_frame_num_minus4);
  FROM_SPS_TO_PP_SF(pic_order_cnt_type);
  FROM_SPS_TO_PP_SF(log2_max_pic_order_cnt_lsb_minus4);
  FROM_SPS_TO_PP_SF(delta_pic_order_always_zero_flag);
#undef FROM_SPS_TO_PP_SF
#undef FROM_SPS_TO_PP_SF2

#define FROM_PPS_TO_PP(a) pic_param.a = pps->a
  FROM_PPS_TO_PP(pic_init_qp_minus26);
  FROM_PPS_TO_PP(pic_init_qs_minus26);
  FROM_PPS_TO_PP(chroma_qp_index_offset);
  FROM_PPS_TO_PP(second_chroma_qp_index_offset);
#undef FROM_PPS_TO_PP

#define FROM_PPS_TO_PP_PF(a) pic_param.pic_fields.bits.a = pps->a
#define FROM_PPS_TO_PP_PF2(a, b) pic_param.pic_fields.bits.b = pps->a
  FROM_PPS_TO_PP_PF(entropy_coding_mode_flag);
  FROM_PPS_TO_PP_PF(weighted_pred_flag);
  FROM_PPS_TO_PP_PF(weighted_bipred_idc);
  FROM_PPS_TO_PP_PF(transform_8x8_mode_flag);

  pic_param.pic_fields.bits.field_pic_flag = 0;
  FROM_PPS_TO_PP_PF(constrained_intra_pred_flag);
  FROM_PPS_TO_PP_PF2(bottom_field_pic_order_in_frame_present_flag,
                     pic_order_present_flag);
  FROM_PPS_TO_PP_PF(deblocking_filter_control_present_flag);
  FROM_PPS_TO_PP_PF(redundant_pic_cnt_present_flag);
  pic_param.pic_fields.bits.reference_pic_flag = pic->ref;
#undef FROM_PPS_TO_PP_PF
#undef FROM_PPS_TO_PP_PF2

  pic_param.frame_num = pic->frame_num;

  InitVAPicture(&pic_param.CurrPic);
  FillVAPicture(&pic_param.CurrPic, pic);

  // Init reference pictures' array.
  for (int i = 0; i < 16; ++i)
    InitVAPicture(&pic_param.ReferenceFrames[i]);

  // And fill it with picture info from DPB.
  FillVARefFramesFromDPB(dpb, pic_param.ReferenceFrames,
                         arraysize(pic_param.ReferenceFrames));

  pic_param.num_ref_frames = sps->max_num_ref_frames;

  if (!vaapi_wrapper_->SubmitBuffer(VAPictureParameterBufferType,
                                    sizeof(pic_param), &pic_param))
    return false;

  VAIQMatrixBufferH264 iq_matrix_buf;
  memset(&iq_matrix_buf, 0, sizeof(iq_matrix_buf));

  if (pps->pic_scaling_matrix_present_flag) {
    for (int i = 0; i < 6; ++i) {
      for (int j = 0; j < 16; ++j)
        iq_matrix_buf.ScalingList4x4[i][kZigzagScan4x4[j]] =
            pps->scaling_list4x4[i][j];
    }

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 64; ++j)
        iq_matrix_buf.ScalingList8x8[i][kZigzagScan8x8[j]] =
            pps->scaling_list8x8[i][j];
    }
  } else {
    for (int i = 0; i < 6; ++i) {
      for (int j = 0; j < 16; ++j)
        iq_matrix_buf.ScalingList4x4[i][kZigzagScan4x4[j]] =
            sps->scaling_list4x4[i][j];
    }

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 64; ++j)
        iq_matrix_buf.ScalingList8x8[i][kZigzagScan8x8[j]] =
            sps->scaling_list8x8[i][j];
    }
  }

  return vaapi_wrapper_->SubmitBuffer(VAIQMatrixBufferType,
                                      sizeof(iq_matrix_buf), &iq_matrix_buf);
}

bool VaapiVideoDecodeAccelerator::VaapiH264Accelerator::SubmitSlice(
    const H264PPS* pps,
    const H264SliceHeader* slice_hdr,
    const H264Picture::Vector& ref_pic_list0,
    const H264Picture::Vector& ref_pic_list1,
    const scoped_refptr<H264Picture>& pic,
    const uint8_t* data,
    size_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VASliceParameterBufferH264 slice_param;
  memset(&slice_param, 0, sizeof(slice_param));

  slice_param.slice_data_size = slice_hdr->nalu_size;
  slice_param.slice_data_offset = 0;
  slice_param.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
  slice_param.slice_data_bit_offset = slice_hdr->header_bit_size;

#define SHDRToSP(a) slice_param.a = slice_hdr->a
  SHDRToSP(first_mb_in_slice);
  slice_param.slice_type = slice_hdr->slice_type % 5;
  SHDRToSP(direct_spatial_mv_pred_flag);

  // TODO posciak: make sure parser sets those even when override flags
  // in slice header is off.
  SHDRToSP(num_ref_idx_l0_active_minus1);
  SHDRToSP(num_ref_idx_l1_active_minus1);
  SHDRToSP(cabac_init_idc);
  SHDRToSP(slice_qp_delta);
  SHDRToSP(disable_deblocking_filter_idc);
  SHDRToSP(slice_alpha_c0_offset_div2);
  SHDRToSP(slice_beta_offset_div2);

  if (((slice_hdr->IsPSlice() || slice_hdr->IsSPSlice()) &&
       pps->weighted_pred_flag) ||
      (slice_hdr->IsBSlice() && pps->weighted_bipred_idc == 1)) {
    SHDRToSP(luma_log2_weight_denom);
    SHDRToSP(chroma_log2_weight_denom);

    SHDRToSP(luma_weight_l0_flag);
    SHDRToSP(luma_weight_l1_flag);

    SHDRToSP(chroma_weight_l0_flag);
    SHDRToSP(chroma_weight_l1_flag);

    for (int i = 0; i <= slice_param.num_ref_idx_l0_active_minus1; ++i) {
      slice_param.luma_weight_l0[i] =
          slice_hdr->pred_weight_table_l0.luma_weight[i];
      slice_param.luma_offset_l0[i] =
          slice_hdr->pred_weight_table_l0.luma_offset[i];

      for (int j = 0; j < 2; ++j) {
        slice_param.chroma_weight_l0[i][j] =
            slice_hdr->pred_weight_table_l0.chroma_weight[i][j];
        slice_param.chroma_offset_l0[i][j] =
            slice_hdr->pred_weight_table_l0.chroma_offset[i][j];
      }
    }

    if (slice_hdr->IsBSlice()) {
      for (int i = 0; i <= slice_param.num_ref_idx_l1_active_minus1; ++i) {
        slice_param.luma_weight_l1[i] =
            slice_hdr->pred_weight_table_l1.luma_weight[i];
        slice_param.luma_offset_l1[i] =
            slice_hdr->pred_weight_table_l1.luma_offset[i];

        for (int j = 0; j < 2; ++j) {
          slice_param.chroma_weight_l1[i][j] =
              slice_hdr->pred_weight_table_l1.chroma_weight[i][j];
          slice_param.chroma_offset_l1[i][j] =
              slice_hdr->pred_weight_table_l1.chroma_offset[i][j];
        }
      }
    }
  }

  static_assert(
      arraysize(slice_param.RefPicList0) == arraysize(slice_param.RefPicList1),
      "Invalid RefPicList sizes");

  for (size_t i = 0; i < arraysize(slice_param.RefPicList0); ++i) {
    InitVAPicture(&slice_param.RefPicList0[i]);
    InitVAPicture(&slice_param.RefPicList1[i]);
  }

  for (size_t i = 0;
       i < ref_pic_list0.size() && i < arraysize(slice_param.RefPicList0);
       ++i) {
    if (ref_pic_list0[i])
      FillVAPicture(&slice_param.RefPicList0[i], ref_pic_list0[i]);
  }
  for (size_t i = 0;
       i < ref_pic_list1.size() && i < arraysize(slice_param.RefPicList1);
       ++i) {
    if (ref_pic_list1[i])
      FillVAPicture(&slice_param.RefPicList1[i], ref_pic_list1[i]);
  }

  if (!vaapi_wrapper_->SubmitBuffer(VASliceParameterBufferType,
                                    sizeof(slice_param), &slice_param))
    return false;

  // Can't help it, blame libva...
  void* non_const_ptr = const_cast<uint8_t*>(data);
  return vaapi_wrapper_->SubmitBuffer(VASliceDataBufferType, size,
                                      non_const_ptr);
}

bool VaapiVideoDecodeAccelerator::VaapiH264Accelerator::SubmitDecode(
    const scoped_refptr<H264Picture>& pic) {
  VLOGF(4) << "Decoding POC " << pic->pic_order_cnt;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<VaapiDecodeSurface> dec_surface =
      H264PictureToVaapiDecodeSurface(pic);

  return vaapi_dec_->DecodeSurface(dec_surface);
}

bool VaapiVideoDecodeAccelerator::VaapiH264Accelerator::OutputPicture(
    const scoped_refptr<H264Picture>& pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<VaapiDecodeSurface> dec_surface =
      H264PictureToVaapiDecodeSurface(pic);
  dec_surface->set_visible_rect(pic->visible_rect);
  vaapi_dec_->SurfaceReady(dec_surface);

  return true;
}

void VaapiVideoDecodeAccelerator::VaapiH264Accelerator::Reset() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  vaapi_wrapper_->DestroyPendingBuffers();
}

scoped_refptr<VaapiVideoDecodeAccelerator::VaapiDecodeSurface>
VaapiVideoDecodeAccelerator::VaapiH264Accelerator::
    H264PictureToVaapiDecodeSurface(const scoped_refptr<H264Picture>& pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VaapiH264Picture* vaapi_pic = pic->AsVaapiH264Picture();
  CHECK(vaapi_pic);
  return vaapi_pic->dec_surface();
}

void VaapiVideoDecodeAccelerator::VaapiH264Accelerator::FillVAPicture(
    VAPictureH264* va_pic,
    scoped_refptr<H264Picture> pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VASurfaceID va_surface_id = VA_INVALID_SURFACE;

  if (!pic->nonexisting) {
    scoped_refptr<VaapiDecodeSurface> dec_surface =
        H264PictureToVaapiDecodeSurface(pic);
    va_surface_id = dec_surface->va_surface()->id();
  }

  va_pic->picture_id = va_surface_id;
  va_pic->frame_idx = pic->frame_num;
  va_pic->flags = 0;

  switch (pic->field) {
    case H264Picture::FIELD_NONE:
      break;
    case H264Picture::FIELD_TOP:
      va_pic->flags |= VA_PICTURE_H264_TOP_FIELD;
      break;
    case H264Picture::FIELD_BOTTOM:
      va_pic->flags |= VA_PICTURE_H264_BOTTOM_FIELD;
      break;
  }

  if (pic->ref) {
    va_pic->flags |= pic->long_term ? VA_PICTURE_H264_LONG_TERM_REFERENCE
                                    : VA_PICTURE_H264_SHORT_TERM_REFERENCE;
  }

  va_pic->TopFieldOrderCnt = pic->top_field_order_cnt;
  va_pic->BottomFieldOrderCnt = pic->bottom_field_order_cnt;
}

int VaapiVideoDecodeAccelerator::VaapiH264Accelerator::FillVARefFramesFromDPB(
    const H264DPB& dpb,
    VAPictureH264* va_pics,
    int num_pics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  H264Picture::Vector::const_reverse_iterator rit;
  int i;

  // Return reference frames in reverse order of insertion.
  // Libva does not document this, but other implementations (e.g. mplayer)
  // do it this way as well.
  for (rit = dpb.rbegin(), i = 0; rit != dpb.rend() && i < num_pics; ++rit) {
    if ((*rit)->ref)
      FillVAPicture(&va_pics[i++], *rit);
  }

  return i;
}

VaapiVideoDecodeAccelerator::VaapiVP8Accelerator::VaapiVP8Accelerator(
    VaapiVideoDecodeAccelerator* vaapi_dec,
    VaapiWrapper* vaapi_wrapper)
    : vaapi_wrapper_(vaapi_wrapper), vaapi_dec_(vaapi_dec) {
  DCHECK(vaapi_wrapper_);
  DCHECK(vaapi_dec_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VaapiVideoDecodeAccelerator::VaapiVP8Accelerator::~VaapiVP8Accelerator() {
  // TODO(mcasas): consider enabling the checker, https://crbug.com/789160
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<VP8Picture>
VaapiVideoDecodeAccelerator::VaapiVP8Accelerator::CreateVP8Picture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<VaapiDecodeSurface> va_surface = vaapi_dec_->CreateSurface();
  if (!va_surface)
    return nullptr;

  return new VaapiVP8Picture(std::move(va_surface));
}

#define ARRAY_MEMCPY_CHECKED(to, from)                               \
  do {                                                               \
    static_assert(sizeof(to) == sizeof(from),                        \
                  #from " and " #to " arrays must be of same size"); \
    memcpy(to, from, sizeof(to));                                    \
  } while (0)

bool VaapiVideoDecodeAccelerator::VaapiVP8Accelerator::SubmitDecode(
    const scoped_refptr<VP8Picture>& pic,
    const Vp8FrameHeader* frame_hdr,
    const scoped_refptr<VP8Picture>& last_frame,
    const scoped_refptr<VP8Picture>& golden_frame,
    const scoped_refptr<VP8Picture>& alt_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VAIQMatrixBufferVP8 iq_matrix_buf;
  memset(&iq_matrix_buf, 0, sizeof(VAIQMatrixBufferVP8));

  const Vp8SegmentationHeader& sgmnt_hdr = frame_hdr->segmentation_hdr;
  const Vp8QuantizationHeader& quant_hdr = frame_hdr->quantization_hdr;
  static_assert(arraysize(iq_matrix_buf.quantization_index) == kMaxMBSegments,
                "incorrect quantization matrix size");
  for (size_t i = 0; i < kMaxMBSegments; ++i) {
    int q = quant_hdr.y_ac_qi;

    if (sgmnt_hdr.segmentation_enabled) {
      if (sgmnt_hdr.segment_feature_mode ==
          Vp8SegmentationHeader::FEATURE_MODE_ABSOLUTE)
        q = sgmnt_hdr.quantizer_update_value[i];
      else
        q += sgmnt_hdr.quantizer_update_value[i];
    }

#define CLAMP_Q(q) std::min(std::max(q, 0), 127)
    static_assert(arraysize(iq_matrix_buf.quantization_index[i]) == 6,
                  "incorrect quantization matrix size");
    iq_matrix_buf.quantization_index[i][0] = CLAMP_Q(q);
    iq_matrix_buf.quantization_index[i][1] = CLAMP_Q(q + quant_hdr.y_dc_delta);
    iq_matrix_buf.quantization_index[i][2] = CLAMP_Q(q + quant_hdr.y2_dc_delta);
    iq_matrix_buf.quantization_index[i][3] = CLAMP_Q(q + quant_hdr.y2_ac_delta);
    iq_matrix_buf.quantization_index[i][4] = CLAMP_Q(q + quant_hdr.uv_dc_delta);
    iq_matrix_buf.quantization_index[i][5] = CLAMP_Q(q + quant_hdr.uv_ac_delta);
#undef CLAMP_Q
  }

  if (!vaapi_wrapper_->SubmitBuffer(
          VAIQMatrixBufferType, sizeof(VAIQMatrixBufferVP8), &iq_matrix_buf))
    return false;

  VAProbabilityDataBufferVP8 prob_buf;
  memset(&prob_buf, 0, sizeof(VAProbabilityDataBufferVP8));

  const Vp8EntropyHeader& entr_hdr = frame_hdr->entropy_hdr;
  ARRAY_MEMCPY_CHECKED(prob_buf.dct_coeff_probs, entr_hdr.coeff_probs);

  if (!vaapi_wrapper_->SubmitBuffer(VAProbabilityBufferType,
                                    sizeof(VAProbabilityDataBufferVP8),
                                    &prob_buf))
    return false;

  VAPictureParameterBufferVP8 pic_param;
  memset(&pic_param, 0, sizeof(VAPictureParameterBufferVP8));
  pic_param.frame_width = frame_hdr->width;
  pic_param.frame_height = frame_hdr->height;

  if (last_frame) {
    scoped_refptr<VaapiDecodeSurface> last_frame_surface =
        VP8PictureToVaapiDecodeSurface(last_frame);
    pic_param.last_ref_frame = last_frame_surface->va_surface()->id();
  } else {
    pic_param.last_ref_frame = VA_INVALID_SURFACE;
  }

  if (golden_frame) {
    scoped_refptr<VaapiDecodeSurface> golden_frame_surface =
        VP8PictureToVaapiDecodeSurface(golden_frame);
    pic_param.golden_ref_frame = golden_frame_surface->va_surface()->id();
  } else {
    pic_param.golden_ref_frame = VA_INVALID_SURFACE;
  }

  if (alt_frame) {
    scoped_refptr<VaapiDecodeSurface> alt_frame_surface =
        VP8PictureToVaapiDecodeSurface(alt_frame);
    pic_param.alt_ref_frame = alt_frame_surface->va_surface()->id();
  } else {
    pic_param.alt_ref_frame = VA_INVALID_SURFACE;
  }

  pic_param.out_of_loop_frame = VA_INVALID_SURFACE;

  const Vp8LoopFilterHeader& lf_hdr = frame_hdr->loopfilter_hdr;

#define FHDR_TO_PP_PF(a, b) pic_param.pic_fields.bits.a = (b)
  FHDR_TO_PP_PF(key_frame, frame_hdr->IsKeyframe() ? 0 : 1);
  FHDR_TO_PP_PF(version, frame_hdr->version);
  FHDR_TO_PP_PF(segmentation_enabled, sgmnt_hdr.segmentation_enabled);
  FHDR_TO_PP_PF(update_mb_segmentation_map,
                sgmnt_hdr.update_mb_segmentation_map);
  FHDR_TO_PP_PF(update_segment_feature_data,
                sgmnt_hdr.update_segment_feature_data);
  FHDR_TO_PP_PF(filter_type, lf_hdr.type);
  FHDR_TO_PP_PF(sharpness_level, lf_hdr.sharpness_level);
  FHDR_TO_PP_PF(loop_filter_adj_enable, lf_hdr.loop_filter_adj_enable);
  FHDR_TO_PP_PF(mode_ref_lf_delta_update, lf_hdr.mode_ref_lf_delta_update);
  FHDR_TO_PP_PF(sign_bias_golden, frame_hdr->sign_bias_golden);
  FHDR_TO_PP_PF(sign_bias_alternate, frame_hdr->sign_bias_alternate);
  FHDR_TO_PP_PF(mb_no_coeff_skip, frame_hdr->mb_no_skip_coeff);
  FHDR_TO_PP_PF(loop_filter_disable, lf_hdr.level == 0);
#undef FHDR_TO_PP_PF

  ARRAY_MEMCPY_CHECKED(pic_param.mb_segment_tree_probs, sgmnt_hdr.segment_prob);

  static_assert(arraysize(sgmnt_hdr.lf_update_value) ==
                    arraysize(pic_param.loop_filter_level),
                "loop filter level arrays mismatch");
  for (size_t i = 0; i < arraysize(sgmnt_hdr.lf_update_value); ++i) {
    int lf_level = lf_hdr.level;
    if (sgmnt_hdr.segmentation_enabled) {
      if (sgmnt_hdr.segment_feature_mode ==
          Vp8SegmentationHeader::FEATURE_MODE_ABSOLUTE)
        lf_level = sgmnt_hdr.lf_update_value[i];
      else
        lf_level += sgmnt_hdr.lf_update_value[i];
    }

    // Clamp to [0..63] range.
    lf_level = std::min(std::max(lf_level, 0), 63);
    pic_param.loop_filter_level[i] = lf_level;
  }

  static_assert(
      arraysize(lf_hdr.ref_frame_delta) ==
              arraysize(pic_param.loop_filter_deltas_ref_frame) &&
          arraysize(lf_hdr.mb_mode_delta) ==
              arraysize(pic_param.loop_filter_deltas_mode) &&
          arraysize(lf_hdr.ref_frame_delta) == arraysize(lf_hdr.mb_mode_delta),
      "loop filter deltas arrays size mismatch");
  for (size_t i = 0; i < arraysize(lf_hdr.ref_frame_delta); ++i) {
    pic_param.loop_filter_deltas_ref_frame[i] = lf_hdr.ref_frame_delta[i];
    pic_param.loop_filter_deltas_mode[i] = lf_hdr.mb_mode_delta[i];
  }

#define FHDR_TO_PP(a) pic_param.a = frame_hdr->a
  FHDR_TO_PP(prob_skip_false);
  FHDR_TO_PP(prob_intra);
  FHDR_TO_PP(prob_last);
  FHDR_TO_PP(prob_gf);
#undef FHDR_TO_PP

  ARRAY_MEMCPY_CHECKED(pic_param.y_mode_probs, entr_hdr.y_mode_probs);
  ARRAY_MEMCPY_CHECKED(pic_param.uv_mode_probs, entr_hdr.uv_mode_probs);
  ARRAY_MEMCPY_CHECKED(pic_param.mv_probs, entr_hdr.mv_probs);

  pic_param.bool_coder_ctx.range = frame_hdr->bool_dec_range;
  pic_param.bool_coder_ctx.value = frame_hdr->bool_dec_value;
  pic_param.bool_coder_ctx.count = frame_hdr->bool_dec_count;

  if (!vaapi_wrapper_->SubmitBuffer(VAPictureParameterBufferType,
                                    sizeof(pic_param), &pic_param))
    return false;

  VASliceParameterBufferVP8 slice_param;
  memset(&slice_param, 0, sizeof(slice_param));
  slice_param.slice_data_size = frame_hdr->frame_size;
  slice_param.slice_data_offset = frame_hdr->first_part_offset;
  slice_param.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
  slice_param.macroblock_offset = frame_hdr->macroblock_bit_offset;
  // Number of DCT partitions plus control partition.
  slice_param.num_of_partitions = frame_hdr->num_of_dct_partitions + 1;

  // Per VAAPI, this size only includes the size of the macroblock data in
  // the first partition (in bytes), so we have to subtract the header size.
  slice_param.partition_size[0] =
      frame_hdr->first_part_size - ((frame_hdr->macroblock_bit_offset + 7) / 8);

  for (size_t i = 0; i < frame_hdr->num_of_dct_partitions; ++i)
    slice_param.partition_size[i + 1] = frame_hdr->dct_partition_sizes[i];

  if (!vaapi_wrapper_->SubmitBuffer(VASliceParameterBufferType,
                                    sizeof(VASliceParameterBufferVP8),
                                    &slice_param))
    return false;

  void* non_const_ptr = const_cast<uint8_t*>(frame_hdr->data);
  if (!vaapi_wrapper_->SubmitBuffer(VASliceDataBufferType,
                                    frame_hdr->frame_size, non_const_ptr))
    return false;

  scoped_refptr<VaapiDecodeSurface> dec_surface =
      VP8PictureToVaapiDecodeSurface(pic);

  return vaapi_dec_->DecodeSurface(dec_surface);
}

bool VaapiVideoDecodeAccelerator::VaapiVP8Accelerator::OutputPicture(
    const scoped_refptr<VP8Picture>& pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<VaapiDecodeSurface> dec_surface =
      VP8PictureToVaapiDecodeSurface(pic);
  dec_surface->set_visible_rect(pic->visible_rect);
  vaapi_dec_->SurfaceReady(dec_surface);
  return true;
}

scoped_refptr<VaapiVideoDecodeAccelerator::VaapiDecodeSurface>
VaapiVideoDecodeAccelerator::VaapiVP8Accelerator::
    VP8PictureToVaapiDecodeSurface(const scoped_refptr<VP8Picture>& pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VaapiVP8Picture* vaapi_pic = pic->AsVaapiVP8Picture();
  CHECK(vaapi_pic);
  return vaapi_pic->dec_surface();
}

VaapiVideoDecodeAccelerator::VaapiVP9Accelerator::VaapiVP9Accelerator(
    VaapiVideoDecodeAccelerator* vaapi_dec,
    VaapiWrapper* vaapi_wrapper)
    : vaapi_wrapper_(vaapi_wrapper), vaapi_dec_(vaapi_dec) {
  DCHECK(vaapi_wrapper_);
  DCHECK(vaapi_dec_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VaapiVideoDecodeAccelerator::VaapiVP9Accelerator::~VaapiVP9Accelerator() {
  // TODO(mcasas): consider enabling the checker, https://crbug.com/789160
  // CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<VP9Picture>
VaapiVideoDecodeAccelerator::VaapiVP9Accelerator::CreateVP9Picture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<VaapiDecodeSurface> va_surface = vaapi_dec_->CreateSurface();
  if (!va_surface)
    return nullptr;

  return new VaapiVP9Picture(std::move(va_surface));
}

bool VaapiVideoDecodeAccelerator::VaapiVP9Accelerator::SubmitDecode(
    const scoped_refptr<VP9Picture>& pic,
    const Vp9SegmentationParams& seg,
    const Vp9LoopFilterParams& lf,
    const std::vector<scoped_refptr<VP9Picture>>& ref_pictures,
    const base::Closure& done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |done_cb| should be null as we return false from IsFrameContextRequired().
  DCHECK(done_cb.is_null());

  VADecPictureParameterBufferVP9 pic_param;
  memset(&pic_param, 0, sizeof(pic_param));

  const Vp9FrameHeader* frame_hdr = pic->frame_hdr.get();
  DCHECK(frame_hdr);

  pic_param.frame_width = base::checked_cast<uint16_t>(frame_hdr->frame_width);
  pic_param.frame_height =
      base::checked_cast<uint16_t>(frame_hdr->frame_height);

  CHECK_EQ(ref_pictures.size(), arraysize(pic_param.reference_frames));
  for (size_t i = 0; i < arraysize(pic_param.reference_frames); ++i) {
    VASurfaceID va_surface_id;
    if (ref_pictures[i]) {
      scoped_refptr<VaapiDecodeSurface> surface =
          VP9PictureToVaapiDecodeSurface(ref_pictures[i]);
      va_surface_id = surface->va_surface()->id();
    } else {
      va_surface_id = VA_INVALID_SURFACE;
    }

    pic_param.reference_frames[i] = va_surface_id;
  }

#define FHDR_TO_PP_PF1(a) pic_param.pic_fields.bits.a = frame_hdr->a
#define FHDR_TO_PP_PF2(a, b) pic_param.pic_fields.bits.a = b
  FHDR_TO_PP_PF2(subsampling_x, frame_hdr->subsampling_x == 1);
  FHDR_TO_PP_PF2(subsampling_y, frame_hdr->subsampling_y == 1);
  FHDR_TO_PP_PF2(frame_type, frame_hdr->IsKeyframe() ? 0 : 1);
  FHDR_TO_PP_PF1(show_frame);
  FHDR_TO_PP_PF1(error_resilient_mode);
  FHDR_TO_PP_PF1(intra_only);
  FHDR_TO_PP_PF1(allow_high_precision_mv);
  FHDR_TO_PP_PF2(mcomp_filter_type, frame_hdr->interpolation_filter);
  FHDR_TO_PP_PF1(frame_parallel_decoding_mode);
  FHDR_TO_PP_PF1(reset_frame_context);
  FHDR_TO_PP_PF1(refresh_frame_context);
  FHDR_TO_PP_PF2(frame_context_idx, frame_hdr->frame_context_idx_to_save_probs);
  FHDR_TO_PP_PF2(segmentation_enabled, seg.enabled);
  FHDR_TO_PP_PF2(segmentation_temporal_update, seg.temporal_update);
  FHDR_TO_PP_PF2(segmentation_update_map, seg.update_map);
  FHDR_TO_PP_PF2(last_ref_frame, frame_hdr->ref_frame_idx[0]);
  FHDR_TO_PP_PF2(last_ref_frame_sign_bias,
                 frame_hdr->ref_frame_sign_bias[Vp9RefType::VP9_FRAME_LAST]);
  FHDR_TO_PP_PF2(golden_ref_frame, frame_hdr->ref_frame_idx[1]);
  FHDR_TO_PP_PF2(golden_ref_frame_sign_bias,
                 frame_hdr->ref_frame_sign_bias[Vp9RefType::VP9_FRAME_GOLDEN]);
  FHDR_TO_PP_PF2(alt_ref_frame, frame_hdr->ref_frame_idx[2]);
  FHDR_TO_PP_PF2(alt_ref_frame_sign_bias,
                 frame_hdr->ref_frame_sign_bias[Vp9RefType::VP9_FRAME_ALTREF]);
  FHDR_TO_PP_PF2(lossless_flag, frame_hdr->quant_params.IsLossless());
#undef FHDR_TO_PP_PF2
#undef FHDR_TO_PP_PF1

  pic_param.filter_level = lf.level;
  pic_param.sharpness_level = lf.sharpness;
  pic_param.log2_tile_rows = frame_hdr->tile_rows_log2;
  pic_param.log2_tile_columns = frame_hdr->tile_cols_log2;
  pic_param.frame_header_length_in_bytes = frame_hdr->uncompressed_header_size;
  pic_param.first_partition_size = frame_hdr->header_size_in_bytes;

  ARRAY_MEMCPY_CHECKED(pic_param.mb_segment_tree_probs, seg.tree_probs);
  ARRAY_MEMCPY_CHECKED(pic_param.segment_pred_probs, seg.pred_probs);

  pic_param.profile = frame_hdr->profile;
  pic_param.bit_depth = frame_hdr->bit_depth;
  DCHECK((pic_param.profile == 0 && pic_param.bit_depth == 8) ||
         (pic_param.profile == 2 && pic_param.bit_depth == 10));

  if (!vaapi_wrapper_->SubmitBuffer(VAPictureParameterBufferType,
                                    sizeof(pic_param), &pic_param))
    return false;

  VASliceParameterBufferVP9 slice_param;
  memset(&slice_param, 0, sizeof(slice_param));
  slice_param.slice_data_size = frame_hdr->frame_size;
  slice_param.slice_data_offset = 0;
  slice_param.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;

  static_assert(arraysize(Vp9SegmentationParams::feature_enabled) ==
                    arraysize(slice_param.seg_param),
                "seg_param array of incorrect size");
  for (size_t i = 0; i < arraysize(slice_param.seg_param); ++i) {
    VASegmentParameterVP9& seg_param = slice_param.seg_param[i];
#define SEG_TO_SP_SF(a, b) seg_param.segment_flags.fields.a = b
    SEG_TO_SP_SF(
        segment_reference_enabled,
        seg.FeatureEnabled(i, Vp9SegmentationParams::SEG_LVL_REF_FRAME));
    SEG_TO_SP_SF(segment_reference,
                 seg.FeatureData(i, Vp9SegmentationParams::SEG_LVL_REF_FRAME));
    SEG_TO_SP_SF(segment_reference_skipped,
                 seg.FeatureEnabled(i, Vp9SegmentationParams::SEG_LVL_SKIP));
#undef SEG_TO_SP_SF

    ARRAY_MEMCPY_CHECKED(seg_param.filter_level, lf.lvl[i]);

    seg_param.luma_dc_quant_scale = seg.y_dequant[i][0];
    seg_param.luma_ac_quant_scale = seg.y_dequant[i][1];
    seg_param.chroma_dc_quant_scale = seg.uv_dequant[i][0];
    seg_param.chroma_ac_quant_scale = seg.uv_dequant[i][1];
  }

  if (!vaapi_wrapper_->SubmitBuffer(VASliceParameterBufferType,
                                    sizeof(slice_param), &slice_param))
    return false;

  void* non_const_ptr = const_cast<uint8_t*>(frame_hdr->data);
  if (!vaapi_wrapper_->SubmitBuffer(VASliceDataBufferType,
                                    frame_hdr->frame_size, non_const_ptr))
    return false;

  scoped_refptr<VaapiDecodeSurface> dec_surface =
      VP9PictureToVaapiDecodeSurface(pic);

  return vaapi_dec_->DecodeSurface(dec_surface);
}

bool VaapiVideoDecodeAccelerator::VaapiVP9Accelerator::OutputPicture(
    const scoped_refptr<VP9Picture>& pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<VaapiDecodeSurface> dec_surface =
      VP9PictureToVaapiDecodeSurface(pic);
  dec_surface->set_visible_rect(pic->visible_rect);
  vaapi_dec_->SurfaceReady(dec_surface);
  return true;
}

bool VaapiVideoDecodeAccelerator::VaapiVP9Accelerator::GetFrameContext(
    const scoped_refptr<VP9Picture>& pic,
    Vp9FrameContext* frame_ctx) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED() << "Frame context update not supported";
  return false;
}

scoped_refptr<VaapiVideoDecodeAccelerator::VaapiDecodeSurface>
VaapiVideoDecodeAccelerator::VaapiVP9Accelerator::
    VP9PictureToVaapiDecodeSurface(const scoped_refptr<VP9Picture>& pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VaapiVP9Picture* vaapi_pic = pic->AsVaapiVP9Picture();
  CHECK(vaapi_pic);
  return vaapi_pic->dec_surface();
}

// static
VideoDecodeAccelerator::SupportedProfiles
VaapiVideoDecodeAccelerator::GetSupportedProfiles() {
  return VaapiWrapper::GetSupportedDecodeProfiles();
}

}  // namespace media
