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

namespace {
// TODO(pnoland): check if this is the correct base URL for all images.
static constexpr char kImageURLFormat[] =
    "http://www.google.com/images?q=tbn:%s";

GURL ImageUrlFromId(const std::string& image_id) {
  return GURL(base::StringPrintf(kImageURLFormat, image_id.c_str()));
}
}  // namespace

ContextualContentSuggestionsServiceProxy::
    ContextualContentSuggestionsServiceProxy(
        ntp_snippets::ContextualContentSuggestionsService* service,
        std::unique_ptr<ContextualSuggestionsMetricsReporter> metrics_reporter)
    : service_(service),
      metrics_reporter_(std::move(metrics_reporter)),
      last_ukm_source_id_(ukm::kInvalidSourceId),
      weak_ptr_factory_(this) {}

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

  FetchImageImpl(suggestion_iter->second.image_id, std::move(callback));
}

void ContextualContentSuggestionsServiceProxy::FetchContextualSuggestionFavicon(
    const std::string& suggestion_id,
    ntp_snippets::ImageFetchedCallback callback) {
  auto suggestion_iter = suggestions_.find(suggestion_id);
  if (suggestion_iter == suggestions_.end()) {
    DVLOG(1) << "Unkown suggestion ID: " << suggestion_id;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), gfx::Image()));
    return;
  }

  FetchImageImpl(suggestion_iter->second.favicon_image_id, std::move(callback));
}

void ContextualContentSuggestionsServiceProxy::ClearState() {
  suggestions_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ContextualContentSuggestionsServiceProxy::ReportEvent(
    ukm::SourceId ukm_source_id,
    ContextualSuggestionsEvent event) {
  DCHECK(ukm_source_id != ukm::kInvalidSourceId);

  // Flush the previous page (if any) and setup the new page.
  if (ukm_source_id != last_ukm_source_id_) {
    if (last_ukm_source_id_ != ukm::kInvalidSourceId)
      metrics_reporter_->Flush();
    last_ukm_source_id_ = ukm_source_id;
    metrics_reporter_->SetupForPage(ukm_source_id);
  }

  metrics_reporter_->RecordEvent(event);
}

void ContextualContentSuggestionsServiceProxy::FlushMetrics() {
  if (last_ukm_source_id_ != ukm::kInvalidSourceId)
    metrics_reporter_->Flush();
  last_ukm_source_id_ = ukm::kInvalidSourceId;
}

void ContextualContentSuggestionsServiceProxy::FetchImageImpl(
    const std::string& image_id,
    ntp_snippets::ImageFetchedCallback callback) {
  GURL image_url = ImageUrlFromId(image_id);

  ntp_snippets::ContentSuggestion::ID synthetic_cache_id(
      ntp_snippets::Category::FromKnownCategory(
          ntp_snippets::KnownCategories::CONTEXTUAL),
      image_id);

  service_->FetchContextualSuggestionImage(synthetic_cache_id, image_url,
                                           std::move(callback));
}

void ContextualContentSuggestionsServiceProxy::CacheSuggestions(
    ClustersCallback callback,
    std::string peek_text,
    std::vector<Cluster> clusters) {
  suggestions_.clear();
  for (auto& cluster : clusters) {
    for (auto& suggestion : cluster.suggestions) {
      suggestions_.emplace(std::make_pair(suggestion.id, suggestion));
    }
  }
  std::move(callback).Run(peek_text, std::move(clusters));
}

}  // namespace contextual_suggestions
