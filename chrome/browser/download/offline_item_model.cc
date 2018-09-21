// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/offline_item_model.h"

#include "chrome/browser/download/offline_item_model_manager.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"

using offline_items_collection::ContentId;
using offline_items_collection::OfflineItem;

OfflineItemModel::OfflineItemModel(OfflineItemModelManager* manager,
                                   const OfflineItem& offline_item)
    : manager_(manager),
      offline_item_(std::make_unique<OfflineItem>(offline_item)) {
  offline_items_collection::OfflineContentAggregator* aggregator =
      OfflineContentAggregatorFactory::GetForBrowserContext(
          manager_->browser_context());
  offline_item_observer_ =
      std::make_unique<FilteredOfflineItemObserver>(aggregator);
  offline_item_observer_->AddObserver(offline_item_->id, this);
}

OfflineItemModel::~OfflineItemModel() {
  if (offline_item_)
    offline_item_observer_->RemoveObserver(offline_item_->id, this);
}

int64_t OfflineItemModel::GetCompletedBytes() const {
  return offline_item_ ? offline_item_->received_bytes : 0;
}

int64_t OfflineItemModel::GetTotalBytes() const {
  if (!offline_item_)
    return 0;
  return offline_item_->total_size_bytes > 0 ? offline_item_->total_size_bytes
                                             : 0;
}

int OfflineItemModel::PercentComplete() const {
  if (GetTotalBytes() <= 0)
    return -1;

  return static_cast<int>(GetCompletedBytes() * 100.0 / GetTotalBytes());
}

bool OfflineItemModel::WasUINotified() const {
  const OfflineItemModelData* data =
      manager_->GetOrCreateOfflineItemModelData(offline_item_->id);
  return data->was_ui_notified_;
}

void OfflineItemModel::SetWasUINotified(bool was_ui_notified) {
  OfflineItemModelData* data =
      manager_->GetOrCreateOfflineItemModelData(offline_item_->id);
  data->was_ui_notified_ = was_ui_notified;
}

void OfflineItemModel::OnItemRemoved(const ContentId& id) {
  for (auto& obs : observers_)
    obs.OnDownloadDestroyed();
  offline_item_.reset();
}

void OfflineItemModel::OnItemUpdated(const OfflineItem& item) {
  offline_item_ = std::make_unique<OfflineItem>(item);
  for (auto& obs : observers_)
    obs.OnDownloadUpdated();
}
