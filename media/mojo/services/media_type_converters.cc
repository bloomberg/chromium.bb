// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_type_converters.h"

#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/interfaces/demuxer_stream.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace mojo {

#define ASSERT_ENUM_EQ(media_enum, media_prefix, mojo_prefix, value)    \
  static_assert(media::media_prefix##value ==                           \
                    static_cast<media::media_enum>(mojo_prefix##value), \
                "Mismatched enum: " #media_prefix #value                \
                " != " #mojo_prefix #value)

#define ASSERT_ENUM_EQ_RAW(media_enum, media_enum_value, mojo_enum_value) \
  static_assert(media::media_enum_value ==                                \
                    static_cast<media::media_enum>(mojo_enum_value),      \
                "Mismatched enum: " #media_enum_value " != " #mojo_enum_value)

// BufferingState.
ASSERT_ENUM_EQ(BufferingState, BUFFERING_, BUFFERING_STATE_, HAVE_NOTHING);
ASSERT_ENUM_EQ(BufferingState, BUFFERING_, BUFFERING_STATE_, HAVE_ENOUGH);

// AudioCodec.
ASSERT_ENUM_EQ_RAW(AudioCodec, kUnknownAudioCodec, AUDIO_CODEC_UNKNOWN);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, AAC);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, MP3);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, PCM);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, Vorbis);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, FLAC);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, AMR_NB);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, PCM_MULAW);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, GSM_MS);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, PCM_S16BE);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, PCM_S24BE);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, Opus);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, PCM_ALAW);
ASSERT_ENUM_EQ_RAW(AudioCodec, kAudioCodecMax, AUDIO_CODEC_MAX);

// ChannelLayout.
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _NONE);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _UNSUPPORTED);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _MONO);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _STEREO);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _2_1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _SURROUND);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _4_0);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _2_2);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _QUAD);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _5_0);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _5_1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _5_0_BACK);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _5_1_BACK);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _7_0);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _7_1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _7_1_WIDE);
ASSERT_ENUM_EQ(ChannelLayout,
               CHANNEL_LAYOUT,
               CHANNEL_LAYOUT_k,
               _STEREO_DOWNMIX);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _2POINT1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _3_1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _4_1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _6_0);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _6_0_FRONT);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _HEXAGONAL);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _6_1);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _6_1_BACK);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _6_1_FRONT);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _7_0_FRONT);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _7_1_WIDE_BACK);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _OCTAGONAL);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _DISCRETE);
ASSERT_ENUM_EQ(ChannelLayout,
               CHANNEL_LAYOUT,
               CHANNEL_LAYOUT_k,
               _STEREO_AND_KEYBOARD_MIC);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _4_1_QUAD_SIDE);
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _MAX);

// SampleFormat.
ASSERT_ENUM_EQ_RAW(SampleFormat, kUnknownSampleFormat, SAMPLE_FORMAT_UNKNOWN);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, U8);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, S16);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, S32);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, F32);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, PlanarS16);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, PlanarF32);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, Max);

// DemuxerStream Type.  Note: Mojo DemuxerStream's don't have the TEXT type.
ASSERT_ENUM_EQ_RAW(DemuxerStream::Type,
                   DemuxerStream::UNKNOWN,
                   DemuxerStream::TYPE_UNKNOWN);
ASSERT_ENUM_EQ_RAW(DemuxerStream::Type,
                   DemuxerStream::AUDIO,
                   DemuxerStream::TYPE_AUDIO);
ASSERT_ENUM_EQ_RAW(DemuxerStream::Type,
                   DemuxerStream::VIDEO,
                   DemuxerStream::TYPE_VIDEO);
ASSERT_ENUM_EQ_RAW(DemuxerStream::Type,
                   DemuxerStream::NUM_TYPES,
                   DemuxerStream::TYPE_LAST_TYPE + 2);

// DemuxerStream Status.
ASSERT_ENUM_EQ_RAW(DemuxerStream::Status,
                   DemuxerStream::kOk,
                   DemuxerStream::STATUS_OK);
ASSERT_ENUM_EQ_RAW(DemuxerStream::Status,
                   DemuxerStream::kAborted,
                   DemuxerStream::STATUS_ABORTED);
ASSERT_ENUM_EQ_RAW(DemuxerStream::Status,
                   DemuxerStream::kConfigChanged,
                   DemuxerStream::STATUS_CONFIG_CHANGED);

// VideoFormat.
ASSERT_ENUM_EQ_RAW(VideoFrame::Format,
                   VideoFrame::UNKNOWN,
                   VIDEO_FORMAT_UNKNOWN);
