// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_ITUNES_JSON_REQUEST_H_
#define IOS_CHROME_BROWSER_PAYMENTS_ITUNES_JSON_REQUEST_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace payment_request_util {

// A class that fetches a JSON formatted response from the iTunes Store using
// the iTunes Search API and a basic JSON parser to parse the response as a
// DictionaryValue.
class ITunesJsonRequest : public net::URLFetcherDelegate {
 public:
  // Callback to pass back the parsed json dictionary returned from iTunes.
  // Invoked with NULL if there is an error.
  typedef base::Callback<void(std::unique_ptr<base::DictionaryValue>)> Callback;

  ITunesJsonRequest(const Callback& callback,
                    net::URLRequestContextGetter* context_getter);
  ~ITunesJsonRequest() override;

  // Used to express the type of request. Please view the following for info:
  // https://affiliate.itunes.apple.com/resources/documentation/itunes-store-web-service-search-api/
  enum ITunesStoreRequestType {
    LOOKUP,
    SEARCH,
  };

  // Starts to fetch results for the given |request_type| and |request_query|.
  void Start(ITunesStoreRequestType request_type,
             const std::string& request_query);
  void Stop();

 private:
  // Callbacks for JSON parsing.
  typedef base::Callback<void(std::unique_ptr<base::Value> result)>
      SuccessCallback;
  typedef base::Callback<void(const std::string& error)> ErrorCallback;

  void ParseJson(const std::string& raw_json_string,
                 const SuccessCallback& success_callback,
                 const ErrorCallback& error_callback);

  void OnJsonParseSuccess(std::unique_ptr<base::Value> parsed_json);
  void OnJsonParseError(const std::string& error);

  // net::URLFetcherDelegate overrides:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  Callback callback_;
  net::URLRequestContextGetter* context_getter_;

  std::unique_ptr<net::URLFetcher> fetcher_;
  base::WeakPtrFactory<ITunesJsonRequest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ITunesJsonRequest);
};

}  // namespace payment_request_util

#endif  // IOS_CHROME_BROWSER_PAYMENTS_ITUNES_JSON_REQUEST_H_
