// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_JSON_REQUEST_H_
#define COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_JSON_REQUEST_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/ntp_snippets/remote/json_request.h"
#include "components/ntp_snippets/status.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/http/http_request_headers.h"

namespace base {
class Value;
}  // namespace base

namespace ntp_snippets {

namespace internal {

// A request to query contextual suggestions.
class ContextualJsonRequest : public net::URLFetcherDelegate {
 public:
  // A client can expect error_details only, if there was any error during the
  // fetching or parsing. In successful cases, it will be an empty string.
  using CompletedCallback =
      base::OnceCallback<void(std::unique_ptr<base::Value> result,
                              FetchResult result_code,
                              const std::string& error_details)>;

  // Builds authenticated and non-authenticated ContextualJsonRequests.
  class Builder {
   public:
    Builder();
    Builder(Builder&&);
    ~Builder();

    // Builds a Request object that contains all data to fetch new snippets.
    std::unique_ptr<ContextualJsonRequest> Build() const;

    Builder& SetAuthentication(const std::string& account_id,
                               const std::string& auth_header);
    Builder& SetParseJsonCallback(ParseJSONCallback callback);
    Builder& SetUrl(const GURL& url);
    Builder& SetUrlRequestContextGetter(
        const scoped_refptr<net::URLRequestContextGetter>& context_getter);
    Builder& SetContentUrl(const GURL& url);

    // These preview methods allow to inspect the Request without exposing it
    // publicly.
    std::string PreviewRequestBodyForTesting() { return BuildBody(); }
    std::string PreviewRequestHeadersForTesting() { return BuildHeaders(); }

   private:
    std::string BuildHeaders() const;
    std::string BuildBody() const;
    std::unique_ptr<net::URLFetcher> BuildURLFetcher(
        net::URLFetcherDelegate* request,
        const std::string& headers,
        const std::string& body) const;

    std::string auth_header_;
    ParseJSONCallback parse_json_callback_;
    GURL url_;
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

    // The URL for which to fetch contextual suggestions for.
    GURL content_url_;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  ContextualJsonRequest(const ParseJSONCallback& callback);
  ContextualJsonRequest(ContextualJsonRequest&&);
  ~ContextualJsonRequest() override;

  void Start(CompletedCallback callback);

  std::string GetResponseString() const;

 private:
  // URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void ParseJsonResponse();
  void OnJsonParsed(std::unique_ptr<base::Value> result);
  void OnJsonError(const std::string& error);

  // The fetcher for downloading the snippets. Only non-null if a fetch is
  // currently ongoing.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // This callback is called to parse a json string. It contains callbacks for
  // error and success cases.
  ParseJSONCallback parse_json_callback_;

  // The callback to notify when URLFetcher finished and results are available.
  CompletedCallback request_completed_callback_;

  base::WeakPtrFactory<ContextualJsonRequest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContextualJsonRequest);
};

}  // namespace internal

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTEXTUAL_CONTEXTUAL_JSON_REQUEST_H_
