// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_url_fetcher_factory.h"

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "content/common/net/url_fetcher_impl.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_status.h"

ScopedURLFetcherFactory::ScopedURLFetcherFactory(
    content::URLFetcherFactory* factory) {
  DCHECK(!URLFetcherImpl::factory());
  URLFetcherImpl::set_factory(factory);
}

ScopedURLFetcherFactory::~ScopedURLFetcherFactory() {
  DCHECK(URLFetcherImpl::factory());
  URLFetcherImpl::set_factory(NULL);
}

TestURLFetcher::TestURLFetcher(int id,
                               const GURL& url,
                               content::URLFetcherDelegate* d)
    : id_(id),
      original_url_(url),
      delegate_(d),
      did_receive_last_chunk_(false),
      fake_load_flags_(0),
      fake_response_code_(-1),
      fake_response_destination_(STRING),
      fake_was_fetched_via_proxy_(false),
      fake_max_retries_(0) {
}

TestURLFetcher::~TestURLFetcher() {
}

void TestURLFetcher::SetUploadData(const std::string& upload_content_type,
                                   const std::string& upload_content) {
  upload_data_ = upload_content;
}

void TestURLFetcher::SetChunkedUpload(const std::string& upload_content_type) {
}

void TestURLFetcher::AppendChunkToUpload(const std::string& data,
                                         bool is_last_chunk) {
  DCHECK(!did_receive_last_chunk_);
  did_receive_last_chunk_ = is_last_chunk;
  chunks_.push_back(data);
}

void TestURLFetcher::SetLoadFlags(int load_flags) {
  fake_load_flags_= load_flags;
}

int TestURLFetcher::GetLoadFlags() const {
  return fake_load_flags_;
}

void TestURLFetcher::SetReferrer(const std::string& referrer) {
}

void TestURLFetcher::SetExtraRequestHeaders(
    const std::string& extra_request_headers) {
  fake_extra_request_headers_.Clear();
  fake_extra_request_headers_.AddHeadersFromString(extra_request_headers);
}

void TestURLFetcher::GetExtraRequestHeaders(net::HttpRequestHeaders* headers) {
  *headers = fake_extra_request_headers_;
}

void TestURLFetcher::SetRequestContext(
    net::URLRequestContextGetter* request_context_getter) {
}

void TestURLFetcher::SetAutomaticallyRetryOn5xx(bool retry) {
}

void TestURLFetcher::SetMaxRetries(int max_retries) {
  fake_max_retries_ = max_retries;
}

int TestURLFetcher::GetMaxRetries() const {
  return fake_max_retries_;
}

base::TimeDelta TestURLFetcher::GetBackoffDelay() const {
  return fake_backoff_delay_;
}

void TestURLFetcher::SaveResponseToTemporaryFile(
    scoped_refptr<base::MessageLoopProxy> file_message_loop_proxy) {
}

net::HttpResponseHeaders* TestURLFetcher::GetResponseHeaders() const {
  return fake_response_headers_;
}

net::HostPortPair TestURLFetcher::GetSocketAddress() const {
  NOTIMPLEMENTED();
  return net::HostPortPair();
}

bool TestURLFetcher::WasFetchedViaProxy() const {
  return fake_was_fetched_via_proxy_;
}

void TestURLFetcher::Start() {
  // Overriden to do nothing. It is assumed the caller will notify the delegate.
}

void TestURLFetcher::StartWithRequestContextGetter(
    net::URLRequestContextGetter* request_context_getter) {
  NOTIMPLEMENTED();
}

const GURL& TestURLFetcher::GetOriginalURL() const {
  return original_url_;
}

const GURL& TestURLFetcher::GetURL() const {
  return fake_url_;
}

const net::URLRequestStatus& TestURLFetcher::GetStatus() const {
  return fake_status_;
}

int TestURLFetcher::GetResponseCode() const {
  return fake_response_code_;
}

const net::ResponseCookies& TestURLFetcher::GetCookies() const {
  return fake_cookies_;
}

bool TestURLFetcher::FileErrorOccurred(
    base::PlatformFileError* out_error_code) const {
  NOTIMPLEMENTED();
  return false;
}

void TestURLFetcher::ReceivedContentWasMalformed() {
}

