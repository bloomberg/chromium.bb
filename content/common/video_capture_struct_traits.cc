// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/video_capture_struct_traits.h"

#include "services/video_capture/public/interfaces/video_capture_device_proxy_traits.h"
#include "services/video_capture/public/interfaces/video_capture_format_traits.h"

namespace mojo {

// static
bool StructTraits<content::mojom::VideoCaptureParamsDataView,
                  media::VideoCaptureParams>::
    Read(content::mojom::VideoCaptureParamsDataView data,
         media::VideoCaptureParams* out) {
  if (!data.ReadRequestedFormat(&out->requested_format))
    return false;
  if (!data.ReadResolutionChangePolicy(&out->resolution_change_policy))
    return false;
  if (!data.ReadPowerLineFrequency(&out->power_line_frequency))
    return false;
  return true;
}

}  // namespace mojo
