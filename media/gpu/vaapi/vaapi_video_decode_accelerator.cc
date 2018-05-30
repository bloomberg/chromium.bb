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
#include "media/base/unaligned_shared_memory.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/format_utils.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_h264_accelerator.h"
#include "media/gpu/vaapi/vaapi_picture.h"
#include "media/gpu/vaapi/vaapi_vp8_accelerator.h"
#include "media/gpu/vaapi/vaapi_vp9_accelerator.h"
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

// Returns the preferred VA_RT_FORMAT for the given |profile|.
unsigned int GetVaFormatForVideoCodecProfile(VideoCodecProfile profile) {
  if (profile == VP9PROFILE_PROFILE2 || profile == VP9PROFILE_PROFILE3)
    return VA_RT_FORMAT_YUV420_10BPP;
  return VA_RT_FORMAT_YUV420;
}

void ReportToUMA(VAVDADecoderFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAVDA.DecoderFailure", failure,
                            VAVDA_DECODER_FAILURES_MAX + 1);
}

#if defined(USE_OZONE)
void CloseGpuMemoryBufferHandle(const gfx::GpuMemoryBufferHandle& handle) {
  for (const auto& fd : handle.native_pixmap_handle.fds) {
    // Close the fd by wrapping it in a ScopedFD and letting
    // it fall out of scope.
    base::ScopedFD scoped_fd(fd.fd);
  }
}
#endif

}  // namespace

#define RETURN_AND_NOTIFY_ON_FAILURE(result, log, error_code, ret) \
  do {                                                             \
    if (!(result)) {                                               \
      VLOGF(1) << log;                                             \
      NotifyError(error_code);                                     \
      return ret;                                                  \
    }                                                              \
  } while (0)

class VaapiVideoDecodeAccelerator::InputBuffer {
 public:
  InputBuffer() = default;
  InputBuffer(uint32_t id,
              std::unique_ptr<UnalignedSharedMemory> shm,
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
  UnalignedSharedMemory* shm() const { return shm_.get(); }

 private:
  const int32_t id_ = -1;
  const std::unique_ptr<UnalignedSharedMemory> shm_;
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

  UMA_HISTOGRAM_BOOLEAN("Media.VAVDA.VaapiWrapperCreationSuccess",
                        vaapi_wrapper_.get());
  if (!vaapi_wrapper_.get()) {
    VLOGF(1) << "Failed initializing VAAPI for profile "
             << GetProfileName(profile);
    return false;
  }

  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    decoder_.reset(new H264Decoder(
        std::make_unique<VaapiH264Accelerator>(this, vaapi_wrapper_)));
  } else if (profile >= VP8PROFILE_MIN && profile <= VP8PROFILE_MAX) {
    decoder_.reset(new VP8Decoder(
        std::make_unique<VaapiVP8Accelerator>(this, vaapi_wrapper_)));
  } else if (profile >= VP9PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    decoder_.reset(new VP9Decoder(
        std::make_unique<VaapiVP9Accelerator>(this, vaapi_wrapper_)));
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
  TRACE_COUNTER1("media,gpu", "Vaapi frames at client", num_frames_at_client_);
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
    auto shm = std::make_unique<UnalignedSharedMemory>(
        bitstream_buffer.handle(), bitstream_buffer.size(), true);
    RETURN_AND_NOTIFY_ON_FAILURE(
        shm->MapAt(bitstream_buffer.offset(), bitstream_buffer.size()),
        "Failed to map input buffer", UNREADABLE_INPUT, );

    auto input_buffer = std::make_unique<InputBuffer>(
        bitstream_buffer.id(), std::move(shm),
        BindToCurrentLoop(
            base::Bind(&Client::NotifyEndOfBitstreamBuffer, client_)));
    input_buffers_.push(std::move(input_buffer));
  }

  TRACE_COUNTER1("media,gpu", "Vaapi input buffers", input_buffers_.size());
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

bool VaapiVideoDecodeAccelerator::GetCurrInputBuffer_Locked() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (curr_input_buffer_.get())
    return true;

  // Will only wait if it is expected that in current state new buffers will
  // be queued from the client via Decode(). The state can change during wait.
  while (input_buffers_.empty() && (state_ == kDecoding || state_ == kIdle))
    input_ready_.Wait();

  // We could have got woken up in a different state or never got to sleep
  // due to current state.
  if (state_ != kDecoding && state_ != kIdle)
    return false;

  DCHECK(!input_buffers_.empty());
  curr_input_buffer_ = std::move(input_buffers_.front());
  input_buffers_.pop();
  TRACE_COUNTER1("media,gpu", "Input buffers", input_buffers_.size());

  if (curr_input_buffer_->IsFlushRequest()) {
    VLOGF(4) << "New flush buffer";
    return true;
  }

  VLOGF(4) << "New |curr_input_buffer_|, id: " << curr_input_buffer_->id()
           << " size: " << curr_input_buffer_->shm()->size() << "B";
  decoder_->SetStream(
      curr_input_buffer_->id(),
      static_cast<uint8_t*>(curr_input_buffer_->shm()->memory()),
      curr_input_buffer_->shm()->size());

  return true;
}

void VaapiVideoDecodeAccelerator::ReturnCurrInputBuffer_Locked() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();
  DCHECK(curr_input_buffer_.get());
  curr_input_buffer_.reset();
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
  VLOGF(4) << "Decode task";

  // Try to decode what stream data is (still) in the decoder until we run out
  // of it.
  while (GetCurrInputBuffer_Locked()) {
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

      case AcceleratedVideoDecoder::kNoKey:
        NOTREACHED() << "Should not reach here unless this class accepts "
                        "encrypted streams.";
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
  TRACE_COUNTER1("media,gpu", "Vaapi frames at client", num_frames_at_client_);

  output_buffers_.push(picture_buffer_id);
  TryOutputSurface();
}

void VaapiVideoDecodeAccelerator::FlushTask() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(curr_input_buffer_ && curr_input_buffer_->IsFlushRequest());

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
  if (curr_input_buffer_)
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
  TRACE_COUNTER1("media,gpu", "Vaapi input buffers", input_buffers_.size());

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

  // TODO(mcasas): consider deleting |decoder_| on
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

bool VaapiVideoDecodeAccelerator::DecodeVASurface(
    const scoped_refptr<VASurface>& va_surface) {
  const bool result =
      vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(va_surface->id());
  if (!result)
    VLOGF(1) << "Failed decoding picture";
  return result;
}

void VaapiVideoDecodeAccelerator::VASurfaceReady(
    const scoped_refptr<VASurface>& va_surface,
    int32_t bitstream_id,
    const gfx::Rect& visible_rect) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VaapiVideoDecodeAccelerator::VASurfaceReady, weak_this_,
                   va_surface, bitstream_id, visible_rect));
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
                 va_surface, bitstream_id, visible_rect));

  TryOutputSurface();
}

scoped_refptr<VASurface> VaapiVideoDecodeAccelerator::CreateVASurface() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  if (available_va_surfaces_.empty())
    return nullptr;

  DCHECK(!awaiting_va_surfaces_recycle_);
  scoped_refptr<VASurface> va_surface(new VASurface(
      available_va_surfaces_.front(), requested_pic_size_,
      vaapi_wrapper_->va_surface_format(), va_surface_release_cb_));
  available_va_surfaces_.pop_front();

  return va_surface;
}

// static
VideoDecodeAccelerator::SupportedProfiles
VaapiVideoDecodeAccelerator::GetSupportedProfiles() {
  return VaapiWrapper::GetSupportedDecodeProfiles();
}

}  // namespace media
