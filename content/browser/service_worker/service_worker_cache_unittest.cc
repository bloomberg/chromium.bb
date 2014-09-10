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
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "storage/common/blob/blob_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/blob/blob_storage_context.h"
#include "webkit/browser/blob/blob_url_request_job_factory.h"

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

}  // namespace

class ServiceWorkerCacheTest : public testing::Test {
 public:
  ServiceWorkerCacheTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        callback_error_(ServiceWorkerCache::ErrorTypeOK) {}

  virtual void SetUp() OVERRIDE {
    ChromeBlobStorageContext* blob_storage_context =
        ChromeBlobStorageContext::GetFor(&browser_context_);
    // Wait for chrome_blob_storage_context to finish initializing.
    base::RunLoop().RunUntilIdle();
    blob_storage_context_ = blob_storage_context->context();

    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
    url_request_job_factory_->SetProtocolHandler(
        "blob", CreateMockBlobProtocolHandler(blob_storage_context->context()));

    net::URLRequestContext* url_request_context =
        browser_context_.GetRequestContext()->GetURLRequestContext();

    url_request_context->set_job_factory(url_request_job_factory_.get());

    CreateRequests(blob_storage_context);

    if (MemoryOnly()) {
      cache_ = ServiceWorkerCache::CreateMemoryCache(
          url_request_context,
          blob_storage_context->context()->AsWeakPtr());
    } else {
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
      cache_ = ServiceWorkerCache::CreatePersistentCache(
          temp_dir_.path(),
          url_request_context,
          blob_storage_context->context()->AsWeakPtr());
    }
    CreateBackend();
  }

  void CreateRequests(ChromeBlobStorageContext* blob_storage_context) {
    std::map<std::string, std::string> headers;
    headers.insert(std::make_pair("a", "a"));
    headers.insert(std::make_pair("b", "b"));
    body_request_.reset(new ServiceWorkerFetchRequest(
        GURL("http://example.com/body.html"), "GET", headers, GURL(""), false));
    no_body_request_.reset(
        new ServiceWorkerFetchRequest(GURL("http://example.com/no_body.html"),
                                      "GET",
                                      headers,
                                      GURL(""),
                                      false));

    std::string expected_response;
    for (int i = 0; i < 100; ++i)
      expected_blob_data_ += kTestData;

    scoped_refptr<storage::BlobData> blob_data(
        new storage::BlobData("blob-id:myblob"));
    blob_data->AppendData(expected_blob_data_);

    blob_handle_ =
        blob_storage_context->context()->AddFinishedBlob(blob_data.get());

    body_response_.reset(
        new ServiceWorkerResponse(GURL("http://example.com/body.html"),
                                  200,
                                  "OK",
                                  headers,
                                  blob_handle_->uuid()));

    no_body_response_.reset(new ServiceWorkerResponse(
        GURL("http://example.com/no_body.html"), 200, "OK", headers, ""));
  }

  void CreateBackend() {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_->CreateBackend(base::Bind(&ServiceWorkerCacheTest::ErrorTypeCallback,
                                     base::Unretained(this),
                                     base::Unretained(loop.get())));
    loop->Run();
    EXPECT_EQ(ServiceWorkerCache::ErrorTypeOK, callback_error_);
  }

  bool Put(ServiceWorkerFetchRequest* request,
           ServiceWorkerResponse* response) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Put(request,
                response,
                base::Bind(&ServiceWorkerCacheTest::ErrorTypeCallback,
                           base::Unretained(this),
                           base::Unretained(loop.get())));
    // TODO(jkarlin): These functions should use base::RunLoop().RunUntilIdle()
    // once the cache uses a passed in MessageLoopProxy instead of the CACHE
    // thread.
    loop->Run();

    return callback_error_ == ServiceWorkerCache::ErrorTypeOK;
  }

  bool Match(ServiceWorkerFetchRequest* request) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Match(request,
                  base::Bind(&ServiceWorkerCacheTest::ResponseAndErrorCallback,
                             base::Unretained(this),
                             base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == ServiceWorkerCache::ErrorTypeOK;
  }

  bool Delete(ServiceWorkerFetchRequest* request) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());

    cache_->Delete(request,
                   base::Bind(&ServiceWorkerCacheTest::ErrorTypeCallback,
                              base::Unretained(this),
                              base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == ServiceWorkerCache::ErrorTypeOK;
  }

  void ErrorTypeCallback(base::RunLoop* run_loop,
                         ServiceWorkerCache::ErrorType error) {
    callback_error_ = error;
    run_loop->Quit();
  }

  void ResponseAndErrorCallback(
      base::RunLoop* run_loop,
      ServiceWorkerCache::ErrorType error,
      scoped_ptr<ServiceWorkerResponse> response,
      scoped_ptr<storage::BlobDataHandle> body_handle) {
    callback_error_ = error;
    callback_response_ = response.Pass();

    if (error == ServiceWorkerCache::ErrorTypeOK &&
        !callback_response_->blob_uuid.empty()) {
      callback_response_data_ = body_handle.Pass();
    }

    run_loop->Quit();
  }

  void CopyBody(storage::BlobDataHandle* blob_handle, std::string* output) {
    storage::BlobData* data = blob_handle->data();
    std::vector<storage::BlobData::Item> items = data->items();
    for (size_t i = 0, max = items.size(); i < max; ++i)
      output->append(items[i].bytes(), items[i].length());
  }

  virtual bool MemoryOnly() { return false; }

 protected:
  TestBrowserContext browser_context_;
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  storage::BlobStorageContext* blob_storage_context_;

  base::ScopedTempDir temp_dir_;
  scoped_ptr<ServiceWorkerCache> cache_;

  scoped_ptr<ServiceWorkerFetchRequest> body_request_;
  scoped_ptr<ServiceWorkerResponse> body_response_;
  scoped_ptr<ServiceWorkerFetchRequest> no_body_request_;
  scoped_ptr<ServiceWorkerResponse> no_body_response_;
  scoped_ptr<storage::BlobDataHandle> blob_handle_;
  std::string expected_blob_data_;

  ServiceWorkerCache::ErrorType callback_error_;
  scoped_ptr<ServiceWorkerResponse> callback_response_;
  scoped_ptr<storage::BlobDataHandle> callback_response_data_;
};

