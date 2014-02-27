// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_sender/external_video_encoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/cast/cast_defines.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/video/video_encode_accelerator.h"

namespace {
// We allocate more input buffers than what is asked for by
// RequireBitstreamBuffers() due to potential threading timing.
static const int kInputBufferExtraCount = 1;
static const int kOutputBufferCount = 3;

void LogFrameEncodedEvent(media::cast::CastEnvironment* const cast_environment,
                          const base::TimeTicks& capture_time) {
  cast_environment->Logging()->InsertFrameEvent(
      cast_environment->Clock()->NowTicks(),
      media::cast::kVideoFrameEncoded,
      media::cast::GetVideoRtpTimestamp(capture_time),
      media::cast::kFrameIdUnknown);
}
}  // namespace

namespace media {
namespace cast {

// Container for the associated data of a video frame being processed.
struct EncodedFrameReturnData {
  EncodedFrameReturnData(base::TimeTicks c_time,
                         VideoEncoder::FrameEncodedCallback callback) {
    capture_time = c_time;
    frame_encoded_callback = callback;
  }
  base::TimeTicks capture_time;
  VideoEncoder::FrameEncodedCallback frame_encoded_callback;
};

// The ExternalVideoEncoder class can be deleted directly by cast, while
// LocalVideoEncodeAcceleratorClient stays around long enough to properly shut
// down the VideoEncodeAccelerator.
class LocalVideoEncodeAcceleratorClient
    : public VideoEncodeAccelerator::Client,
      public base::RefCountedThreadSafe<LocalVideoEncodeAcceleratorClient> {
 public:
  LocalVideoEncodeAcceleratorClient(
      scoped_refptr<CastEnvironment> cast_environment,
      scoped_refptr<GpuVideoAcceleratorFactories> gpu_factories,
      const base::WeakPtr<ExternalVideoEncoder>& weak_owner)
      : cast_environment_(cast_environment),
        gpu_factories_(gpu_factories),
        encoder_task_runner_(gpu_factories->GetTaskRunner()),
        weak_owner_(weak_owner),
        last_encoded_frame_id_(kStartFrameId) {
    DCHECK(encoder_task_runner_);
  }

  // Initialize the real HW encoder.
  void Initialize(const VideoSenderConfig& video_config) {
    DCHECK(gpu_factories_);
    DCHECK(encoder_task_runner_);

    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    video_encode_accelerator_ =
        gpu_factories_->CreateVideoEncodeAccelerator().Pass();
    if (!video_encode_accelerator_)
      return;

    VideoCodecProfile output_profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
    switch (video_config.codec) {
      case transport::kVp8:
        output_profile = media::VP8PROFILE_MAIN;
        break;
      case transport::kH264:
        output_profile = media::H264PROFILE_MAIN;
        break;
    }
    codec_ = video_config.codec;
    max_frame_rate_ = video_config.max_frame_rate;

    // Asynchronous initialization call; NotifyInitializeDone or NotifyError
    // will be called once the HW is initialized.
    video_encode_accelerator_->Initialize(
        media::VideoFrame::I420,
        gfx::Size(video_config.width, video_config.height),
        output_profile,
        video_config.start_bitrate,
        this);
  }

  // Free the HW.
  void Destroy() {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    if (video_encode_accelerator_) {
      video_encode_accelerator_.release()->Destroy();
    }
  }

  void SetBitRate(uint32 bit_rate) {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    video_encode_accelerator_->RequestEncodingParametersChange(bit_rate,
                                                               max_frame_rate_);
  }

  void EncodeVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& capture_time,
      bool key_frame_requested,
      const VideoEncoder::FrameEncodedCallback& frame_encoded_callback) {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    if (input_buffers_free_.empty()) {
      NOTREACHED();
      VLOG(2) << "EncodeVideoFrame(): drop frame due to no hw buffers";
      return;
    }
    const int index = input_buffers_free_.back();
    base::SharedMemory* input_buffer = input_buffers_[index];

    // TODO(pwestin): this allocation and copy can be removed once we don't
    // pass the video frame through liblingle.

    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::WrapExternalPackedMemory(
            video_frame->format(),
            video_frame->coded_size(),
            video_frame->visible_rect(),
            video_frame->natural_size(),
            reinterpret_cast<uint8*>(input_buffer->memory()),
            input_buffer->mapped_size(),
            input_buffer->handle(),
            video_frame->GetTimestamp(),
            base::Bind(&LocalVideoEncodeAcceleratorClient::FinishedWithInBuffer,
                       this,
                       index));

    if (!frame) {
      VLOG(1) << "EncodeVideoFrame(): failed to create frame";
      NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    // Do a stride copy of the input frame to match the input requirements.
    media::CopyYPlane(video_frame->data(VideoFrame::kYPlane),
                      video_frame->stride(VideoFrame::kYPlane),
                      video_frame->natural_size().height(),
                      frame.get());
    media::CopyUPlane(video_frame->data(VideoFrame::kUPlane),
                      video_frame->stride(VideoFrame::kUPlane),
                      video_frame->natural_size().height(),
                      frame.get());
    media::CopyVPlane(video_frame->data(VideoFrame::kVPlane),
                      video_frame->stride(VideoFrame::kVPlane),
                      video_frame->natural_size().height(),
                      frame.get());

    encoded_frame_data_storage_.push_back(
        EncodedFrameReturnData(capture_time, frame_encoded_callback));

    // BitstreamBufferReady will be called once the encoder is done.
    video_encode_accelerator_->Encode(frame, key_frame_requested);
  }

 protected:
  virtual void NotifyInitializeDone() OVERRIDE {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());

    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(&ExternalVideoEncoder::EncoderInitialized, weak_owner_));
  }

