// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/h264_vt_encoder.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "media/base/mac/corevideo_glue.h"
#include "media/base/mac/video_frame_mac.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/constants.h"
#include "media/cast/sender/video_frame_factory.h"

namespace media {
namespace cast {

namespace {

// Container for the associated data of a video frame being processed.
struct InProgressFrameEncode {
  const RtpTimeTicks rtp_timestamp;
  const base::TimeTicks reference_time;
  const VideoEncoder::FrameEncodedCallback frame_encoded_callback;

  InProgressFrameEncode(RtpTimeTicks rtp,
                        base::TimeTicks r_time,
                        VideoEncoder::FrameEncodedCallback callback)
      : rtp_timestamp(rtp),
        reference_time(r_time),
        frame_encoded_callback(callback) {}
};

base::ScopedCFTypeRef<CFDictionaryRef>
DictionaryWithKeysAndValues(CFTypeRef* keys, CFTypeRef* values, size_t size) {
  return base::ScopedCFTypeRef<CFDictionaryRef>(CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, size, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
}

base::ScopedCFTypeRef<CFDictionaryRef> DictionaryWithKeyValue(CFTypeRef key,
                                                              CFTypeRef value) {
  CFTypeRef keys[1] = {key};
  CFTypeRef values[1] = {value};
  return DictionaryWithKeysAndValues(keys, values, 1);
}

base::ScopedCFTypeRef<CFArrayRef> ArrayWithIntegers(const int* v, size_t size) {
  std::vector<CFNumberRef> numbers;
  numbers.reserve(size);
  for (const int* end = v + size; v < end; ++v)
    numbers.push_back(CFNumberCreate(nullptr, kCFNumberSInt32Type, v));
  base::ScopedCFTypeRef<CFArrayRef> array(CFArrayCreate(
      kCFAllocatorDefault, reinterpret_cast<const void**>(&numbers[0]),
      numbers.size(), &kCFTypeArrayCallBacks));
  for (auto& number : numbers) {
    CFRelease(number);
  }
  return array;
}

template <typename NalSizeType>
void CopyNalsToAnnexB(char* avcc_buffer,
                      const size_t avcc_size,
                      std::string* annexb_buffer) {
  static_assert(sizeof(NalSizeType) == 1 || sizeof(NalSizeType) == 2 ||
                    sizeof(NalSizeType) == 4,
                "NAL size type has unsupported size");
  static const char startcode_3[3] = {0, 0, 1};
  DCHECK(avcc_buffer);
  DCHECK(annexb_buffer);
  size_t bytes_left = avcc_size;
  while (bytes_left > 0) {
    DCHECK_GT(bytes_left, sizeof(NalSizeType));
    NalSizeType nal_size;
    base::ReadBigEndian(avcc_buffer, &nal_size);
    bytes_left -= sizeof(NalSizeType);
    avcc_buffer += sizeof(NalSizeType);

    DCHECK_GE(bytes_left, nal_size);
    annexb_buffer->append(startcode_3, sizeof(startcode_3));
    annexb_buffer->append(avcc_buffer, nal_size);
    bytes_left -= nal_size;
    avcc_buffer += nal_size;
  }
}

// Copy a H.264 frame stored in a CM sample buffer to an Annex B buffer. Copies
// parameter sets for keyframes before the frame data as well.
void CopySampleBufferToAnnexBBuffer(CoreMediaGlue::CMSampleBufferRef sbuf,
                                    std::string* annexb_buffer,
                                    bool keyframe) {
  // Perform two pass, one to figure out the total output size, and another to
  // copy the data after having performed a single output allocation. Note that
  // we'll allocate a bit more because we'll count 4 bytes instead of 3 for
  // video NALs.

  OSStatus status;

  // Get the sample buffer's block buffer and format description.
  auto bb = CoreMediaGlue::CMSampleBufferGetDataBuffer(sbuf);
  DCHECK(bb);
  auto fdesc = CoreMediaGlue::CMSampleBufferGetFormatDescription(sbuf);
  DCHECK(fdesc);

  size_t bb_size = CoreMediaGlue::CMBlockBufferGetDataLength(bb);
  size_t total_bytes = bb_size;

  size_t pset_count;
  int nal_size_field_bytes;
  status = CoreMediaGlue::CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
      fdesc, 0, nullptr, nullptr, &pset_count, &nal_size_field_bytes);
  if (status ==
      CoreMediaGlue::kCMFormatDescriptionBridgeError_InvalidParameter) {
    DLOG(WARNING) << " assuming 2 parameter sets and 4 bytes NAL length header";
    pset_count = 2;
    nal_size_field_bytes = 4;
  } else if (status != noErr) {
    DLOG(ERROR)
        << " CMVideoFormatDescriptionGetH264ParameterSetAtIndex failed: "
        << status;
    return;
  }

  if (keyframe) {
    const uint8_t* pset;
    size_t pset_size;
    for (size_t pset_i = 0; pset_i < pset_count; ++pset_i) {
      status =
          CoreMediaGlue::CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
              fdesc, pset_i, &pset, &pset_size, nullptr, nullptr);
      if (status != noErr) {
        DLOG(ERROR)
            << " CMVideoFormatDescriptionGetH264ParameterSetAtIndex failed: "
            << status;
        return;
      }
      total_bytes += pset_size + nal_size_field_bytes;
    }
  }

