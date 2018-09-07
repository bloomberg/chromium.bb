// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/offline_item_model_manager.h"

OfflineItemModelManager::OfflineItemModelManager() = default;

OfflineItemModelManager::~OfflineItemModelManager() = default;

OfflineItemModel* OfflineItemModelManager::GetOrCreateOfflineItemModel(
    const OfflineItem& offline_item) {
  auto it = offline_item_models_.find(offline_item.id);
  if (it != offline_item_models_.end())
    return it->second.get();
  offline_item_models_[offline_item.id] =
      std::make_unique<OfflineItemModel>(offline_item);
  return offline_item_models_[offline_item.id].get();
}

void OfflineItemModelManager::RemoveOfflineItemModel(const ContentId& id) {
  offline_item_models_.erase(id);
}
