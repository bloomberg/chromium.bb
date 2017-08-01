// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_CONTEXTUAL_SUGGESTIONS_FETCHER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_CONTEXTUAL_SUGGESTIONS_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"
#include "components/ntp_snippets/status.h"

namespace ntp_snippets {

// Fetches contextual suggestions from the server.
class ContextualSuggestionsFetcher {
 public:
  using OptionalSuggestions = base::Optional<RemoteSuggestion::PtrVector>;

  // If fetching fails, the optional will be empty.
  using SuggestionsAvailableCallback =
      base::OnceCallback<void(Status status,
                              OptionalSuggestions fetched_suggestions)>;

  virtual ~ContextualSuggestionsFetcher() = default;

  virtual void FetchContextualSuggestions(
      const GURL& url,
      SuggestionsAvailableCallback callback) = 0;

  virtual const std::string& GetLastStatusForTesting() const = 0;
  virtual const std::string& GetLastJsonForTesting() const = 0;
  virtual const GURL& GetFetchUrlForTesting() const = 0;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_CONTEXTUAL_SUGGESTIONS_FETCHER_H_
