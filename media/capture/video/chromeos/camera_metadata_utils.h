// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/mojo/arc_camera3_metadata.mojom.h"

namespace media {

const arc::mojom::CameraMetadataEntryPtr* GetMetadataEntry(
    const arc::mojom::CameraMetadataPtr& camera_metadata,
    arc::mojom::CameraMetadataTag tag);

void MergeMetadata(arc::mojom::CameraMetadataPtr* to,
                   const arc::mojom::CameraMetadataPtr& from);

}  // namespace media
