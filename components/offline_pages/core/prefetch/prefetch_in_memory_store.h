// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_IN_MEMORY_STORE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_IN_MEMORY_STORE_H_

#include "components/offline_pages/core/prefetch/prefetch_store.h"

namespace offline_pages {

// A PrefetchStore implementation that keeps all persisted data in memory.
class PrefetchInMemoryStore : public PrefetchStore {
 public:
  PrefetchInMemoryStore();
  ~PrefetchInMemoryStore() override;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_IN_MEMORY_STORE_H_
