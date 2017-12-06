// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_devices_typemap_traits.h"

#include "base/logging.h"

namespace mojo {

// static
blink::mojom::MediaDeviceType
EnumTraits<blink::mojom::MediaDeviceType, content::MediaDeviceType>::ToMojom(
    content::MediaDeviceType type) {
  switch (type) {
    case content::MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      return blink::mojom::MediaDeviceType::MEDIA_AUDIO_INPUT;
    case content::MediaDeviceType::MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      return blink::mojom::MediaDeviceType::MEDIA_VIDEO_INPUT;
    case content::MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT:
      return blink::mojom::MediaDeviceType::MEDIA_AUDIO_OUTPUT;
    default:
      break;
  }
  NOTREACHED();
  return blink::mojom::MediaDeviceType::NUM_MEDIA_DEVICE_TYPES;
}

// static
bool EnumTraits<blink::mojom::MediaDeviceType, content::MediaDeviceType>::
    FromMojom(blink::mojom::MediaDeviceType input,
              content::MediaDeviceType* out) {
  switch (input) {
    case blink::mojom::MediaDeviceType::MEDIA_AUDIO_INPUT:
      *out = content::MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_INPUT;
      return true;
    case blink::mojom::MediaDeviceType::MEDIA_VIDEO_INPUT:
      *out = content::MediaDeviceType::MEDIA_DEVICE_TYPE_VIDEO_INPUT;
      return true;
    case blink::mojom::MediaDeviceType::MEDIA_AUDIO_OUTPUT:
      *out = content::MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT;
      return true;
    default:
      break;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<
    blink::mojom::MediaDeviceInfoDataView,
    content::MediaDeviceInfo>::Read(blink::mojom::MediaDeviceInfoDataView input,
                                    content::MediaDeviceInfo* out) {
  if (!input.ReadDeviceId(&out->device_id))
    return false;
  if (!input.ReadLabel(&out->label))
    return false;
  if (!input.ReadGroupId(&out->group_id))
    return false;
  return true;
}

}  // namespace mojo
