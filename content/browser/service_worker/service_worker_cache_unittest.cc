// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/quota/mock_quota_manager_proxy.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/referrer.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
const char kTestData[] = "Hello World";

// Returns a BlobProtocolHandler that uses |blob_storage_context|. Caller owns
// the memory.
storage::BlobProtocolHandler* CreateMockBlobProtocolHandler(
    storage::BlobStorageContext* blob_storage_context) {
  // The FileSystemContext and MessageLoopProxy are not actually used but a
  // MessageLoopProxy is needed to avoid a DCHECK in BlobURLRequestJob ctor.
  return new storage::BlobProtocolHandler(
      blob_storage_context, NULL, base::MessageLoopProxy::current().get());
}

// A disk_cache::Backend wrapper that can delay operations.
class DelayableBackend : public disk_cache::Backend {
 public:
  DelayableBackend(scoped_ptr<disk_cache::Backend> backend)
      : backend_(backend.Pass()), delay_open_(false) {}

  // disk_cache::Backend overrides
  net::CacheType GetCacheType() const override {
    return backend_->GetCacheType();
  }
  int32 GetEntryCount() const override { return backend_->GetEntryCount(); }
  int OpenEntry(const std::string& key,
                disk_cache::Entry** entry,
                const CompletionCallback& callback) override {
    if (delay_open_) {
      open_entry_callback_ =
          base::Bind(&DelayableBackend::OpenEntryDelayedImpl,
                     base::Unretained(this), key, entry, callback);
      return net::ERR_IO_PENDING;
    }

    return backend_->OpenEntry(key, entry, callback);
  }
  int CreateEntry(const std::string& key,
                  disk_cache::Entry** entry,
                  const CompletionCallback& callback) override {
    return backend_->CreateEntry(key, entry, callback);
  }
  int DoomEntry(const std::string& key,
                const CompletionCallback& callback) override {
    return backend_->DoomEntry(key, callback);
  }
  int DoomAllEntries(const CompletionCallback& callback) override {
    return backend_->DoomAllEntries(callback);
  }
  int DoomEntriesBetween(base::Time initial_time,
                         base::Time end_time,
                         const CompletionCallback& callback) override {
    return backend_->DoomEntriesBetween(initial_time, end_time, callback);
  }
  int DoomEntriesSince(base::Time initial_time,
                       const CompletionCallback& callback) override {
    return backend_->DoomEntriesSince(initial_time, callback);
  }
  scoped_ptr<Iterator> CreateIterator() override {
    return backend_->CreateIterator();
  }
  void GetStats(
      std::vector<std::pair<std::string, std::string>>* stats) override {
    return backend_->GetStats(stats);
  }
  void OnExternalCacheHit(const std::string& key) override {
    return backend_->OnExternalCacheHit(key);
  }

  // Call to continue a delayed open.
  void OpenEntryContinue() {
    EXPECT_FALSE(open_entry_callback_.is_null());
    open_entry_callback_.Run();
  }

  void set_delay_open(bool value) { delay_open_ = value; }

 private:
  void OpenEntryDelayedImpl(const std::string& key,
                            disk_cache::Entry** entry,
                            const CompletionCallback& callback) {
    int rv = backend_->OpenEntry(key, entry, callback);
    if (rv != net::ERR_IO_PENDING)
      callback.Run(rv);
  }

  scoped_ptr<disk_cache::Backend> backend_;
  bool delay_open_;
  base::Closure open_entry_callback_;
};

}  // namespace

// A ServiceWorkerCache that can optionally delay during backend creation.
class TestServiceWorkerCache : public ServiceWorkerCache {
 public:
  TestServiceWorkerCache(
      const GURL& origin,
      const base::FilePath& path,
      net::URLRequestContext* request_context,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context)
      : ServiceWorkerCache(origin,
                           path,
                           request_context,
                           quota_manager_proxy,
                           blob_context),
        delay_backend_creation_(false) {}

  void CreateBackend(const ErrorCallback& callback) override {
    backend_creation_callback_ = callback;
    if (delay_backend_creation_)
      return;
    ContinueCreateBackend();
  }

