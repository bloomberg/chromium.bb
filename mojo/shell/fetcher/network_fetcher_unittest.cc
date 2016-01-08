// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/fetcher/network_fetcher.h"

#include <stdint.h>

#include <utility>

#include "base/at_exit.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace {

const char k200Request[] = "http://request_expect_200";
const char k404Request[] = "http://request_expect_404";
const char k504Request[] = "http://request_expect_504";
const char kErrorRequest[] = "http://request_expect_error";

class TestURLLoaderImpl : public URLLoader {
 public:
  explicit TestURLLoaderImpl(InterfaceRequest<URLLoader> request)
      : binding_(this, std::move(request)) {}
  ~TestURLLoaderImpl() override {}

 private:
  // URLLoader implementation.
  void Start(URLRequestPtr request,
             const Callback<void(URLResponsePtr)>& callback) override {
    URLResponsePtr response(URLResponse::New());
    response->url = request->url;
    if (request->url == std::string(k200Request)) {
      response->mime_type = "text/html";
      response->status_code = 200;
    } else if (request->url == std::string(k404Request)) {
      response->mime_type = "text/html";
      response->status_code = 404;
    } else if (request->url == std::string(k504Request)) {
      response->mime_type = "text/html";
      response->status_code = 504;
    } else {
      response->error = NetworkError::New();
      response->error->code = -2;
    }
    callback.Run(std::move(response));
  }
  void FollowRedirect(const Callback<void(URLResponsePtr)>& callback) override {
    NOTREACHED();
  }
  void QueryStatus(
      const Callback<void(URLLoaderStatusPtr)>& callback) override {
    NOTREACHED();
  }

  StrongBinding<URLLoader> binding_;
  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderImpl);
};

class TestURLLoaderFactoryImpl : public URLLoaderFactory {
 public:
  explicit TestURLLoaderFactoryImpl(InterfaceRequest<URLLoaderFactory> request)
      : binding_(this, std::move(request)) {}
  ~TestURLLoaderFactoryImpl() override {}

 private:
  // URLLoaderFactory implementation.
  void CreateURLLoader(InterfaceRequest<URLLoader> loader) override {
    new TestURLLoaderImpl(std::move(loader));
  }

  StrongBinding<URLLoaderFactory> binding_;
  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderFactoryImpl);
};

class FetchCallbackHelper {
 public:
  FetchCallbackHelper() : run_loop_(nullptr) {}
  ~FetchCallbackHelper() {}

  Fetcher::FetchCallback GetCallback() {
    return base::Bind(&FetchCallbackHelper::CallbackHandler,
                      base::Unretained(this));
  }

  void WaitForCallback() {
    base::RunLoop run_loop;
    base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
    run_loop.Run();
  }

  Fetcher* fetcher() const { return fetcher_.get(); }

 private:
  void CallbackHandler(scoped_ptr<Fetcher> fetcher) {
    fetcher_ = std::move(fetcher);
    if (run_loop_)
      run_loop_->Quit();
  }

  // If it is not null, it points to a stack-allocated base::RunLoop instance in
  // WaitForCallback().
  base::RunLoop* run_loop_;
  scoped_ptr<Fetcher> fetcher_;
  DISALLOW_COPY_AND_ASSIGN(FetchCallbackHelper);
};

class NetworkFetcherTest : public testing::Test {
 public:
  NetworkFetcherTest() {}
  ~NetworkFetcherTest() override {}

 protected:
  // Overridden from testing::Test:
  void SetUp() override {
    // Automatically destroyed when |url_loader_factory_| is closed.
    new TestURLLoaderFactoryImpl(GetProxy(&url_loader_factory_));
  }

  // When |expect_fetch_success| is false, |expected_status_code| is ignored.
  void TestFetchURL(const std::string& url,
                    bool expect_fetch_success,
                    uint32_t expected_status_code) {
    FetchCallbackHelper helper;

    URLRequestPtr request(URLRequest::New());
    request->url = url;
    new NetworkFetcher(true, std::move(request), url_loader_factory_.get(),
                       helper.GetCallback());
    helper.WaitForCallback();

    if (!expect_fetch_success) {
      ASSERT_FALSE(helper.fetcher());
    } else {
      ASSERT_TRUE(helper.fetcher());
      URLResponsePtr response = helper.fetcher()->AsURLResponse(nullptr, 0);
      ASSERT_TRUE(response);
      EXPECT_EQ(url, response->url);
      EXPECT_EQ(expected_status_code, response->status_code);
    }
  }

 private:
  base::ShadowingAtExitManager at_exit_;
  base::MessageLoop loop_;
  URLLoaderFactoryPtr url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkFetcherTest);
};

TEST_F(NetworkFetcherTest, FetchSucceeded200) {
  TestFetchURL(k200Request, true, 200u);
}

TEST_F(NetworkFetcherTest, FetchSucceeded404) {
  TestFetchURL(k404Request, true, 404u);
}

TEST_F(NetworkFetcherTest, FetchSucceeded504) {
  TestFetchURL(k504Request, true, 504u);
}

TEST_F(NetworkFetcherTest, FetchFailed) {
  TestFetchURL(kErrorRequest, false, 0u);
}

}  // namespace
}  // namespace shell
}  // namespace mojo
