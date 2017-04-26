// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediacapabilitiesclient_impl.h"

#include "media/base/mime_util.h"
#include "third_party/WebKit/public/platform/modules/media_capabilities/WebAudioConfiguration.h"
#include "third_party/WebKit/public/platform/modules/media_capabilities/WebMediaCapabilitiesInfo.h"
#include "third_party/WebKit/public/platform/modules/media_capabilities/WebMediaConfiguration.h"
#include "third_party/WebKit/public/platform/modules/media_capabilities/WebVideoConfiguration.h"

namespace media {

WebMediaCapabilitiesClientImpl::WebMediaCapabilitiesClientImpl() = default;

WebMediaCapabilitiesClientImpl::~WebMediaCapabilitiesClientImpl() = default;

void WebMediaCapabilitiesClientImpl::DecodingInfo(
    const blink::WebMediaConfiguration& configuration,
    std::unique_ptr<blink::WebMediaCapabilitiesQueryCallbacks> callbacks) {
  std::unique_ptr<blink::WebMediaCapabilitiesInfo> info(
      new blink::WebMediaCapabilitiesInfo());

  SupportsType audio_support = IsSupported;
  SupportsType video_support = IsSupported;

  if (configuration.audio_configuration) {
    const blink::WebAudioConfiguration& audio_config =
        configuration.audio_configuration.value();
    std::vector<std::string> codec_vector;
    SplitCodecsToVector(audio_config.codec.Ascii(), &codec_vector, false);

    // TODO(chcunningham): Update to throw exception pending outcome of
    // https://github.com/WICG/media-capabilities/issues/32
    DCHECK_LE(codec_vector.size(), 1U);

    audio_support =
        IsSupportedMediaFormat(audio_config.mime_type.Ascii(), codec_vector);
  }

  if (configuration.video_configuration) {
    const blink::WebVideoConfiguration& video_config =
        configuration.video_configuration.value();
    std::vector<std::string> codec_vector;
    SplitCodecsToVector(video_config.codec.Ascii(), &codec_vector, false);

    // TODO(chcunningham): Update to throw exception pending outcome of
    // https://github.com/WICG/media-capabilities/issues/32
    DCHECK_LE(codec_vector.size(), 1U);

    video_support =
        IsSupportedMediaFormat(video_config.mime_type.Ascii(), codec_vector);
  }

  // TODO(chcunningham): API should never have to mask uncertainty. Log a metric
  // for any content type that is "maybe" supported.
  if (video_support == MayBeSupported)
    video_support = IsSupported;
  if (audio_support == MayBeSupported)
    audio_support = IsSupported;

  info->supported =
      audio_support == IsSupported && video_support == IsSupported;

  // TODO(chcunningham, mlamouri): real implementation for these.
  info->smooth = info->power_efficient = info->supported;

  callbacks->OnSuccess(std::move(info));
}

}  // namespace media
