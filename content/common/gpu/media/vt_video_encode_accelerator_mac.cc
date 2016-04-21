// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/vt_video_encode_accelerator_mac.h"

#include <memory>

#include "base/mac/mac_util.h"
#include "base/thread_task_runner_handle.h"
#include "media/base/mac/coremedia_glue.h"
#include "media/base/mac/corevideo_glue.h"
#include "media/base/mac/video_frame_mac.h"

namespace content {

namespace {

// TODO(emircan): Check if we can find the actual system capabilities via
// creating VTCompressionSessions with varying requirements.
// See crbug.com/584784.
const size_t kBitsPerByte = 8;
const size_t kDefaultResolutionWidth = 640;
const size_t kDefaultResolutionHeight = 480;
const size_t kMaxFrameRateNumerator = 30;
const size_t kMaxFrameRateDenominator = 1;
const size_t kMaxResolutionWidth = 4096;
const size_t kMaxResolutionHeight = 2160;
const size_t kNumInputBuffers = 3;

}  // namespace

struct VTVideoEncodeAccelerator::InProgressFrameEncode {
  InProgressFrameEncode(base::TimeDelta rtp_timestamp,
                        base::TimeTicks ref_time)
      : timestamp(rtp_timestamp), reference_time(ref_time) {}
  const base::TimeDelta timestamp;
  const base::TimeTicks reference_time;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InProgressFrameEncode);
};

struct VTVideoEncodeAccelerator::EncodeOutput {
  EncodeOutput(VTEncodeInfoFlags info_flags, CMSampleBufferRef sbuf)
      : info(info_flags), sample_buffer(sbuf, base::scoped_policy::RETAIN) {}
  const VTEncodeInfoFlags info;
  const base::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(EncodeOutput);
};

struct VTVideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(int32_t id,
                     std::unique_ptr<base::SharedMemory> shm,
                     size_t size)
      : id(id), shm(std::move(shm)), size(size) {}
  const int32_t id;
  const std::unique_ptr<base::SharedMemory> shm;
  const size_t size;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BitstreamBufferRef);
};

VTVideoEncodeAccelerator::VTVideoEncodeAccelerator()
    : client_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      encoder_thread_("VTEncoderThread"),
      encoder_task_weak_factory_(this) {
  encoder_weak_ptr_ = encoder_task_weak_factory_.GetWeakPtr();
}

VTVideoEncodeAccelerator::~VTVideoEncodeAccelerator() {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  Destroy();
  DCHECK(!encoder_thread_.IsRunning());
  DCHECK(!encoder_task_weak_factory_.HasWeakPtrs());
}

media::VideoEncodeAccelerator::SupportedProfiles
VTVideoEncodeAccelerator::GetSupportedProfiles() {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  SupportedProfiles profiles;
  // Check if HW encoder is supported initially.
  videotoolbox_glue_ = VideoToolboxGlue::Get();
  if (!videotoolbox_glue_) {
    DLOG(ERROR) << "Failed creating VideoToolbox glue.";
    return profiles;
  }
  if (!base::mac::IsOSMavericksOrLater()) {
    DLOG(ERROR) << "VideoToolbox hardware encoder is supported on Mac OS 10.9 "
                  "and later.";
    return profiles;
  }
  const bool rv = CreateCompressionSession(
      media::video_toolbox::DictionaryWithKeysAndValues(nullptr, nullptr, 0),
      gfx::Size(kDefaultResolutionWidth, kDefaultResolutionHeight), true);
  DestroyCompressionSession();
  if (!rv) {
    VLOG(1)
        << "Hardware encode acceleration is not available on this platform.";
    return profiles;
  }

  SupportedProfile profile;
  profile.profile = media::H264PROFILE_BASELINE;
  profile.max_framerate_numerator = kMaxFrameRateNumerator;
  profile.max_framerate_denominator = kMaxFrameRateDenominator;
  profile.max_resolution = gfx::Size(kMaxResolutionWidth, kMaxResolutionHeight);
  profiles.push_back(profile);
  return profiles;
}

