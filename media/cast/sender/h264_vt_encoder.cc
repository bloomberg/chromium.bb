// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/h264_vt_encoder.h"

#include <string>
#include <vector>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "media/base/mac/corevideo_glue.h"
#include "media/base/mac/video_frame_mac.h"
#include "media/cast/sender/video_frame_factory.h"

namespace media {
namespace cast {

namespace {

// Container for the associated data of a video frame being processed.
struct InProgressFrameEncode {
  const RtpTimestamp rtp_timestamp;
  const base::TimeTicks reference_time;
  const VideoEncoder::FrameEncodedCallback frame_encoded_callback;

  InProgressFrameEncode(RtpTimestamp rtp,
                        base::TimeTicks r_time,
                        VideoEncoder::FrameEncodedCallback callback)
      : rtp_timestamp(rtp),
        reference_time(r_time),
        frame_encoded_callback(callback) {}
};

base::ScopedCFTypeRef<CFDictionaryRef> DictionaryWithKeysAndValues(
    CFTypeRef* keys,
    CFTypeRef* values,
    size_t size) {
  return base::ScopedCFTypeRef<CFDictionaryRef>(CFDictionaryCreate(
      kCFAllocatorDefault,
      keys,
      values,
      size,
      &kCFTypeDictionaryKeyCallBacks,
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

// Implementation of the VideoFrameFactory interface using |CVPixelBufferPool|.
class VideoFrameFactoryCVPixelBufferPoolImpl : public VideoFrameFactory {
 public:
  VideoFrameFactoryCVPixelBufferPoolImpl(
      const base::ScopedCFTypeRef<CVPixelBufferPoolRef>& pool,
      const gfx::Size& frame_size)
      : pool_(pool),
        frame_size_(frame_size) {}

  ~VideoFrameFactoryCVPixelBufferPoolImpl() override {}

  scoped_refptr<VideoFrame> MaybeCreateFrame(
      const gfx::Size& frame_size, base::TimeDelta timestamp) override {
    if (frame_size != frame_size_)
      return nullptr;  // Buffer pool is not a match for requested frame size.

    base::ScopedCFTypeRef<CVPixelBufferRef> buffer;
    if (CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pool_,
                                           buffer.InitializeInto()) !=
            kCVReturnSuccess)
      return nullptr;  // Buffer pool has run out of pixel buffers.
    DCHECK(buffer);

    return VideoFrame::WrapCVPixelBuffer(buffer, timestamp);
  }

 private:
  const base::ScopedCFTypeRef<CVPixelBufferPoolRef> pool_;
  const gfx::Size frame_size_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameFactoryCVPixelBufferPoolImpl);
};

}  // namespace

// static
bool H264VideoToolboxEncoder::IsSupported(
    const VideoSenderConfig& video_config) {
  return video_config.codec == CODEC_VIDEO_H264 && VideoToolboxGlue::Get();
}

H264VideoToolboxEncoder::H264VideoToolboxEncoder(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const VideoSenderConfig& video_config,
    const gfx::Size& frame_size,
    uint32 first_frame_id,
    const StatusChangeCallback& status_change_cb)
    : cast_environment_(cast_environment),
      videotoolbox_glue_(VideoToolboxGlue::Get()),
      frame_size_(frame_size),
      status_change_cb_(status_change_cb),
      next_frame_id_(first_frame_id),
      encode_next_frame_as_keyframe_(false) {
  DCHECK(!frame_size_.IsEmpty());
  DCHECK(!status_change_cb_.is_null());

  OperationalStatus operational_status;
  if (video_config.codec == CODEC_VIDEO_H264 && videotoolbox_glue_) {
    operational_status = Initialize(video_config) ?
        STATUS_INITIALIZED : STATUS_INVALID_CONFIGURATION;
  } else {
    operational_status = STATUS_UNSUPPORTED_CODEC;
  }
  cast_environment_->PostTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(status_change_cb_, operational_status));
}

H264VideoToolboxEncoder::~H264VideoToolboxEncoder() {
  Teardown();
}

bool H264VideoToolboxEncoder::Initialize(
    const VideoSenderConfig& video_config) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!compression_session_);

