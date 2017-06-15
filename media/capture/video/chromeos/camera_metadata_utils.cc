// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_metadata_utils.h"

#include <set>

namespace media {

const arc::mojom::CameraMetadataEntryPtr* GetMetadataEntry(
    const arc::mojom::CameraMetadataPtr& camera_metadata,
    arc::mojom::CameraMetadataTag tag) {
  if (!camera_metadata->entries.has_value()) {
    return nullptr;
  }
  for (const auto& entry : camera_metadata->entries.value()) {
    if (entry->tag == tag) {
      return &entry;
    }
  }
  return nullptr;
}

void MergeMetadata(arc::mojom::CameraMetadataPtr* to,
                   const arc::mojom::CameraMetadataPtr& from) {
  DCHECK(to);
  (*to)->entry_count += from->entry_count;
  (*to)->entry_capacity += from->entry_count;
  (*to)->data_count += from->data_count;
  (*to)->data_capacity += from->data_count;

  if (!from->entries) {
    return;
  }

  std::set<arc::mojom::CameraMetadataTag> tags;
  if ((*to)->entries) {
    for (const auto& entry : (*to)->entries.value()) {
      tags.insert(entry->tag);
    }
  } else {
    (*to)->entries = std::vector<arc::mojom::CameraMetadataEntryPtr>();
  }
  for (const auto& entry : from->entries.value()) {
    if (tags.find(entry->tag) != tags.end()) {
      LOG(ERROR) << "Found duplicated entries for tag " << entry->tag;
      continue;
    }
    tags.insert(entry->tag);
    (*to)->entries->push_back(entry->Clone());
  }
}

}  // namespace media
