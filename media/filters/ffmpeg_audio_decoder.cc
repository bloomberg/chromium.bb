// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_audio_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/pipeline.h"
#include "media/base/sample_format.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

// Helper structure for managing multiple decoded audio frames per packet.
struct QueuedAudioBuffer {
  AudioDecoder::Status status;
  scoped_refptr<AudioBuffer> buffer;
};

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

FFmpegAudioDecoder::FFmpegAudioDecoder(
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : message_loop_(message_loop),
      weak_factory_(this),
      demuxer_stream_(NULL),
      codec_context_(NULL),
      bits_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_NONE),
      channels_(0),
      samples_per_second_(0),
      av_sample_format_(0),
      last_input_timestamp_(kNoTimestamp()),
      output_frames_to_drop_(0),
      av_frame_(NULL) {
}

void FFmpegAudioDecoder::Initialize(
    DemuxerStream* stream,
    const PipelineStatusCB& status_cb,
    const StatisticsCB& statistics_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  PipelineStatusCB initialize_cb = BindToCurrentLoop(status_cb);

  FFmpegGlue::InitializeFFmpeg();

  if (demuxer_stream_) {
    // TODO(scherkus): initialization currently happens more than once in
    // PipelineIntegrationTest.BasicPlayback.
    LOG(ERROR) << "Initialize has already been called.";
    CHECK(false);
  }

  weak_this_ = weak_factory_.GetWeakPtr();
  demuxer_stream_ = stream;

  if (!ConfigureDecoder()) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  statistics_cb_ = statistics_cb;
  initialize_cb.Run(PIPELINE_OK);
}

void FFmpegAudioDecoder::Read(const ReadCB& read_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!read_cb.is_null());
  CHECK(read_cb_.is_null()) << "Overlapping decodes are not supported.";

  read_cb_ = BindToCurrentLoop(read_cb);

  // If we don't have any queued audio from the last packet we decoded, ask for
  // more data from the demuxer to satisfy this read.
  if (queued_audio_.empty()) {
    ReadFromDemuxerStream();
    return;
  }

  base::ResetAndReturn(&read_cb_).Run(
      queued_audio_.front().status, queued_audio_.front().buffer);
  queued_audio_.pop_front();
}

int FFmpegAudioDecoder::bits_per_channel() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return bits_per_channel_;
}

ChannelLayout FFmpegAudioDecoder::channel_layout() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return channel_layout_;
}

int FFmpegAudioDecoder::samples_per_second() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return samples_per_second_;
}

void FFmpegAudioDecoder::Reset(const base::Closure& closure) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::Closure reset_cb = BindToCurrentLoop(closure);

  avcodec_flush_buffers(codec_context_);
  ResetTimestampState();
  queued_audio_.clear();
  reset_cb.Run();
}

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
  // TODO(scherkus): should we require Stop() to be called? this might end up
  // getting called on a random thread due to refcounting.
  ReleaseFFmpegResources();
}

void FFmpegAudioDecoder::ReadFromDemuxerStream() {
  DCHECK(!read_cb_.is_null());
  demuxer_stream_->Read(base::Bind(
      &FFmpegAudioDecoder::BufferReady, weak_this_));
}