ASSERT_ENUM_EQ_RAW(VideoFrame::Format, VideoFrame::YV12, VIDEO_FORMAT_YV12);
ASSERT_ENUM_EQ_RAW(VideoFrame::Format, VideoFrame::YV16, VIDEO_FORMAT_YV16);
ASSERT_ENUM_EQ_RAW(VideoFrame::Format, VideoFrame::I420, VIDEO_FORMAT_I420);
ASSERT_ENUM_EQ_RAW(VideoFrame::Format, VideoFrame::YV12A, VIDEO_FORMAT_YV12A);
#if defined(VIDEO_HOLE)
ASSERT_ENUM_EQ_RAW(VideoFrame::Format, VideoFrame::HOLE, VIDEO_FORMAT_HOLE);
#endif
ASSERT_ENUM_EQ_RAW(VideoFrame::Format,
                   VideoFrame::NATIVE_TEXTURE,
                   VIDEO_FORMAT_NATIVE_TEXTURE);
ASSERT_ENUM_EQ_RAW(VideoFrame::Format, VideoFrame::YV12J, VIDEO_FORMAT_YV12J);
ASSERT_ENUM_EQ_RAW(VideoFrame::Format, VideoFrame::NV12, VIDEO_FORMAT_NV12);
ASSERT_ENUM_EQ_RAW(VideoFrame::Format, VideoFrame::YV24, VIDEO_FORMAT_YV24);
ASSERT_ENUM_EQ_RAW(VideoFrame::Format,
                   VideoFrame::FORMAT_MAX,
                   VIDEO_FORMAT_FORMAT_MAX);

// VideoCodec
ASSERT_ENUM_EQ_RAW(VideoCodec, kUnknownVideoCodec, VIDEO_CODEC_UNKNOWN);
ASSERT_ENUM_EQ(VideoCodec, kCodec, VIDEO_CODEC_, H264);
ASSERT_ENUM_EQ(VideoCodec, kCodec, VIDEO_CODEC_, VC1);
ASSERT_ENUM_EQ(VideoCodec, kCodec, VIDEO_CODEC_, MPEG2);
ASSERT_ENUM_EQ(VideoCodec, kCodec, VIDEO_CODEC_, MPEG4);
ASSERT_ENUM_EQ(VideoCodec, kCodec, VIDEO_CODEC_, Theora);
ASSERT_ENUM_EQ(VideoCodec, kCodec, VIDEO_CODEC_, VP8);
ASSERT_ENUM_EQ(VideoCodec, kCodec, VIDEO_CODEC_, VP9);
ASSERT_ENUM_EQ_RAW(VideoCodec, kVideoCodecMax, VIDEO_CODEC_Max);

// VideoCodecProfile
ASSERT_ENUM_EQ(VideoCodecProfile,
               ,
               VIDEO_CODEC_PROFILE_,
               VIDEO_CODEC_PROFILE_UNKNOWN);
ASSERT_ENUM_EQ(VideoCodecProfile,
               ,
               VIDEO_CODEC_PROFILE_,
               VIDEO_CODEC_PROFILE_MIN);
ASSERT_ENUM_EQ(VideoCodecProfile, , VIDEO_CODEC_PROFILE_, H264PROFILE_MIN);
ASSERT_ENUM_EQ(VideoCodecProfile, , VIDEO_CODEC_PROFILE_, H264PROFILE_BASELINE);
ASSERT_ENUM_EQ(VideoCodecProfile, , VIDEO_CODEC_PROFILE_, H264PROFILE_MAIN);
ASSERT_ENUM_EQ(VideoCodecProfile, , VIDEO_CODEC_PROFILE_, H264PROFILE_EXTENDED);
ASSERT_ENUM_EQ(VideoCodecProfile, , VIDEO_CODEC_PROFILE_, H264PROFILE_HIGH);
ASSERT_ENUM_EQ(VideoCodecProfile,
               ,
               VIDEO_CODEC_PROFILE_,
               H264PROFILE_HIGH10PROFILE);
ASSERT_ENUM_EQ(VideoCodecProfile,
               ,
               VIDEO_CODEC_PROFILE_,
               H264PROFILE_HIGH422PROFILE);
ASSERT_ENUM_EQ(VideoCodecProfile,
               ,
               VIDEO_CODEC_PROFILE_,
               H264PROFILE_HIGH444PREDICTIVEPROFILE);
ASSERT_ENUM_EQ(VideoCodecProfile,
               ,
               VIDEO_CODEC_PROFILE_,
               H264PROFILE_SCALABLEBASELINE);
