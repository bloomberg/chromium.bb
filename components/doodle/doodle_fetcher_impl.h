// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOODLE_DOODLE_FETCHER_IMPL_H_
#define COMPONENTS_DOODLE_DOODLE_FETCHER_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/doodle/doodle_fetcher.h"
#include "components/doodle/doodle_types.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

class GoogleURLTracker;

namespace base {
class DictionaryValue;
class Value;
}

namespace doodle {

// This class provides information about any recent doodle.
// It works asynchronously and calls a callback when finished fetching the
// information from the remote enpoint.
class DoodleFetcherImpl : public DoodleFetcher, public net::URLFetcherDelegate {
 public:
  // Callback for JSON parsing to allow injecting platform-dependent code.
  using ParseJSONCallback = base::Callback<void(
      const std::string& unsafe_json,
      const base::Callback<void(std::unique_ptr<base::Value> json)>& success,
      const base::Callback<void(const std::string&)>& error)>;

  DoodleFetcherImpl(
      scoped_refptr<net::URLRequestContextGetter> download_context,
      GoogleURLTracker* google_url_tracker,
      const ParseJSONCallback& json_parsing_callback);
  ~DoodleFetcherImpl() override;

  // Fetches a doodle asynchronously. The |callback| is called with a
  // DoodleState indicating whether the request succeded in fetching a doodle.
  // If a fetch is already running, the callback will be queued and invoked with
  // result from the next completed request.
  void FetchDoodle(FinishedCallback callback) override;

 private:
  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // ParseJSONCallback Success callback
  void OnJsonParsed(std::unique_ptr<base::Value> json);
  // ParseJSONCallback Failure callback
  void OnJsonParseFailed(const std::string& error_message);

  base::Optional<DoodleConfig> ParseDoodle(
      const base::DictionaryValue& ddljson) const;
  bool ParseImage(const base::DictionaryValue& image_dict,
                  const std::string& image_name,
                  DoodleImage* image) const;
  void ParseBaseInformation(const base::DictionaryValue& ddljson,
                            DoodleConfig* config) const;
  GURL ParseRelativeUrl(const base::DictionaryValue& dict_value,
                        const std::string& key) const;

  void RespondToAllCallbacks(DoodleState state,
                             const base::Optional<DoodleConfig>& config);
  GURL GetGoogleBaseUrl() const;

  // Returns whether a fetch is still in progress. A fetch begins when a
  // callback is added and ends when the last callback was called.
  bool IsFetchInProgress() const { return !callbacks_.empty(); }

  // Parameters set from constructor.
  scoped_refptr<net::URLRequestContextGetter> const download_context_;
  ParseJSONCallback json_parsing_callback_;
  GoogleURLTracker* google_url_tracker_;

  std::vector<FinishedCallback> callbacks_;
  std::unique_ptr<net::URLFetcher> fetcher_;

  base::WeakPtrFactory<DoodleFetcherImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DoodleFetcherImpl);
};

}  // namespace doodle

#endif  // COMPONENTS_DOODLE_DOODLE_FETCHER_IMPL_H_
