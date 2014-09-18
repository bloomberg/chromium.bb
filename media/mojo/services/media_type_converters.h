// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_TYPE_CONVERTERS_H_
#define MEDIA_MOJO_SERVICES_MEDIA_TYPE_CONVERTERS_H_

#include "base/memory/ref_counted.h"
#include "media/mojo/interfaces/media_types.mojom.h"

namespace media {
class AudioDecoderConfig;
class DecoderBuffer;
}

namespace mojo {

template <>
struct TypeConverter<MediaDecoderBufferPtr,
                     scoped_refptr<media::DecoderBuffer> > {
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

}  // namespace mojo

#endif  // MEDIA_MOJO_SERVICES_MEDIA_TYPE_CONVERTERS_H_
