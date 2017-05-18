// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DISPATCHER_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DISPATCHER_IMPL_H_

#include "base/macros.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"

namespace offline_pages {

class PrefetchDispatcherImpl : public PrefetchDispatcher {
 public:
  PrefetchDispatcherImpl();
  ~PrefetchDispatcherImpl() override;

  // PrefetchDispatcher implementation:
  void AddCandidatePrefetchURLs(
      const std::vector<PrefetchURL>& suggested_urls) override;
  void RemoveAllUnprocessedPrefetchURLs(const std::string& name_space) override;
  void RemovePrefetchURLsByClientId(const ClientId& client_id) override;
  void BeginBackgroundTask(std::unique_ptr<ScopedBackgroundTask> task) override;
  void StopBackgroundTask(ScopedBackgroundTask* task) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefetchDispatcherImpl);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_DISPATCHER_IMPL_H_
