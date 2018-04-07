// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_content_suggestions_service_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace contextual_suggestions {

ContextualContentSuggestionsServiceProxy::
    ContextualContentSuggestionsServiceProxy(
        ntp_snippets::ContextualContentSuggestionsService* service)
    : service_(service), weak_ptr_factory_(this) {}

ContextualContentSuggestionsServiceProxy::
    ~ContextualContentSuggestionsServiceProxy() {}

void ContextualContentSuggestionsServiceProxy::FetchContextualSuggestions(
    const GURL& url,
    ClustersCallback callback) {
  service_->FetchContextualSuggestionClusters(
      url, base::BindOnce(
               &ContextualContentSuggestionsServiceProxy::CacheSuggestions,
               weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ContextualContentSuggestionsServiceProxy::FetchContextualSuggestionImage(
    const std::string& suggestion_id,
    ntp_snippets::ImageFetchedCallback callback) {
  auto suggestion_iter = suggestions_.find(suggestion_id);
  if (suggestion_iter == suggestions_.end()) {
    DVLOG(1) << "Unkown suggestion ID: " << suggestion_id;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), gfx::Image()));
    return;
  }

  // In order to fetch an image or favicon for a suggestion, we are synthesizing
  // a suggestion ID for it, so that it could be cached in the cached image
  // fetcher inside of the service.

  // Short term implementation.
  GURL image_url = suggestion_iter->second.salient_image_url();

  // This will be the same after this line.
  ntp_snippets::ContentSuggestion::ID synthetic_id(
      ntp_snippets::Category::FromKnownCategory(
          ntp_snippets::KnownCategories::CONTEXTUAL),
      suggestion_id);

  service_->FetchContextualSuggestionImage(synthetic_id, image_url,
                                           std::move(callback));
}

void ContextualContentSuggestionsServiceProxy::FetchContextualSuggestionFavicon(
    const std::string& suggestion_id,
    ntp_snippets::ImageFetchedCallback callback) {}

void ContextualContentSuggestionsServiceProxy::ClearState() {
  suggestions_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ContextualContentSuggestionsServiceProxy::ReportEvent(
    ukm::SourceId ukm_source_id,
    ContextualSuggestionsEvent event) {
  service_->ReportEvent(ukm_source_id, event);
}

void ContextualContentSuggestionsServiceProxy::CacheSuggestions(
    ClustersCallback callback,
    std::string peek_text,
    std::vector<Cluster> clusters) {
  suggestions_.clear();
  for (auto& cluster : clusters) {
    for (auto& suggestion : cluster.suggestions) {
      suggestions_.emplace(std::make_pair(suggestion->id(), *suggestion));
    }
  }
  std::move(callback).Run(peek_text, std::move(clusters));
}

}  // namespace contextual_suggestions
