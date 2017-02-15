// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/fetch_request.h"

namespace content {

BackgroundFetchDataManager::BackgroundFetchDataManager(
    BackgroundFetchContext* background_fetch_context)
    : background_fetch_context_(background_fetch_context) {
  DCHECK(background_fetch_context_);
  // TODO(harkness) Read from persistent storage and recreate requests.
}

BackgroundFetchDataManager::~BackgroundFetchDataManager() {}

void BackgroundFetchDataManager::CreateRequest(
    const FetchRequest& fetch_request) {
  FetchIdentifier id(fetch_request.service_worker_registration_id(),
                     fetch_request.tag());
  if (fetch_map_.find(id) != fetch_map_.end()) {
    DLOG(ERROR) << "Origin " << fetch_request.origin()
                << " has already created a fetch request with tag "
                << fetch_request.tag();
    // TODO(harkness) Figure out how to return errors like this.
    return;
  }
  fetch_map_[id] = fetch_request;
}

}  // namespace content
