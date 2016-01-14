// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_type_converters.h"

#include <stddef.h>
#include <stdint.h>

#include "base/numerics/safe_conversions.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_key_information.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_keys.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/mojo/interfaces/demuxer_stream.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"

namespace mojo {

#define ASSERT_ENUM_EQ(media_enum, media_prefix, mojo_prefix, value) \
  static_assert(media::media_prefix##value ==                        \
                    static_cast<media::media_enum>(                  \
                        media::interfaces::mojo_prefix##value),      \
                "Mismatched enum: " #media_prefix #value             \
                " != " #mojo_prefix #value)

#define ASSERT_ENUM_EQ_RAW(media_enum, media_enum_value, mojo_enum_value)     \
  static_assert(                                                              \
      media::media_enum_value ==                                              \
          static_cast<media::media_enum>(media::interfaces::mojo_enum_value), \
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
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, EAC3);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, PCM_ALAW);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, ALAC);
ASSERT_ENUM_EQ(AudioCodec, kCodec, AUDIO_CODEC_, AC3);
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
ASSERT_ENUM_EQ_RAW(VideoPixelFormat,
                   PIXEL_FORMAT_UNKNOWN,
                   VIDEO_FORMAT_UNKNOWN);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_I420, VIDEO_FORMAT_I420);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_YV12, VIDEO_FORMAT_YV12);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_YV16, VIDEO_FORMAT_YV16);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_YV12A, VIDEO_FORMAT_YV12A);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_YV24, VIDEO_FORMAT_YV24);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_NV12, VIDEO_FORMAT_NV12);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_NV21, VIDEO_FORMAT_NV21);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_UYVY, VIDEO_FORMAT_UYVY);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_YUY2, VIDEO_FORMAT_YUY2);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_ARGB, VIDEO_FORMAT_ARGB);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_XRGB, VIDEO_FORMAT_XRGB);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_RGB24, VIDEO_FORMAT_RGB24);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_RGB32, VIDEO_FORMAT_RGB32);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_MJPEG, VIDEO_FORMAT_MJPEG);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_MT21, VIDEO_FORMAT_MT21);
ASSERT_ENUM_EQ_RAW(VideoPixelFormat, PIXEL_FORMAT_MAX, VIDEO_FORMAT_FORMAT_MAX);

// ColorSpace.
ASSERT_ENUM_EQ_RAW(ColorSpace,
                   COLOR_SPACE_UNSPECIFIED,
                   COLOR_SPACE_UNSPECIFIED);
ASSERT_ENUM_EQ_RAW(ColorSpace, COLOR_SPACE_JPEG, COLOR_SPACE_JPEG);
ASSERT_ENUM_EQ_RAW(ColorSpace, COLOR_SPACE_HD_REC709, COLOR_SPACE_HD_REC709);
ASSERT_ENUM_EQ_RAW(ColorSpace, COLOR_SPACE_SD_REC601, COLOR_SPACE_SD_REC601);
ASSERT_ENUM_EQ_RAW(ColorSpace, COLOR_SPACE_MAX, COLOR_SPACE_MAX);

// VideoCodec
ASSERT_ENUM_EQ_RAW(VideoCodec, kUnknownVideoCodec, VIDEO_CODEC_UNKNOWN);
ASSERT_ENUM_EQ(VideoCodec, kCodec, VIDEO_CODEC_, H264);
ASSERT_ENUM_EQ(VideoCodec, kCodec, VIDEO_CODEC_, HEVC);
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

// Decryptor Status
ASSERT_ENUM_EQ_RAW(Decryptor::Status,
                   Decryptor::kSuccess,
                   Decryptor::STATUS_SUCCESS);
ASSERT_ENUM_EQ_RAW(Decryptor::Status,
                   Decryptor::kNoKey,
                   Decryptor::STATUS_NO_KEY);
