// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetched_signed_exchange_cache.h"

#include "base/feature_list.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"

namespace content {

namespace {

// The max number of cached entry per one frame. This limit is intended to
// prevent OOM crash of the browser process.
constexpr size_t kMaxEntrySize = 100u;

}  // namespace

PrefetchedSignedExchangeCache::Entry::Entry() = default;
PrefetchedSignedExchangeCache::Entry::~Entry() = default;

void PrefetchedSignedExchangeCache::Entry::SetOuterUrl(const GURL& outer_url) {
  outer_url_ = outer_url;
}
void PrefetchedSignedExchangeCache::Entry::SetOuterResponse(
    std::unique_ptr<const network::ResourceResponseHead> outer_response) {
  outer_response_ = std::move(outer_response);
}
void PrefetchedSignedExchangeCache::Entry::SetHeaderIntegrity(
    std::unique_ptr<const net::SHA256HashValue> header_integrity) {
  header_integrity_ = std::move(header_integrity);
}
void PrefetchedSignedExchangeCache::Entry::SetInnerUrl(const GURL& inner_url) {
  inner_url_ = inner_url;
}
void PrefetchedSignedExchangeCache::Entry::SetInnerResponse(
    std::unique_ptr<const network::ResourceResponseHead> inner_response) {
  inner_response_ = std::move(inner_response);
}
void PrefetchedSignedExchangeCache::Entry::SetCompletionStatus(
    std::unique_ptr<const network::URLLoaderCompletionStatus>
        completion_status) {
  completion_status_ = std::move(completion_status);
}
void PrefetchedSignedExchangeCache::Entry::SetBlobDataHandle(
    std::unique_ptr<const storage::BlobDataHandle> blob_data_handle) {
  blob_data_handle_ = std::move(blob_data_handle);
}

PrefetchedSignedExchangeCache::PrefetchedSignedExchangeCache() {
  DCHECK(base::FeatureList::IsEnabled(
      features::kSignedExchangeSubresourcePrefetch));
}

PrefetchedSignedExchangeCache::~PrefetchedSignedExchangeCache() {}

void PrefetchedSignedExchangeCache::Store(
    std::unique_ptr<const Entry> cached_exchange) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (exchanges_.size() > kMaxEntrySize)
    return;
  DCHECK(cached_exchange->outer_url().is_valid());
  DCHECK(cached_exchange->outer_response());
  DCHECK(cached_exchange->header_integrity());
  DCHECK(cached_exchange->inner_url().is_valid());
  DCHECK(cached_exchange->inner_response());
  DCHECK(cached_exchange->completion_status());
  DCHECK(cached_exchange->blob_data_handle());
  const GURL outer_url = cached_exchange->outer_url();
  exchanges_[outer_url] = std::move(cached_exchange);
}

}  // namespace content