  annexb_buffer->reserve(total_bytes);

  // Copy all parameter sets before keyframes.
  if (keyframe) {
    const uint8_t* pset;
    size_t pset_size;
    for (size_t pset_i = 0; pset_i < pset_count; ++pset_i) {
      status =
          CoreMediaGlue::CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
              fdesc, pset_i, &pset, &pset_size, nullptr, nullptr);
      if (status != noErr) {
        DLOG(ERROR)
            << " CMVideoFormatDescriptionGetH264ParameterSetAtIndex failed: "
            << status;
        return;
      }
      static const char startcode_4[4] = {0, 0, 0, 1};
      annexb_buffer->append(startcode_4, sizeof(startcode_4));
      annexb_buffer->append(reinterpret_cast<const char*>(pset), pset_size);
    }
  }

  // Block buffers can be composed of non-contiguous chunks. For the sake of
  // keeping this code simple, flatten non-contiguous block buffers.
  base::ScopedCFTypeRef<CoreMediaGlue::CMBlockBufferRef> contiguous_bb(
      bb, base::scoped_policy::RETAIN);
  if (!CoreMediaGlue::CMBlockBufferIsRangeContiguous(bb, 0, 0)) {
    contiguous_bb.reset();
    status = CoreMediaGlue::CMBlockBufferCreateContiguous(
        kCFAllocatorDefault, bb, kCFAllocatorDefault, nullptr, 0, 0, 0,
        contiguous_bb.InitializeInto());
    if (status != noErr) {
      DLOG(ERROR) << " CMBlockBufferCreateContiguous failed: " << status;
      return;
    }
  }

  // Copy all the NAL units. In the process convert them from AVCC format
  // (length header) to AnnexB format (start code).
  char* bb_data;
  status = CoreMediaGlue::CMBlockBufferGetDataPointer(contiguous_bb, 0, nullptr,
                                                      nullptr, &bb_data);
  if (status != noErr) {
    DLOG(ERROR) << " CMBlockBufferGetDataPointer failed: " << status;
    return;
  }

  if (nal_size_field_bytes == 1) {
    CopyNalsToAnnexB<uint8_t>(bb_data, bb_size, annexb_buffer);
  } else if (nal_size_field_bytes == 2) {
    CopyNalsToAnnexB<uint16_t>(bb_data, bb_size, annexb_buffer);
  } else if (nal_size_field_bytes == 4) {
    CopyNalsToAnnexB<uint32_t>(bb_data, bb_size, annexb_buffer);
  } else {
    NOTREACHED();
  }
}

}  // namespace

