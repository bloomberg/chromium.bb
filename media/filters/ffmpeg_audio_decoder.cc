// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_audio_decoder.h"

#include "base/callback_helpers.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/sample_format.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

// Returns true if the decode result was end of stream.
static inline bool IsEndOfStream(int result,
                                 int decoded_size,
                                 const scoped_refptr<DecoderBuffer>& input) {
  // Three conditions to meet to declare end of stream for this decoder:
  // 1. FFmpeg didn't read anything.
  // 2. FFmpeg didn't output anything.
  // 3. An end of stream buffer is received.
  return result == 0 && decoded_size == 0 && input->end_of_stream();
}

// Return the number of channels from the data in |frame|.
static inline int DetermineChannels(AVFrame* frame) {
#if defined(CHROMIUM_NO_AVFRAME_CHANNELS)
  // When use_system_ffmpeg==1, libav's AVFrame doesn't have channels field.
  return av_get_channel_layout_nb_channels(frame->channel_layout);
#else
  return frame->channels;
#endif
}

// Called by FFmpeg's allocation routine to free a buffer. |opaque| is the
// AudioBuffer allocated, so unref it.
static void ReleaseAudioBufferImpl(void* opaque, uint8* data) {
  scoped_refptr<AudioBuffer> buffer;
  buffer.swap(reinterpret_cast<AudioBuffer**>(&opaque));
}

// Called by FFmpeg's allocation routine to allocate a buffer. Uses
// AVCodecContext.opaque to get the object reference in order to call
// GetAudioBuffer() to do the actual allocation.
static int GetAudioBuffer(struct AVCodecContext* s, AVFrame* frame, int flags) {
  DCHECK(s->codec->capabilities & CODEC_CAP_DR1);
  DCHECK_EQ(s->codec_type, AVMEDIA_TYPE_AUDIO);

  // Since this routine is called by FFmpeg when a buffer is required for audio
  // data, use the values supplied by FFmpeg (ignoring the current settings).
  // FFmpegDecode() gets to determine if the buffer is useable or not.
  AVSampleFormat format = static_cast<AVSampleFormat>(frame->format);
  SampleFormat sample_format = AVSampleFormatToSampleFormat(format);
  int channels = DetermineChannels(frame);
  if ((channels <= 0) || (channels >= limits::kMaxChannels)) {
    DLOG(ERROR) << "Requested number of channels (" << channels
                << ") exceeds limit.";
    return AVERROR(EINVAL);
  }

  int bytes_per_channel = SampleFormatToBytesPerChannel(sample_format);
  if (frame->nb_samples <= 0)
    return AVERROR(EINVAL);

  // Determine how big the buffer should be and allocate it. FFmpeg may adjust
  // how big each channel data is in order to meet the alignment policy, so
  // we need to take this into consideration.
  int buffer_size_in_bytes =
      av_samples_get_buffer_size(&frame->linesize[0],
                                 channels,
                                 frame->nb_samples,
                                 format,
                                 AudioBuffer::kChannelAlignment);
  // Check for errors from av_samples_get_buffer_size().
  if (buffer_size_in_bytes < 0)
    return buffer_size_in_bytes;
  int frames_required = buffer_size_in_bytes / bytes_per_channel / channels;
  DCHECK_GE(frames_required, frame->nb_samples);
  scoped_refptr<AudioBuffer> buffer =
      AudioBuffer::CreateBuffer(sample_format, channels, frames_required);

  // Initialize the data[] and extended_data[] fields to point into the memory
  // allocated for AudioBuffer. |number_of_planes| will be 1 for interleaved
  // audio and equal to |channels| for planar audio.
  int number_of_planes = buffer->channel_data().size();
  if (number_of_planes <= AV_NUM_DATA_POINTERS) {
    DCHECK_EQ(frame->extended_data, frame->data);
    for (int i = 0; i < number_of_planes; ++i)
      frame->data[i] = buffer->channel_data()[i];
  } else {
    // There are more channels than can fit into data[], so allocate
    // extended_data[] and fill appropriately.
    frame->extended_data = static_cast<uint8**>(
        av_malloc(number_of_planes * sizeof(*frame->extended_data)));
    int i = 0;
    for (; i < AV_NUM_DATA_POINTERS; ++i)
      frame->extended_data[i] = frame->data[i] = buffer->channel_data()[i];
    for (; i < number_of_planes; ++i)
      frame->extended_data[i] = buffer->channel_data()[i];
  }

  // Now create an AVBufferRef for the data just allocated. It will own the
  // reference to the AudioBuffer object.
  void* opaque = NULL;
  buffer.swap(reinterpret_cast<AudioBuffer**>(&opaque));
  frame->buf[0] = av_buffer_create(
      frame->data[0], buffer_size_in_bytes, ReleaseAudioBufferImpl, opaque, 0);
  return 0;
}

