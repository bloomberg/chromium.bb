// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace offline_pages {

class PrefetchDispatcher;

// Main class and entry point for the Offline Pages Prefetching feature, that
// controls the lifetime of all major subcomponents of the prefetching system.
class PrefetchService : public KeyedService {
 public:
  ~PrefetchService() override = default;

  virtual PrefetchDispatcher* GetDispatcher() = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_H_
