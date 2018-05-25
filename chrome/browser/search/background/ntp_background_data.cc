// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_background_data.h"

CollectionInfo::CollectionInfo() = default;
CollectionInfo::CollectionInfo(const CollectionInfo&) = default;
CollectionInfo::CollectionInfo(CollectionInfo&&) = default;
CollectionInfo::~CollectionInfo() = default;

CollectionInfo& CollectionInfo::operator=(const CollectionInfo&) = default;
CollectionInfo& CollectionInfo::operator=(CollectionInfo&&) = default;

bool operator==(const CollectionInfo& lhs, const CollectionInfo& rhs) {
  return lhs.collection_id == rhs.collection_id &&
         lhs.collection_name == rhs.collection_name &&
         lhs.preview_image_url == rhs.preview_image_url;
}

bool operator!=(const CollectionInfo& lhs, const CollectionInfo& rhs) {
  return !(lhs == rhs);
}

CollectionInfo CollectionInfo::CreateFromProto(
    const ntp::background::Collection& collection) {
  CollectionInfo collection_info;
  collection_info.collection_id = collection.collection_id();
  collection_info.collection_name = collection.collection_name();
  // Use the first preview image as the representative one for the collection.
  if (collection.preview_size() > 0 && collection.preview(0).has_image_url()) {
    collection_info.preview_image_url = GURL(collection.preview(0).image_url());
  }

  return collection_info;
}