  void ContinueCreateBackend() {
    ServiceWorkerCache::CreateBackend(backend_creation_callback_);
  }

  void set_delay_backend_creation(bool delay) {
    delay_backend_creation_ = delay;
  }

  // Swap the existing backend with a delayable one. The backend must have been
  // created before calling this.
  DelayableBackend* UseDelayableBackend() {
    EXPECT_TRUE(backend_);
    DelayableBackend* delayable_backend = new DelayableBackend(backend_.Pass());
    backend_.reset(delayable_backend);
    return delayable_backend;
  }

 private:
  ~TestServiceWorkerCache() override {}

  bool delay_backend_creation_;
  ErrorCallback backend_creation_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestServiceWorkerCache);
};

class ServiceWorkerCacheTest : public testing::Test {
 public:
  ServiceWorkerCacheTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        callback_error_(ServiceWorkerCache::ERROR_TYPE_OK),
        callback_closed_(false) {}

  void SetUp() override {
    ChromeBlobStorageContext* blob_storage_context =
        ChromeBlobStorageContext::GetFor(&browser_context_);
    // Wait for chrome_blob_storage_context to finish initializing.
    base::RunLoop().RunUntilIdle();
    blob_storage_context_ = blob_storage_context->context();

    quota_manager_proxy_ = new MockQuotaManagerProxy(
        nullptr, base::MessageLoopProxy::current().get());

    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
    url_request_job_factory_->SetProtocolHandler(
        "blob", CreateMockBlobProtocolHandler(blob_storage_context->context()));

    net::URLRequestContext* url_request_context =
        browser_context_.GetRequestContext()->GetURLRequestContext();

    url_request_context->set_job_factory(url_request_job_factory_.get());

    CreateRequests(blob_storage_context);

    if (!MemoryOnly())
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath path = MemoryOnly() ? base::FilePath() : temp_dir_.path();

    cache_ = make_scoped_refptr(new TestServiceWorkerCache(
        GURL("http://example.com"), path, url_request_context,
        quota_manager_proxy_, blob_storage_context->context()->AsWeakPtr()));
  }

  void TearDown() override {
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
    base::RunLoop().RunUntilIdle();
  }

  void CreateRequests(ChromeBlobStorageContext* blob_storage_context) {
    ServiceWorkerHeaderMap headers;
    headers.insert(std::make_pair("a", "a"));
    headers.insert(std::make_pair("b", "b"));
    body_request_ =
        ServiceWorkerFetchRequest(GURL("http://example.com/body.html"), "GET",
                                  headers, Referrer(), false);
    no_body_request_ =
        ServiceWorkerFetchRequest(GURL("http://example.com/no_body.html"),
                                  "GET",
                                  headers,
                                  Referrer(),
                                  false);

    std::string expected_response;
    for (int i = 0; i < 100; ++i)
      expected_blob_data_ += kTestData;

    scoped_ptr<storage::BlobDataBuilder> blob_data(
        new storage::BlobDataBuilder("blob-id:myblob"));
    blob_data->AppendData(expected_blob_data_);

    blob_handle_ =
        blob_storage_context->context()->AddFinishedBlob(blob_data.get());

    body_response_ =
        ServiceWorkerResponse(GURL("http://example.com/body.html"),
                              200,
                              "OK",
                              blink::WebServiceWorkerResponseTypeDefault,
                              headers,
                              blob_handle_->uuid(),
                              expected_blob_data_.size(),
                              GURL());

    no_body_response_ =
        ServiceWorkerResponse(GURL("http://example.com/no_body.html"),
                              200,
                              "OK",
                              blink::WebServiceWorkerResponseTypeDefault,
                              headers,
                              "",
                              0,
                              GURL());
  }

  scoped_ptr<ServiceWorkerFetchRequest> CopyFetchRequest(
      const ServiceWorkerFetchRequest& request) {
    return make_scoped_ptr(new ServiceWorkerFetchRequest(request.url,
                                                         request.method,
                                                         request.headers,
                                                         request.referrer,
                                                         request.is_reload));
  }