FFmpegAudioDecoder::FFmpegAudioDecoder(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner),
      state_(kUninitialized),
      bytes_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_NONE),
      channels_(0),
      samples_per_second_(0),
      av_sample_format_(0),
      last_input_timestamp_(kNoTimestamp()),
      output_frames_to_drop_(0) {}

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
  DCHECK_EQ(state_, kUninitialized);
  DCHECK(!codec_context_);
  DCHECK(!av_frame_);
}

void FFmpegAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                    const PipelineStatusCB& status_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!config.is_encrypted());

  FFmpegGlue::InitializeFFmpeg();

  config_ = config;
  PipelineStatusCB initialize_cb = BindToCurrentLoop(status_cb);

  if (!config.IsValidConfig() || !ConfigureDecoder()) {
    initialize_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  // Success!
  state_ = kNormal;
  initialize_cb.Run(PIPELINE_OK);
}

void FFmpegAudioDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                                const DecodeCB& decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!decode_cb.is_null());
  CHECK_NE(state_, kUninitialized);
  DecodeCB decode_cb_bound = BindToCurrentLoop(decode_cb);

  if (state_ == kError) {
    decode_cb_bound.Run(kDecodeError, NULL);
    return;
  }

  // Return empty frames if decoding has finished.
  if (state_ == kDecodeFinished) {
    decode_cb_bound.Run(kOk, AudioBuffer::CreateEOSBuffer());
    return;
  }

  if (!buffer) {
    decode_cb_bound.Run(kAborted, NULL);
    return;
  }

  DecodeBuffer(buffer, decode_cb_bound);
}

scoped_refptr<AudioBuffer> FFmpegAudioDecoder::GetDecodeOutput() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (queued_audio_.empty())
    return NULL;
  scoped_refptr<AudioBuffer> out = queued_audio_.front();
  queued_audio_.pop_front();
  return out;
}

int FFmpegAudioDecoder::bits_per_channel() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return bytes_per_channel_ * 8;
}

ChannelLayout FFmpegAudioDecoder::channel_layout() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return channel_layout_;
}

int FFmpegAudioDecoder::samples_per_second() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return samples_per_second_;
}

void FFmpegAudioDecoder::Reset(const base::Closure& closure) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  avcodec_flush_buffers(codec_context_.get());
  state_ = kNormal;
  ResetTimestampState();
  task_runner_->PostTask(FROM_HERE, closure);
}

void FFmpegAudioDecoder::Stop(const base::Closure& closure) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::ScopedClosureRunner runner(BindToCurrentLoop(closure));

  if (state_ == kUninitialized)
    return;

  ReleaseFFmpegResources();
  ResetTimestampState();
  state_ = kUninitialized;
}

