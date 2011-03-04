// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/test_url_fetcher_factory.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "net/url_request/url_request_status.h"

TestURLFetcher::TestURLFetcher(int id,
                               const GURL& url,
                               URLFetcher::RequestType request_type,
                               URLFetcher::Delegate* d)
    : URLFetcher(url, request_type, d),
      id_(id),
      original_url_(url),
      did_receive_last_chunk_(false) {
}

void TestURLFetcher::AppendChunkToUpload(const std::string& data,
                                         bool is_last_chunk) {
  DCHECK(!did_receive_last_chunk_);
  did_receive_last_chunk_ = is_last_chunk;
  chunks_.push_back(data);
}

TestURLFetcherFactory::TestURLFetcherFactory() {}

TestURLFetcherFactory::~TestURLFetcherFactory() {}

URLFetcher* TestURLFetcherFactory::CreateURLFetcher(
    int id,
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcher::Delegate* d) {
  TestURLFetcher* fetcher = new TestURLFetcher(id, url, request_type, d);
  fetchers_[id] = fetcher;
  return fetcher;
}

TestURLFetcher* TestURLFetcherFactory::GetFetcherByID(int id) const {
  Fetchers::const_iterator i = fetchers_.find(id);
  return i == fetchers_.end() ? NULL : i->second;
}

void TestURLFetcherFactory::RemoveFetcherFromMap(int id) {
  Fetchers::iterator i = fetchers_.find(id);
  DCHECK(i != fetchers_.end());
  fetchers_.erase(i);
}

// This class is used by the FakeURLFetcherFactory below.
class FakeURLFetcher : public URLFetcher {
 public:
  // Normal URL fetcher constructor but also takes in a pre-baked response.
  FakeURLFetcher(const GURL& url, RequestType request_type, Delegate* d,
                 const std::string& response_data, bool success)
    : URLFetcher(url, request_type, d),
      url_(url),
      response_data_(response_data),
      success_(success),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  }

  // Start the request.  This will call the given delegate asynchronously
  // with the pre-baked response as parameter.
  virtual void Start() {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(&FakeURLFetcher::RunDelegate));
  }

 private:
  virtual ~FakeURLFetcher() {
  }

  // This is the method which actually calls the delegate that is passed in the
  // constructor.
  void RunDelegate() {
    net::URLRequestStatus status;
    status.set_status(success_ ? net::URLRequestStatus::SUCCESS :
                                 net::URLRequestStatus::FAILED);
    delegate()->OnURLFetchComplete(this, url_, status, success_ ? 200 : 500,
                                   ResponseCookies(), response_data_);
  }

  // Pre-baked response data and flag which indicates whether the request should
  // be successful or not.
  GURL url_;
  std::string response_data_;
  bool success_;

  // Method factory used to run the delegate.
  ScopedRunnableMethodFactory<FakeURLFetcher> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeURLFetcher);
};

FakeURLFetcherFactory::FakeURLFetcherFactory() {}

FakeURLFetcherFactory::~FakeURLFetcherFactory() {}

URLFetcher* FakeURLFetcherFactory::CreateURLFetcher(
    int id,
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcher::Delegate* d) {
  FakeResponseMap::const_iterator it = fake_responses_.find(url);
  if (it == fake_responses_.end()) {
    // If we don't have a baked response for that URL we return NULL.
    DLOG(ERROR) << "No baked response for URL: " << url.spec();
    return NULL;
  }
  return new FakeURLFetcher(url, request_type, d,
                            it->second.first, it->second.second);
}

void FakeURLFetcherFactory::SetFakeResponse(const std::string& url,
                                            const std::string& response_data,
                                            bool success) {
  // Overwrite existing URL if it already exists.
  fake_responses_[GURL(url)] = std::make_pair(response_data, success);
}

void FakeURLFetcherFactory::ClearFakeReponses() {
  fake_responses_.clear();
}
