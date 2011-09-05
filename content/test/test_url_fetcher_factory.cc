// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_url_fetcher_factory.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "net/url_request/url_request_status.h"

ScopedURLFetcherFactory::ScopedURLFetcherFactory(URLFetcher::Factory* factory) {
  DCHECK(!URLFetcher::factory());
  URLFetcher::set_factory(factory);
}

ScopedURLFetcherFactory::~ScopedURLFetcherFactory() {
  DCHECK(URLFetcher::factory());
  URLFetcher::set_factory(NULL);
}

TestURLFetcher::TestURLFetcher(int id,
                               const GURL& url,
                               URLFetcher::RequestType request_type,
                               URLFetcher::Delegate* d)
    : URLFetcher(url, request_type, d),
      id_(id),
      original_url_(url),
      did_receive_last_chunk_(false) {
}

TestURLFetcher::~TestURLFetcher() {
}

void TestURLFetcher::AppendChunkToUpload(const std::string& data,
                                         bool is_last_chunk) {
  DCHECK(!did_receive_last_chunk_);
  did_receive_last_chunk_ = is_last_chunk;
  chunks_.push_back(data);
}

const GURL& TestURLFetcher::original_url() const {
  return original_url_;
}

void TestURLFetcher::set_status(const net::URLRequestStatus& status) {
  fake_status_ = status;
}

void TestURLFetcher::SetResponseString(const std::string& response) {
  SetResponseDestinationForTesting(STRING);
  fake_response_string_ = response;
}

void TestURLFetcher::SetResponseFilePath(const FilePath& path) {
  SetResponseDestinationForTesting(TEMP_FILE);
  fake_response_file_path_ = path;
}

bool TestURLFetcher::GetResponseAsString(
    std::string* out_response_string) const {
  if (GetResponseDestinationForTesting() != STRING)
    return false;

  *out_response_string = fake_response_string_;
  return true;
}

bool TestURLFetcher::GetResponseAsFilePath(
    bool take_ownership, FilePath* out_response_path) const {
  if (GetResponseDestinationForTesting() != TEMP_FILE)
    return false;

  *out_response_path = fake_response_file_path_;
  return true;
}

TestURLFetcherFactory::TestURLFetcherFactory()
    : ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

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

const GURL& TestURLFetcher::url() const {
  return fake_url_;
}

const net::URLRequestStatus& TestURLFetcher::status() const {
  return fake_status_;
}

int TestURLFetcher::response_code() const {
  return fake_response_code_;
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
      status_(success ? net::URLRequestStatus::SUCCESS :
                        net::URLRequestStatus::FAILED, 0),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  }

  // Start the request.  This will call the given delegate asynchronously
  // with the pre-baked response as parameter.
  virtual void Start() OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(&FakeURLFetcher::RunDelegate));
  }

  // These methods are overriden so we can use the version of
  // OnURLFetchComplete that only has a single URLFetcher argument.
  virtual const net::ResponseCookies& cookies() const OVERRIDE {
    return cookies_;
  }

  virtual const std::string& GetResponseStringRef() const OVERRIDE {
    return response_data_;
  }

  virtual bool GetResponseAsString(
      std::string* out_response_string) const OVERRIDE {
    *out_response_string = response_data_;
    return true;
  }

  virtual int response_code() const OVERRIDE {
    return success_ ? 200 : 500;
  }

  virtual const net::URLRequestStatus& status() const OVERRIDE {
    return status_;
  }

 private:
  virtual ~FakeURLFetcher() {
  }

  // This is the method which actually calls the delegate that is passed in the
  // constructor.
  void RunDelegate() {
    delegate()->OnURLFetchComplete(this);
  }

  // Pre-baked response data and flag which indicates whether the request should
  // be successful or not.
  GURL url_;
  std::string response_data_;
  bool success_;
  net::URLRequestStatus status_;
  net::ResponseCookies cookies_;

  // Method factory used to run the delegate.
  ScopedRunnableMethodFactory<FakeURLFetcher> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeURLFetcher);
};

FakeURLFetcherFactory::FakeURLFetcherFactory()
    : ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

FakeURLFetcherFactory::FakeURLFetcherFactory(
    URLFetcher::Factory* default_factory)
    : ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      default_factory_(default_factory) {
}

FakeURLFetcherFactory::~FakeURLFetcherFactory() {}

URLFetcher* FakeURLFetcherFactory::CreateURLFetcher(
    int id,
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcher::Delegate* d) {
  FakeResponseMap::const_iterator it = fake_responses_.find(url);
  if (it == fake_responses_.end()) {
    if (default_factory_ == NULL) {
      // If we don't have a baked response for that URL we return NULL.
      DLOG(ERROR) << "No baked response for URL: " << url.spec();
      return NULL;
    } else {
      return default_factory_->CreateURLFetcher(id, url, request_type, d);
    }
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

URLFetcherFactory::URLFetcherFactory() {}

URLFetcherFactory::~URLFetcherFactory() {}

URLFetcher* URLFetcherFactory::CreateURLFetcher(
    int id,
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcher::Delegate* d) {
  return new URLFetcher(url, request_type, d);
}