void FFmpegAudioDecoder::BufferReady(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& input) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!read_cb_.is_null());
  DCHECK(queued_audio_.empty());
  DCHECK_EQ(status != DemuxerStream::kOk, !input.get()) << status;

  if (status == DemuxerStream::kAborted) {
    DCHECK(!input.get());
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    return;
  }

  if (status == DemuxerStream::kConfigChanged) {
    DCHECK(!input.get());

    // Send a "end of stream" buffer to the decode loop
    // to output any remaining data still in the decoder.
    RunDecodeLoop(DecoderBuffer::CreateEOSBuffer(), true);

    DVLOG(1) << "Config changed.";

    if (!ConfigureDecoder()) {
      base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
      return;
    }

    ResetTimestampState();

    if (queued_audio_.empty()) {
      ReadFromDemuxerStream();
      return;
    }

    base::ResetAndReturn(&read_cb_).Run(
        queued_audio_.front().status, queued_audio_.front().buffer);
    queued_audio_.pop_front();
    return;
  }

  DCHECK_EQ(status, DemuxerStream::kOk);
  DCHECK(input.get());

  // Make sure we are notified if http://crbug.com/49709 returns.  Issue also
  // occurs with some damaged files.
  if (!input->end_of_stream() && input->timestamp() == kNoTimestamp() &&
      output_timestamp_helper_->base_timestamp() == kNoTimestamp()) {
    DVLOG(1) << "Received a buffer without timestamps!";
    base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
    return;
  }

  bool is_vorbis = codec_context_->codec_id == AV_CODEC_ID_VORBIS;
  if (!input->end_of_stream()) {
    if (last_input_timestamp_ == kNoTimestamp()) {
      if (is_vorbis && (input->timestamp() < base::TimeDelta())) {
        // Dropping frames for negative timestamps as outlined in section A.2
        // in the Vorbis spec. http://xiph.org/vorbis/doc/Vorbis_I_spec.html
        output_frames_to_drop_ = floor(
            0.5 + -input->timestamp().InSecondsF() * samples_per_second_);
      } else {
        last_input_timestamp_ = input->timestamp();
      }
    } else if (input->timestamp() != kNoTimestamp()) {
      if (input->timestamp() < last_input_timestamp_) {
        base::TimeDelta diff = input->timestamp() - last_input_timestamp_;
        DVLOG(1) << "Input timestamps are not monotonically increasing! "
                 << " ts " << input->timestamp().InMicroseconds() << " us"
                 << " diff " << diff.InMicroseconds() << " us";
        base::ResetAndReturn(&read_cb_).Run(kDecodeError, NULL);
        return;
      }

      last_input_timestamp_ = input->timestamp();
    }
  }

  RunDecodeLoop(input, false);

  // We exhausted the provided packet, but it wasn't enough for a frame.  Ask
  // for more data in order to fulfill this read.
  if (queued_audio_.empty()) {
    ReadFromDemuxerStream();
    return;
  }

  // Execute callback to return the first frame we decoded.
  base::ResetAndReturn(&read_cb_).Run(
      queued_audio_.front().status, queued_audio_.front().buffer);
  queued_audio_.pop_front();
}

bool FFmpegAudioDecoder::ConfigureDecoder() {
  const AudioDecoderConfig& config = demuxer_stream_->audio_decoder_config();

  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid audio stream -"
                << " codec: " << config.codec()
                << " channel layout: " << config.channel_layout()
                << " bits per channel: " << config.bits_per_channel()
                << " samples per second: " << config.samples_per_second();
    return false;
  }

  if (config.is_encrypted()) {
    DLOG(ERROR) << "Encrypted audio stream not supported";
    return false;
  }

  if (codec_context_ &&
      (bits_per_channel_ != config.bits_per_channel() ||
       channel_layout_ != config.channel_layout() ||
       samples_per_second_ != config.samples_per_second())) {
    DVLOG(1) << "Unsupported config change :";
    DVLOG(1) << "\tbits_per_channel : " << bits_per_channel_
             << " -> " << config.bits_per_channel();
    DVLOG(1) << "\tchannel_layout : " << channel_layout_
             << " -> " << config.channel_layout();
    DVLOG(1) << "\tsample_rate : " << samples_per_second_
             << " -> " << config.samples_per_second();
    return false;
  }

  // Release existing decoder resources if necessary.
  ReleaseFFmpegResources();

  // Initialize AVCodecContext structure.
  codec_context_ = avcodec_alloc_context3(NULL);
  AudioDecoderConfigToAVCodecContext(config, codec_context_);

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open2(codec_context_, codec, NULL) < 0) {
    DLOG(ERROR) << "Could not initialize audio decoder: "
                << codec_context_->codec_id;
    return false;
  }

  // Success!
  av_frame_ = avcodec_alloc_frame();
  channel_layout_ = config.channel_layout();
  samples_per_second_ = config.samples_per_second();
  output_timestamp_helper_.reset(
      new AudioTimestampHelper(config.samples_per_second()));

  // Store initial values to guard against midstream configuration changes.
  channels_ = codec_context_->channels;
  av_sample_format_ = codec_context_->sample_fmt;
  sample_format_ = AVSampleFormatToSampleFormat(
      static_cast<AVSampleFormat>(av_sample_format_));
  bits_per_channel_ = SampleFormatToBytesPerChannel(sample_format_) * 8;

  return true;
}

void FFmpegAudioDecoder::ReleaseFFmpegResources() {
  if (codec_context_) {
    av_free(codec_context_->extradata);
    avcodec_close(codec_context_);
    av_free(codec_context_);
  }

  if (av_frame_) {
    av_free(av_frame_);
    av_frame_ = NULL;
  }
}

void FFmpegAudioDecoder::ResetTimestampState() {
  output_timestamp_helper_->SetBaseTimestamp(kNoTimestamp());
  last_input_timestamp_ = kNoTimestamp();
  output_frames_to_drop_ = 0;
}

