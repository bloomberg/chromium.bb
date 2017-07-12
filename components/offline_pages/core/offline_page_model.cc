// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_model.h"

#include "url/gurl.h"

namespace offline_pages {

const int64_t OfflinePageModel::kInvalidOfflineId;

OfflinePageModel::SavePageParams::SavePageParams()
    : proposed_offline_id(OfflinePageModel::kInvalidOfflineId),
      is_background(false) {}

OfflinePageModel::SavePageParams::SavePageParams(const SavePageParams& other) {
  url = other.url;
  client_id = other.client_id;
  proposed_offline_id = other.proposed_offline_id;
  original_url = other.original_url;
  is_background = other.is_background;
  request_origin = other.request_origin;
}

OfflinePageModel::SavePageParams::~SavePageParams() {}

OfflinePageModel::DeletedPageInfo::DeletedPageInfo(
    int64_t offline_id,
    const ClientId& client_id,
    const std::string& request_origin)
    : offline_id(offline_id),
      client_id(client_id),
      request_origin(request_origin) {}

// static
bool OfflinePageModel::CanSaveURL(const GURL& url) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

OfflinePageModel::OfflinePageModel() {}

OfflinePageModel::~OfflinePageModel() {}

}  // namespace offline_pages