ASSERT_ENUM_EQ_RAW(Decryptor::Status,
                   Decryptor::kNeedMoreData,
                   Decryptor::STATUS_NEED_MORE_DATA);
ASSERT_ENUM_EQ_RAW(Decryptor::Status,
                   Decryptor::kError,
                   Decryptor::STATUS_ERROR);

// CdmException
#define ASSERT_CDM_EXCEPTION(value)                                \
  static_assert(media::MediaKeys::value ==                         \
                    static_cast<media::MediaKeys::Exception>(      \
                        media::interfaces::CDM_EXCEPTION_##value), \
                "Mismatched CDM Exception")
ASSERT_CDM_EXCEPTION(NOT_SUPPORTED_ERROR);
ASSERT_CDM_EXCEPTION(INVALID_STATE_ERROR);
ASSERT_CDM_EXCEPTION(INVALID_ACCESS_ERROR);
ASSERT_CDM_EXCEPTION(QUOTA_EXCEEDED_ERROR);
ASSERT_CDM_EXCEPTION(UNKNOWN_ERROR);
ASSERT_CDM_EXCEPTION(CLIENT_ERROR);
ASSERT_CDM_EXCEPTION(OUTPUT_ERROR);

// CDM Session Type
#define ASSERT_CDM_SESSION_TYPE(value)                               \
  static_assert(media::MediaKeys::value ==                           \
                    static_cast<media::MediaKeys::SessionType>(      \
                        media::interfaces::ContentDecryptionModule:: \
                            SESSION_TYPE_##value),                   \
                "Mismatched CDM Session Type")
ASSERT_CDM_SESSION_TYPE(TEMPORARY_SESSION);
ASSERT_CDM_SESSION_TYPE(PERSISTENT_LICENSE_SESSION);
ASSERT_CDM_SESSION_TYPE(PERSISTENT_RELEASE_MESSAGE_SESSION);

// CDM InitDataType
#define ASSERT_CDM_INIT_DATA_TYPE(value)                             \
  static_assert(media::EmeInitDataType::value ==                     \
                    static_cast<media::EmeInitDataType>(             \
                        media::interfaces::ContentDecryptionModule:: \
                            INIT_DATA_TYPE_##value),                 \
                "Mismatched CDM Init Data Type")
ASSERT_CDM_INIT_DATA_TYPE(UNKNOWN);
ASSERT_CDM_INIT_DATA_TYPE(WEBM);
ASSERT_CDM_INIT_DATA_TYPE(CENC);
ASSERT_CDM_INIT_DATA_TYPE(KEYIDS);

// CDM Key Status
#define ASSERT_CDM_KEY_STATUS(value)                                  \
  static_assert(media::CdmKeyInformation::value ==                    \
                    static_cast<media::CdmKeyInformation::KeyStatus>( \
                        media::interfaces::CDM_KEY_STATUS_##value),   \
                "Mismatched CDM Key Status")
ASSERT_CDM_KEY_STATUS(USABLE);
ASSERT_CDM_KEY_STATUS(INTERNAL_ERROR);
ASSERT_CDM_KEY_STATUS(EXPIRED);
ASSERT_CDM_KEY_STATUS(OUTPUT_RESTRICTED);
ASSERT_CDM_KEY_STATUS(OUTPUT_DOWNSCALED);
ASSERT_CDM_KEY_STATUS(KEY_STATUS_PENDING);

// CDM Message Type
#define ASSERT_CDM_MESSAGE_TYPE(value)                                \
  static_assert(media::MediaKeys::value ==                            \
                    static_cast<media::MediaKeys::MessageType>(       \
                        media::interfaces::CDM_MESSAGE_TYPE_##value), \
                "Mismatched CDM Message Type")
ASSERT_CDM_MESSAGE_TYPE(LICENSE_REQUEST);
ASSERT_CDM_MESSAGE_TYPE(LICENSE_RENEWAL);
ASSERT_CDM_MESSAGE_TYPE(LICENSE_RELEASE);

namespace {

// Copy the data for plane |plane| from |input| into the vector |dest|. This
// function only copies the actual frame data. Any padding data is skipped.
void CopyPlaneDataToVector(const scoped_refptr<media::VideoFrame>& input,
                           size_t plane,
                           std::vector<uint8_t>* dest) {
  DCHECK(dest->empty());
  uint8_t* source = input->data(plane);
  size_t num_rows = input->rows(plane);
  size_t stride = input->stride(plane);
  size_t row_bytes = input->row_bytes(plane);
  DCHECK_GE(stride, row_bytes);

  // Copy |row_bytes| for each row, but increment by |stride| to point at the
  // subsequent row.
  dest->reserve(num_rows * row_bytes);
  for (size_t i = 0; i < num_rows; ++i) {
    dest->insert(dest->end(), source, source + row_bytes);
    source += stride;
  }
}

// Copy the data from |input| into the plane |plane| of |frame|. If there is
// padding in |frame|, it is unchanged.
void CopyPlaneData(const std::vector<uint8_t>& input,
                   size_t plane,
                   const scoped_refptr<media::VideoFrame>& frame) {
  const uint8_t* source = input.data();
  uint8_t* dest = frame->data(plane);
  size_t num_rows = frame->rows(plane);
  size_t stride = frame->stride(plane);
  size_t row_bytes = frame->row_bytes(plane);
  DCHECK_GE(stride, row_bytes);
  DCHECK_EQ(input.size(), num_rows * row_bytes);

  // Copy |row_bytes| for each row. |input| contains only the data bytes, so
  // |source| is only incremented by |row_bytes|. |dest| may contain padding,
  // so increment by |stride| to point at the subsequent row.
  for (size_t i = 0; i < num_rows; ++i) {
    memcpy(dest, source, row_bytes);
    source += row_bytes;
    dest += stride;
  }
}

}  // namespace

// static
media::interfaces::SubsampleEntryPtr TypeConverter<
    media::interfaces::SubsampleEntryPtr,
    media::SubsampleEntry>::Convert(const media::SubsampleEntry& input) {
  media::interfaces::SubsampleEntryPtr mojo_subsample_entry(
      media::interfaces::SubsampleEntry::New());
  mojo_subsample_entry->clear_bytes = input.clear_bytes;
  mojo_subsample_entry->cypher_bytes = input.cypher_bytes;
  return mojo_subsample_entry;
}

// static
media::SubsampleEntry
TypeConverter<media::SubsampleEntry, media::interfaces::SubsampleEntryPtr>::
    Convert(const media::interfaces::SubsampleEntryPtr& input) {
  return media::SubsampleEntry(input->clear_bytes, input->cypher_bytes);
}

// static
media::interfaces::DecryptConfigPtr TypeConverter<
    media::interfaces::DecryptConfigPtr,
    media::DecryptConfig>::Convert(const media::DecryptConfig& input) {
  media::interfaces::DecryptConfigPtr mojo_decrypt_config(
      media::interfaces::DecryptConfig::New());
  mojo_decrypt_config->key_id = input.key_id();
  mojo_decrypt_config->iv = input.iv();
  mojo_decrypt_config->subsamples =
      Array<media::interfaces::SubsampleEntryPtr>::From(input.subsamples());
  return mojo_decrypt_config;
}

// static
scoped_ptr<media::DecryptConfig>
TypeConverter<scoped_ptr<media::DecryptConfig>,
              media::interfaces::DecryptConfigPtr>::
    Convert(const media::interfaces::DecryptConfigPtr& input) {
  return make_scoped_ptr(new media::DecryptConfig(
      input->key_id, input->iv,
      input->subsamples.To<std::vector<media::SubsampleEntry>>()));
}

// static
media::interfaces::DecoderBufferPtr
TypeConverter<media::interfaces::DecoderBufferPtr,
              scoped_refptr<media::DecoderBuffer>>::
    Convert(const scoped_refptr<media::DecoderBuffer>& input) {
  DCHECK(input);

  media::interfaces::DecoderBufferPtr mojo_buffer(
      media::interfaces::DecoderBuffer::New());
  if (input->end_of_stream())
    return mojo_buffer;

  mojo_buffer->timestamp_usec = input->timestamp().InMicroseconds();
  mojo_buffer->duration_usec = input->duration().InMicroseconds();
  mojo_buffer->is_key_frame = input->is_key_frame();
  mojo_buffer->data_size = base::checked_cast<uint32_t>(input->data_size());
  mojo_buffer->side_data_size =
      base::checked_cast<uint32_t>(input->side_data_size());
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

  if (input->decrypt_config()) {
    mojo_buffer->decrypt_config =
        media::interfaces::DecryptConfig::From(*input->decrypt_config());
  }

  // TODO(dalecurtis): We intentionally do not serialize the data section of
  // the DecoderBuffer here; this must instead be done by clients via their
  // own DataPipe.  See http://crbug.com/432960

  return mojo_buffer;
}

// static
scoped_refptr<media::DecoderBuffer>
TypeConverter<scoped_refptr<media::DecoderBuffer>,
              media::interfaces::DecoderBufferPtr>::
    Convert(const media::interfaces::DecoderBufferPtr& input) {
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
media::interfaces::AudioDecoderConfigPtr TypeConverter<
    media::interfaces::AudioDecoderConfigPtr,
    media::AudioDecoderConfig>::Convert(const media::AudioDecoderConfig&
                                            input) {
  media::interfaces::AudioDecoderConfigPtr config(
      media::interfaces::AudioDecoderConfig::New());
  config->codec = static_cast<media::interfaces::AudioCodec>(input.codec());
  config->sample_format =
      static_cast<media::interfaces::SampleFormat>(input.sample_format());
  config->channel_layout =
      static_cast<media::interfaces::ChannelLayout>(input.channel_layout());
  config->samples_per_second = input.samples_per_second();
  if (!input.extra_data().empty()) {
    std::vector<uint8_t> extra_data = input.extra_data();
    config->extra_data.Swap(&extra_data);
  }
  config->seek_preroll_usec = input.seek_preroll().InMicroseconds();
  config->codec_delay = input.codec_delay();
  config->is_encrypted = input.is_encrypted();
  return config;
}

// static
media::AudioDecoderConfig
TypeConverter<media::AudioDecoderConfig,
              media::interfaces::AudioDecoderConfigPtr>::
    Convert(const media::interfaces::AudioDecoderConfigPtr& input) {
  media::AudioDecoderConfig config;
  config.Initialize(
      static_cast<media::AudioCodec>(input->codec),
      static_cast<media::SampleFormat>(input->sample_format),
      static_cast<media::ChannelLayout>(input->channel_layout),
      input->samples_per_second, input->extra_data, input->is_encrypted,
      base::TimeDelta::FromMicroseconds(input->seek_preroll_usec),
      input->codec_delay);
  return config;
}

// static
media::interfaces::VideoDecoderConfigPtr TypeConverter<
    media::interfaces::VideoDecoderConfigPtr,
    media::VideoDecoderConfig>::Convert(const media::VideoDecoderConfig&
                                            input) {
  media::interfaces::VideoDecoderConfigPtr config(
      media::interfaces::VideoDecoderConfig::New());
  config->codec = static_cast<media::interfaces::VideoCodec>(input.codec());
  config->profile =
      static_cast<media::interfaces::VideoCodecProfile>(input.profile());
  config->format = static_cast<media::interfaces::VideoFormat>(input.format());
  config->color_space =
      static_cast<media::interfaces::ColorSpace>(input.color_space());
  config->coded_size = Size::From(input.coded_size());
  config->visible_rect = Rect::From(input.visible_rect());
  config->natural_size = Size::From(input.natural_size());
  config->extra_data = mojo::Array<uint8_t>::From(input.extra_data());
  config->is_encrypted = input.is_encrypted();
  return config;
}

// static
media::VideoDecoderConfig
TypeConverter<media::VideoDecoderConfig,
              media::interfaces::VideoDecoderConfigPtr>::
    Convert(const media::interfaces::VideoDecoderConfigPtr& input) {
  media::VideoDecoderConfig config;
  config.Initialize(
      static_cast<media::VideoCodec>(input->codec),
      static_cast<media::VideoCodecProfile>(input->profile),
      static_cast<media::VideoPixelFormat>(input->format),
      static_cast<media::ColorSpace>(input->color_space),
      input->coded_size.To<gfx::Size>(), input->visible_rect.To<gfx::Rect>(),
      input->natural_size.To<gfx::Size>(), input->extra_data,
      input->is_encrypted);
  return config;
}

// static
media::interfaces::CdmKeyInformationPtr TypeConverter<
    media::interfaces::CdmKeyInformationPtr,
    media::CdmKeyInformation>::Convert(const media::CdmKeyInformation& input) {
  media::interfaces::CdmKeyInformationPtr info(
      media::interfaces::CdmKeyInformation::New());
  std::vector<uint8_t> key_id_copy(input.key_id);
  info->key_id.Swap(&key_id_copy);
  info->status = static_cast<media::interfaces::CdmKeyStatus>(input.status);
  info->system_code = input.system_code;
  return info;
}

// static
scoped_ptr<media::CdmKeyInformation>
TypeConverter<scoped_ptr<media::CdmKeyInformation>,
              media::interfaces::CdmKeyInformationPtr>::
    Convert(const media::interfaces::CdmKeyInformationPtr& input) {
  return make_scoped_ptr(new media::CdmKeyInformation(
      input->key_id.storage(),
      static_cast<media::CdmKeyInformation::KeyStatus>(input->status),
      input->system_code));
}

// static
media::interfaces::CdmConfigPtr
TypeConverter<media::interfaces::CdmConfigPtr, media::CdmConfig>::Convert(
    const media::CdmConfig& input) {
  media::interfaces::CdmConfigPtr config(media::interfaces::CdmConfig::New());
  config->allow_distinctive_identifier = input.allow_distinctive_identifier;
  config->allow_persistent_state = input.allow_persistent_state;
  config->use_hw_secure_codecs = input.use_hw_secure_codecs;
  return config;
}

// static
media::CdmConfig
TypeConverter<media::CdmConfig, media::interfaces::CdmConfigPtr>::Convert(
    const media::interfaces::CdmConfigPtr& input) {
  media::CdmConfig config;
  config.allow_distinctive_identifier = input->allow_distinctive_identifier;
  config.allow_persistent_state = input->allow_persistent_state;
  config.use_hw_secure_codecs = input->use_hw_secure_codecs;
  return config;
}

// static
media::interfaces::AudioBufferPtr
TypeConverter<media::interfaces::AudioBufferPtr,
              scoped_refptr<media::AudioBuffer>>::
    Convert(const scoped_refptr<media::AudioBuffer>& input) {
  media::interfaces::AudioBufferPtr buffer(
      media::interfaces::AudioBuffer::New());
  buffer->sample_format =
      static_cast<media::interfaces::SampleFormat>(input->sample_format_);
  buffer->channel_layout =
      static_cast<media::interfaces::ChannelLayout>(input->channel_layout());
  buffer->channel_count = input->channel_count();
  buffer->sample_rate = input->sample_rate();
  buffer->frame_count = input->frame_count();
  buffer->end_of_stream = input->end_of_stream();
  buffer->timestamp_usec = input->timestamp().InMicroseconds();

  if (!input->end_of_stream()) {
    std::vector<uint8_t> input_data(input->data_.get(),
                                    input->data_.get() + input->data_size_);
    buffer->data.Swap(&input_data);
  }

  return buffer;
}

// static
scoped_refptr<media::AudioBuffer>
TypeConverter<scoped_refptr<media::AudioBuffer>,
              media::interfaces::AudioBufferPtr>::
    Convert(const media::interfaces::AudioBufferPtr& input) {
  if (input->end_of_stream)
    return media::AudioBuffer::CreateEOSBuffer();

  // Setup channel pointers.  AudioBuffer::CopyFrom() will only use the first
  // one in the case of interleaved data.
  std::vector<const uint8_t*> channel_ptrs(input->channel_count, nullptr);
  std::vector<uint8_t> storage = input->data.storage();
  const size_t size_per_channel = storage.size() / input->channel_count;
  DCHECK_EQ(0u, storage.size() % input->channel_count);
  for (int i = 0; i < input->channel_count; ++i)
    channel_ptrs[i] = storage.data() + i * size_per_channel;

  return media::AudioBuffer::CopyFrom(
      static_cast<media::SampleFormat>(input->sample_format),
      static_cast<media::ChannelLayout>(input->channel_layout),
      input->channel_count, input->sample_rate, input->frame_count,
      &channel_ptrs[0],
      base::TimeDelta::FromMicroseconds(input->timestamp_usec));
}

// static
media::interfaces::VideoFramePtr
TypeConverter<media::interfaces::VideoFramePtr,
              scoped_refptr<media::VideoFrame>>::
    Convert(const scoped_refptr<media::VideoFrame>& input) {
  media::interfaces::VideoFramePtr buffer(media::interfaces::VideoFrame::New());
  buffer->end_of_stream =
      input->metadata()->IsTrue(media::VideoFrameMetadata::END_OF_STREAM);
  if (buffer->end_of_stream)
    return buffer;

  // handle non EOS buffer.
  buffer->format = static_cast<media::interfaces::VideoFormat>(input->format());
  buffer->coded_size = Size::From(input->coded_size());
  buffer->visible_rect = Rect::From(input->visible_rect());
  buffer->natural_size = Size::From(input->natural_size());
  buffer->timestamp_usec = input->timestamp().InMicroseconds();

  if (!input->coded_size().IsEmpty()) {
    // TODO(jrummell): Use a shared buffer rather than copying the data for
    // each plane.
    std::vector<uint8_t> y_data;
    CopyPlaneDataToVector(input, media::VideoFrame::kYPlane, &y_data);
    buffer->y_data.Swap(&y_data);

    std::vector<uint8_t> u_data;
    CopyPlaneDataToVector(input, media::VideoFrame::kUPlane, &u_data);
    buffer->u_data.Swap(&u_data);

    std::vector<uint8_t> v_data;
    CopyPlaneDataToVector(input, media::VideoFrame::kVPlane, &v_data);
    buffer->v_data.Swap(&v_data);
  }

  return buffer;
}

// static
scoped_refptr<media::VideoFrame>
TypeConverter<scoped_refptr<media::VideoFrame>,
              media::interfaces::VideoFramePtr>::
    Convert(const media::interfaces::VideoFramePtr& input) {
  if (input->end_of_stream)
    return media::VideoFrame::CreateEOSFrame();

  scoped_refptr<media::VideoFrame> frame = media::VideoFrame::CreateFrame(
      static_cast<media::VideoPixelFormat>(input->format),
      input->coded_size.To<gfx::Size>(), input->visible_rect.To<gfx::Rect>(),
      input->natural_size.To<gfx::Size>(),
      base::TimeDelta::FromMicroseconds(input->timestamp_usec));
  CopyPlaneData(input->y_data.storage(), media::VideoFrame::kYPlane, frame);
  CopyPlaneData(input->u_data.storage(), media::VideoFrame::kUPlane, frame);
  CopyPlaneData(input->v_data.storage(), media::VideoFrame::kVPlane, frame);

  return frame;
}

}  // namespace mojo
