// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_request_test_base.h"

#include "base/threading/thread_task_runner_handle.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace offline_pages {

PrefetchRequestTestBase::PrefetchRequestTestBase()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_),
      request_context_(new net::TestURLRequestContextGetter(
          base::ThreadTaskRunnerHandle::Get())) {}

PrefetchRequestTestBase::~PrefetchRequestTestBase() {}

void PrefetchRequestTestBase::RespondWithNetError(int net_error) {
  net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
  DCHECK(url_fetcher);
  url_fetcher->set_status(net::URLRequestStatus::FromError(net_error));
  url_fetcher->SetResponseString("");
  url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
}

void PrefetchRequestTestBase::RespondWithHttpError(int http_error) {
  net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
  DCHECK(url_fetcher);
  url_fetcher->set_status(net::URLRequestStatus());
  url_fetcher->set_response_code(http_error);
  url_fetcher->SetResponseString("");
  url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
}

void PrefetchRequestTestBase::RespondWithData(const std::string& data) {
  net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
  DCHECK(url_fetcher);
  url_fetcher->set_status(net::URLRequestStatus());
  url_fetcher->set_response_code(net::HTTP_OK);
  url_fetcher->SetResponseString(data);
  url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
}

}  // namespace offline_pages
