// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/offline_item_model.h"

#include "chrome/browser/download/offline_item_model_manager.h"

OfflineItemModel::OfflineItemModel(
    OfflineItemModelManager* manager,
    const offline_items_collection::OfflineItem& offline_item)
    : manager_(manager), offline_item_(offline_item) {}

OfflineItemModel::~OfflineItemModel() = default;

int64_t OfflineItemModel::GetCompletedBytes() const {
  return offline_item_.received_bytes;
}

int64_t OfflineItemModel::GetTotalBytes() const {
  return offline_item_.total_size_bytes > 0 ? offline_item_.total_size_bytes
                                            : 0;
}

int OfflineItemModel::PercentComplete() const {
  if (GetTotalBytes() <= 0)
    return -1;

  return static_cast<int>(GetCompletedBytes() * 100.0 / GetTotalBytes());
}

bool OfflineItemModel::WasUINotified() const {
  const OfflineItemModelData* data =
      manager_->GetOrCreateOfflineItemModelData(offline_item_.id);
  return data->was_ui_notified_;
}

void OfflineItemModel::SetWasUINotified(bool was_ui_notified) {
  OfflineItemModelData* data =
      manager_->GetOrCreateOfflineItemModelData(offline_item_.id);
  data->was_ui_notified_ = was_ui_notified;
}
