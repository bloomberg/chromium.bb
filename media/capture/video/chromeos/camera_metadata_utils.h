// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_METADATA_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_METADATA_UTILS_H_

#include "media/capture/video/chromeos/mojo/camera_metadata.mojom.h"

namespace media {

const arc::mojom::CameraMetadataEntryPtr* GetMetadataEntry(
    const arc::mojom::CameraMetadataPtr& camera_metadata,
    arc::mojom::CameraMetadataTag tag);

void MergeMetadata(arc::mojom::CameraMetadataPtr* to,
                   const arc::mojom::CameraMetadataPtr& from);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_METADATA_UTILS_H_
