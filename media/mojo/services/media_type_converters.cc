// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_type_converters.h"

#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/cdm_key_information.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_keys.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/interfaces/demuxer_stream.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"

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
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, ALAC);
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
ASSERT_ENUM_EQ_RAW(VideoFrame::Format, VideoFrame::ARGB, VIDEO_FORMAT_ARGB);
ASSERT_ENUM_EQ_RAW(VideoFrame::Format, VideoFrame::YV12HD, VIDEO_FORMAT_YV12HD);
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

// CdmException
#define ASSERT_CDM_EXCEPTION(value)                                        \
  static_assert(                                                           \
      media::MediaKeys::value ==                                           \
          static_cast<media::MediaKeys::Exception>(CDM_EXCEPTION_##value), \
      "Mismatched CDM Exception")
ASSERT_CDM_EXCEPTION(NOT_SUPPORTED_ERROR);
ASSERT_CDM_EXCEPTION(INVALID_STATE_ERROR);
ASSERT_CDM_EXCEPTION(INVALID_ACCESS_ERROR);
ASSERT_CDM_EXCEPTION(QUOTA_EXCEEDED_ERROR);
ASSERT_CDM_EXCEPTION(UNKNOWN_ERROR);
ASSERT_CDM_EXCEPTION(CLIENT_ERROR);
ASSERT_CDM_EXCEPTION(OUTPUT_ERROR);

// CDM Session Type
#define ASSERT_CDM_SESSION_TYPE(value)                                  \
  static_assert(media::MediaKeys::value ==                              \
                    static_cast<media::MediaKeys::SessionType>(         \
                        ContentDecryptionModule::SESSION_TYPE_##value), \
                "Mismatched CDM Session Type")
ASSERT_CDM_SESSION_TYPE(TEMPORARY_SESSION);
ASSERT_CDM_SESSION_TYPE(PERSISTENT_LICENSE_SESSION);
ASSERT_CDM_SESSION_TYPE(PERSISTENT_RELEASE_MESSAGE_SESSION);

// CDM Key Status
#define ASSERT_CDM_KEY_STATUS(value)                                  \
  static_assert(media::CdmKeyInformation::value ==                    \
                    static_cast<media::CdmKeyInformation::KeyStatus>( \
                        CDM_KEY_STATUS_##value),                      \
                "Mismatched CDM Key Status")
ASSERT_CDM_KEY_STATUS(USABLE);
ASSERT_CDM_KEY_STATUS(INTERNAL_ERROR);
ASSERT_CDM_KEY_STATUS(EXPIRED);
ASSERT_CDM_KEY_STATUS(OUTPUT_NOT_ALLOWED);

// CDM Message Type
#define ASSERT_CDM_MESSAGE_TYPE(value)                                       \
  static_assert(                                                             \
      media::MediaKeys::value == static_cast<media::MediaKeys::MessageType>( \
                                     CDM_MESSAGE_TYPE_##value),              \
      "Mismatched CDM Message Type")
ASSERT_CDM_MESSAGE_TYPE(LICENSE_REQUEST);
ASSERT_CDM_MESSAGE_TYPE(LICENSE_RENEWAL);
ASSERT_CDM_MESSAGE_TYPE(LICENSE_RELEASE);

// static
SubsampleEntryPtr
TypeConverter<SubsampleEntryPtr, media::SubsampleEntry>::Convert(
    const media::SubsampleEntry& input) {
  SubsampleEntryPtr mojo_subsample_entry(SubsampleEntry::New());
  mojo_subsample_entry->clear_bytes = input.clear_bytes;
  mojo_subsample_entry->cypher_bytes = input.cypher_bytes;
  return mojo_subsample_entry.Pass();
}

// static
media::SubsampleEntry
TypeConverter<media::SubsampleEntry, SubsampleEntryPtr>::Convert(
    const SubsampleEntryPtr& input) {
  return media::SubsampleEntry(input->clear_bytes, input->cypher_bytes);
}

// static
DecryptConfigPtr TypeConverter<DecryptConfigPtr, media::DecryptConfig>::Convert(
    const media::DecryptConfig& input) {
  DecryptConfigPtr mojo_decrypt_config(DecryptConfig::New());
  mojo_decrypt_config->key_id = input.key_id();
  mojo_decrypt_config->iv = input.iv();
  mojo_decrypt_config->subsamples =
      Array<SubsampleEntryPtr>::From(input.subsamples());
  return mojo_decrypt_config.Pass();
}

// static
scoped_ptr<media::DecryptConfig>
TypeConverter<scoped_ptr<media::DecryptConfig>, DecryptConfigPtr>::Convert(
    const DecryptConfigPtr& input) {
  return make_scoped_ptr(new media::DecryptConfig(
      input->key_id, input->iv,
      input->subsamples.To<std::vector<media::SubsampleEntry>>()));
}

// static
MediaDecoderBufferPtr TypeConverter<MediaDecoderBufferPtr,
    scoped_refptr<media::DecoderBuffer> >::Convert(
        const scoped_refptr<media::DecoderBuffer>& input) {
  DCHECK(input);

  MediaDecoderBufferPtr mojo_buffer(MediaDecoderBuffer::New());
  if (input->end_of_stream())
    return mojo_buffer.Pass();

  mojo_buffer->timestamp_usec = input->timestamp().InMicroseconds();
  mojo_buffer->duration_usec = input->duration().InMicroseconds();
  mojo_buffer->is_key_frame = input->is_key_frame();
  mojo_buffer->data_size = input->data_size();
  mojo_buffer->side_data_size = input->side_data_size();
  mojo_buffer->front_discard_usec =
      input->discard_padding().first.InMicroseconds();
  mojo_buffer->back_discard_usec =
      input->discard_padding().second.InMicroseconds();
  mojo_buffer->splice_timestamp_usec =
      input->splice_timestamp().InMicroseconds();

  // Note: The side data is always small, so this copy is okay.
  std::vector<uint8_t> side_data(input->side_data(),
                                 input->side_data() + input->side_data_size());
  mojo_buffer->side_data.Swap(&side_data);

  if (input->decrypt_config())
    mojo_buffer->decrypt_config = DecryptConfig::From(*input->decrypt_config());

  // TODO(dalecurtis): We intentionally do not serialize the data section of
  // the DecoderBuffer here; this must instead be done by clients via their
  // own DataPipe.  See http://crbug.com/432960

  return mojo_buffer.Pass();
}

// static
scoped_refptr<media::DecoderBuffer>  TypeConverter<
    scoped_refptr<media::DecoderBuffer>, MediaDecoderBufferPtr>::Convert(
        const MediaDecoderBufferPtr& input) {
  if (!input->data_size)
    return media::DecoderBuffer::CreateEOSBuffer();

  scoped_refptr<media::DecoderBuffer> buffer(
      new media::DecoderBuffer(input->data_size));
  if (input->side_data_size)
    buffer->CopySideDataFrom(&input->side_data.front(), input->side_data_size);

  buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(input->timestamp_usec));
  buffer->set_duration(
      base::TimeDelta::FromMicroseconds(input->duration_usec));

  if (input->is_key_frame)
    buffer->set_is_key_frame(true);

  if (input->decrypt_config) {
    buffer->set_decrypt_config(
        input->decrypt_config.To<scoped_ptr<media::DecryptConfig>>());
  }

  media::DecoderBuffer::DiscardPadding discard_padding(
      base::TimeDelta::FromMicroseconds(input->front_discard_usec),
      base::TimeDelta::FromMicroseconds(input->back_discard_usec));
  buffer->set_discard_padding(discard_padding);
  buffer->set_splice_timestamp(
      base::TimeDelta::FromMicroseconds(input->splice_timestamp_usec));

  // TODO(dalecurtis): We intentionally do not deserialize the data section of
  // the DecoderBuffer here; this must instead be done by clients via their
  // own DataPipe.  See http://crbug.com/432960

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
    std::vector<uint8_t> data(input.extra_data(),
                              input.extra_data() + input.extra_data_size());
    config->extra_data.Swap(&data);
  }
  config->seek_preroll_usec = input.seek_preroll().InMicroseconds();
  config->codec_delay = input.codec_delay();
  config->is_encrypted = input.is_encrypted();
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
      input->is_encrypted,
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
    std::vector<uint8_t> data(input.extra_data(),
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

// static
CdmKeyInformationPtr
TypeConverter<CdmKeyInformationPtr, media::CdmKeyInformation>::Convert(
    const media::CdmKeyInformation& input) {
  CdmKeyInformationPtr info(CdmKeyInformation::New());
  std::vector<uint8_t> key_id_copy(input.key_id);
  info->key_id.Swap(&key_id_copy);
  info->status = static_cast<CdmKeyStatus>(input.status);
  info->system_code = input.system_code;
  return info.Pass();
}

// static
scoped_ptr<media::CdmKeyInformation> TypeConverter<
    scoped_ptr<media::CdmKeyInformation>,
    CdmKeyInformationPtr>::Convert(const CdmKeyInformationPtr& input) {
  scoped_ptr<media::CdmKeyInformation> info(new media::CdmKeyInformation);
  info->key_id = input->key_id.storage();
  info->status =
      static_cast<media::CdmKeyInformation::KeyStatus>(input->status);
  info->system_code = input->system_code;
  return info.Pass();
}

}  // namespace mojo
