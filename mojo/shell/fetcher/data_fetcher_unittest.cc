// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/fetcher/data_fetcher.h"

#include <stdint.h>

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace {

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

class DataFetcherTest : public testing::Test {
 public:
  DataFetcherTest() {}
  ~DataFetcherTest() override {}

 protected:
  void TestFetchURL(const std::string& url,
                    uint32_t expected_status_code,
                    const std::string& expected_mime_type,
                    const std::string& expected_body) {
    FetchCallbackHelper helper;
    DataFetcher::Start(GURL(url), helper.GetCallback());
    helper.WaitForCallback();

    ASSERT_TRUE(helper.fetcher());
    URLResponsePtr response = helper.fetcher()->AsURLResponse(nullptr, 0);
    ASSERT_TRUE(response);
    EXPECT_EQ(url, response->url);
    EXPECT_EQ(expected_status_code, response->status_code);

    if (expected_status_code != 200)
      return;

    ASSERT_TRUE(response->body.is_valid());
    EXPECT_EQ(expected_mime_type, response->mime_type);

    uint32_t num_bytes = 0;
    Handle body_handle = response->body.release();

    MojoHandleSignalsState hss;
    ASSERT_EQ(MOJO_RESULT_OK,
              MojoWait(body_handle.value(), MOJO_HANDLE_SIGNAL_READABLE,
                       MOJO_DEADLINE_INDEFINITE, &hss));

    MojoResult result = MojoReadData(body_handle.value(), nullptr, &num_bytes,
                                     MOJO_READ_DATA_FLAG_QUERY);
    ASSERT_EQ(MOJO_RESULT_OK, result);

    scoped_ptr<char[]> body(new char[num_bytes]);
    result = MojoReadData(body_handle.value(), body.get(), &num_bytes,
                          MOJO_READ_DATA_FLAG_ALL_OR_NONE);
    ASSERT_EQ(MOJO_RESULT_OK, result);
    EXPECT_EQ(expected_body, std::string(body.get(), num_bytes));
  }

 private:
  base::MessageLoop loop_;

  DISALLOW_COPY_AND_ASSIGN(DataFetcherTest);
};

TEST_F(DataFetcherTest, BasicSuccess) {
  TestFetchURL("data:text/html,Hello world", 200, "text/html", "Hello world");
}

TEST_F(DataFetcherTest, BasicFailure) {
  TestFetchURL("about:blank", 400, std::string(), std::string());
  TestFetchURL("data:;base64,aGVs_-_-", 400, std::string(), std::string());
}

}  // namespace
}  // namespace shell
}  // namespace mojo