  scoped_ptr<ServiceWorkerResponse> CopyFetchResponse(
      const ServiceWorkerResponse& response) {
    scoped_ptr<ServiceWorkerResponse> sw_response(
        new ServiceWorkerResponse(response.url,
                                  response.status_code,
                                  response.status_text,
                                  response.response_type,
                                  response.headers,
                                  response.blob_uuid,
                                  response.blob_size,
                                  response.stream_url));
    return sw_response.Pass();
  }

  bool Put(const ServiceWorkerFetchRequest& request,
           const ServiceWorkerResponse& response) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Put(CopyFetchRequest(request),
                CopyFetchResponse(response),
                base::Bind(&ServiceWorkerCacheTest::ResponseAndErrorCallback,
                           base::Unretained(this),
                           base::Unretained(loop.get())));
    // TODO(jkarlin): These functions should use base::RunLoop().RunUntilIdle()
    // once the cache uses a passed in MessageLoopProxy instead of the CACHE
    // thread.
    loop->Run();

    return callback_error_ == ServiceWorkerCache::ERROR_TYPE_OK;
  }

  bool Match(const ServiceWorkerFetchRequest& request) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Match(CopyFetchRequest(request),
                  base::Bind(&ServiceWorkerCacheTest::ResponseAndErrorCallback,
                             base::Unretained(this),
                             base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == ServiceWorkerCache::ERROR_TYPE_OK;
  }

  bool Delete(const ServiceWorkerFetchRequest& request) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Delete(CopyFetchRequest(request),
                   base::Bind(&ServiceWorkerCacheTest::ErrorTypeCallback,
                              base::Unretained(this),
                              base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == ServiceWorkerCache::ERROR_TYPE_OK;
  }

  bool Keys() {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Keys(base::Bind(&ServiceWorkerCacheTest::RequestsCallback,
                            base::Unretained(this),
                            base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == ServiceWorkerCache::ERROR_TYPE_OK;
  }

  bool Close() {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Close(base::Bind(&ServiceWorkerCacheTest::CloseCallback,
                             base::Unretained(this),
                             base::Unretained(loop.get())));
    loop->Run();
    return callback_closed_;
  }

  void RequestsCallback(base::RunLoop* run_loop,
                        ServiceWorkerCache::ErrorType error,
                        scoped_ptr<ServiceWorkerCache::Requests> requests) {
    callback_error_ = error;
    callback_strings_.clear();
    if (requests) {
      for (size_t i = 0u; i < requests->size(); ++i)
        callback_strings_.push_back(requests->at(i).url.spec());
    }
    if (run_loop)
      run_loop->Quit();
  }

  void ErrorTypeCallback(base::RunLoop* run_loop,
                         ServiceWorkerCache::ErrorType error) {
    callback_error_ = error;
    if (run_loop)
      run_loop->Quit();
  }

  void ResponseAndErrorCallback(
      base::RunLoop* run_loop,
      ServiceWorkerCache::ErrorType error,
      scoped_ptr<ServiceWorkerResponse> response,
      scoped_ptr<storage::BlobDataHandle> body_handle) {
    callback_error_ = error;
    callback_response_ = response.Pass();
    callback_response_data_.reset();
    if (error == ServiceWorkerCache::ERROR_TYPE_OK &&
        !callback_response_->blob_uuid.empty()) {
      callback_response_data_ = body_handle.Pass();
    }

    if (run_loop)
      run_loop->Quit();
  }

  void CloseCallback(base::RunLoop* run_loop) {
    EXPECT_FALSE(callback_closed_);
    callback_closed_ = true;
    if (run_loop)
      run_loop->Quit();
  }

  void CopyBody(storage::BlobDataHandle* blob_handle, std::string* output) {
    scoped_ptr<storage::BlobDataSnapshot> data = blob_handle->CreateSnapshot();
    const auto& items = data->items();
    for (const auto& item : items) {
      output->append(item->bytes(), item->length());
    }
  }

  bool VerifyKeys(const std::vector<std::string>& expected_keys) {
    if (expected_keys.size() != callback_strings_.size())
      return false;

    std::set<std::string> found_set;
    for (int i = 0, max = callback_strings_.size(); i < max; ++i)
      found_set.insert(callback_strings_[i]);

    for (int i = 0, max = expected_keys.size(); i < max; ++i) {
      if (found_set.find(expected_keys[i]) == found_set.end())
        return false;
    }
    return true;
  }

  bool TestResponseType(blink::WebServiceWorkerResponseType response_type) {
    body_response_.response_type = response_type;
    EXPECT_TRUE(Put(body_request_, body_response_));
    EXPECT_TRUE(Match(body_request_));
    EXPECT_TRUE(Delete(body_request_));
    return response_type == callback_response_->response_type;
  }

  void VerifyAllOpsFail() {
    EXPECT_FALSE(Put(no_body_request_, no_body_response_));
    EXPECT_FALSE(Match(no_body_request_));
    EXPECT_FALSE(Delete(body_request_));
    EXPECT_FALSE(Keys());
  }

  virtual bool MemoryOnly() { return false; }

 protected:
  TestBrowserContext browser_context_;
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  storage::BlobStorageContext* blob_storage_context_;

  base::ScopedTempDir temp_dir_;
  scoped_refptr<TestServiceWorkerCache> cache_;

  ServiceWorkerFetchRequest body_request_;
  ServiceWorkerResponse body_response_;
  ServiceWorkerFetchRequest no_body_request_;
  ServiceWorkerResponse no_body_response_;
  scoped_ptr<storage::BlobDataHandle> blob_handle_;
  std::string expected_blob_data_;

  ServiceWorkerCache::ErrorType callback_error_;
  scoped_ptr<ServiceWorkerResponse> callback_response_;
  scoped_ptr<storage::BlobDataHandle> callback_response_data_;
  std::vector<std::string> callback_strings_;
  bool callback_closed_;
};

class ServiceWorkerCacheTestP : public ServiceWorkerCacheTest,
                                public testing::WithParamInterface<bool> {
  bool MemoryOnly() override { return !GetParam(); }
};

class ServiceWorkerCacheMemoryOnlyTest
    : public ServiceWorkerCacheTest,
      public testing::WithParamInterface<bool> {
  bool MemoryOnly() override { return true; }
};

TEST_P(ServiceWorkerCacheTestP, PutNoBody) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(callback_response_);
  EXPECT_STREQ(no_body_response_.url.spec().c_str(),
               callback_response_->url.spec().c_str());
  EXPECT_FALSE(callback_response_data_);
  EXPECT_STREQ("", callback_response_->blob_uuid.c_str());
  EXPECT_EQ(0u, callback_response_->blob_size);
}

TEST_P(ServiceWorkerCacheTestP, PutBody) {
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(callback_response_);
  EXPECT_STREQ(body_response_.url.spec().c_str(),
               callback_response_->url.spec().c_str());
  EXPECT_TRUE(callback_response_data_);
  EXPECT_STRNE("", callback_response_->blob_uuid.c_str());
  EXPECT_EQ(expected_blob_data_.size(), callback_response_->blob_size);

  std::string response_body;
  CopyBody(callback_response_data_.get(), &response_body);
  EXPECT_STREQ(expected_blob_data_.c_str(), response_body.c_str());
}

TEST_P(ServiceWorkerCacheTestP, ResponseURLDiffersFromRequestURL) {
  no_body_response_.url = GURL("http://example.com/foobar");
  EXPECT_STRNE("http://example.com/foobar",
               no_body_request_.url.spec().c_str());
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_STREQ("http://example.com/foobar",
               callback_response_->url.spec().c_str());
}

TEST_P(ServiceWorkerCacheTestP, ResponseURLEmpty) {
  no_body_response_.url = GURL();
  EXPECT_STRNE("", no_body_request_.url.spec().c_str());
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_STREQ("", callback_response_->url.spec().c_str());
}

TEST_F(ServiceWorkerCacheTest, PutBodyDropBlobRef) {
  scoped_ptr<base::RunLoop> loop(new base::RunLoop());
  cache_->Put(CopyFetchRequest(body_request_),
              CopyFetchResponse(body_response_),
              base::Bind(&ServiceWorkerCacheTestP::ResponseAndErrorCallback,
                         base::Unretained(this),
                         base::Unretained(loop.get())));
  // The handle should be held by the cache now so the deref here should be
  // okay.
  blob_handle_.reset();
  loop->Run();

  EXPECT_EQ(ServiceWorkerCache::ERROR_TYPE_OK, callback_error_);
}

TEST_P(ServiceWorkerCacheTestP, PutReplace) {
  EXPECT_TRUE(Put(body_request_, no_body_response_));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_FALSE(callback_response_data_);

  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(callback_response_data_);

  EXPECT_TRUE(Put(body_request_, no_body_response_));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_FALSE(callback_response_data_);
}

TEST_P(ServiceWorkerCacheTestP, MatchNoBody) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_EQ(200, callback_response_->status_code);
  EXPECT_STREQ("OK", callback_response_->status_text.c_str());
  EXPECT_STREQ("http://example.com/no_body.html",
               callback_response_->url.spec().c_str());
  EXPECT_STREQ("", callback_response_->blob_uuid.c_str());
  EXPECT_EQ(0u, callback_response_->blob_size);
}

TEST_P(ServiceWorkerCacheTestP, MatchBody) {
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_EQ(200, callback_response_->status_code);
  EXPECT_STREQ("OK", callback_response_->status_text.c_str());
  EXPECT_STREQ("http://example.com/body.html",
               callback_response_->url.spec().c_str());
  EXPECT_STRNE("", callback_response_->blob_uuid.c_str());
  EXPECT_EQ(expected_blob_data_.size(), callback_response_->blob_size);

  std::string response_body;
  CopyBody(callback_response_data_.get(), &response_body);
  EXPECT_STREQ(expected_blob_data_.c_str(), response_body.c_str());
}

TEST_P(ServiceWorkerCacheTestP, Vary) {
  body_request_.headers["vary_foo"] = "foo";
  body_response_.headers["vary"] = "vary_foo";
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["vary_foo"] = "bar";
  EXPECT_FALSE(Match(body_request_));

  body_request_.headers.erase("vary_foo");
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(ServiceWorkerCacheTestP, EmptyVary) {
  body_response_.headers["vary"] = "";
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["zoo"] = "zoo";
  EXPECT_TRUE(Match(body_request_));
}

TEST_P(ServiceWorkerCacheTestP, NoVaryButDiffHeaders) {
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["zoo"] = "zoo";
  EXPECT_TRUE(Match(body_request_));
}

TEST_P(ServiceWorkerCacheTestP, VaryMultiple) {
  body_request_.headers["vary_foo"] = "foo";
  body_request_.headers["vary_bar"] = "bar";
  body_response_.headers["vary"] = " vary_foo    , vary_bar";
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["vary_bar"] = "foo";
  EXPECT_FALSE(Match(body_request_));

  body_request_.headers.erase("vary_bar");
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(ServiceWorkerCacheTestP, VaryNewHeader) {
  body_request_.headers["vary_foo"] = "foo";
  body_response_.headers["vary"] = " vary_foo, vary_bar";
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));

  body_request_.headers["vary_bar"] = "bar";
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(ServiceWorkerCacheTestP, VaryStar) {
  body_response_.headers["vary"] = "*";
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_FALSE(Match(body_request_));
}

TEST_P(ServiceWorkerCacheTestP, EmptyKeys) {
  EXPECT_TRUE(Keys());
  EXPECT_EQ(0u, callback_strings_.size());
}

TEST_P(ServiceWorkerCacheTestP, TwoKeys) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Keys());
  EXPECT_EQ(2u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back(no_body_request_.url.spec());
  expected_keys.push_back(body_request_.url.spec());
  EXPECT_TRUE(VerifyKeys(expected_keys));
}

TEST_P(ServiceWorkerCacheTestP, TwoKeysThenOne) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Keys());
  EXPECT_EQ(2u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back(no_body_request_.url.spec());
  expected_keys.push_back(body_request_.url.spec());
  EXPECT_TRUE(VerifyKeys(expected_keys));

  EXPECT_TRUE(Delete(body_request_));
  EXPECT_TRUE(Keys());
  EXPECT_EQ(1u, callback_strings_.size());
  std::vector<std::string> expected_key;
  expected_key.push_back(no_body_request_.url.spec());
  EXPECT_TRUE(VerifyKeys(expected_key));
}

