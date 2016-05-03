// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_FETCHER_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_FETCHER_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

namespace ntp_snippets {

// Fetches snippet data for the NTP from the server
class NTPSnippetsFetcher : public net::URLFetcherDelegate {
 public:
  // If problems occur (explained in |status_message|), |snippets_json| is
  // empty; otherwise, |status_message| is empty.
  using SnippetsAvailableCallback =
      base::Callback<void(const std::string& snippets_json,
                          const std::string& status_message)>;
  using SnippetsAvailableCallbackList =
      base::CallbackList<void(const std::string&, const std::string&)>;

  NTPSnippetsFetcher(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      bool is_stable_channel);
  ~NTPSnippetsFetcher() override;

  // Adds a callback that is called when a new set of snippets are downloaded.
  std::unique_ptr<SnippetsAvailableCallbackList::Subscription> AddCallback(
      const SnippetsAvailableCallback& callback) WARN_UNUSED_RESULT;

  // Fetches snippets from the server. |hosts| can be used to restrict the
  // results to a set of hosts, e.g. "www.google.com". If it is empty, no
  // restrictions are applied.
  //
  // If an ongoing fetch exists, it will be cancelled and a new one started,
  // without triggering additional callbacks (i.e. not noticeable by
  // subscribers).
  void FetchSnippets(const std::set<std::string>& hosts, int count);

 private:
  // URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Holds the URL request context.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // The fetcher for downloading the snippets.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // The callbacks to notify when new snippets get fetched.
  SnippetsAvailableCallbackList callback_list_;

  // Flag for picking the right (stable/non-stable) API key for Chrome Reader
  bool is_stable_channel_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsFetcher);
};
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_FETCHER_H_
