// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/cluster.h"

#include "components/ntp_snippets/contextual/contextual_suggestion.h"

namespace contextual_suggestions {

Cluster::Cluster() = default;

// MSVC doesn't support defaulted move constructors, so we have to define it
// ourselves.
Cluster::Cluster(Cluster&& other) noexcept
    : title(std::move(other.title)),
      suggestions(std::move(other.suggestions)) {}

Cluster::~Cluster() = default;

ClusterBuilder::ClusterBuilder(const std::string& title) {
  cluster_.title = title;
}

ClusterBuilder::ClusterBuilder(const ClusterBuilder& other) {
  cluster_.title = other.cluster_.title;
  for (const auto& suggestion : other.cluster_.suggestions) {
    AddSuggestion(SuggestionBuilder(suggestion.url)
                      .Title(suggestion.title)
                      .PublisherName(suggestion.publisher_name)
                      .Snippet(suggestion.snippet)
                      .ImageId(suggestion.image_id)
                      .Build());
  }
}

ClusterBuilder& ClusterBuilder::AddSuggestion(ContextualSuggestion suggestion) {
  cluster_.suggestions.emplace_back(std::move(suggestion));
  return *this;
}

Cluster ClusterBuilder::Build() {
  return std::move(cluster_);
}

}  // namespace contextual_suggestions