  // Note that the encoder object is given to the compression session as the
  // callback context using a raw pointer. The C API does not allow us to use
  // a smart pointer, nor is this encoder ref counted. However, this is still
  // safe, because we 1) we own the compression session and 2) we tear it down
  // safely. When destructing the encoder, the compression session is flushed
  // and invalidated. Internally, VideoToolbox will join all of its threads
  // before returning to the client. Therefore, when control returns to us, we
  // are guaranteed that the output callback will not execute again.

  // On OS X, allow the hardware encoder. Don't require it, it does not support
  // all configurations (some of which are used for testing).
  base::ScopedCFTypeRef<CFDictionaryRef> encoder_spec;
#if !defined(OS_IOS)
  encoder_spec = DictionaryWithKeyValue(
      videotoolbox_glue_
          ->kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder(),
      kCFBooleanTrue);
#endif

  // Certain encoders prefer kCVPixelFormatType_422YpCbCr8, which is not
  // supported through VideoFrame. We can force 420 formats to be used instead.
  const int formats[] = {
    kCVPixelFormatType_420YpCbCr8Planar,
    CoreVideoGlue::kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
  };
  // Keep these attachment settings in-sync with those in ConfigureSession().
  CFTypeRef attachments_keys[] = {
    kCVImageBufferColorPrimariesKey,
    kCVImageBufferTransferFunctionKey,
    kCVImageBufferYCbCrMatrixKey
  };
  CFTypeRef attachments_values[] = {
    kCVImageBufferColorPrimaries_ITU_R_709_2,
    kCVImageBufferTransferFunction_ITU_R_709_2,
    kCVImageBufferYCbCrMatrix_ITU_R_709_2
  };
  CFTypeRef buffer_attributes_keys[] = {
    kCVPixelBufferPixelFormatTypeKey,
    kCVBufferPropagatedAttachmentsKey
  };
  CFTypeRef buffer_attributes_values[] = {
    ArrayWithIntegers(formats, arraysize(formats)).release(),
    DictionaryWithKeysAndValues(attachments_keys,
                                attachments_values,
                                arraysize(attachments_keys)).release()
  };
  const base::ScopedCFTypeRef<CFDictionaryRef> buffer_attributes =
      DictionaryWithKeysAndValues(buffer_attributes_keys,
                                  buffer_attributes_values,
                                  arraysize(buffer_attributes_keys));
  for (auto& v : buffer_attributes_values)
    CFRelease(v);

  VTCompressionSessionRef session;
  OSStatus status = videotoolbox_glue_->VTCompressionSessionCreate(
      kCFAllocatorDefault, frame_size_.width(), frame_size_.height(),
      CoreMediaGlue::kCMVideoCodecType_H264, encoder_spec, buffer_attributes,
      nullptr /* compressedDataAllocator */,
      &H264VideoToolboxEncoder::CompressionCallback,
      reinterpret_cast<void*>(this), &session);
  if (status != noErr) {
    DLOG(ERROR) << " VTCompressionSessionCreate failed: " << status;
    return false;
  }
  compression_session_.reset(session);

  ConfigureSession(video_config);

  return true;
}

void H264VideoToolboxEncoder::ConfigureSession(
    const VideoSenderConfig& video_config) {
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
      (video_config.min_bitrate + video_config.max_bitrate) / 2);
  SetSessionProperty(
      videotoolbox_glue_->kVTCompressionPropertyKey_ExpectedFrameRate(),
      video_config.max_frame_rate);
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
  if (video_config.max_number_of_video_buffers_used > 0) {
    SetSessionProperty(
        videotoolbox_glue_->kVTCompressionPropertyKey_MaxFrameDelayCount(),
        video_config.max_number_of_video_buffers_used);
  }
}

