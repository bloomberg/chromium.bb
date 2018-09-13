// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_
#define CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_

#include "components/offline_items_collection/core/offline_item.h"

class OfflineItemModelManager;

// This class is an abstraction for common UI tasks and properties associated
// with an OfflineItem. This item is short lived, all the state needs be stored
// in OfflineItemModelData.
class OfflineItemModel {
 public:
  // Constructs a OfflineItemModel.
  OfflineItemModel(OfflineItemModelManager* manager,
                   const offline_items_collection::OfflineItem& offline_item);
  ~OfflineItemModel();

  // Get the number of bytes that has completed so far.
  int64_t GetCompletedBytes() const;

  // Get the total number of bytes for this download. Should return 0 if the
  // total size of the download is not known.
  int64_t GetTotalBytes() const;

  // Rough percent complete. Returns -1 if the progress is unknown.
  int PercentComplete() const;

  // Returns |true| if the UI has been notified about this download. By default,
  // this value is |false| and should be changed explicitly using
  // SetWasUINotified().
  bool WasUINotified() const;

  // Change what's returned by WasUINotified().
  void SetWasUINotified(bool should_notify);

 private:
  OfflineItemModelManager* manager_;

  offline_items_collection::OfflineItem offline_item_;

  DISALLOW_COPY_AND_ASSIGN(OfflineItemModel);
};

#endif  // CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_
