// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_type_converters.h"

#include "base/macros.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/mojo/interfaces/demuxer_stream.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace mojo {

#define ASSERT_ENUM_EQ(media_enum, media_prefix, mojo_prefix, value)     \
  COMPILE_ASSERT(media::media_prefix##value ==                           \
                     static_cast<media::media_enum>(mojo_prefix##value), \
                 value##_enum_value_differs)

// BufferingState.
ASSERT_ENUM_EQ(BufferingState, BUFFERING_, BUFFERING_STATE_, HAVE_NOTHING);
ASSERT_ENUM_EQ(BufferingState, BUFFERING_, BUFFERING_STATE_, HAVE_ENOUGH);

// AudioCodec.
COMPILE_ASSERT(media::kUnknownAudioCodec ==
                   static_cast<media::AudioCodec>(AUDIO_CODEC_UNKNOWN),
               kUnknownAudioCodec_enum_value_differs);
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
COMPILE_ASSERT(media::kAudioCodecMax ==
                   static_cast<media::AudioCodec>(AUDIO_CODEC_MAX),
               kAudioCodecMax_enum_value_differs);

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
ASSERT_ENUM_EQ(ChannelLayout, CHANNEL_LAYOUT, CHANNEL_LAYOUT_k, _MAX);

// SampleFormat.
COMPILE_ASSERT(media::kUnknownSampleFormat ==
                   static_cast<media::SampleFormat>(SAMPLE_FORMAT_UNKNOWN),
               kUnknownSampleFormat_enum_value_differs);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, U8);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, S16);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, S32);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, F32);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, PlanarS16);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, PlanarF32);
ASSERT_ENUM_EQ(SampleFormat, kSampleFormat, SAMPLE_FORMAT_, Max);

// DemuxerStream Type.
COMPILE_ASSERT(media::DemuxerStream::UNKNOWN ==
                   static_cast<media::DemuxerStream::Type>(
                       mojo::DemuxerStream::TYPE_UNKNOWN),
               DemuxerStream_Type_enum_value_differs);
COMPILE_ASSERT(media::DemuxerStream::AUDIO ==
                   static_cast<media::DemuxerStream::Type>(
                       mojo::DemuxerStream::TYPE_AUDIO),
               DemuxerStream_Type_enum_value_differs);
// Update this if new media::DemuxerStream::Type values are introduced.
COMPILE_ASSERT(media::DemuxerStream::NUM_TYPES ==
                   static_cast<media::DemuxerStream::Type>(
                       mojo::DemuxerStream::TYPE_LAST_TYPE + 3),
               DemuxerStream_Type_enum_value_differs);

// DemuxerStream Status.
COMPILE_ASSERT(media::DemuxerStream::kOk ==
                   static_cast<media::DemuxerStream::Status>(
                       mojo::DemuxerStream::STATUS_OK),
               DemuxerStream_Status_enum_value_differs);
COMPILE_ASSERT(media::DemuxerStream::kAborted ==
                   static_cast<media::DemuxerStream::Status>(
                       mojo::DemuxerStream::STATUS_ABORTED),
               DemuxerStream_Status_enum_value_differs);
COMPILE_ASSERT(media::DemuxerStream::kConfigChanged ==
                   static_cast<media::DemuxerStream::Status>(
                       mojo::DemuxerStream::STATUS_CONFIG_CHANGED),
               DemuxerStream_Status_enum_value_differs);

// static
MediaDecoderBufferPtr TypeConverter<MediaDecoderBufferPtr,
    scoped_refptr<media::DecoderBuffer> >::Convert(
        const scoped_refptr<media::DecoderBuffer>& input) {
  MediaDecoderBufferPtr mojo_buffer(MediaDecoderBuffer::New());
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
  mojo::AudioDecoderConfigPtr config(mojo::AudioDecoderConfig::New());
  config->codec = static_cast<mojo::AudioCodec>(input.codec());
  config->sample_format =
      static_cast<mojo::SampleFormat>(input.sample_format());
  config->channel_layout =
      static_cast<mojo::ChannelLayout>(input.channel_layout());
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
  config.Initialize(static_cast<media::AudioCodec>(input->codec),
                    static_cast<media::SampleFormat>(input->sample_format),
                    static_cast<media::ChannelLayout>(input->channel_layout),
                    input->samples_per_second,
                    &input->extra_data.front(),
                    input->extra_data.size(),
                    false,
                    false,
                    base::TimeDelta::FromMicroseconds(input->seek_preroll_usec),
                    input->codec_delay);
  return config;
}

}  // namespace mojo