void H264VideoToolboxEncoder::Teardown() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If the compression session exists, invalidate it. This blocks until all
  // pending output callbacks have returned and any internal threads have
  // joined, ensuring no output callback ever sees a dangling encoder pointer.
  if (compression_session_) {
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

  if (!compression_session_) {
    DLOG(ERROR) << " compression session is null";
    return false;
  }

  if (video_frame->visible_rect().size() != frame_size_)
    return false;

  // Wrap the VideoFrame in a CVPixelBuffer. In all cases, no data will be
  // copied. If the VideoFrame was created by this encoder's video frame
  // factory, then the returned CVPixelBuffer will have been obtained from the
  // compression session's pixel buffer pool. This will eliminate a copy of the
  // frame into memory visible by the hardware encoder. The VideoFrame's
  // lifetime is extended for the lifetime of the returned CVPixelBuffer.
  auto pixel_buffer = media::WrapVideoFrameInCVPixelBuffer(*video_frame);
  if (!pixel_buffer) {
    return false;
  }

  auto timestamp_cm = CoreMediaGlue::CMTimeMake(
      (reference_time - base::TimeTicks()).InMicroseconds(), USEC_PER_SEC);

  scoped_ptr<InProgressFrameEncode> request(new InProgressFrameEncode(
      TimeDeltaToRtpDelta(video_frame->timestamp(), kVideoFrequency),
      reference_time, frame_encoded_callback));

  base::ScopedCFTypeRef<CFDictionaryRef> frame_props;
  if (encode_next_frame_as_keyframe_) {
    frame_props = DictionaryWithKeyValue(
        videotoolbox_glue_->kVTEncodeFrameOptionKey_ForceKeyFrame(),
        kCFBooleanTrue);
    encode_next_frame_as_keyframe_ = false;
  }

  VTEncodeInfoFlags info;
  OSStatus status = videotoolbox_glue_->VTCompressionSessionEncodeFrame(
      compression_session_, pixel_buffer, timestamp_cm,
      CoreMediaGlue::CMTime{0, 0, 0, 0}, frame_props,
      reinterpret_cast<void*>(request.release()), &info);
  if (status != noErr) {
    DLOG(ERROR) << " VTCompressionSessionEncodeFrame failed: " << status;
    return false;
  }
  if ((info & VideoToolboxGlue::kVTEncodeInfo_FrameDropped)) {
    DLOG(ERROR) << " frame dropped";
    return false;
  }

  return true;
}

void H264VideoToolboxEncoder::SetBitRate(int new_bit_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // VideoToolbox does not seem to support bitrate reconfiguration.
}

void H264VideoToolboxEncoder::GenerateKeyFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(compression_session_);

  encode_next_frame_as_keyframe_ = true;
}

void H264VideoToolboxEncoder::LatestFrameIdToReference(uint32 /*frame_id*/) {
  // Not supported by VideoToolbox in any meaningful manner.
}

scoped_ptr<VideoFrameFactory>
H264VideoToolboxEncoder::CreateVideoFrameFactory() {
  if (!videotoolbox_glue_ || !compression_session_)
    return nullptr;
  base::ScopedCFTypeRef<CVPixelBufferPoolRef> pool(
      videotoolbox_glue_->VTCompressionSessionGetPixelBufferPool(
          compression_session_),
      base::scoped_policy::RETAIN);
  return scoped_ptr<VideoFrameFactory>(
      new VideoFrameFactoryCVPixelBufferPoolImpl(pool, frame_size_));
}

void H264VideoToolboxEncoder::EmitFrames() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!compression_session_) {
    DLOG(ERROR) << " compression session is null";
    return;
  }

  OSStatus status = videotoolbox_glue_->VTCompressionSessionCompleteFrames(
      compression_session_, CoreMediaGlue::CMTime{0, 0, 0, 0});
  if (status != noErr) {
    DLOG(ERROR) << " VTCompressionSessionCompleteFrames failed: " << status;
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
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(encoder->status_change_cb_, STATUS_CODEC_RUNTIME_ERROR));
  } else if ((info & VideoToolboxGlue::kVTEncodeInfo_FrameDropped)) {
    DVLOG(2) << " frame dropped";
  } else {
    auto sample_attachments = static_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(
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
  const uint32 frame_id = encoder->next_frame_id_++;

  scoped_ptr<EncodedFrame> encoded_frame(new EncodedFrame());
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

  encoder->cast_environment_->PostTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(request->frame_encoded_callback,
                 base::Passed(&encoded_frame)));
}

