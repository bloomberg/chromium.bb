// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"

namespace offline_pages {

PrefetchServiceImpl::PrefetchServiceImpl()
    : dispatcher_(base::MakeUnique<PrefetchDispatcherImpl>()) {}

PrefetchServiceImpl::~PrefetchServiceImpl() = default;

PrefetchDispatcher* PrefetchServiceImpl::GetDispatcher() {
  return dispatcher_.get();
};

void PrefetchServiceImpl::Shutdown() {}

}  // namespace offline_pages