  virtual void NotifyError(VideoEncodeAccelerator::Error error) OVERRIDE {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());
    VLOG(1) << "ExternalVideoEncoder NotifyError: " << error;

    if (video_encode_accelerator_) {
      video_encode_accelerator_.release()->Destroy();
    }
    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(&ExternalVideoEncoder::EncoderError, weak_owner_));
  }

  // Called to allocate the input and output buffers.
  virtual void RequireBitstreamBuffers(unsigned int input_count,
                                       const gfx::Size& input_coded_size,
                                       size_t output_buffer_size) OVERRIDE {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());
    DCHECK(video_encode_accelerator_);

    for (unsigned int i = 0; i < input_count + kInputBufferExtraCount; ++i) {
      base::SharedMemory* shm =
          gpu_factories_->CreateSharedMemory(media::VideoFrame::AllocationSize(
              media::VideoFrame::I420, input_coded_size));
      if (!shm) {
        VLOG(1) << "RequireBitstreamBuffers(): failed to create input buffer ";
        return;
      }
      input_buffers_.push_back(shm);
      input_buffers_free_.push_back(i);
    }

    for (int j = 0; j < kOutputBufferCount; ++j) {
      base::SharedMemory* shm =
          gpu_factories_->CreateSharedMemory(output_buffer_size);
      if (!shm) {
        VLOG(1) << "RequireBitstreamBuffers(): failed to create input buffer ";
        return;
      }
      output_buffers_.push_back(shm);
    }
    // Immediately provide all output buffers to the VEA.
    for (size_t i = 0; i < output_buffers_.size(); ++i) {
      video_encode_accelerator_->UseOutputBitstreamBuffer(
          media::BitstreamBuffer(static_cast<int32>(i),
                                 output_buffers_[i]->handle(),
                                 output_buffers_[i]->mapped_size()));
    }
  }

  // Encoder has encoded a frame and it's available in one of out output
  // buffers.
  virtual void BitstreamBufferReady(int32 bitstream_buffer_id,
                                    size_t payload_size,
                                    bool key_frame) OVERRIDE {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());
    if (bitstream_buffer_id < 0 ||
        bitstream_buffer_id >= static_cast<int32>(output_buffers_.size())) {
      NOTREACHED();
      VLOG(1) << "BitstreamBufferReady(): invalid bitstream_buffer_id="
              << bitstream_buffer_id;
      NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    base::SharedMemory* output_buffer = output_buffers_[bitstream_buffer_id];
    if (payload_size > output_buffer->mapped_size()) {
      NOTREACHED();
      VLOG(1) << "BitstreamBufferReady(): invalid payload_size = "
              << payload_size;
      NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    if (encoded_frame_data_storage_.empty()) {
      NOTREACHED();
      NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
    scoped_ptr<transport::EncodedVideoFrame> encoded_frame(
        new transport::EncodedVideoFrame());

    encoded_frame->codec = codec_;
    encoded_frame->key_frame = key_frame;
    encoded_frame->last_referenced_frame_id = last_encoded_frame_id_;
    last_encoded_frame_id_++;
    encoded_frame->frame_id = last_encoded_frame_id_;
    encoded_frame->rtp_timestamp =
        GetVideoRtpTimestamp(encoded_frame_data_storage_.front().capture_time);
    if (key_frame) {
      // Self referenced.
      encoded_frame->last_referenced_frame_id = encoded_frame->frame_id;
    }

    encoded_frame->data.insert(
        0, static_cast<const char*>(output_buffer->memory()), payload_size);

    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(LogFrameEncodedEvent,
                   cast_environment_,
                   encoded_frame_data_storage_.front().capture_time));

    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(encoded_frame_data_storage_.front().frame_encoded_callback,
                   base::Passed(&encoded_frame),
                   encoded_frame_data_storage_.front().capture_time));

    encoded_frame_data_storage_.pop_front();

    // We need to re-add the output buffer to the encoder after we are done
    // with it.
    video_encode_accelerator_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
        bitstream_buffer_id,
        output_buffers_[bitstream_buffer_id]->handle(),
        output_buffers_[bitstream_buffer_id]->mapped_size()));
  }

 private:
  // Encoder is done with the provided input buffer.
  void FinishedWithInBuffer(int input_index) {
    DCHECK(encoder_task_runner_);
    DCHECK(encoder_task_runner_->RunsTasksOnCurrentThread());
    DCHECK_GE(input_index, 0);
    DCHECK_LT(input_index, static_cast<int>(input_buffers_.size()));
    VLOG(2) << "EncodeFrameFinished(): index=" << input_index;
    input_buffers_free_.push_back(input_index);
  }

  friend class base::RefCountedThreadSafe<LocalVideoEncodeAcceleratorClient>;

  virtual ~LocalVideoEncodeAcceleratorClient() {}

  const scoped_refptr<CastEnvironment> cast_environment_;
  scoped_refptr<GpuVideoAcceleratorFactories> gpu_factories_;
  scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner_;
  const base::WeakPtr<ExternalVideoEncoder> weak_owner_;

  scoped_ptr<media::VideoEncodeAccelerator> video_encode_accelerator_;
  int max_frame_rate_;
  transport::VideoCodec codec_;
  uint32 last_encoded_frame_id_;

  // Shared memory buffers for input/output with the VideoAccelerator.
  ScopedVector<base::SharedMemory> input_buffers_;
  ScopedVector<base::SharedMemory> output_buffers_;

  // Input buffers ready to be filled with input from Encode(). As a LIFO since
  // we don't care about ordering.
  std::vector<int> input_buffers_free_;

  // FIFO list.
  std::list<EncodedFrameReturnData> encoded_frame_data_storage_;

  DISALLOW_COPY_AND_ASSIGN(LocalVideoEncodeAcceleratorClient);
};