class H264VideoToolboxEncoder::VideoFrameFactoryImpl
    : public base::RefCountedThreadSafe<VideoFrameFactoryImpl>,
      public VideoFrameFactory {
 public:
  // Type that proxies the VideoFrameFactory interface to this class.
  class Proxy;

  VideoFrameFactoryImpl(const base::WeakPtr<H264VideoToolboxEncoder>& encoder,
                        const scoped_refptr<CastEnvironment>& cast_environment)
      : encoder_(encoder), cast_environment_(cast_environment) {}

  scoped_refptr<VideoFrame> MaybeCreateFrame(
      const gfx::Size& frame_size,
      base::TimeDelta timestamp) final {
    if (frame_size.IsEmpty()) {
      DVLOG(1) << "Rejecting empty video frame.";
      return nullptr;
    }

    base::AutoLock auto_lock(lock_);

    // If the pool size does not match, speculatively reset the encoder to use
    // the new size and return null. Cache the new frame size right away and
    // toss away the pixel buffer pool to avoid spurious tasks until the encoder
    // is done resetting.
    if (frame_size != pool_frame_size_) {
      DVLOG(1) << "MaybeCreateFrame: Detected frame size change.";
      cast_environment_->PostTask(
          CastEnvironment::MAIN, FROM_HERE,
          base::Bind(&H264VideoToolboxEncoder::UpdateFrameSize, encoder_,
                     frame_size));
      pool_frame_size_ = frame_size;
      pool_.reset();
      return nullptr;
    }

    if (!pool_) {
      DVLOG(1) << "MaybeCreateFrame: No pixel buffer pool.";
      return nullptr;
    }

    // Allocate a pixel buffer from the pool and return a wrapper VideoFrame.
    base::ScopedCFTypeRef<CVPixelBufferRef> buffer;
    auto status = CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pool_,
                                                     buffer.InitializeInto());
    if (status != kCVReturnSuccess) {
      DLOG(ERROR) << "CVPixelBufferPoolCreatePixelBuffer failed: " << status;
      return nullptr;
    }

    DCHECK(buffer);
    return VideoFrame::WrapCVPixelBuffer(buffer, timestamp);
  }

  void Update(const base::ScopedCFTypeRef<CVPixelBufferPoolRef>& pool,
              const gfx::Size& frame_size) {
    base::AutoLock auto_lock(lock_);
    pool_ = pool;
    pool_frame_size_ = frame_size;
  }

 private:
  friend class base::RefCountedThreadSafe<VideoFrameFactoryImpl>;
  ~VideoFrameFactoryImpl() final {}

  base::Lock lock_;
  base::ScopedCFTypeRef<CVPixelBufferPoolRef> pool_;
  gfx::Size pool_frame_size_;

  // Weak back reference to the encoder and the cast envrionment so we can
  // message the encoder when the frame size changes.
  const base::WeakPtr<H264VideoToolboxEncoder> encoder_;
  const scoped_refptr<CastEnvironment> cast_environment_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameFactoryImpl);
};

class H264VideoToolboxEncoder::VideoFrameFactoryImpl::Proxy
    : public VideoFrameFactory {
 public:
  explicit Proxy(
      const scoped_refptr<VideoFrameFactoryImpl>& video_frame_factory)
      : video_frame_factory_(video_frame_factory) {
    DCHECK(video_frame_factory_);
  }

  scoped_refptr<VideoFrame> MaybeCreateFrame(
      const gfx::Size& frame_size,
      base::TimeDelta timestamp) final {
    return video_frame_factory_->MaybeCreateFrame(frame_size, timestamp);
  }

 private:
  ~Proxy() final {}

  const scoped_refptr<VideoFrameFactoryImpl> video_frame_factory_;

  DISALLOW_COPY_AND_ASSIGN(Proxy);
};

// static
bool H264VideoToolboxEncoder::IsSupported(
    const VideoSenderConfig& video_config) {
  return video_config.codec == CODEC_VIDEO_H264 && VideoToolboxGlue::Get();
}

H264VideoToolboxEncoder::H264VideoToolboxEncoder(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const VideoSenderConfig& video_config,
    const StatusChangeCallback& status_change_cb)
    : cast_environment_(cast_environment),
      videotoolbox_glue_(VideoToolboxGlue::Get()),
      video_config_(video_config),
      status_change_cb_(status_change_cb),
      last_frame_id_(kFirstFrameId - 1),
      encode_next_frame_as_keyframe_(false),
      power_suspended_(false),
      weak_factory_(this) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!status_change_cb_.is_null());

  OperationalStatus operational_status =
      H264VideoToolboxEncoder::IsSupported(video_config)
          ? STATUS_INITIALIZED
          : STATUS_UNSUPPORTED_CODEC;
  cast_environment_->PostTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(status_change_cb_, operational_status));

  if (operational_status == STATUS_INITIALIZED) {
    // Create the shared video frame factory. It persists for the combined
    // lifetime of the encoder and all video frame factory proxies created by
    // |CreateVideoFrameFactory| that reference it.
    video_frame_factory_ =
        scoped_refptr<VideoFrameFactoryImpl>(new VideoFrameFactoryImpl(
            weak_factory_.GetWeakPtr(), cast_environment_));

    // Register for power state changes.
    auto power_monitor = base::PowerMonitor::Get();
    if (power_monitor) {
      power_monitor->AddObserver(this);
      VLOG(1) << "Registered for power state changes.";
    } else {
      DLOG(WARNING) << "No power monitor. Process suspension will invalidate "
                       "the encoder.";
    }
  }
}