TEST_P(ServiceWorkerCacheTestP, DeleteNoBody) {
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_TRUE(Delete(no_body_request_));
  EXPECT_FALSE(Match(no_body_request_));
  EXPECT_FALSE(Delete(no_body_request_));
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_TRUE(Match(no_body_request_));
  EXPECT_TRUE(Delete(no_body_request_));
}

TEST_P(ServiceWorkerCacheTestP, DeleteBody) {
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(Delete(body_request_));
  EXPECT_FALSE(Match(body_request_));
  EXPECT_FALSE(Delete(body_request_));
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Match(body_request_));
  EXPECT_TRUE(Delete(body_request_));
}

TEST_P(ServiceWorkerCacheTestP, QuickStressNoBody) {
  for (int i = 0; i < 100; ++i) {
    EXPECT_FALSE(Match(no_body_request_));
    EXPECT_TRUE(Put(no_body_request_, no_body_response_));
    EXPECT_TRUE(Match(no_body_request_));
    EXPECT_TRUE(Delete(no_body_request_));
  }
}

TEST_P(ServiceWorkerCacheTestP, QuickStressBody) {
  for (int i = 0; i < 100; ++i) {
    ASSERT_FALSE(Match(body_request_));
    ASSERT_TRUE(Put(body_request_, body_response_));
    ASSERT_TRUE(Match(body_request_));
    ASSERT_TRUE(Delete(body_request_));
  }
}