void FFmpegAudioDecoder::RunDecodeLoop(
    const scoped_refptr<DecoderBuffer>& input,
    bool skip_eos_append) {
  AVPacket packet;
  av_init_packet(&packet);
  if (input->end_of_stream()) {
    packet.data = NULL;
    packet.size = 0;
  } else {
    packet.data = const_cast<uint8*>(input->data());
    packet.size = input->data_size();
  }

  // Each audio packet may contain several frames, so we must call the decoder
  // until we've exhausted the packet.  Regardless of the packet size we always
  // want to hand it to the decoder at least once, otherwise we would end up
  // skipping end of stream packets since they have a size of zero.
  do {
    // Reset frame to default values.
    avcodec_get_frame_defaults(av_frame_);

    int frame_decoded = 0;
    int result = avcodec_decode_audio4(
        codec_context_, av_frame_, &frame_decoded, &packet);

    if (result < 0) {
      DCHECK(!input->end_of_stream())
          << "End of stream buffer produced an error! "
          << "This is quite possibly a bug in the audio decoder not handling "
          << "end of stream AVPackets correctly.";

      DLOG(ERROR)
          << "Error decoding an audio frame with timestamp: "
          << input->timestamp().InMicroseconds() << " us, duration: "
          << input->duration().InMicroseconds() << " us, packet size: "
          << input->data_size() << " bytes";

      // TODO(dalecurtis): We should return a kDecodeError here instead:
      // http://crbug.com/145276
      break;
    }

    // Update packet size and data pointer in case we need to call the decoder
    // with the remaining bytes from this packet.
    packet.size -= result;
    packet.data += result;

    if (output_timestamp_helper_->base_timestamp() == kNoTimestamp() &&
        !input->end_of_stream()) {
      DCHECK(input->timestamp() != kNoTimestamp());
      if (output_frames_to_drop_ > 0) {
        // Currently Vorbis is the only codec that causes us to drop samples.
        // If we have to drop samples it always means the timeline starts at 0.
        DCHECK_EQ(codec_context_->codec_id, AV_CODEC_ID_VORBIS);
        output_timestamp_helper_->SetBaseTimestamp(base::TimeDelta());
      } else {
        output_timestamp_helper_->SetBaseTimestamp(input->timestamp());
      }
    }

    int decoded_frames = 0;
#ifdef CHROMIUM_NO_AVFRAME_CHANNELS
    int channels = av_get_channel_layout_nb_channels(
        av_frame_->channel_layout);
#else
    int channels = av_frame_->channels;
#endif
    if (frame_decoded) {
      if (av_frame_->sample_rate != samples_per_second_ ||
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
        QueuedAudioBuffer queue_entry = { kDecodeError, NULL };
        queued_audio_.push_back(queue_entry);
        break;
      }
      decoded_frames = av_frame_->nb_samples;
    }

    int frames_to_skip = 0;
    if (decoded_frames > 0 && output_frames_to_drop_ > 0) {
      frames_to_skip = std::min(decoded_frames, output_frames_to_drop_);
      output_frames_to_drop_ -= frames_to_skip;
    }

    scoped_refptr<AudioBuffer> output;
    if (frames_to_skip < decoded_frames) {
      DCHECK_EQ(sample_format_,
                AVSampleFormatToSampleFormat(
                    static_cast<AVSampleFormat>(av_frame_->format)));
      base::TimeDelta start_time = output_timestamp_helper_->GetTimestamp();
      output = AudioBuffer::CopyFrom(
          sample_format_,
          channels_,
          decoded_frames,
          av_frame_->extended_data,
          start_time,
          output_timestamp_helper_->GetFrameDuration(decoded_frames));
      if (frames_to_skip > 0) {
        output->TrimStart(frames_to_skip);
        // Reset the timestamp to the correct value since the previous frames
        // are to be ignored, not skipped. Duration will have been adjusted
        // correctly.
        output->set_timestamp(start_time);
      }
      output_timestamp_helper_->AddFrames(decoded_frames - frames_to_skip);
    } else if (IsEndOfStream(result, decoded_frames, input) &&
               !skip_eos_append) {
      DCHECK_EQ(packet.size, 0);
      output = AudioBuffer::CreateEOSBuffer();
    }

    if (output.get()) {
      QueuedAudioBuffer queue_entry = { kOk, output };
      queued_audio_.push_back(queue_entry);
    }

    // Decoding finished successfully, update statistics.
    if (result > 0) {
      PipelineStatistics statistics;
      statistics.audio_bytes_decoded = result;
      statistics_cb_.Run(statistics);
    }
  } while (packet.size > 0);
}

}  // namespace media
