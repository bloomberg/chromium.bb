// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model.h"

#include "base/logging.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_metadata_store.h"
#include "url/gurl.h"

namespace offline_pages {

OfflinePageModel::OfflinePageModel(scoped_ptr<OfflinePageMetadataStore> store,
                                   OfflinePageArchiver* archiver)
    : store_(store.Pass()),
      archiver_(archiver) {
  DCHECK(archiver);
}

OfflinePageModel::~OfflinePageModel() {
}

void OfflinePageModel::Shutdown() {
}

void OfflinePageModel::SavePageOffline(const GURL& url) {
}

void OfflinePageModel::OnCreateArchiveDone(
    OfflinePageArchiver::Request* request,
    OfflinePageArchiver::ArchiverResult result,
    const base::FilePath& file_path) {
  // TODO(fgorski): Match request against one of the expected requests
  // TODO(fgorski): Create an entry in the offline pages metadata store for that
  //                request.
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
