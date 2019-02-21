// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feed_internals/feed_internals_page_handler.h"

#include <utility>

#include "base/feature_list.h"
#include "components/feed/content/feed_host_service.h"
#include "components/feed/core/feed_scheduler_host.h"
#include "components/feed/core/user_classifier.h"
#include "components/feed/feed_feature_list.h"

FeedInternalsPageHandler::FeedInternalsPageHandler(
    feed_internals::mojom::PageHandlerRequest request,
    feed::FeedHostService* feed_host_service)
    : binding_(this, std::move(request)),
      feed_scheduler_host_(feed_host_service->GetSchedulerHost()) {}

FeedInternalsPageHandler::~FeedInternalsPageHandler() = default;

void FeedInternalsPageHandler::GetGeneralProperties(
    GetGeneralPropertiesCallback callback) {
  auto properties = feed_internals::mojom::Properties::New();

  properties->is_feed_enabled =
      base::FeatureList::IsEnabled(feed::kInterestFeedContentSuggestions);

  std::move(callback).Run(std::move(properties));
}

void FeedInternalsPageHandler::GetUserClassifierProperties(
    GetUserClassifierPropertiesCallback callback) {
  auto properties = feed_internals::mojom::UserClassifier::New();

  feed::UserClassifier* user_classifier =
      feed_scheduler_host_->user_classifier();

  properties->user_class_description =
      user_classifier->GetUserClassDescriptionForDebugging();
  properties->avg_hours_between_views = user_classifier->GetEstimatedAvgTime(
      feed::UserClassifier::Event::kSuggestionsViewed);
  properties->avg_hours_between_uses = user_classifier->GetEstimatedAvgTime(
      feed::UserClassifier::Event::kSuggestionsUsed);

  std::move(callback).Run(std::move(properties));
}

void FeedInternalsPageHandler::ClearUserClassifierProperties() {
  feed_scheduler_host_->user_classifier()->ClearClassificationForDebugging();
}
