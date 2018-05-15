// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_METADATA_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_METADATA_UTILS_H_

#include "media/base/media_export.h"
#include "media/capture/video/chromeos/mojo/camera_metadata.mojom.h"

namespace media {

cros::mojom::CameraMetadataEntryPtr* GetMetadataEntry(
    const cros::mojom::CameraMetadataPtr& camera_metadata,
    cros::mojom::CameraMetadataTag tag);

// Sort the camera metadata entries using the metadata tags.
void SortCameraMetadata(cros::mojom::CameraMetadataPtr* camera_metadata);

void MergeMetadata(cros::mojom::CameraMetadataPtr* to,
                   const cros::mojom::CameraMetadataPtr& from);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_METADATA_UTILS_H_