void FFmpegAudioDecoder::DecodeBuffer(
    const scoped_refptr<DecoderBuffer>& buffer,
    const DecodeCB& decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kDecodeFinished);
  DCHECK_NE(state_, kError);

  DCHECK(buffer);

  // During decode, because reads are issued asynchronously, it is possible to
  // receive multiple end of stream buffers since each decode is acked. When the
  // first end of stream buffer is read, FFmpeg may still have frames queued
  // up in the decoder so we need to go through the decode loop until it stops
  // giving sensible data.  After that, the decoder should output empty
  // frames.  There are three states the decoder can be in:
  //
  //   kNormal: This is the starting state. Buffers are decoded. Decode errors
  //            are discarded.
  //   kFlushCodec: There isn't any more input data. Call avcodec_decode_audio4
  //                until no more data is returned to flush out remaining
  //                frames. The input buffer is ignored at this point.
  //   kDecodeFinished: All calls return empty frames.
  //   kError: Unexpected error happened.
  //
  // These are the possible state transitions.
  //
  // kNormal -> kFlushCodec:
  //     When buffer->end_of_stream() is first true.
  // kNormal -> kError:
  //     A decoding error occurs and decoding needs to stop.
  // kFlushCodec -> kDecodeFinished:
  //     When avcodec_decode_audio4() returns 0 data.
  // kFlushCodec -> kError:
  //     When avcodec_decode_audio4() errors out.
  // (any state) -> kNormal:
  //     Any time Reset() is called.

  // Make sure we are notified if http://crbug.com/49709 returns.  Issue also
  // occurs with some damaged files.
  if (!buffer->end_of_stream() && buffer->timestamp() == kNoTimestamp() &&
      output_timestamp_helper_->base_timestamp() == kNoTimestamp()) {
    DVLOG(1) << "Received a buffer without timestamps!";
    decode_cb.Run(kDecodeError, NULL);
    return;
  }

  if (!buffer->end_of_stream()) {
    if (last_input_timestamp_ == kNoTimestamp() &&
        codec_context_->codec_id == AV_CODEC_ID_VORBIS &&
        buffer->timestamp() < base::TimeDelta()) {
      // Dropping frames for negative timestamps as outlined in section A.2
      // in the Vorbis spec. http://xiph.org/vorbis/doc/Vorbis_I_spec.html
      output_frames_to_drop_ = floor(
          0.5 + -buffer->timestamp().InSecondsF() * samples_per_second_);
    } else {
      if (last_input_timestamp_ != kNoTimestamp() &&
          buffer->timestamp() < last_input_timestamp_) {
        const base::TimeDelta diff =
            buffer->timestamp() - last_input_timestamp_;
        DLOG(WARNING)
            << "Input timestamps are not monotonically increasing! "
            << " ts " << buffer->timestamp().InMicroseconds() << " us"
            << " diff " << diff.InMicroseconds() << " us";
      }

      last_input_timestamp_ = buffer->timestamp();
    }
  }

  // Transition to kFlushCodec on the first end of stream buffer.
  if (state_ == kNormal && buffer->end_of_stream()) {
    state_ = kFlushCodec;
  }

  if (!FFmpegDecode(buffer)) {
    state_ = kError;
    decode_cb.Run(kDecodeError, NULL);
    return;
  }

  if (queued_audio_.empty()) {
    if (state_ == kFlushCodec) {
      DCHECK(buffer->end_of_stream());
      state_ = kDecodeFinished;
      decode_cb.Run(kOk, AudioBuffer::CreateEOSBuffer());
      return;
    }

    decode_cb.Run(kNotEnoughData, NULL);
    return;
  }

  decode_cb.Run(kOk, queued_audio_.front());
  queued_audio_.pop_front();
}

