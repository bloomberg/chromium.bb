// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_REQUEST_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_REQUEST_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

// Request to CloudPrint with timeout control.
// Delegate should delete this object once it is deleting.
class CloudPrintRequest : public net::URLFetcherDelegate,
                          public base::SupportsWeakPtr<CloudPrintRequest> {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Invoked when |fetcher_| finished fetching successfully.
    // Use for erasing instance of CloudPrintRequest class.
    virtual void OnFetchComplete(const std::string& response) = 0;

    // Invoked when |fetcher_| finished fetching successfully.
    // Use for erasing instance of CloudPrintRequest class.
    virtual void OnFetchError(const std::string& server_api,
                              int server_code,
                              int server_http_code) = 0;

    // Invoked when timeout is reached.
    // Use for erasing instance of CloudPrintRequest class.
    virtual void OnFetchTimeoutReached() = 0;
  };

  virtual ~CloudPrintRequest();

  // Creates GET request.
  static scoped_ptr<CloudPrintRequest> CreateGet(const GURL& url,
                                                 Delegate* delegate);

  // Creates POST request.
  static scoped_ptr<CloudPrintRequest> CreatePost(const GURL& url,
                                                  const std::string& content,
                                                  const std::string& mimetype,
                                                  Delegate* delegate);

  // Starts request. Once fetch was completed, parser will be called.
  void Run(const std::string& access_token,
           scoped_refptr<net::URLRequestContextGetter> context_getter);

  // Add header to request.
  void AddHeader(const std::string& header);

 private:
  CloudPrintRequest(const GURL& url,
                    net::URLFetcher::RequestType method,
                    Delegate* delegate);

  // net::URLFetcherDelegate methods:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Method for handling timeout.
  void OnRequestTimeout();

  scoped_ptr<net::URLFetcher> fetcher_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintRequest);
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_REQUEST_H_

