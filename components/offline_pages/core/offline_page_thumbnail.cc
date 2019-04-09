// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_thumbnail.h"

#include <iostream>
#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/offline_pages/core/offline_store_utils.h"

namespace offline_pages {

OfflinePageThumbnail::OfflinePageThumbnail() = default;
OfflinePageThumbnail::OfflinePageThumbnail(int64_t id,
                                           base::Time in_expiration,
                                           const std::string& in_thumbnail,
                                           const std::string& in_favicon)
    : offline_id(id),
      expiration(in_expiration),
      thumbnail(in_thumbnail),
      favicon(in_favicon) {}
OfflinePageThumbnail::OfflinePageThumbnail(const OfflinePageThumbnail& other) =
    default;
OfflinePageThumbnail::OfflinePageThumbnail(OfflinePageThumbnail&& other) =
    default;
OfflinePageThumbnail::~OfflinePageThumbnail() {}

bool OfflinePageThumbnail::operator==(const OfflinePageThumbnail& other) const {
  return offline_id == other.offline_id && expiration == other.expiration &&
         thumbnail == other.thumbnail && favicon == other.favicon;
}

bool OfflinePageThumbnail::operator<(const OfflinePageThumbnail& other) const {
  return offline_id < other.offline_id;
}

}  // namespace offline_pages