bool FFmpegAudioDecoder::FFmpegDecode(
    const scoped_refptr<DecoderBuffer>& buffer) {

  DCHECK(queued_audio_.empty());

  AVPacket packet;
  av_init_packet(&packet);
  if (buffer->end_of_stream()) {
    packet.data = NULL;
    packet.size = 0;
  } else {
    packet.data = const_cast<uint8*>(buffer->data());
    packet.size = buffer->data_size();
  }

  // Each audio packet may contain several frames, so we must call the decoder
  // until we've exhausted the packet.  Regardless of the packet size we always
  // want to hand it to the decoder at least once, otherwise we would end up
  // skipping end of stream packets since they have a size of zero.
  do {
    int frame_decoded = 0;
    int result = avcodec_decode_audio4(
        codec_context_.get(), av_frame_.get(), &frame_decoded, &packet);

    if (result < 0) {
      DCHECK(!buffer->end_of_stream())
          << "End of stream buffer produced an error! "
          << "This is quite possibly a bug in the audio decoder not handling "
          << "end of stream AVPackets correctly.";

      DLOG(WARNING)
          << "Failed to decode an audio frame with timestamp: "
          << buffer->timestamp().InMicroseconds() << " us, duration: "
          << buffer->duration().InMicroseconds() << " us, packet size: "
          << buffer->data_size() << " bytes";

      break;
    }

    // Update packet size and data pointer in case we need to call the decoder
    // with the remaining bytes from this packet.
    packet.size -= result;
    packet.data += result;

    if (output_timestamp_helper_->base_timestamp() == kNoTimestamp() &&
        !buffer->end_of_stream()) {
      DCHECK(buffer->timestamp() != kNoTimestamp());
      if (output_frames_to_drop_ > 0) {
        // Currently Vorbis is the only codec that causes us to drop samples.
        // If we have to drop samples it always means the timeline starts at 0.
        DCHECK_EQ(codec_context_->codec_id, AV_CODEC_ID_VORBIS);
        output_timestamp_helper_->SetBaseTimestamp(base::TimeDelta());
      } else {
        output_timestamp_helper_->SetBaseTimestamp(buffer->timestamp());
      }
    }

    scoped_refptr<AudioBuffer> output;
    int decoded_frames = 0;
    int original_frames = 0;
    int channels = DetermineChannels(av_frame_.get());
    if (frame_decoded) {

      // TODO(rileya) Remove this check once we properly support midstream audio
      // config changes.
      if (av_frame_->sample_rate != config_.samples_per_second() ||
          channels != channels_ ||
          av_frame_->format != av_sample_format_) {
        DLOG(ERROR) << "Unsupported midstream configuration change!"
                    << " Sample Rate: " << av_frame_->sample_rate << " vs "
                    << samples_per_second_
                    << ", Channels: " << channels << " vs "
                    << channels_
                    << ", Sample Format: " << av_frame_->format << " vs "
                    << av_sample_format_;

        // This is an unrecoverable error, so bail out.
        queued_audio_.clear();
        av_frame_unref(av_frame_.get());
        return false;
      }

      // Get the AudioBuffer that the data was decoded into. Adjust the number
      // of frames, in case fewer than requested were actually decoded.
      output = reinterpret_cast<AudioBuffer*>(
          av_buffer_get_opaque(av_frame_->buf[0]));

      DCHECK_EQ(channels_, output->channel_count());
      original_frames = av_frame_->nb_samples;
      int unread_frames = output->frame_count() - original_frames;
      DCHECK_GE(unread_frames, 0);
      if (unread_frames > 0)
        output->TrimEnd(unread_frames);

      // If there are frames to drop, get rid of as many as we can.
      if (output_frames_to_drop_ > 0) {
        int drop = std::min(output->frame_count(), output_frames_to_drop_);
        output->TrimStart(drop);
        output_frames_to_drop_ -= drop;
      }

      decoded_frames = output->frame_count();
      av_frame_unref(av_frame_.get());
    }

    // WARNING: |av_frame_| no longer has valid data at this point.

    if (decoded_frames > 0) {
      // Set the timestamp/duration once all the extra frames have been
      // discarded.
      output->set_timestamp(output_timestamp_helper_->GetTimestamp());
      output->set_duration(
          output_timestamp_helper_->GetFrameDuration(decoded_frames));
      output_timestamp_helper_->AddFrames(decoded_frames);
    } else if (IsEndOfStream(result, original_frames, buffer)) {
      DCHECK_EQ(packet.size, 0);
      output = AudioBuffer::CreateEOSBuffer();
    } else {
      // In case all the frames in the buffer were dropped.
      output = NULL;
    }

    if (output.get())
      queued_audio_.push_back(output);

  } while (packet.size > 0);

  return true;
}

