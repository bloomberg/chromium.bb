// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/prefetched_pages_tracker_impl.h"

#include "base/bind.h"
#include "components/offline_pages/core/client_namespace_constants.h"

using offline_pages::OfflinePageItem;
using offline_pages::OfflinePageModel;
using offline_pages::OfflinePageModelQuery;
using offline_pages::OfflinePageModelQueryBuilder;

namespace ntp_snippets {

namespace {

std::unique_ptr<OfflinePageModelQuery> BuildPrefetchedPagesQuery(
    OfflinePageModel* model) {
  OfflinePageModelQueryBuilder builder;
  builder.RequireNamespace(offline_pages::kSuggestedArticlesNamespace);
  return builder.Build(model->GetPolicyController());
}

bool IsOfflineItemPrefetchedPage(const OfflinePageItem& offline_page_item) {
  return offline_page_item.client_id.name_space ==
         offline_pages::kSuggestedArticlesNamespace;
}

const GURL& GetOfflinePageUrl(const OfflinePageItem& offline_page_item) {
  return offline_page_item.original_url != GURL()
             ? offline_page_item.original_url
             : offline_page_item.url;
}

}  // namespace

PrefetchedPagesTrackerImpl::PrefetchedPagesTrackerImpl(
    OfflinePageModel* offline_page_model)
    : initialized_(false),
      offline_page_model_(offline_page_model),
      weak_ptr_factory_(this) {
  DCHECK(offline_page_model_);
  // If Offline Page model is not loaded yet, it will process our query
  // once it has finished loading.
  offline_page_model_->GetPagesMatchingQuery(
      BuildPrefetchedPagesQuery(offline_page_model),
      base::Bind(&PrefetchedPagesTrackerImpl::Initialize,
                 weak_ptr_factory_.GetWeakPtr()));
}

PrefetchedPagesTrackerImpl::~PrefetchedPagesTrackerImpl() {
  offline_page_model_->RemoveObserver(this);
}

bool PrefetchedPagesTrackerImpl::IsInitialized() const {
  return initialized_;
}

void PrefetchedPagesTrackerImpl::AddInitializationCompletedCallback(
    base::OnceCallback<void()> callback) {
  if (IsInitialized()) {
    std::move(callback).Run();
  }
  initialization_completed_callbacks_.push_back(std::move(callback));
}

bool PrefetchedPagesTrackerImpl::PrefetchedOfflinePageExists(
    const GURL& url) const {
  DCHECK(initialized_);
  return prefetched_urls_.count(url) == 1;
}

void PrefetchedPagesTrackerImpl::OfflinePageModelLoaded(
    OfflinePageModel* model) {
  // Ignored. Offline Page model delayes our requests until it is loaded.
}

void PrefetchedPagesTrackerImpl::OfflinePageAdded(
    OfflinePageModel* model,
    const OfflinePageItem& added_page) {
  if (IsOfflineItemPrefetchedPage(added_page)) {
    AddOfflinePage(added_page);
  }
}

void PrefetchedPagesTrackerImpl::OfflinePageDeleted(
    int64_t offline_id,
    const offline_pages::ClientId& client_id) {
  std::map<int64_t, GURL>::iterator it =
      offline_id_to_url_mapping_.find(offline_id);
  if (it != offline_id_to_url_mapping_.end()) {
    DCHECK(prefetched_urls_.count(it->second));
    prefetched_urls_.erase(it->second);
    offline_id_to_url_mapping_.erase(it);
  }
}

void PrefetchedPagesTrackerImpl::Initialize(
    const std::vector<OfflinePageItem>& all_prefetched_offline_pages) {
  for (const OfflinePageItem& item : all_prefetched_offline_pages) {
    DCHECK(IsOfflineItemPrefetchedPage(item));
    AddOfflinePage(item);
  }

  initialized_ = true;
  offline_page_model_->AddObserver(this);
  for (auto& callback : initialization_completed_callbacks_) {
    std::move(callback).Run();
  }
}

void PrefetchedPagesTrackerImpl::AddOfflinePage(
    const OfflinePageItem& offline_page_item) {
  const GURL& url = GetOfflinePageUrl(offline_page_item);
  DCHECK(!prefetched_urls_.count(url));
  prefetched_urls_.insert(url);
  offline_id_to_url_mapping_[offline_page_item.offline_id] = url;
}

}  // namespace ntp_snippets