bool VTVideoEncodeAccelerator::Initialize(
    media::VideoPixelFormat format,
    const gfx::Size& input_visible_size,
    media::VideoCodecProfile output_profile,
    uint32_t initial_bitrate,
    Client* client) {
  DVLOG(3) << __FUNCTION__
           << ": input_format=" << media::VideoPixelFormatToString(format)
           << ", input_visible_size=" << input_visible_size.ToString()
           << ", output_profile=" << output_profile
           << ", initial_bitrate=" << initial_bitrate;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(client);

  if (media::PIXEL_FORMAT_I420 != format) {
    DLOG(ERROR) << "Input format not supported= "
                << media::VideoPixelFormatToString(format);
    return false;
  }
  if (media::H264PROFILE_BASELINE != output_profile) {
    DLOG(ERROR) << "Output profile not supported= "
                << output_profile;
    return false;
  }

  videotoolbox_glue_ = VideoToolboxGlue::Get();
  if (!videotoolbox_glue_) {
    DLOG(ERROR) << "Failed creating VideoToolbox glue.";
    return false;
  }

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();
  input_visible_size_ = input_visible_size;
  frame_rate_ = kMaxFrameRateNumerator / kMaxFrameRateDenominator;
  target_bitrate_ = initial_bitrate;
  bitstream_buffer_size_ = input_visible_size.GetArea();

  if (!encoder_thread_.Start()) {
    DLOG(ERROR) << "Failed spawning encoder thread.";
    return false;
  }
  encoder_thread_task_runner_ = encoder_thread_.task_runner();

  if (!ResetCompressionSession()) {
    DLOG(ERROR) << "Failed creating compression session.";
    return false;
  }

  client_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Client::RequireBitstreamBuffers, client_, kNumInputBuffers,
                 input_visible_size_, bitstream_buffer_size_));
  return true;
}

void VTVideoEncodeAccelerator::Encode(
    const scoped_refptr<media::VideoFrame>& frame,
    bool force_keyframe) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VTVideoEncodeAccelerator::EncodeTask,
                            base::Unretained(this), frame, force_keyframe));
}

void VTVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    const media::BitstreamBuffer& buffer) {
  DVLOG(3) << __FUNCTION__ << ": buffer size=" << buffer.size();
  DCHECK(thread_checker_.CalledOnValidThread());

  if (buffer.size() < bitstream_buffer_size_) {
    DLOG(ERROR) << "Output BitstreamBuffer isn't big enough: " << buffer.size()
                << " vs. " << bitstream_buffer_size_;
    client_->NotifyError(kInvalidArgumentError);
    return;
  }

  std::unique_ptr<base::SharedMemory> shm(
      new base::SharedMemory(buffer.handle(), false));
  if (!shm->Map(buffer.size())) {
    DLOG(ERROR) << "Failed mapping shared memory.";
    client_->NotifyError(kPlatformFailureError);
    return;
  }

  std::unique_ptr<BitstreamBufferRef> buffer_ref(
      new BitstreamBufferRef(buffer.id(), std::move(shm), buffer.size()));

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VTVideoEncodeAccelerator::UseOutputBitstreamBufferTask,
                 base::Unretained(this), base::Passed(&buffer_ref)));
}

void VTVideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOG(3) << __FUNCTION__ << ": bitrate=" << bitrate
           << ": framerate=" << framerate;
  DCHECK(thread_checker_.CalledOnValidThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VTVideoEncodeAccelerator::RequestEncodingParametersChangeTask,
                 base::Unretained(this), bitrate, framerate));
}

void VTVideoEncodeAccelerator::Destroy() {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Cancel all callbacks.
  client_ptr_factory_.reset();

  if (encoder_thread_.IsRunning()) {
    encoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VTVideoEncodeAccelerator::DestroyTask,
                   base::Unretained(this)));
    encoder_thread_.Stop();
  } else {
    DestroyTask();
  }
}

