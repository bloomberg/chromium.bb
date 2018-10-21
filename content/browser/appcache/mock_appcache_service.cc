// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/mock_appcache_service.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace content {

MockAppCacheService::MockAppCacheService()
    : AppCacheServiceImpl(nullptr),
      mock_delete_appcaches_for_origin_result_(net::OK),
      delete_called_count_(0),
      weak_factory_(this) {
  storage_.reset(new MockAppCacheStorage(this));
}

MockAppCacheService::~MockAppCacheService() = default;

void MockAppCacheService::DeleteAppCachesForOrigin(
    const url::Origin& origin,
    net::CompletionOnceCallback callback) {
  ++delete_called_count_;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                mock_delete_appcaches_for_origin_result_));
}

base::WeakPtr<AppCacheServiceImpl> MockAppCacheService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
