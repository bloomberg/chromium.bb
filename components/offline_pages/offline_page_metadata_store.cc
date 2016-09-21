// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_metadata_store.h"

namespace offline_pages {

StoreUpdateResult::StoreUpdateResult(StoreState state) : store_state(state) {}

StoreUpdateResult::~StoreUpdateResult() {}

OfflinePageMetadataStore::OfflinePageMetadataStore() {
}

OfflinePageMetadataStore::~OfflinePageMetadataStore() {
}

}  // namespace offline_pages
