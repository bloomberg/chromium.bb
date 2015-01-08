// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_TYPE_CONVERTERS_H_
#define MEDIA_MOJO_SERVICES_MEDIA_TYPE_CONVERTERS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/interfaces/media_types.mojom.h"

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
class DecoderBuffer;
class DecryptConfig;
struct SubsampleEntry;
struct CdmKeyInformation;
}

namespace mojo {

template <>
struct TypeConverter<SubsampleEntryPtr, media::SubsampleEntry> {
  static SubsampleEntryPtr Convert(const media::SubsampleEntry& input);
};
template <>
struct TypeConverter<media::SubsampleEntry, SubsampleEntryPtr> {
  static media::SubsampleEntry Convert(const SubsampleEntryPtr& input);
};

template <>
struct TypeConverter<DecryptConfigPtr, media::DecryptConfig> {
  static DecryptConfigPtr Convert(const media::DecryptConfig& input);
};
template <>
struct TypeConverter<scoped_ptr<media::DecryptConfig>, DecryptConfigPtr> {
  static scoped_ptr<media::DecryptConfig> Convert(
      const DecryptConfigPtr& input);
};

template <>
struct TypeConverter<MediaDecoderBufferPtr,
                     scoped_refptr<media::DecoderBuffer>> {
  static MediaDecoderBufferPtr Convert(
      const scoped_refptr<media::DecoderBuffer>& input);
};
template<>
struct TypeConverter<scoped_refptr<media::DecoderBuffer>,
                     MediaDecoderBufferPtr> {
  static scoped_refptr<media::DecoderBuffer> Convert(
      const MediaDecoderBufferPtr& input);
};

template <>
struct TypeConverter<AudioDecoderConfigPtr, media::AudioDecoderConfig> {
  static AudioDecoderConfigPtr Convert(const media::AudioDecoderConfig& input);
};
template <>
struct TypeConverter<media::AudioDecoderConfig, AudioDecoderConfigPtr> {
  static media::AudioDecoderConfig Convert(const AudioDecoderConfigPtr& input);
};

template <>
struct TypeConverter<VideoDecoderConfigPtr, media::VideoDecoderConfig> {
  static VideoDecoderConfigPtr Convert(const media::VideoDecoderConfig& input);
};
template <>
struct TypeConverter<media::VideoDecoderConfig, VideoDecoderConfigPtr> {
  static media::VideoDecoderConfig Convert(const VideoDecoderConfigPtr& input);
};

template <>
struct TypeConverter<CdmKeyInformationPtr, media::CdmKeyInformation> {
  static CdmKeyInformationPtr Convert(const media::CdmKeyInformation& input);
};
template <>
struct TypeConverter<scoped_ptr<media::CdmKeyInformation>,
                     CdmKeyInformationPtr> {
  static scoped_ptr<media::CdmKeyInformation> Convert(
      const CdmKeyInformationPtr& input);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_SERVICES_MEDIA_TYPE_CONVERTERS_H_