TEST_P(ServiceWorkerCacheTestP, PutResponseType) {
  EXPECT_TRUE(TestResponseType(blink::WebServiceWorkerResponseTypeBasic));
  EXPECT_TRUE(TestResponseType(blink::WebServiceWorkerResponseTypeCORS));
  EXPECT_TRUE(TestResponseType(blink::WebServiceWorkerResponseTypeDefault));
  EXPECT_TRUE(TestResponseType(blink::WebServiceWorkerResponseTypeError));
  EXPECT_TRUE(TestResponseType(blink::WebServiceWorkerResponseTypeOpaque));
}

TEST_F(ServiceWorkerCacheTest, CaselessServiceWorkerResponseHeaders) {
  // ServiceWorkerCache depends on ServiceWorkerResponse having caseless
  // headers so that it can quickly lookup vary headers.
  ServiceWorkerResponse response(GURL("http://www.example.com"),
                                 200,
                                 "OK",
                                 blink::WebServiceWorkerResponseTypeDefault,
                                 ServiceWorkerHeaderMap(),
                                 "",
                                 0,
                                 GURL());
  response.headers["content-type"] = "foo";
  response.headers["Content-Type"] = "bar";
  EXPECT_EQ("bar", response.headers["content-type"]);
}

TEST_F(ServiceWorkerCacheTest, CaselessServiceWorkerFetchRequestHeaders) {
  // ServiceWorkerCache depends on ServiceWorkerFetchRequest having caseless
  // headers so that it can quickly lookup vary headers.
  ServiceWorkerFetchRequest request(GURL("http://www.example.com"),
                                         "GET",
                                         ServiceWorkerHeaderMap(),
                                         Referrer(),
                                         false);
  request.headers["content-type"] = "foo";
  request.headers["Content-Type"] = "bar";
  EXPECT_EQ("bar", request.headers["content-type"]);
}

