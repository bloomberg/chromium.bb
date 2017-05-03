// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_

#include <vector>

#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "url/gurl.h"

namespace offline_pages {

class PrefetchServiceImpl : public PrefetchService {
 public:
  PrefetchServiceImpl();
  ~PrefetchServiceImpl() override;

  // PrefetchService implementation:
  void AddCandidatePrefetchURLs(
      const std::vector<PrefetchURL>& suggested_urls) override;
  void RemoveAllUnprocessedPrefetchURLs(const std::string& name_space) override;
  void RemovePrefetchURLsByClientId(const ClientId& client_id) override;
  void BeginBackgroundTask(std::unique_ptr<ScopedBackgroundTask> task) override;
  void StopBackgroundTask(ScopedBackgroundTask* task) override;

  // KeyedService implementation:
  void Shutdown() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefetchServiceImpl);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_