void VTVideoEncodeAccelerator::EncodeTask(
    const scoped_refptr<media::VideoFrame>& frame,
    bool force_keyframe) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(compression_session_);
  DCHECK(frame);

  // TODO(emircan): See if we can eliminate a copy here by using
  // CVPixelBufferPool for the allocation of incoming VideoFrames.
  base::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer =
      media::WrapVideoFrameInCVPixelBuffer(*frame);
  base::ScopedCFTypeRef<CFDictionaryRef> frame_props =
      media::video_toolbox::DictionaryWithKeyValue(
          videotoolbox_glue_->kVTEncodeFrameOptionKey_ForceKeyFrame(),
          force_keyframe ? kCFBooleanTrue : kCFBooleanFalse);

  base::TimeTicks ref_time;
  if (!frame->metadata()->GetTimeTicks(
          media::VideoFrameMetadata::REFERENCE_TIME, &ref_time)) {
    ref_time = base::TimeTicks::Now();
  }
  auto timestamp_cm = CoreMediaGlue::CMTimeMake(
      frame->timestamp().InMicroseconds(), USEC_PER_SEC);
  // Wrap information we'll need after the frame is encoded in a heap object.
  // We'll get the pointer back from the VideoToolbox completion callback.
  std::unique_ptr<InProgressFrameEncode> request(
      new InProgressFrameEncode(frame->timestamp(), ref_time));

  // We can pass the ownership of |request| to the encode callback if
  // successful. Otherwise let it fall out of scope.
  OSStatus status = videotoolbox_glue_->VTCompressionSessionEncodeFrame(
      compression_session_, pixel_buffer, timestamp_cm,
      CoreMediaGlue::CMTime{0, 0, 0, 0}, frame_props,
      reinterpret_cast<void*>(request.get()), nullptr);
  if (status != noErr) {
    DLOG(ERROR) << " VTCompressionSessionEncodeFrame failed: " << status;
    NotifyError(kPlatformFailureError);
  } else {
    CHECK(request.release());
  }
}

void VTVideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // If there is already EncodeOutput waiting, copy its output first.
  if (!encoder_output_queue_.empty()) {
    std::unique_ptr<VTVideoEncodeAccelerator::EncodeOutput> encode_output =
        std::move(encoder_output_queue_.front());
    encoder_output_queue_.pop_front();
    ReturnBitstreamBuffer(std::move(encode_output), std::move(buffer_ref));
    return;
  }

  bitstream_buffer_queue_.push_back(std::move(buffer_ref));
}

void VTVideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    uint32_t bitrate,
    uint32_t framerate) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  frame_rate_ = framerate > 1 ? framerate : 1;
  target_bitrate_ = bitrate > 1 ? bitrate : 1;

  if (!compression_session_) {
    NotifyError(kPlatformFailureError);
    return;
  }

  media::video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_, videotoolbox_glue_);
  // TODO(emircan): See crbug.com/425352.
  bool rv = session_property_setter.Set(
      videotoolbox_glue_->kVTCompressionPropertyKey_AverageBitRate(),
      target_bitrate_);
  rv &= session_property_setter.Set(
      videotoolbox_glue_->kVTCompressionPropertyKey_ExpectedFrameRate(),
      frame_rate_);
  rv &= session_property_setter.Set(
      videotoolbox_glue_->kVTCompressionPropertyKey_DataRateLimits(),
      media::video_toolbox::ArrayWithIntegerAndFloat(
          target_bitrate_ / kBitsPerByte, 1.0f));
  DLOG_IF(ERROR, !rv) << "Couldn't change session encoding parameters.";
}

void VTVideoEncodeAccelerator::DestroyTask() {
  DCHECK(thread_checker_.CalledOnValidThread() ||
         (encoder_thread_.IsRunning() &&
          encoder_thread_task_runner_->BelongsToCurrentThread()));

  // Cancel all encoder thread callbacks.
  encoder_task_weak_factory_.InvalidateWeakPtrs();

  // This call blocks until all pending frames are flushed out.
  DestroyCompressionSession();
}

