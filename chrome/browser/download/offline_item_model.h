// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_
#define CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_

#include "components/offline_items_collection/core/offline_item.h"

// This class is an abstraction for common UI tasks and properties associated
// with an OfflineItem.
class OfflineItemModel {
 public:
  // Constructs a OfflineItemModel.
  explicit OfflineItemModel(
      const offline_items_collection::OfflineItem& offline_item);
  ~OfflineItemModel();

  bool was_ui_notified() const { return was_ui_notified_; }

  void set_was_ui_notified(bool was_ui_notified) {
    was_ui_notified_ = was_ui_notified;
  }

 private:
  bool was_ui_notified_;

  DISALLOW_COPY_AND_ASSIGN(OfflineItemModel);
};

#endif  // CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_
