// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_URL_FETCHER_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_URL_FETCHER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher_delegate.h"

class TranslateURLFetcher : public net::URLFetcherDelegate {
 public:
  // Callback type for Request().
  typedef base::Callback<void(int, bool, const std::string&)> Callback;

  // Represents internal state if the fetch is completed successfully.
  enum State {
    IDLE,        // No fetch request was issued.
    REQUESTING,  // A fetch request was issued, but not finished yet.
    COMPLETED,   // The last fetch request was finished successfully.
    FAILED,      // The last fetch request was finished with a failure.
  };

  explicit TranslateURLFetcher(int id);
  virtual ~TranslateURLFetcher();

  // Requests to |url|. |callback| will be invoked when the function returns
  // true, and the request is finished asynchronously.
  // Returns false if the previous request is not finished, or the request
  // is omitted due to retry limitation.
  bool Request(const GURL& url, const Callback& callback);

  // Gets internal state.
  State state() { return state_; }

  // net::URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  // URL to send the request.
  GURL url_;

  // ID which is assigned to the URLFetcher.
  const int id_;

  // Internal state.
  enum State state_;

  // URLFetcher instance.
  scoped_ptr<net::URLFetcher> fetcher_;

  // Callback passed at Request(). It will be invoked when an asynchronous
  // fetch operation is finished.
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(TranslateURLFetcher);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_URL_FETCHER_H_
