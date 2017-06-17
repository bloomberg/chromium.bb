// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/add_unique_urls_task.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {

// Adds new prefetch item entries to the store using the URLs and client IDs
// from |prefetch_urls| and the client's |name_space|. Also cleans up entries in
// the Zombie state from the client's |name_space| except for the ones
// whose URL is contained in |prefetch_urls|.
// Returns the number of added prefecth items.
static int AddUrlsAndCleanupZombies(
    const std::string& name_space,
    const std::vector<PrefetchURL>& prefetch_urls) {
  NOTIMPLEMENTED();
  return 1;
}

// TODO(fgorski): replace this with the SQL executor.
static void Execute(base::RepeatingCallback<int()> command_callback,
                    base::OnceCallback<void(int)> result_callback) {
  std::move(result_callback).Run(command_callback.Run());
}
}

AddUniqueUrlsTask::AddUniqueUrlsTask(
    const std::string& name_space,
    const std::vector<PrefetchURL>& prefetch_urls)
    : name_space_(name_space),
      prefetch_urls_(prefetch_urls),
      weak_ptr_factory_(this) {}

AddUniqueUrlsTask::~AddUniqueUrlsTask() {}

void AddUniqueUrlsTask::Run() {
  Execute(base::BindRepeating(&AddUrlsAndCleanupZombies, name_space_,
                              prefetch_urls_),
          base::BindOnce(&AddUniqueUrlsTask::OnUrlsAdded,
                         weak_ptr_factory_.GetWeakPtr()));
}

void AddUniqueUrlsTask::OnUrlsAdded(int added_entry_count) {
  // TODO(carlosk): schedule NWake here if at least one new entry was added to
  // the store.
  TaskComplete();
}

}  // namespace offline_pages
