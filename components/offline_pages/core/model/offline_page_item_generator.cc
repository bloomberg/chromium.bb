// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_item_generator.h"

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/offline_pages/core/model/offline_store_utils.h"
#include "url/gurl.h"

namespace offline_pages {

OfflinePageItemGenerator::OfflinePageItemGenerator() = default;

OfflinePageItemGenerator::~OfflinePageItemGenerator() {}

OfflinePageItem OfflinePageItemGenerator::CreateItem() {
  OfflinePageItem item;
  item.offline_id = OfflineStoreUtils::GenerateOfflineId();
  item.client_id.name_space = namespace_;
  item.client_id.id = base::Int64ToString(item.offline_id);
  item.request_origin = request_origin_;
  item.url = url_;
  item.original_url = original_url_;
  return item;
}

void OfflinePageItemGenerator::SetNamespace(const std::string& name_space) {
  namespace_ = name_space;
}

void OfflinePageItemGenerator::SetRequestOrigin(
    const std::string& request_origin) {
  request_origin_ = request_origin;
}

void OfflinePageItemGenerator::SetUrl(const GURL& url) {
  url_ = url;
}

void OfflinePageItemGenerator::SetOriginalUrl(const GURL& url) {
  original_url_ = url;
}

}  // namespace offline_pages