void VTVideoEncodeAccelerator::NotifyError(
    media::VideoEncodeAccelerator::Error error) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  client_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Client::NotifyError, client_, error));
}

// static
void VTVideoEncodeAccelerator::CompressionCallback(void* encoder_opaque,
                                                   void* request_opaque,
                                                   OSStatus status,
                                                   VTEncodeInfoFlags info,
                                                   CMSampleBufferRef sbuf) {
  // This function may be called asynchronously, on a different thread from the
  // one that calls VTCompressionSessionEncodeFrame.
  DVLOG(3) << __FUNCTION__;

  auto encoder = reinterpret_cast<VTVideoEncodeAccelerator*>(encoder_opaque);
  DCHECK(encoder);

  // Release InProgressFrameEncode, since we don't have support to return
  // timestamps at this point.
  std::unique_ptr<InProgressFrameEncode> request(
      reinterpret_cast<InProgressFrameEncode*>(request_opaque));
  request.reset();

  // EncodeOutput holds onto CMSampleBufferRef when posting task between
  // threads.
  std::unique_ptr<EncodeOutput> encode_output(new EncodeOutput(info, sbuf));

  // This method is NOT called on |encoder_thread_|, so we still need to
  // post a task back to it to do work.
  encoder->encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VTVideoEncodeAccelerator::CompressionCallbackTask,
                            encoder->encoder_weak_ptr_, status,
                            base::Passed(&encode_output)));
}

void VTVideoEncodeAccelerator::CompressionCallbackTask(
    OSStatus status,
    std::unique_ptr<EncodeOutput> encode_output) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (status != noErr) {
    DLOG(ERROR) << " encode failed: " << status;
    NotifyError(kPlatformFailureError);
    return;
  }

  // If there isn't any BitstreamBuffer to copy into, add it to a queue for
  // later use.
  if (bitstream_buffer_queue_.empty()) {
    encoder_output_queue_.push_back(std::move(encode_output));
    return;
  }

  std::unique_ptr<VTVideoEncodeAccelerator::BitstreamBufferRef> buffer_ref =
      std::move(bitstream_buffer_queue_.front());
  bitstream_buffer_queue_.pop_front();
  ReturnBitstreamBuffer(std::move(encode_output), std::move(buffer_ref));
}

void VTVideoEncodeAccelerator::ReturnBitstreamBuffer(
    std::unique_ptr<EncodeOutput> encode_output,
    std::unique_ptr<VTVideoEncodeAccelerator::BitstreamBufferRef> buffer_ref) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (encode_output->info & VideoToolboxGlue::kVTEncodeInfo_FrameDropped) {
    DVLOG(2) << " frame dropped";
    client_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Client::BitstreamBufferReady, client_,
                              buffer_ref->id, 0, false));
    return;
  }

  auto sample_attachments = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(
      CoreMediaGlue::CMSampleBufferGetSampleAttachmentsArray(
          encode_output->sample_buffer.get(), true),
      0));
  const bool keyframe =
      !CFDictionaryContainsKey(sample_attachments,
                               CoreMediaGlue::kCMSampleAttachmentKey_NotSync());

  size_t used_buffer_size = 0;
  const bool copy_rv = media::video_toolbox::CopySampleBufferToAnnexBBuffer(
      encode_output->sample_buffer.get(), keyframe, buffer_ref->size,
      reinterpret_cast<char*>(buffer_ref->shm->memory()), &used_buffer_size);
  if (!copy_rv) {
    DLOG(ERROR) << "Cannot copy output from SampleBuffer to AnnexBBuffer.";
    used_buffer_size = 0;
  }

  client_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Client::BitstreamBufferReady, client_,
                            buffer_ref->id, used_buffer_size, keyframe));
}

