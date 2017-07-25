// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_stream_typemap_traits.h"

#include "base/logging.h"

namespace mojo {

// static
content::mojom::MediaStreamType
EnumTraits<content::mojom::MediaStreamType, content::MediaStreamType>::ToMojom(
    content::MediaStreamType type) {
  switch (type) {
    case content::MediaStreamType::MEDIA_NO_SERVICE:
      return content::mojom::MediaStreamType::MEDIA_NO_SERVICE;
    case content::MediaStreamType::MEDIA_DEVICE_AUDIO_CAPTURE:
      return content::mojom::MediaStreamType::MEDIA_DEVICE_AUDIO_CAPTURE;
    case content::MediaStreamType::MEDIA_DEVICE_VIDEO_CAPTURE:
      return content::mojom::MediaStreamType::MEDIA_DEVICE_VIDEO_CAPTURE;
    case content::MediaStreamType::MEDIA_TAB_AUDIO_CAPTURE:
      return content::mojom::MediaStreamType::MEDIA_TAB_AUDIO_CAPTURE;
    case content::MediaStreamType::MEDIA_TAB_VIDEO_CAPTURE:
      return content::mojom::MediaStreamType::MEDIA_TAB_VIDEO_CAPTURE;
    case content::MediaStreamType::MEDIA_DESKTOP_VIDEO_CAPTURE:
      return content::mojom::MediaStreamType::MEDIA_DESKTOP_VIDEO_CAPTURE;
    case content::MediaStreamType::MEDIA_DESKTOP_AUDIO_CAPTURE:
      return content::mojom::MediaStreamType::MEDIA_DESKTOP_AUDIO_CAPTURE;
    case content::MediaStreamType::NUM_MEDIA_TYPES:
      return content::mojom::MediaStreamType::NUM_MEDIA_TYPES;
  }
  NOTREACHED();
  return content::mojom::MediaStreamType::MEDIA_NO_SERVICE;
}

// static
bool EnumTraits<content::mojom::MediaStreamType, content::MediaStreamType>::
    FromMojom(content::mojom::MediaStreamType input,
              content::MediaStreamType* out) {
  switch (input) {
    case content::mojom::MediaStreamType::MEDIA_NO_SERVICE:
      *out = content::MediaStreamType::MEDIA_NO_SERVICE;
      return true;
    case content::mojom::MediaStreamType::MEDIA_DEVICE_AUDIO_CAPTURE:
      *out = content::MediaStreamType::MEDIA_DEVICE_AUDIO_CAPTURE;
      return true;
    case content::mojom::MediaStreamType::MEDIA_DEVICE_VIDEO_CAPTURE:
      *out = content::MediaStreamType::MEDIA_DEVICE_VIDEO_CAPTURE;
      return true;
    case content::mojom::MediaStreamType::MEDIA_TAB_AUDIO_CAPTURE:
      *out = content::MediaStreamType::MEDIA_TAB_AUDIO_CAPTURE;
      return true;
    case content::mojom::MediaStreamType::MEDIA_TAB_VIDEO_CAPTURE:
      *out = content::MediaStreamType::MEDIA_TAB_VIDEO_CAPTURE;
      return true;
    case content::mojom::MediaStreamType::MEDIA_DESKTOP_VIDEO_CAPTURE:
      *out = content::MediaStreamType::MEDIA_DESKTOP_VIDEO_CAPTURE;
      return true;
    case content::mojom::MediaStreamType::MEDIA_DESKTOP_AUDIO_CAPTURE:
      *out = content::MediaStreamType::MEDIA_DESKTOP_AUDIO_CAPTURE;
      return true;
    case content::mojom::MediaStreamType::NUM_MEDIA_TYPES:
      *out = content::MediaStreamType::NUM_MEDIA_TYPES;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
