// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"

namespace offline_pages {

class PrefetchServiceImpl : public PrefetchService {
 public:
  PrefetchServiceImpl();
  ~PrefetchServiceImpl() override;

  // PrefetchService implementation:
  PrefetchDispatcher* GetDispatcher() override;

  // KeyedService implementation:
  void Shutdown() override;

 private:
  std::unique_ptr<PrefetchDispatcher> dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchServiceImpl);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_
