// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model.h"

#include "base/logging.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_metadata_store.h"
#include "url/gurl.h"

namespace offline_pages {

OfflinePageModel::OfflinePageModel(scoped_ptr<OfflinePageMetadataStore> store)
    : store_(store.Pass()) {
}

OfflinePageModel::~OfflinePageModel() {
}

void OfflinePageModel::Shutdown() {
  NOTIMPLEMENTED();
}

void OfflinePageModel::SavePageOffline(const GURL& url) {
  NOTIMPLEMENTED();
}

std::vector<OfflinePageItem> OfflinePageModel::GetAllOfflinePages() {
  NOTIMPLEMENTED();
  return std::vector<OfflinePageItem>();
}

OfflinePageMetadataStore* OfflinePageModel::GetStoreForTesting() {
  return store_.get();
}

}  // namespace offline_pages