H264VideoToolboxEncoder::~H264VideoToolboxEncoder() {
  DestroyCompressionSession();

  // If video_frame_factory_ is not null, the encoder registered for power state
  // changes in the ctor and it must now unregister.
  if (video_frame_factory_) {
    auto power_monitor = base::PowerMonitor::Get();
    if (power_monitor)
      power_monitor->RemoveObserver(this);
  }
}

void H264VideoToolboxEncoder::ResetCompressionSession() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Ignore reset requests while power suspended.
  if (power_suspended_)
    return;

  // Notify that we're resetting the encoder.
  cast_environment_->PostTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(status_change_cb_, STATUS_CODEC_REINIT_PENDING));

  // Destroy the current session, if any.
  DestroyCompressionSession();

  // On OS X, allow the hardware encoder. Don't require it, it does not support
  // all configurations (some of which are used for testing).
  base::ScopedCFTypeRef<CFDictionaryRef> encoder_spec;
#if !defined(OS_IOS)
  encoder_spec = DictionaryWithKeyValue(
      videotoolbox_glue_
          ->kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder(),
      kCFBooleanTrue);
#endif

  // Force 420v so that clients can easily use these buffers as GPU textures.
  const int format[] = {
      CoreVideoGlue::kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange};

  // Keep these attachment settings in-sync with those in ConfigureSession().
  CFTypeRef attachments_keys[] = {kCVImageBufferColorPrimariesKey,
                                  kCVImageBufferTransferFunctionKey,
                                  kCVImageBufferYCbCrMatrixKey};
  CFTypeRef attachments_values[] = {kCVImageBufferColorPrimaries_ITU_R_709_2,
                                    kCVImageBufferTransferFunction_ITU_R_709_2,
                                    kCVImageBufferYCbCrMatrix_ITU_R_709_2};
  CFTypeRef buffer_attributes_keys[] = {kCVPixelBufferPixelFormatTypeKey,
                                        kCVBufferPropagatedAttachmentsKey};
  CFTypeRef buffer_attributes_values[] = {
      ArrayWithIntegers(format, arraysize(format)).release(),
      DictionaryWithKeysAndValues(attachments_keys, attachments_values,
                                  arraysize(attachments_keys)).release()};
  const base::ScopedCFTypeRef<CFDictionaryRef> buffer_attributes =
      DictionaryWithKeysAndValues(buffer_attributes_keys,
                                  buffer_attributes_values,
                                  arraysize(buffer_attributes_keys));
  for (auto& v : buffer_attributes_values)
    CFRelease(v);

  // Create the compression session.

  // Note that the encoder object is given to the compression session as the
  // callback context using a raw pointer. The C API does not allow us to use a
  // smart pointer, nor is this encoder ref counted. However, this is still
  // safe, because we 1) we own the compression session and 2) we tear it down
  // safely. When destructing the encoder, the compression session is flushed
  // and invalidated. Internally, VideoToolbox will join all of its threads
  // before returning to the client. Therefore, when control returns to us, we
  // are guaranteed that the output callback will not execute again.
  OSStatus status = videotoolbox_glue_->VTCompressionSessionCreate(
      kCFAllocatorDefault, frame_size_.width(), frame_size_.height(),
      CoreMediaGlue::kCMVideoCodecType_H264, encoder_spec, buffer_attributes,
      nullptr /* compressedDataAllocator */,
      &H264VideoToolboxEncoder::CompressionCallback,
      reinterpret_cast<void*>(this), compression_session_.InitializeInto());
  if (status != noErr) {
    DLOG(ERROR) << " VTCompressionSessionCreate failed: " << status;
    // Notify that reinitialization has failed.
    cast_environment_->PostTask(
        CastEnvironment::MAIN, FROM_HERE,
        base::Bind(status_change_cb_, STATUS_CODEC_INIT_FAILED));
    return;
  }

  // Configure the session (apply session properties based on the current state
  // of the encoder, experimental tuning and requirements).
  ConfigureCompressionSession();

  // Update the video frame factory.
  base::ScopedCFTypeRef<CVPixelBufferPoolRef> pool(
      videotoolbox_glue_->VTCompressionSessionGetPixelBufferPool(
          compression_session_),
      base::scoped_policy::RETAIN);
  video_frame_factory_->Update(pool, frame_size_);

  // Notify that reinitialization is done.
  cast_environment_->PostTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(status_change_cb_, STATUS_INITIALIZED));
}