ASSERT_ENUM_EQ(VideoCodecProfile,
               ,
               VIDEO_CODEC_PROFILE_,
               H264PROFILE_SCALABLEHIGH);
ASSERT_ENUM_EQ(VideoCodecProfile,
               ,
               VIDEO_CODEC_PROFILE_,
               H264PROFILE_STEREOHIGH);
ASSERT_ENUM_EQ(VideoCodecProfile,
               ,
               VIDEO_CODEC_PROFILE_,
               H264PROFILE_MULTIVIEWHIGH);
ASSERT_ENUM_EQ(VideoCodecProfile, , VIDEO_CODEC_PROFILE_, H264PROFILE_MAX);
ASSERT_ENUM_EQ(VideoCodecProfile, , VIDEO_CODEC_PROFILE_, VP8PROFILE_MIN);
ASSERT_ENUM_EQ(VideoCodecProfile, , VIDEO_CODEC_PROFILE_, VP8PROFILE_ANY);
ASSERT_ENUM_EQ(VideoCodecProfile, , VIDEO_CODEC_PROFILE_, VP8PROFILE_MAX);
ASSERT_ENUM_EQ(VideoCodecProfile, , VIDEO_CODEC_PROFILE_, VP9PROFILE_MIN);
ASSERT_ENUM_EQ(VideoCodecProfile, , VIDEO_CODEC_PROFILE_, VP9PROFILE_ANY);
ASSERT_ENUM_EQ(VideoCodecProfile, , VIDEO_CODEC_PROFILE_, VP9PROFILE_MAX);
ASSERT_ENUM_EQ(VideoCodecProfile,
               ,
               VIDEO_CODEC_PROFILE_,
               VIDEO_CODEC_PROFILE_MAX);

// static
MediaDecoderBufferPtr TypeConverter<MediaDecoderBufferPtr,
    scoped_refptr<media::DecoderBuffer> >::Convert(
        const scoped_refptr<media::DecoderBuffer>& input) {
  MediaDecoderBufferPtr mojo_buffer(MediaDecoderBuffer::New());
  DCHECK(!mojo_buffer->data.is_valid());

  if (input->end_of_stream())
    return mojo_buffer.Pass();

  mojo_buffer->timestamp_usec = input->timestamp().InMicroseconds();
  mojo_buffer->duration_usec = input->duration().InMicroseconds();
  mojo_buffer->data_size = input->data_size();
  mojo_buffer->side_data_size = input->side_data_size();
  mojo_buffer->front_discard_usec =
      input->discard_padding().first.InMicroseconds();
  mojo_buffer->back_discard_usec =
      input->discard_padding().second.InMicroseconds();
  mojo_buffer->splice_timestamp_usec =
      input->splice_timestamp().InMicroseconds();

  // TODO(tim): Assuming this is small so allowing extra copies.
  std::vector<uint8> side_data(input->side_data(),
                               input->side_data() + input->side_data_size());
  mojo_buffer->side_data.Swap(&side_data);

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = input->data_size();
  DataPipe data_pipe(options);
  mojo_buffer->data = data_pipe.consumer_handle.Pass();

  uint32_t num_bytes = input->data_size();
  // TODO(tim): ALL_OR_NONE isn't really appropriate. Check success?
  // If fails, we'd still return the buffer, but we'd need to HandleWatch
  // to fill the pipe at a later time, which means the de-marshalling code
  // needs to wait for a readable pipe (which it currently doesn't).
  WriteDataRaw(data_pipe.producer_handle.get(),
               input->data(),
               &num_bytes,
               MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  return mojo_buffer.Pass();
}

// static
scoped_refptr<media::DecoderBuffer>  TypeConverter<
    scoped_refptr<media::DecoderBuffer>, MediaDecoderBufferPtr>::Convert(
        const MediaDecoderBufferPtr& input) {
  if (!input->data.is_valid())
    return media::DecoderBuffer::CreateEOSBuffer();

  uint32_t num_bytes  = 0;
  // TODO(tim): We're assuming that because we always write to the pipe above
  // before sending the MediaDecoderBuffer that the pipe is readable when
  // we get here.
  ReadDataRaw(input->data.get(), NULL, &num_bytes, MOJO_READ_DATA_FLAG_QUERY);
  CHECK_EQ(num_bytes, input->data_size) << "Pipe error converting buffer";

  scoped_ptr<uint8[]> data(new uint8[num_bytes]);  // Uninitialized.
  ReadDataRaw(input->data.get(), data.get(), &num_bytes,
              MOJO_READ_DATA_FLAG_ALL_OR_NONE);
  CHECK_EQ(num_bytes, input->data_size) << "Pipe error converting buffer";

  // TODO(tim): We can't create a media::DecoderBuffer that has side_data
  // without copying data because it wants to ensure alignment. Could we
  // read directly into a pre-padded DecoderBuffer?
  scoped_refptr<media::DecoderBuffer> buffer;
  if (input->side_data_size) {
    buffer = media::DecoderBuffer::CopyFrom(data.get(),
                                            num_bytes,
                                            &input->side_data.front(),
                                            input->side_data_size);
  } else {
    buffer = media::DecoderBuffer::CopyFrom(data.get(), num_bytes);
  }

  buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(input->timestamp_usec));
  buffer->set_duration(
      base::TimeDelta::FromMicroseconds(input->duration_usec));
  media::DecoderBuffer::DiscardPadding discard_padding(
      base::TimeDelta::FromMicroseconds(input->front_discard_usec),
      base::TimeDelta::FromMicroseconds(input->back_discard_usec));
  buffer->set_discard_padding(discard_padding);
  buffer->set_splice_timestamp(
      base::TimeDelta::FromMicroseconds(input->splice_timestamp_usec));
  return buffer;
}

