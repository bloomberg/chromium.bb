// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_PROVIDER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/ntp_snippets/content_suggestions_provider.h"

namespace ntp_snippets {

class RemoteSuggestionsFetcher;

// Retrieves fresh content data (articles) from the server, stores them and
// provides them as content suggestions.
class RemoteSuggestionsProvider : public ContentSuggestionsProvider {
 public:
  // Callback to notify with the result of a fetch.
  // TODO(jkrcal): Change to OnceCallback? A OnceCallback does only have a
  // move-constructor which seems problematic for google mock.
  using FetchStatusCallback = base::Callback<void(Status status_code)>;

  ~RemoteSuggestionsProvider() override;

  // Fetches suggestions from the server for all remote categories and replaces
  // old suggestions by the new ones. The request to the server is performed as
  // an background request. Background requests are used for actions not
  // triggered by the user and have lower priority on the server. After the
  // fetch finished, the provided |callback| will be triggered with the status
  // of the fetch (unless nullptr).
  virtual void RefetchInTheBackground(const FetchStatusCallback& callback) = 0;

  virtual const RemoteSuggestionsFetcher* suggestions_fetcher_for_debugging()
      const = 0;

  // Get the url with favicon for the suggestion.
  virtual GURL GetUrlWithFavicon(
      const ContentSuggestion::ID& suggestion_id) const = 0;

  // Whether the service is explicity disabled.
  virtual bool IsDisabled() const = 0;

 protected:
  RemoteSuggestionsProvider(Observer* observer);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_PROVIDER_H_