void FFmpegAudioDecoder::ReleaseFFmpegResources() {
  codec_context_.reset();
  av_frame_.reset();
}

bool FFmpegAudioDecoder::ConfigureDecoder() {
  if (!config_.IsValidConfig()) {
    DLOG(ERROR) << "Invalid audio stream -"
                << " codec: " << config_.codec()
                << " channel layout: " << config_.channel_layout()
                << " bits per channel: " << config_.bits_per_channel()
                << " samples per second: " << config_.samples_per_second();
    return false;
  }

  if (config_.is_encrypted()) {
    DLOG(ERROR) << "Encrypted audio stream not supported";
    return false;
  }

  // TODO(rileya) Remove this check once we properly support midstream audio
  // config changes.
  if (codec_context_.get() &&
      (bytes_per_channel_ != config_.bytes_per_channel() ||
       channel_layout_ != config_.channel_layout() ||
       samples_per_second_ != config_.samples_per_second())) {
    DVLOG(1) << "Unsupported config change :";
    DVLOG(1) << "\tbytes_per_channel : " << bytes_per_channel_
             << " -> " << config_.bytes_per_channel();
    DVLOG(1) << "\tchannel_layout : " << channel_layout_
             << " -> " << config_.channel_layout();
    DVLOG(1) << "\tsample_rate : " << samples_per_second_
             << " -> " << config_.samples_per_second();
    return false;
  }

  // Release existing decoder resources if necessary.
  ReleaseFFmpegResources();

  // Initialize AVCodecContext structure.
  codec_context_.reset(avcodec_alloc_context3(NULL));
  AudioDecoderConfigToAVCodecContext(config_, codec_context_.get());

  codec_context_->opaque = this;
  codec_context_->get_buffer2 = GetAudioBuffer;
  codec_context_->refcounted_frames = 1;

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open2(codec_context_.get(), codec, NULL) < 0) {
    DLOG(ERROR) << "Could not initialize audio decoder: "
                << codec_context_->codec_id;
    ReleaseFFmpegResources();
    state_ = kUninitialized;
    return false;
  }

  // Success!
  av_frame_.reset(av_frame_alloc());
  channel_layout_ = config_.channel_layout();
  samples_per_second_ = config_.samples_per_second();
  output_timestamp_helper_.reset(
      new AudioTimestampHelper(config_.samples_per_second()));

  // Store initial values to guard against midstream configuration changes.
  channels_ = codec_context_->channels;
  if (channels_ != ChannelLayoutToChannelCount(channel_layout_)) {
    DLOG(ERROR) << "Audio configuration specified "
                << ChannelLayoutToChannelCount(channel_layout_)
                << " channels, but FFmpeg thinks the file contains "
                << channels_ << " channels";
    ReleaseFFmpegResources();
    state_ = kUninitialized;
    return false;
  }
  av_sample_format_ = codec_context_->sample_fmt;
  sample_format_ = AVSampleFormatToSampleFormat(
      static_cast<AVSampleFormat>(av_sample_format_));
  bytes_per_channel_ = SampleFormatToBytesPerChannel(sample_format_);

  return true;
}

void FFmpegAudioDecoder::ResetTimestampState() {
  output_timestamp_helper_->SetBaseTimestamp(kNoTimestamp());
  last_input_timestamp_ = kNoTimestamp();
  output_frames_to_drop_ = 0;
}

}  // namespace media