void H264VideoToolboxEncoder::ConfigureCompressionSession() {
  SetSessionProperty(
      videotoolbox_glue_->kVTCompressionPropertyKey_ProfileLevel(),
      videotoolbox_glue_->kVTProfileLevel_H264_Main_AutoLevel());
  SetSessionProperty(videotoolbox_glue_->kVTCompressionPropertyKey_RealTime(),
                     true);
  SetSessionProperty(
      videotoolbox_glue_->kVTCompressionPropertyKey_AllowFrameReordering(),
      false);
  SetSessionProperty(
      videotoolbox_glue_->kVTCompressionPropertyKey_MaxKeyFrameInterval(), 240);
  SetSessionProperty(
      videotoolbox_glue_
          ->kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration(),
      240);
  // TODO(jfroy): implement better bitrate control
  //              https://crbug.com/425352
  SetSessionProperty(
      videotoolbox_glue_->kVTCompressionPropertyKey_AverageBitRate(),
      (video_config_.min_bitrate + video_config_.max_bitrate) / 2);
  SetSessionProperty(
      videotoolbox_glue_->kVTCompressionPropertyKey_ExpectedFrameRate(),
      video_config_.max_frame_rate);
  // Keep these attachment settings in-sync with those in Initialize().
  SetSessionProperty(
      videotoolbox_glue_->kVTCompressionPropertyKey_ColorPrimaries(),
      kCVImageBufferColorPrimaries_ITU_R_709_2);
  SetSessionProperty(
      videotoolbox_glue_->kVTCompressionPropertyKey_TransferFunction(),
      kCVImageBufferTransferFunction_ITU_R_709_2);
  SetSessionProperty(
      videotoolbox_glue_->kVTCompressionPropertyKey_YCbCrMatrix(),
      kCVImageBufferYCbCrMatrix_ITU_R_709_2);
  if (video_config_.max_number_of_video_buffers_used > 0) {
    SetSessionProperty(
        videotoolbox_glue_->kVTCompressionPropertyKey_MaxFrameDelayCount(),
        video_config_.max_number_of_video_buffers_used);
  }
}

void H264VideoToolboxEncoder::DestroyCompressionSession() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If the compression session exists, invalidate it. This blocks until all
  // pending output callbacks have returned and any internal threads have
  // joined, ensuring no output callback ever sees a dangling encoder pointer.
  //
  // Before destroying the compression session, the video frame factory's pool
  // is updated to null so that no thread will produce new video frames via the
  // factory until a new compression session is created. The current frame size
  // is passed to prevent the video frame factory from posting |UpdateFrameSize|
  // tasks. Indeed, |DestroyCompressionSession| is either called from
  // |ResetCompressionSession|, in which case a new pool and frame size will be
  // set, or from callsites that require that there be no compression session
  // (ex: the dtor).
  if (compression_session_) {
    video_frame_factory_->Update(
        base::ScopedCFTypeRef<CVPixelBufferPoolRef>(nullptr), frame_size_);
    videotoolbox_glue_->VTCompressionSessionInvalidate(compression_session_);
    compression_session_.reset();
  }
}