class ServiceWorkerCacheTestP : public ServiceWorkerCacheTest,
                                public testing::WithParamInterface<bool> {
  virtual bool MemoryOnly() OVERRIDE { return !GetParam(); }
};

TEST_P(ServiceWorkerCacheTestP, PutNoBody) {
  EXPECT_TRUE(Put(no_body_request_.get(), no_body_response_.get()));
}

TEST_P(ServiceWorkerCacheTestP, PutBody) {
  EXPECT_TRUE(Put(body_request_.get(), body_response_.get()));
}

TEST_P(ServiceWorkerCacheTestP, PutBodyDropBlobRef) {
  scoped_ptr<base::RunLoop> loop(new base::RunLoop());
  cache_->Put(body_request_.get(),
              body_response_.get(),
              base::Bind(&ServiceWorkerCacheTestP::ErrorTypeCallback,
                         base::Unretained(this),
                         base::Unretained(loop.get())));
  // The handle should be held by the cache now so the deref here should be
  // okay.
  blob_handle_.reset();
  loop->Run();

  EXPECT_EQ(ServiceWorkerCache::ErrorTypeOK, callback_error_);
}

TEST_P(ServiceWorkerCacheTestP, DeleteNoBody) {
  EXPECT_TRUE(Put(no_body_request_.get(), no_body_response_.get()));
  EXPECT_TRUE(Match(no_body_request_.get()));
  EXPECT_TRUE(Delete(no_body_request_.get()));
  EXPECT_FALSE(Match(no_body_request_.get()));
  EXPECT_FALSE(Delete(no_body_request_.get()));
  EXPECT_TRUE(Put(no_body_request_.get(), no_body_response_.get()));
  EXPECT_TRUE(Match(no_body_request_.get()));
  EXPECT_TRUE(Delete(no_body_request_.get()));
}

TEST_P(ServiceWorkerCacheTestP, DeleteBody) {
  EXPECT_TRUE(Put(body_request_.get(), body_response_.get()));
  EXPECT_TRUE(Match(body_request_.get()));
  EXPECT_TRUE(Delete(body_request_.get()));
  EXPECT_FALSE(Match(body_request_.get()));
  EXPECT_FALSE(Delete(body_request_.get()));
  EXPECT_TRUE(Put(body_request_.get(), body_response_.get()));
  EXPECT_TRUE(Match(body_request_.get()));
  EXPECT_TRUE(Delete(body_request_.get()));
}

TEST_P(ServiceWorkerCacheTestP, MatchNoBody) {
  EXPECT_TRUE(Put(no_body_request_.get(), no_body_response_.get()));
  EXPECT_TRUE(Match(no_body_request_.get()));
  EXPECT_EQ(200, callback_response_->status_code);
  EXPECT_STREQ("OK", callback_response_->status_text.c_str());
  EXPECT_STREQ("http://example.com/no_body.html",
               callback_response_->url.spec().c_str());
}

TEST_P(ServiceWorkerCacheTestP, MatchBody) {
  EXPECT_TRUE(Put(body_request_.get(), body_response_.get()));
  EXPECT_TRUE(Match(body_request_.get()));
  EXPECT_EQ(200, callback_response_->status_code);
  EXPECT_STREQ("OK", callback_response_->status_text.c_str());
  EXPECT_STREQ("http://example.com/body.html",
               callback_response_->url.spec().c_str());
  std::string response_body;
  CopyBody(callback_response_data_.get(), &response_body);
  EXPECT_STREQ(expected_blob_data_.c_str(), response_body.c_str());
}

TEST_P(ServiceWorkerCacheTestP, QuickStressNoBody) {
  for (int i = 0; i < 100; ++i) {
    EXPECT_FALSE(Match(no_body_request_.get()));
    EXPECT_TRUE(Put(no_body_request_.get(), no_body_response_.get()));
    EXPECT_TRUE(Match(no_body_request_.get()));
    EXPECT_TRUE(Delete(no_body_request_.get()));
  }
}

TEST_P(ServiceWorkerCacheTestP, QuickStressBody) {
  for (int i = 0; i < 100; ++i) {
    ASSERT_FALSE(Match(body_request_.get()));
    ASSERT_TRUE(Put(body_request_.get(), body_response_.get()));
    ASSERT_TRUE(Match(body_request_.get()));
    ASSERT_TRUE(Delete(body_request_.get()));
  }
}

INSTANTIATE_TEST_CASE_P(ServiceWorkerCacheTest,
                        ServiceWorkerCacheTestP,
                        ::testing::Values(false, true));

}  // namespace content
