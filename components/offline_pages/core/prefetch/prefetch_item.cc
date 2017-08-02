// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_item.h"

namespace offline_pages {

PrefetchItem::PrefetchItem() = default;

PrefetchItem::PrefetchItem(const PrefetchItem& other) = default;

PrefetchItem::PrefetchItem(PrefetchItem&& other) = default;

PrefetchItem::~PrefetchItem() = default;

PrefetchItem& PrefetchItem::operator=(const PrefetchItem& other) = default;

PrefetchItem& PrefetchItem::operator=(PrefetchItem&& other) = default;

bool PrefetchItem::operator==(const PrefetchItem& other) const {
  return offline_id == other.offline_id && guid == other.guid &&
         client_id == other.client_id && state == other.state &&
         url == other.url && final_archived_url == other.final_archived_url &&
         generate_bundle_attempts == other.generate_bundle_attempts &&
         get_operation_attempts == other.get_operation_attempts &&
         download_initiation_attempts == other.download_initiation_attempts &&
         operation_name == other.operation_name &&
         archive_body_name == other.archive_body_name &&
         archive_body_length == other.archive_body_length &&
         creation_time == other.creation_time &&
         freshness_time == other.freshness_time &&
         error_code == other.error_code;
}

bool PrefetchItem::operator!=(const PrefetchItem& other) const {
  return !(*this == other);
}

bool PrefetchItem::operator<(const PrefetchItem& other) const {
  return offline_id < other.offline_id;
}

}  // namespace offline_pages
