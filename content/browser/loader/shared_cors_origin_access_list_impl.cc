// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/shared_cors_origin_access_list_impl.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

SharedCorsOriginAccessListImpl::SharedCorsOriginAccessListImpl()
    : SharedCorsOriginAccessList() {}

void SharedCorsOriginAccessListImpl::SetAllowListForOrigin(
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> patterns,
    base::OnceClosure closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &SharedCorsOriginAccessListImpl::SetAllowListForOriginOnIOThread,
          base::RetainedRef(this), source_origin, std::move(patterns)),
      std::move(closure));
}

void SharedCorsOriginAccessListImpl::SetBlockListForOrigin(
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> patterns,
    base::OnceClosure closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &SharedCorsOriginAccessListImpl::SetBlockListForOriginOnIOThread,
          base::RetainedRef(this), source_origin, std::move(patterns)),
      std::move(closure));
}

const network::cors::OriginAccessList&
SharedCorsOriginAccessListImpl::GetOriginAccessList() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return origin_access_list_;
}

SharedCorsOriginAccessListImpl::~SharedCorsOriginAccessListImpl() = default;

void SharedCorsOriginAccessListImpl::SetAllowListForOriginOnIOThread(
    const url::Origin source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> patterns) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  origin_access_list_.SetAllowListForOrigin(source_origin, patterns);
}

void SharedCorsOriginAccessListImpl::SetBlockListForOriginOnIOThread(
    const url::Origin source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> patterns) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  origin_access_list_.SetBlockListForOrigin(source_origin, patterns);
}

}  // namespace content
