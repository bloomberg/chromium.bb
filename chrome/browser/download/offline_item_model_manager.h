// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_MANAGER_H_
#define CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_MANAGER_H_

#include <memory>

#include "chrome/browser/download/offline_item_model.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_items_collection/core/offline_item.h"

using ContentId = offline_items_collection::ContentId;
using OfflineItem = offline_items_collection::OfflineItem;

// Class for managing all the OfflineModels for a profile.
class OfflineItemModelManager : public KeyedService {
 public:
  // Constructs a OfflineItemModelManager.
  OfflineItemModelManager();
  ~OfflineItemModelManager() override;

  // Returns the OfflineItemModel for the ContentId, if not found, an empty
  // OfflineItemModel will be created and returned.
  OfflineItemModel* GetOrCreateOfflineItemModel(
      const OfflineItem& offline_item);

  void RemoveOfflineItemModel(const ContentId& id);

 private:
  std::map<ContentId, std::unique_ptr<OfflineItemModel>> offline_item_models_;

  DISALLOW_COPY_AND_ASSIGN(OfflineItemModelManager);
};

#endif  // CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_MANAGER_H_