// A ref-counted structure that is shared to provide concurrent access to the
// VideoFrameFactory instance for the current encoder.  OnEncoderReplaced() can
// change |factory| whenever an encoder instance has been replaced, while users
// of CreateVideoFrameFactory() may attempt to read/use |factory| by any thread
// at any time.
struct SizeAdaptableH264VideoToolboxVideoEncoder::FactoryHolder
    : public base::RefCountedThreadSafe<FactoryHolder> {
  base::Lock lock;
  scoped_ptr<VideoFrameFactory> factory;

 private:
  friend class base::RefCountedThreadSafe<FactoryHolder>;
  ~FactoryHolder() {}
};

SizeAdaptableH264VideoToolboxVideoEncoder::
    SizeAdaptableH264VideoToolboxVideoEncoder(
        const scoped_refptr<CastEnvironment>& cast_environment,
        const VideoSenderConfig& video_config,
        const StatusChangeCallback& status_change_cb)
        : SizeAdaptableVideoEncoderBase(cast_environment,
                                        video_config,
                                        status_change_cb),
          holder_(new FactoryHolder()) {}

SizeAdaptableH264VideoToolboxVideoEncoder::
    ~SizeAdaptableH264VideoToolboxVideoEncoder() {}

// A proxy allowing SizeAdaptableH264VideoToolboxVideoEncoder to swap out the
// VideoFrameFactory instance to match one appropriate for the current encoder
// instance.
class SizeAdaptableH264VideoToolboxVideoEncoder::VideoFrameFactoryProxy
    : public VideoFrameFactory {
 public:
  explicit VideoFrameFactoryProxy(const scoped_refptr<FactoryHolder>& holder)
      : holder_(holder) {}

  ~VideoFrameFactoryProxy() override {}

  scoped_refptr<VideoFrame> MaybeCreateFrame(
      const gfx::Size& frame_size, base::TimeDelta timestamp) override {
    base::AutoLock auto_lock(holder_->lock);
    return holder_->factory ?
        holder_->factory->MaybeCreateFrame(frame_size, timestamp) : nullptr;
  }

 private:
  const scoped_refptr<FactoryHolder> holder_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameFactoryProxy);
};

scoped_ptr<VideoFrameFactory>
    SizeAdaptableH264VideoToolboxVideoEncoder::CreateVideoFrameFactory() {
  return scoped_ptr<VideoFrameFactory>(new VideoFrameFactoryProxy(holder_));
}

scoped_ptr<VideoEncoder>
    SizeAdaptableH264VideoToolboxVideoEncoder::CreateEncoder() {
  return scoped_ptr<VideoEncoder>(new H264VideoToolboxEncoder(
      cast_environment(),
      video_config(),
      frame_size(),
      last_frame_id() + 1,
      CreateEncoderStatusChangeCallback()));
}

void SizeAdaptableH264VideoToolboxVideoEncoder::OnEncoderReplaced(
    VideoEncoder* replacement_encoder) {
  scoped_ptr<VideoFrameFactory> current_factory(
      replacement_encoder->CreateVideoFrameFactory());
  base::AutoLock auto_lock(holder_->lock);
  holder_->factory = current_factory.Pass();
  SizeAdaptableVideoEncoderBase::OnEncoderReplaced(replacement_encoder);
}

void SizeAdaptableH264VideoToolboxVideoEncoder::DestroyEncoder() {
  {
    base::AutoLock auto_lock(holder_->lock);
    holder_->factory.reset();
  }
  SizeAdaptableVideoEncoderBase::DestroyEncoder();
}

}  // namespace cast
}  // namespace media