bool H264VideoToolboxEncoder::EncodeVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& reference_time,
    const FrameEncodedCallback& frame_encoded_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!frame_encoded_callback.is_null());

  // Reject empty video frames.
  const gfx::Size frame_size = video_frame->visible_rect().size();
  if (frame_size.IsEmpty()) {
    DVLOG(1) << "Rejecting empty video frame.";
    return false;
  }

  // Handle frame size changes. This will reset the compression session.
  if (frame_size != frame_size_) {
    DVLOG(1) << "EncodeVideoFrame: Detected frame size change.";
    UpdateFrameSize(frame_size);
  }

  // Need a compression session to continue.
  if (!compression_session_) {
    DLOG(ERROR) << "No compression session.";
    return false;
  }

  // Wrap the VideoFrame in a CVPixelBuffer. In all cases, no data will be
  // copied. If the VideoFrame was created by this encoder's video frame
  // factory, then the returned CVPixelBuffer will have been obtained from the
  // compression session's pixel buffer pool. This will eliminate a copy of the
  // frame into memory visible by the hardware encoder. The VideoFrame's
  // lifetime is extended for the lifetime of the returned CVPixelBuffer.
  auto pixel_buffer = media::WrapVideoFrameInCVPixelBuffer(*video_frame);
  if (!pixel_buffer) {
    DLOG(ERROR) << "WrapVideoFrameInCVPixelBuffer failed.";
    return false;
  }

  // Convert the frame timestamp to CMTime.
  auto timestamp_cm = CoreMediaGlue::CMTimeMake(
      (reference_time - base::TimeTicks()).InMicroseconds(), USEC_PER_SEC);

  // Wrap information we'll need after the frame is encoded in a heap object.
  // We'll get the pointer back from the VideoToolbox completion callback.
  scoped_ptr<InProgressFrameEncode> request(new InProgressFrameEncode(
      RtpTimeTicks::FromTimeDelta(video_frame->timestamp(), kVideoFrequency),
      reference_time, frame_encoded_callback));

  // Build a suitable frame properties dictionary for keyframes.
  base::ScopedCFTypeRef<CFDictionaryRef> frame_props;
  if (encode_next_frame_as_keyframe_) {
    frame_props = DictionaryWithKeyValue(
        videotoolbox_glue_->kVTEncodeFrameOptionKey_ForceKeyFrame(),
        kCFBooleanTrue);
    encode_next_frame_as_keyframe_ = false;
  }

  // Submit the frame to the compression session. The function returns as soon
  // as the frame has been enqueued.
  OSStatus status = videotoolbox_glue_->VTCompressionSessionEncodeFrame(
      compression_session_, pixel_buffer, timestamp_cm,
      CoreMediaGlue::CMTime{0, 0, 0, 0}, frame_props,
      reinterpret_cast<void*>(request.release()), nullptr);
  if (status != noErr) {
    DLOG(ERROR) << " VTCompressionSessionEncodeFrame failed: " << status;
    return false;
  }

  return true;
}

void H264VideoToolboxEncoder::UpdateFrameSize(const gfx::Size& size_needed) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Our video frame factory posts a task to update the frame size when its
  // cache of the frame size differs from what the client requested. To avoid
  // spurious encoder resets, check again here.
  if (size_needed == frame_size_) {
    DCHECK(compression_session_);
    return;
  }

  VLOG(1) << "Resetting compression session (for frame size change from "
          << frame_size_.ToString() << " to " << size_needed.ToString() << ").";

  // If there is an existing session, finish every pending frame.
  if (compression_session_) {
    EmitFrames();
  }

  // Store the new frame size.
  frame_size_ = size_needed;

  // Reset the compression session.
  ResetCompressionSession();
}

void H264VideoToolboxEncoder::SetBitRate(int /*new_bit_rate*/) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // VideoToolbox does not seem to support bitrate reconfiguration.
}

void H264VideoToolboxEncoder::GenerateKeyFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  encode_next_frame_as_keyframe_ = true;
}

scoped_ptr<VideoFrameFactory>
H264VideoToolboxEncoder::CreateVideoFrameFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return scoped_ptr<VideoFrameFactory>(
      new VideoFrameFactoryImpl::Proxy(video_frame_factory_));
}

void H264VideoToolboxEncoder::EmitFrames() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!compression_session_)
    return;

  OSStatus status = videotoolbox_glue_->VTCompressionSessionCompleteFrames(
      compression_session_, CoreMediaGlue::CMTime{0, 0, 0, 0});
  if (status != noErr) {
    DLOG(ERROR) << " VTCompressionSessionCompleteFrames failed: " << status;
  }
}

void H264VideoToolboxEncoder::OnSuspend() {
  VLOG(1)
      << "OnSuspend: Emitting all frames and destroying compression session.";
  EmitFrames();
  DestroyCompressionSession();
  power_suspended_ = true;
}

void H264VideoToolboxEncoder::OnResume() {
  power_suspended_ = false;

  // Reset the compression session only if the frame size is not zero (which
  // will obviously fail). It is possible for the frame size to be zero if no
  // frame was submitted for encoding or requested from the video frame factory
  // before suspension.
  if (!frame_size_.IsEmpty()) {
    VLOG(1) << "OnResume: Resetting compression session.";
    ResetCompressionSession();
  }
}

