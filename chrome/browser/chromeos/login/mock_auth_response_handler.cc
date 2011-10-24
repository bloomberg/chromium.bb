// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_auth_response_handler.h"

#include <string>

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/common/net/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "content/test/test_url_fetcher_factory.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

using ::testing::_;
using ::testing::Invoke;

MockAuthResponseHandler::MockAuthResponseHandler(
    const GURL& url,
    const net::URLRequestStatus& status,
    const int code,
    const std::string& data)
    : remote_(url),
      status_(status),
      http_response_code_(code),
      data_(data) {
  // Take the args sent to Handle() and pass them to MockNetwork(), which will
  // use the data passed to the constructor here to fill out the call to
  // OnURLFetchComplete().
  ON_CALL(*this, Handle(_,_))
      .WillByDefault(Invoke(this, &MockAuthResponseHandler::MockNetwork));
}

MockAuthResponseHandler::~MockAuthResponseHandler() {}

void MockAuthResponseHandler::CompleteFetch(
    content::URLFetcherDelegate* delegate,
    const GURL remote,
    const net::URLRequestStatus status,
    const int http_response_code,
    const std::string data) {
  TestURLFetcher fetcher(0, GURL(), URLFetcher::GET, delegate);
  fetcher.set_url(remote);
  fetcher.set_status(status);
  fetcher.set_response_code(http_response_code);
  fetcher.SetResponseString(data);
  delegate->OnURLFetchComplete(&fetcher);
}

URLFetcher* MockAuthResponseHandler::MockNetwork(
    std::string data,
    content::URLFetcherDelegate* delegate) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(MockAuthResponseHandler::CompleteFetch, delegate, remote_,
                 status_, http_response_code_, data_));
  return new URLFetcher(GURL(), URLFetcher::GET, delegate);
}

}  // namespace chromeos
