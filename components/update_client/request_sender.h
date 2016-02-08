// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_REQUEST_SENDER_H_
#define COMPONENTS_UPDATE_CLIENT_REQUEST_SENDER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
}

namespace update_client {

class Configurator;

// Sends a request to one of the urls provided. The class implements a chain
// of responsibility design pattern, where the urls are tried in the order they
// are specified, until the request to one of them succeeds or all have failed.
class RequestSender : public net::URLFetcherDelegate {
 public:
  // The |source| refers to the fetcher object used to make the request. This
  // parameter can be NULL in some error cases.
  typedef base::Callback<void(const net::URLFetcher* source)>
      RequestSenderCallback;

  explicit RequestSender(const scoped_refptr<Configurator>& config);
  ~RequestSender() override;

  void Send(const std::string& request_string,
            const std::vector<GURL>& urls,
            const RequestSenderCallback& request_sender_callback);

 private:
  void SendInternal();

  // Overrides for URLFetcherDelegate.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  const scoped_refptr<Configurator> config_;
  std::vector<GURL> urls_;
  std::vector<GURL>::const_iterator cur_url_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
  std::string request_string_;
  RequestSenderCallback request_sender_callback_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(RequestSender);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_REQUEST_SENDER_H_