TEST_P(ServiceWorkerCacheTestP, QuotaManagerModified) {
  EXPECT_EQ(0, quota_manager_proxy_->notify_storage_modified_count());

  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_EQ(1, quota_manager_proxy_->notify_storage_modified_count());
  EXPECT_LT(0, quota_manager_proxy_->last_notified_delta());
  int64 sum_delta = quota_manager_proxy_->last_notified_delta();

  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_EQ(2, quota_manager_proxy_->notify_storage_modified_count());
  EXPECT_LT(sum_delta, quota_manager_proxy_->last_notified_delta());
  sum_delta += quota_manager_proxy_->last_notified_delta();

  EXPECT_TRUE(Delete(body_request_));
  EXPECT_EQ(3, quota_manager_proxy_->notify_storage_modified_count());
  sum_delta += quota_manager_proxy_->last_notified_delta();

  EXPECT_TRUE(Delete(no_body_request_));
  EXPECT_EQ(4, quota_manager_proxy_->notify_storage_modified_count());
  sum_delta += quota_manager_proxy_->last_notified_delta();

  EXPECT_EQ(0, sum_delta);
}

TEST_F(ServiceWorkerCacheMemoryOnlyTest, MemoryBackedSize) {
  EXPECT_EQ(0, cache_->MemoryBackedSize());
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_LT(0, cache_->MemoryBackedSize());
  int64 no_body_size = cache_->MemoryBackedSize();

  EXPECT_TRUE(Delete(no_body_request_));
  EXPECT_EQ(0, cache_->MemoryBackedSize());

  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_LT(no_body_size, cache_->MemoryBackedSize());

  EXPECT_TRUE(Delete(body_request_));
  EXPECT_EQ(0, cache_->MemoryBackedSize());
}

