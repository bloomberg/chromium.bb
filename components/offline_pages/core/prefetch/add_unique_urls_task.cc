// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/add_unique_urls_task.h"

#include "base/bind.h"

namespace offline_pages {

AddUniqueUrlsTask::AddUniqueUrlsTask(
    PrefetchStore* store,
    const std::vector<PrefetchURL>& prefetch_urls)
    : prefetch_store_(store),
      prefetch_urls_(prefetch_urls),
      weak_ptr_factory_(this) {}

AddUniqueUrlsTask::~AddUniqueUrlsTask() {}

void AddUniqueUrlsTask::Run() {
  CHECK(prefetch_store_);
  TaskComplete();
}

}  // namespace offline_pages