// static
AudioDecoderConfigPtr
TypeConverter<AudioDecoderConfigPtr, media::AudioDecoderConfig>::Convert(
    const media::AudioDecoderConfig& input) {
  AudioDecoderConfigPtr config(AudioDecoderConfig::New());
  config->codec = static_cast<AudioCodec>(input.codec());
  config->sample_format =
      static_cast<SampleFormat>(input.sample_format());
  config->channel_layout =
      static_cast<ChannelLayout>(input.channel_layout());
  config->samples_per_second = input.samples_per_second();
  if (input.extra_data()) {
    std::vector<uint8> data(input.extra_data(),
                            input.extra_data() + input.extra_data_size());
    config->extra_data.Swap(&data);
  }
  config->seek_preroll_usec = input.seek_preroll().InMicroseconds();
  config->codec_delay = input.codec_delay();
  return config.Pass();
}

// static
media::AudioDecoderConfig
TypeConverter<media::AudioDecoderConfig, AudioDecoderConfigPtr>::Convert(
    const AudioDecoderConfigPtr& input) {
  media::AudioDecoderConfig config;
  config.Initialize(
      static_cast<media::AudioCodec>(input->codec),
      static_cast<media::SampleFormat>(input->sample_format),
      static_cast<media::ChannelLayout>(input->channel_layout),
      input->samples_per_second,
      input->extra_data.size() ? &input->extra_data.front() : NULL,
      input->extra_data.size(),
      false,
      false,
      base::TimeDelta::FromMicroseconds(input->seek_preroll_usec),
      input->codec_delay);
  return config;
}

// static
VideoDecoderConfigPtr
TypeConverter<VideoDecoderConfigPtr, media::VideoDecoderConfig>::Convert(
    const media::VideoDecoderConfig& input) {
  VideoDecoderConfigPtr config(VideoDecoderConfig::New());
  config->codec = static_cast<VideoCodec>(input.codec());
  config->profile = static_cast<VideoCodecProfile>(input.profile());
  config->format = static_cast<VideoFormat>(input.format());
  config->coded_size = Size::From(input.coded_size());
  config->visible_rect = Rect::From(input.visible_rect());
  config->natural_size = Size::From(input.natural_size());
  if (input.extra_data()) {
    std::vector<uint8> data(input.extra_data(),
                            input.extra_data() + input.extra_data_size());
    config->extra_data.Swap(&data);
  }
  config->is_encrypted = input.is_encrypted();
  return config.Pass();
}

// static
media::VideoDecoderConfig
TypeConverter<media::VideoDecoderConfig, VideoDecoderConfigPtr>::Convert(
    const VideoDecoderConfigPtr& input) {
  media::VideoDecoderConfig config;
  config.Initialize(
      static_cast<media::VideoCodec>(input->codec),
      static_cast<media::VideoCodecProfile>(input->profile),
      static_cast<media::VideoFrame::Format>(input->format),
      input->coded_size.To<gfx::Size>(),
      input->visible_rect.To<gfx::Rect>(),
      input->natural_size.To<gfx::Size>(),
      input->extra_data.size() ? &input->extra_data.front() : NULL,
      input->extra_data.size(),
      input->is_encrypted,
      false);
  return config;
}

}  // namespace mojo