bool VTVideoEncodeAccelerator::ResetCompressionSession() {
  DCHECK(thread_checker_.CalledOnValidThread());

  DestroyCompressionSession();

  CFTypeRef attributes_keys[] = {
    kCVPixelBufferOpenGLCompatibilityKey,
    kCVPixelBufferIOSurfacePropertiesKey,
    kCVPixelBufferPixelFormatTypeKey
  };
  const int format[] = {
      CoreVideoGlue::kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange};
  CFTypeRef attributes_values[] = {
      kCFBooleanTrue,
      media::video_toolbox::DictionaryWithKeysAndValues(nullptr, nullptr, 0)
          .release(),
      media::video_toolbox::ArrayWithIntegers(format, arraysize(format))
          .release()};
  const base::ScopedCFTypeRef<CFDictionaryRef> attributes =
      media::video_toolbox::DictionaryWithKeysAndValues(
          attributes_keys, attributes_values, arraysize(attributes_keys));
  for (auto& v : attributes_values)
    CFRelease(v);

  bool session_rv =
      CreateCompressionSession(attributes, input_visible_size_, false);
  if (!session_rv) {
    DestroyCompressionSession();
    return false;
  }

  const bool configure_rv = ConfigureCompressionSession();
  if (configure_rv)
    RequestEncodingParametersChange(target_bitrate_, frame_rate_);
  return configure_rv;
}

bool VTVideoEncodeAccelerator::CreateCompressionSession(
    base::ScopedCFTypeRef<CFDictionaryRef> attributes,
    const gfx::Size& input_size,
    bool require_hw_encoding) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<CFTypeRef> encoder_keys;
  std::vector<CFTypeRef> encoder_values;
  if (require_hw_encoding) {
    encoder_keys.push_back(videotoolbox_glue_
      ->kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder());
    encoder_values.push_back(kCFBooleanTrue);
  } else {
    encoder_keys.push_back(videotoolbox_glue_
        ->kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder());
    encoder_values.push_back(kCFBooleanTrue);
  }
  base::ScopedCFTypeRef<CFDictionaryRef> encoder_spec =
      media::video_toolbox::DictionaryWithKeysAndValues(
          encoder_keys.data(), encoder_values.data(), encoder_keys.size());

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
      kCFAllocatorDefault,
      input_size.width(),
      input_size.height(),
      CoreMediaGlue::kCMVideoCodecType_H264,
      encoder_spec,
      attributes,
      nullptr /* compressedDataAllocator */,
      &VTVideoEncodeAccelerator::CompressionCallback,
      reinterpret_cast<void*>(this),
      compression_session_.InitializeInto());
  if (status != noErr) {
    DLOG(ERROR) << " VTCompressionSessionCreate failed: " << status;
    return false;
  }
  DVLOG(3) << " VTCompressionSession created with HW encode: "
           << require_hw_encoding << ", input size=" << input_size.ToString();
  return true;
}

bool VTVideoEncodeAccelerator::ConfigureCompressionSession() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(compression_session_);

  media::video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_, videotoolbox_glue_);
  bool rv = true;
  rv &= session_property_setter.Set(
      videotoolbox_glue_->kVTCompressionPropertyKey_ProfileLevel(),
      videotoolbox_glue_->kVTProfileLevel_H264_Baseline_AutoLevel());
  rv &= session_property_setter.Set(
      videotoolbox_glue_->kVTCompressionPropertyKey_RealTime(), true);
  rv &= session_property_setter.Set(
      videotoolbox_glue_->kVTCompressionPropertyKey_AllowFrameReordering(),
      false);
  DLOG_IF(ERROR, !rv) << " Setting session property failed.";
  return rv;
}

void VTVideoEncodeAccelerator::DestroyCompressionSession() {
  DCHECK(thread_checker_.CalledOnValidThread() ||
         (encoder_thread_.IsRunning() &&
          encoder_thread_task_runner_->BelongsToCurrentThread()));

  if (compression_session_) {
    videotoolbox_glue_->VTCompressionSessionInvalidate(compression_session_);
    compression_session_.reset();
  }
}

} // namespace content
