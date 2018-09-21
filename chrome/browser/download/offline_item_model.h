// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_
#define CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_

#include "chrome/browser/download/download_ui_model.h"
#include "components/offline_items_collection/core/offline_item.h"

class OfflineItemModelManager;

// Implementation of DownloadUIModel that wrappers around a |OfflineItem|.
class OfflineItemModel : public DownloadUIModel {
 public:
  // Constructs a OfflineItemModel.
  OfflineItemModel(OfflineItemModelManager* manager,
                   const offline_items_collection::OfflineItem& offline_item);
  ~OfflineItemModel() override;

  // DownloadUIModel implementation.
  int64_t GetCompletedBytes() const override;
  int64_t GetTotalBytes() const override;
  int PercentComplete() const override;
  bool WasUINotified() const override;
  void SetWasUINotified(bool should_notify) override;

 private:
  OfflineItemModelManager* manager_;

  offline_items_collection::OfflineItem offline_item_;

  DISALLOW_COPY_AND_ASSIGN(OfflineItemModel);
};

#endif  // CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_
