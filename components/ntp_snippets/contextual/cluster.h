// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CLUSTER_H_
#define COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CLUSTER_H_

#include "components/ntp_snippets/contextual/contextual_suggestion.h"

using ntp_snippets::ContextualSuggestion;
using ntp_snippets::SuggestionBuilder;

namespace contextual_suggestions {

// A structure representing a suggestion cluster.
struct Cluster {
  Cluster();
  Cluster(Cluster&&) noexcept;
  ~Cluster();

  std::string title;
  std::vector<ContextualSuggestion> suggestions;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cluster);
};

// Allows concise construction of a cluster and copying, which cluster
// doesn't allow.
class ClusterBuilder {
 public:
  ClusterBuilder(const std::string& title);

  // Allow copying for ease of validation when testing.
  ClusterBuilder(const ClusterBuilder& other);
  ClusterBuilder& AddSuggestion(ContextualSuggestion suggestion);
  Cluster Build();

 private:
  Cluster cluster_;
};

using FetchClustersCallback =
    base::OnceCallback<void(std::string peek_text,
                            std::vector<Cluster> clusters)>;

}  // namespace contextual_suggestions

#endif  // COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CLUSTER_H_