bool H264VideoToolboxEncoder::SetSessionProperty(CFStringRef key,
                                                 int32_t value) {
  base::ScopedCFTypeRef<CFNumberRef> cfvalue(
      CFNumberCreate(nullptr, kCFNumberSInt32Type, &value));
  return videotoolbox_glue_->VTSessionSetProperty(compression_session_, key,
                                                  cfvalue) == noErr;
}

bool H264VideoToolboxEncoder::SetSessionProperty(CFStringRef key, bool value) {
  CFBooleanRef cfvalue = (value) ? kCFBooleanTrue : kCFBooleanFalse;
  return videotoolbox_glue_->VTSessionSetProperty(compression_session_, key,
                                                  cfvalue) == noErr;
}

bool H264VideoToolboxEncoder::SetSessionProperty(CFStringRef key,
                                                 CFStringRef value) {
  return videotoolbox_glue_->VTSessionSetProperty(compression_session_, key,
                                                  value) == noErr;
}

void H264VideoToolboxEncoder::CompressionCallback(void* encoder_opaque,
                                                  void* request_opaque,
                                                  OSStatus status,
                                                  VTEncodeInfoFlags info,
                                                  CMSampleBufferRef sbuf) {
  auto encoder = reinterpret_cast<H264VideoToolboxEncoder*>(encoder_opaque);
  const scoped_ptr<InProgressFrameEncode> request(
      reinterpret_cast<InProgressFrameEncode*>(request_opaque));
  bool keyframe = false;
  bool has_frame_data = false;

  if (status != noErr) {
    DLOG(ERROR) << " encoding failed: " << status;
    encoder->cast_environment_->PostTask(
        CastEnvironment::MAIN, FROM_HERE,
        base::Bind(encoder->status_change_cb_, STATUS_CODEC_RUNTIME_ERROR));
  } else if ((info & VideoToolboxGlue::kVTEncodeInfo_FrameDropped)) {
    DVLOG(2) << " frame dropped";
  } else {
    auto sample_attachments =
        static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(
            CoreMediaGlue::CMSampleBufferGetSampleAttachmentsArray(sbuf, true),
            0));

    // If the NotSync key is not present, it implies Sync, which indicates a
    // keyframe (at least I think, VT documentation is, erm, sparse). Could
    // alternatively use kCMSampleAttachmentKey_DependsOnOthers == false.
    keyframe = !CFDictionaryContainsKey(
                   sample_attachments,
                   CoreMediaGlue::kCMSampleAttachmentKey_NotSync());
    has_frame_data = true;
  }

  // Increment the encoder-scoped frame id and assign the new value to this
  // frame. VideoToolbox calls the output callback serially, so this is safe.
  const uint32_t frame_id = ++encoder->last_frame_id_;

  scoped_ptr<SenderEncodedFrame> encoded_frame(new SenderEncodedFrame());
  encoded_frame->frame_id = frame_id;
  encoded_frame->reference_time = request->reference_time;
  encoded_frame->rtp_timestamp = request->rtp_timestamp;
  if (keyframe) {
    encoded_frame->dependency = EncodedFrame::KEY;
    encoded_frame->referenced_frame_id = frame_id;
  } else {
    encoded_frame->dependency = EncodedFrame::DEPENDENT;
    // H.264 supports complex frame reference schemes (multiple reference
    // frames, slice references, backward and forward references, etc). Cast
    // doesn't support the concept of forward-referencing frame dependencies or
    // multiple frame dependencies; so pretend that all frames are only
    // decodable after their immediately preceding frame is decoded. This will
    // ensure a Cast receiver only attempts to decode the frames sequentially
    // and in order. Furthermore, the encoder is configured to never use forward
    // references (see |kVTCompressionPropertyKey_AllowFrameReordering|). There
    // is no way to prevent multiple reference frames.
    encoded_frame->referenced_frame_id = frame_id - 1;
  }

  if (has_frame_data)
    CopySampleBufferToAnnexBBuffer(sbuf, &encoded_frame->data, keyframe);

  // TODO(miu): Compute and populate the |deadline_utilization| and
  // |lossy_utilization| performance metrics in |encoded_frame|.

  encoded_frame->encode_completion_time =
      encoder->cast_environment_->Clock()->NowTicks();
  encoder->cast_environment_->PostTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(request->frame_encoded_callback,
                 base::Passed(&encoded_frame)));
}

}  // namespace cast
}  // namespace media
