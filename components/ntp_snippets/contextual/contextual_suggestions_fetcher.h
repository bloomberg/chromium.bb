// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_FETCHER_H_
#define COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_FETCHER_H_

#include "components/ntp_snippets/contextual/cluster.h"
#include "components/ntp_snippets/contextual/contextual_suggestion.h"

using contextual_suggestions::FetchClustersCallback;

namespace ntp_snippets {

// Fetches contextual suggestions from the server.
class ContextualSuggestionsFetcher {
 public:

  virtual ~ContextualSuggestionsFetcher() = default;

  // Fetch clusters of suggestions for |url|, calling back to |callback| when
  // complete.
  virtual void FetchContextualSuggestionsClusters(
      const GURL& url,
      FetchClustersCallback callback) = 0;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_SUGGESTIONS_FETCHER_H_
