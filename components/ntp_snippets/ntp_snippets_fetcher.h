// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_FETCHER_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_FETCHER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class Value;
}

namespace ntp_snippets {

// Fetches snippet data for the NTP from the server
class NTPSnippetsFetcher : public net::URLFetcherDelegate {
 public:
  // Callbacks for JSON parsing, needed because the parsing is platform-
  // dependent.
  using SuccessCallback = base::Callback<void(std::unique_ptr<base::Value>)>;
  using ErrorCallback = base::Callback<void(const std::string&)>;
  using ParseJSONCallback = base::Callback<
      void(const std::string&, const SuccessCallback&, const ErrorCallback&)>;

  using OptionalSnippets = base::Optional<NTPSnippet::PtrVector>;
  // |snippets| contains parsed snippets if a fetch succeeded. If problems
  // occur, |snippets| contains no value (no actual vector in base::Optional).
  // Error details can be retrieved using last_status().
  using SnippetsAvailableCallback =
      base::Callback<void(OptionalSnippets snippets)>;

  // Enumeration listing all possible outcomes for fetch attempts. Used for UMA
  // histograms, so do not change existing values.
  enum class FetchResult {
    SUCCESS,
    EMPTY_HOSTS,
    URL_REQUEST_STATUS_ERROR,
    HTTP_ERROR,
    JSON_PARSE_ERROR,
    INVALID_SNIPPET_CONTENT_ERROR,
    RESULT_MAX
  };

  NTPSnippetsFetcher(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      const ParseJSONCallback& parse_json_callback,
      bool is_stable_channel);
  ~NTPSnippetsFetcher() override;

  // Set a callback that is called when a new set of snippets are downloaded,
  // overriding any previously set callback.
  void SetCallback(const SnippetsAvailableCallback& callback);

  // Fetches snippets from the server. |hosts| restricts the results to a set of
  // hosts, e.g. "www.google.com". An empty host set produces an error.
  //
  // If an ongoing fetch exists, it will be cancelled and a new one started,
  // without triggering an additional callback (i.e. not noticeable by
  // subscriber of SetCallback()).
  void FetchSnippetsFromHosts(const std::set<std::string>& hosts, int count);

  // Debug string representing the status/result of the last fetch attempt.
  const std::string& last_status() const { return last_status_; }

  // Returns the last json fetched from the server.
  const std::string& last_json() const {
    return last_fetch_json_;
  }

  // Overrides internal clock for testing purposes.
  void SetTickClockForTesting(std::unique_ptr<base::TickClock> tick_clock) {
    tick_clock_ = std::move(tick_clock);
  }

 private:
  // URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void OnJsonParsed(std::unique_ptr<base::Value> parsed);
  void OnJsonError(const std::string& error);
  void FetchFinished(OptionalSnippets snippets,
                     FetchResult result,
                     const std::string& extra_message);

  // Holds the URL request context.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  const ParseJSONCallback parse_json_callback_;
  base::TimeTicks fetch_start_time_;
  std::string last_status_;
  std::string last_fetch_json_;

  // The fetcher for downloading the snippets.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // The callback to notify when new snippets get fetched.
  SnippetsAvailableCallback snippets_available_callback_;

  // Flag for picking the right (stable/non-stable) API key for Chrome Reader
  bool is_stable_channel_;

  // Allow for an injectable tick clock for testing.
  std::unique_ptr<base::TickClock> tick_clock_;

  base::WeakPtrFactory<NTPSnippetsFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsFetcher);
};
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_FETCHER_H_