TEST_F(ServiceWorkerCacheTest, MemoryBackedSizePersistent) {
  EXPECT_EQ(0, cache_->MemoryBackedSize());
  EXPECT_TRUE(Put(no_body_request_, no_body_response_));
  EXPECT_EQ(0, cache_->MemoryBackedSize());
}

TEST_P(ServiceWorkerCacheTestP, OpsFailOnClosedBackendNeverCreated) {
  cache_->set_delay_backend_creation(
      true);  // Will hang the test if a backend is created.
  EXPECT_TRUE(Close());
  VerifyAllOpsFail();
}

TEST_P(ServiceWorkerCacheTestP, OpsFailOnClosedBackend) {
  // Create the backend and put something in it.
  EXPECT_TRUE(Put(body_request_, body_response_));
  EXPECT_TRUE(Close());
  VerifyAllOpsFail();
}

TEST_P(ServiceWorkerCacheTestP, VerifySerialScheduling) {
  // Start two operations, the first one is delayed but the second isn't. The
  // second should wait for the first.
  EXPECT_TRUE(Keys());  // Opens the backend.
  DelayableBackend* delayable_backend = cache_->UseDelayableBackend();
  delayable_backend->set_delay_open(true);

  scoped_ptr<ServiceWorkerResponse> response1 =
      CopyFetchResponse(body_response_);
  response1->status_code = 1;

  scoped_ptr<base::RunLoop> close_loop1(new base::RunLoop());
  cache_->Put(CopyFetchRequest(body_request_), response1.Pass(),
              base::Bind(&ServiceWorkerCacheTest::ResponseAndErrorCallback,
                         base::Unretained(this), close_loop1.get()));

  // Blocks on opening the cache entry.
  base::RunLoop().RunUntilIdle();

  delayable_backend->set_delay_open(false);
  scoped_ptr<ServiceWorkerResponse> response2 =
      CopyFetchResponse(body_response_);
  response2->status_code = 2;
  scoped_ptr<base::RunLoop> close_loop2(new base::RunLoop());
  cache_->Put(CopyFetchRequest(body_request_), response2.Pass(),
              base::Bind(&ServiceWorkerCacheTest::ResponseAndErrorCallback,
                         base::Unretained(this), close_loop2.get()));

  // The second put operation should wait for the first to complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_response_);

  delayable_backend->OpenEntryContinue();
  close_loop1->Run();
  EXPECT_EQ(1, callback_response_->status_code);
  close_loop2->Run();
  EXPECT_EQ(2, callback_response_->status_code);
}

INSTANTIATE_TEST_CASE_P(ServiceWorkerCacheTest,
                        ServiceWorkerCacheTestP,
                        ::testing::Values(false, true));

}  // namespace content
