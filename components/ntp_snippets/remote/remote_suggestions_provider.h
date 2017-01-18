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
  // TODO(jkrcal): Would be nice to get rid of this another level of statuses.
  // Maybe possible while refactoring the RemoteSuggestionsStatusService? (and
  // letting it notify both the SchedulingRemoteSuggestionsProvider and
  // RemoteSuggestionsProviderImpl or just the scheduling one).
  enum class ProviderStatus { ACTIVE, INACTIVE };
  using ProviderStatusCallback =
      base::RepeatingCallback<void(ProviderStatus status)>;

  // Callback to notify with the result of a fetch.
  // TODO(jkrcal): Change to OnceCallback? A OnceCallback does only have a
  // move-constructor which seems problematic for google mock.
  using FetchStatusCallback = base::Callback<void(Status status_code)>;

  ~RemoteSuggestionsProvider() override;

  // Set a callback to be notified whenever the status of the provider changes.
  // The initial change is also notified (switching from an initial undecided
  // status). If the callback is set after the first change, it is called back
  // immediately.
  virtual void SetProviderStatusCallback(
      std::unique_ptr<ProviderStatusCallback> callback) = 0;

  // Fetches snippets from the server for all remote categories and replaces old
  // snippets by the new ones. The request to the server is performed as an
  // background request. Background requests are used for actions not triggered
  // by the user and have lower priority on the server. After the fetch
  // finished, the provided |callback| will be triggered with the status of the
  // fetch (unless not nullptr).
  virtual void RefetchInTheBackground(
      std::unique_ptr<FetchStatusCallback> callback) = 0;

  virtual const RemoteSuggestionsFetcher* suggestions_fetcher_for_debugging()
      const = 0;

 protected:
  RemoteSuggestionsProvider(Observer* observer);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_PROVIDER_H_
