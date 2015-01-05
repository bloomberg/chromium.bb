// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/h264_vt_encoder.h"

#include <string>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
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

base::ScopedCFTypeRef<CFDictionaryRef> DictionaryWithKeyValue(CFTypeRef key,
                                                              CFTypeRef value) {
  CFTypeRef keys[1] = {key};
  CFTypeRef values[1] = {value};
  return base::ScopedCFTypeRef<CFDictionaryRef>(CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
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
      const base::ScopedCFTypeRef<CVPixelBufferPoolRef>& pool)
      : pool_(pool) {}

  ~VideoFrameFactoryCVPixelBufferPoolImpl() override {}

  scoped_refptr<VideoFrame> CreateFrame(base::TimeDelta timestamp) override {
    base::ScopedCFTypeRef<CVPixelBufferRef> buffer;
    CHECK_EQ(kCVReturnSuccess,
             CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pool_,
                                                buffer.InitializeInto()));
    return VideoFrame::WrapCVPixelBuffer(buffer, timestamp);
  }

 private:
  base::ScopedCFTypeRef<CVPixelBufferPoolRef> pool_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameFactoryCVPixelBufferPoolImpl);
};

}  // namespace

H264VideoToolboxEncoder::H264VideoToolboxEncoder(
    scoped_refptr<CastEnvironment> cast_environment,
    const VideoSenderConfig& video_config,
    const CastInitializationCallback& initialization_cb)
    : cast_environment_(cast_environment),
      videotoolbox_glue_(VideoToolboxGlue::Get()),
      frame_id_(kStartFrameId),
      encode_next_frame_as_keyframe_(false) {
  DCHECK(!initialization_cb.is_null());
  CastInitializationStatus initialization_status;
  if (videotoolbox_glue_) {
    initialization_status = (Initialize(video_config))
                                ? STATUS_VIDEO_INITIALIZED
                                : STATUS_INVALID_VIDEO_CONFIGURATION;
  } else {
    LOG(ERROR) << " VideoToolbox is not available";
    initialization_status = STATUS_HW_VIDEO_ENCODER_NOT_SUPPORTED;
  }
  cast_environment_->PostTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(initialization_cb, initialization_status));
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

  VTCompressionSessionRef session;
  OSStatus status = videotoolbox_glue_->VTCompressionSessionCreate(
      kCFAllocatorDefault, video_config.width, video_config.height,
      CoreMediaGlue::kCMVideoCodecType_H264, encoder_spec,
      nullptr /* sourceImageBufferAttributes */,
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
  SetSessionProperty(
      videotoolbox_glue_->kVTCompressionPropertyKey_ColorPrimaries(),
      kCVImageBufferColorPrimaries_ITU_R_709_2);
  SetSessionProperty(
      videotoolbox_glue_->kVTCompressionPropertyKey_TransferFunction(),
      kCVImageBufferTransferFunction_ITU_R_709_2);
  SetSessionProperty(
      videotoolbox_glue_->kVTCompressionPropertyKey_YCbCrMatrix(),
      kCVImageBufferYCbCrMatrix_ITU_R_709_2);
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
  DCHECK(!reference_time.is_null());

  if (!compression_session_) {
    DLOG(ERROR) << " compression session is null";
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
  base::ScopedCFTypeRef<CVPixelBufferPoolRef> pool(
      videotoolbox_glue_->VTCompressionSessionGetPixelBufferPool(
          compression_session_),
      base::scoped_policy::RETAIN);
  return scoped_ptr<VideoFrameFactory>(
      new VideoFrameFactoryCVPixelBufferPoolImpl(pool));
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
  if (status != noErr) {
    DLOG(ERROR) << " encoding failed: " << status;
    return;
  }
  if ((info & VideoToolboxGlue::kVTEncodeInfo_FrameDropped)) {
    DVLOG(2) << " frame dropped";
    return;
  }

  auto encoder = reinterpret_cast<H264VideoToolboxEncoder*>(encoder_opaque);
  const scoped_ptr<InProgressFrameEncode> request(
      reinterpret_cast<InProgressFrameEncode*>(request_opaque));
  auto sample_attachments = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(
      CoreMediaGlue::CMSampleBufferGetSampleAttachmentsArray(sbuf, true), 0));

  // If the NotSync key is not present, it implies Sync, which indicates a
  // keyframe (at least I think, VT documentation is, erm, sparse). Could
  // alternatively use kCMSampleAttachmentKey_DependsOnOthers == false.
  bool keyframe =
      !CFDictionaryContainsKey(sample_attachments,
                               CoreMediaGlue::kCMSampleAttachmentKey_NotSync());

  // Increment the encoder-scoped frame id and assign the new value to this
  // frame. VideoToolbox calls the output callback serially, so this is safe.
  uint32 frame_id = ++encoder->frame_id_;

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

  CopySampleBufferToAnnexBBuffer(sbuf, &encoded_frame->data, keyframe);

  encoder->cast_environment_->PostTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(request->frame_encoded_callback,
                 base::Passed(&encoded_frame)));
}

}  // namespace cast
}  // namespace media
