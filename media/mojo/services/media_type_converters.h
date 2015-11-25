// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_TYPE_CONVERTERS_H_
#define MEDIA_MOJO_SERVICES_MEDIA_TYPE_CONVERTERS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/interfaces/media_types.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace media {
class AudioBuffer;
class AudioDecoderConfig;
class DecoderBuffer;
class DecryptConfig;
class VideoDecoderConfig;
class VideoFrame;
struct CdmConfig;
struct CdmKeyInformation;
struct SubsampleEntry;
}

// These are specializations of mojo::TypeConverter and have to be in the mojo
// namespace.
namespace mojo {

template <>
struct TypeConverter<media::interfaces::SubsampleEntryPtr,
                     media::SubsampleEntry> {
  static media::interfaces::SubsampleEntryPtr Convert(
      const media::SubsampleEntry& input);
};
template <>
struct TypeConverter<media::SubsampleEntry,
                     media::interfaces::SubsampleEntryPtr> {
  static media::SubsampleEntry Convert(
      const media::interfaces::SubsampleEntryPtr& input);
};

template <>
struct TypeConverter<media::interfaces::DecryptConfigPtr,
                     media::DecryptConfig> {
  static media::interfaces::DecryptConfigPtr Convert(
      const media::DecryptConfig& input);
};
template <>
struct TypeConverter<scoped_ptr<media::DecryptConfig>,
                     media::interfaces::DecryptConfigPtr> {
  static scoped_ptr<media::DecryptConfig> Convert(
      const media::interfaces::DecryptConfigPtr& input);
};

template <>
struct TypeConverter<media::interfaces::DecoderBufferPtr,
                     scoped_refptr<media::DecoderBuffer>> {
  static media::interfaces::DecoderBufferPtr Convert(
      const scoped_refptr<media::DecoderBuffer>& input);
};
template <>
struct TypeConverter<scoped_refptr<media::DecoderBuffer>,
                     media::interfaces::DecoderBufferPtr> {
  static scoped_refptr<media::DecoderBuffer> Convert(
      const media::interfaces::DecoderBufferPtr& input);
};

template <>
struct TypeConverter<media::interfaces::AudioDecoderConfigPtr,
                     media::AudioDecoderConfig> {
  static media::interfaces::AudioDecoderConfigPtr Convert(
      const media::AudioDecoderConfig& input);
};
template <>
struct TypeConverter<media::AudioDecoderConfig,
                     media::interfaces::AudioDecoderConfigPtr> {
  static media::AudioDecoderConfig Convert(
      const media::interfaces::AudioDecoderConfigPtr& input);
};

template <>
struct TypeConverter<media::interfaces::VideoDecoderConfigPtr,
                     media::VideoDecoderConfig> {
  static media::interfaces::VideoDecoderConfigPtr Convert(
      const media::VideoDecoderConfig& input);
};
template <>
struct TypeConverter<media::VideoDecoderConfig,
                     media::interfaces::VideoDecoderConfigPtr> {
  static media::VideoDecoderConfig Convert(
      const media::interfaces::VideoDecoderConfigPtr& input);
};

template <>
struct TypeConverter<media::interfaces::CdmKeyInformationPtr,
                     media::CdmKeyInformation> {
  static media::interfaces::CdmKeyInformationPtr Convert(
      const media::CdmKeyInformation& input);
};
template <>
struct TypeConverter<scoped_ptr<media::CdmKeyInformation>,
                     media::interfaces::CdmKeyInformationPtr> {
  static scoped_ptr<media::CdmKeyInformation> Convert(
      const media::interfaces::CdmKeyInformationPtr& input);
};

template <>
struct TypeConverter<media::interfaces::CdmConfigPtr, media::CdmConfig> {
  static media::interfaces::CdmConfigPtr Convert(const media::CdmConfig& input);
};
template <>
struct TypeConverter<media::CdmConfig, media::interfaces::CdmConfigPtr> {
  static media::CdmConfig Convert(const media::interfaces::CdmConfigPtr& input);
};

template <>
struct TypeConverter<media::interfaces::AudioBufferPtr,
                     scoped_refptr<media::AudioBuffer>> {
  static media::interfaces::AudioBufferPtr Convert(
      const scoped_refptr<media::AudioBuffer>& input);
};
template <>
struct TypeConverter<scoped_refptr<media::AudioBuffer>,
                     media::interfaces::AudioBufferPtr> {
  static scoped_refptr<media::AudioBuffer> Convert(
      const media::interfaces::AudioBufferPtr& input);
};

template <>
struct TypeConverter<media::interfaces::VideoFramePtr,
                     scoped_refptr<media::VideoFrame>> {
  static media::interfaces::VideoFramePtr Convert(
      const scoped_refptr<media::VideoFrame>& input);
};
template <>
struct TypeConverter<scoped_refptr<media::VideoFrame>,
                     media::interfaces::VideoFramePtr> {
  static scoped_refptr<media::VideoFrame> Convert(
      const media::interfaces::VideoFramePtr& input);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_SERVICES_MEDIA_TYPE_CONVERTERS_H_