ExternalVideoEncoder::ExternalVideoEncoder(
    scoped_refptr<CastEnvironment> cast_environment,
    const VideoSenderConfig& video_config,
    scoped_refptr<GpuVideoAcceleratorFactories> gpu_factories)
    : video_config_(video_config),
      cast_environment_(cast_environment),
      encoder_active_(false),
      key_frame_requested_(false),
      skip_next_frame_(false),
      skip_count_(0),
      encoder_task_runner_(gpu_factories->GetTaskRunner()),
      weak_factory_(this) {
  DCHECK(gpu_factories);
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  video_accelerator_client_ = new LocalVideoEncodeAcceleratorClient(
      cast_environment, gpu_factories, weak_factory_.GetWeakPtr());

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalVideoEncodeAcceleratorClient::Initialize,
                 video_accelerator_client_,
                 video_config));
}

ExternalVideoEncoder::~ExternalVideoEncoder() {
  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalVideoEncodeAcceleratorClient::Destroy,
                 video_accelerator_client_));
}

void ExternalVideoEncoder::EncoderInitialized() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  encoder_active_ = true;
}

void ExternalVideoEncoder::EncoderError() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  encoder_active_ = false;
}

bool ExternalVideoEncoder::EncodeVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& capture_time,
    const FrameEncodedCallback& frame_encoded_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  if (!encoder_active_)
    return false;

  if (skip_next_frame_) {
    VLOG(1) << "Skip encoding frame";
    ++skip_count_;
    skip_next_frame_ = false;
    return false;
  }
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  cast_environment_->Logging()->InsertFrameEvent(
      now,
      kVideoFrameSentToEncoder,
      GetVideoRtpTimestamp(capture_time),
      kFrameIdUnknown);

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalVideoEncodeAcceleratorClient::EncodeVideoFrame,
                 video_accelerator_client_,
                 video_frame,
                 capture_time,
                 key_frame_requested_,
                 frame_encoded_callback));

  key_frame_requested_ = false;
  return true;
}

// Inform the encoder about the new target bit rate.
void ExternalVideoEncoder::SetBitRate(int new_bit_rate) {
  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalVideoEncodeAcceleratorClient::SetBitRate,
                 video_accelerator_client_,
                 new_bit_rate));
}

// Inform the encoder to not encode the next frame.
void ExternalVideoEncoder::SkipNextFrame(bool skip_next_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  skip_next_frame_ = skip_next_frame;
}

// Inform the encoder to encode the next frame as a key frame.
void ExternalVideoEncoder::GenerateKeyFrame() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  key_frame_requested_ = true;
}

// Inform the encoder to only reference frames older or equal to frame_id;
void ExternalVideoEncoder::LatestFrameIdToReference(uint32 /*frame_id*/) {
  // Do nothing not supported.
}

int ExternalVideoEncoder::NumberOfSkippedFrames() const {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  return skip_count_;
}

}  //  namespace cast
}  //  namespace media
