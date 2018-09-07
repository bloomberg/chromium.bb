// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/offline_item_model.h"

OfflineItemModel::OfflineItemModel(
    const offline_items_collection::OfflineItem& offline_item)
    : was_ui_notified_(false) {}

OfflineItemModel::~OfflineItemModel() = default;