bool TestURLFetcher::GetResponseAsString(
    std::string* out_response_string) const {
  if (fake_response_destination_ != STRING)
    return false;

  *out_response_string = fake_response_string_;
  return true;
}

bool TestURLFetcher::GetResponseAsFilePath(
    bool take_ownership, FilePath* out_response_path) const {
  if (fake_response_destination_ != TEMP_FILE)
    return false;

  *out_response_path = fake_response_file_path_;
  return true;
}

void TestURLFetcher::set_status(const net::URLRequestStatus& status) {
  fake_status_ = status;
}

void TestURLFetcher::set_was_fetched_via_proxy(bool flag) {
  fake_was_fetched_via_proxy_ = flag;
}

void TestURLFetcher::set_response_headers(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  fake_response_headers_ = headers;
}

void TestURLFetcher::set_backoff_delay(base::TimeDelta backoff_delay) {
  fake_backoff_delay_ = backoff_delay;
}

void TestURLFetcher::SetResponseString(const std::string& response) {
  fake_response_destination_ = STRING;
  fake_response_string_ = response;
}

void TestURLFetcher::SetResponseFilePath(const FilePath& path) {
  fake_response_destination_ = TEMP_FILE;
  fake_response_file_path_ = path;
}

TestURLFetcherFactory::TestURLFetcherFactory()
    : ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

TestURLFetcherFactory::~TestURLFetcherFactory() {}

content::URLFetcher* TestURLFetcherFactory::CreateURLFetcher(
    int id,
    const GURL& url,
    content::URLFetcher::RequestType request_type,
    content::URLFetcherDelegate* d) {
  TestURLFetcher* fetcher = new TestURLFetcher(id, url, d);
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
class FakeURLFetcher : public TestURLFetcher {
 public:
  // Normal URL fetcher constructor but also takes in a pre-baked response.
  FakeURLFetcher(const GURL& url,
                 content::URLFetcherDelegate* d,
                 const std::string& response_data, bool success)
    : TestURLFetcher(0, url, d),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
    set_status(net::URLRequestStatus(
        success ? net::URLRequestStatus::SUCCESS :
            net::URLRequestStatus::FAILED,
        0));
    set_response_code(success ? 200 : 500);
    SetResponseString(response_data);
  }

  // Start the request.  This will call the given delegate asynchronously
  // with the pre-baked response as parameter.
  virtual void Start() OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FakeURLFetcher::RunDelegate, weak_factory_.GetWeakPtr()));
  }

  virtual const GURL& GetURL() const OVERRIDE {
    return TestURLFetcher::GetOriginalURL();
  }

 private:
  virtual ~FakeURLFetcher() {
  }

  // This is the method which actually calls the delegate that is passed in the
  // constructor.
  void RunDelegate() {
    delegate()->OnURLFetchComplete(this);
  }

  base::WeakPtrFactory<FakeURLFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeURLFetcher);
};

FakeURLFetcherFactory::FakeURLFetcherFactory()
    : ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

FakeURLFetcherFactory::FakeURLFetcherFactory(
    content::URLFetcherFactory* default_factory)
    : ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      default_factory_(default_factory) {
}

FakeURLFetcherFactory::~FakeURLFetcherFactory() {}

content::URLFetcher* FakeURLFetcherFactory::CreateURLFetcher(
    int id,
    const GURL& url,
    content::URLFetcher::RequestType request_type,
    content::URLFetcherDelegate* d) {
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
  return new FakeURLFetcher(url, d, it->second.first, it->second.second);
}

void FakeURLFetcherFactory::SetFakeResponse(const std::string& url,
                                            const std::string& response_data,
                                            bool success) {
  // Overwrite existing URL if it already exists.
  fake_responses_[GURL(url)] = std::make_pair(response_data, success);
}

void FakeURLFetcherFactory::ClearFakeResponses() {
  fake_responses_.clear();
}

URLFetcherImplFactory::URLFetcherImplFactory() {}

URLFetcherImplFactory::~URLFetcherImplFactory() {}

content::URLFetcher* URLFetcherImplFactory::CreateURLFetcher(
    int id,
    const GURL& url,
    content::URLFetcher::RequestType request_type,
    content::URLFetcherDelegate* d) {
  return new URLFetcherImpl(url, request_type, d);
}
