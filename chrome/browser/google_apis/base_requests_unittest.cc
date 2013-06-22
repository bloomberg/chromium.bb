// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/base_requests.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/google_apis/request_sender.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kValidJsonString[] = "{ \"test\": 123 }";
const char kInvalidJsonString[] = "$$$";

class FakeGetDataRequest : public GetDataRequest {
 public:
  explicit FakeGetDataRequest(RequestSender* runner,
                              const GetDataCallback& callback)
      : GetDataRequest(runner, NULL, callback) {
  }

  virtual ~FakeGetDataRequest() {
  }

 protected:
  virtual GURL GetURL() const OVERRIDE {
    NOTREACHED();  // This method is not called in tests.
    return GURL();
  }
};

}  // namespace

class BaseRequestsTest : public testing::Test {
 public:
  BaseRequestsTest()
      : parse_json_callback_called_(false),
        get_data_callback_called_(false) {
  }

  void ParseJsonCallback(scoped_ptr<base::Value> value) {
    parse_json_result_ = value.Pass();;
    parse_json_callback_called_ = true;
  }

  void GetDataCallback(GDataErrorCode error, scoped_ptr<base::Value> value) {
    get_data_result_error_ = error;
    get_data_result_value_ = value.Pass();
    get_data_callback_called_ = true;
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);
    runner_.reset(new RequestSender(profile_.get(),
                                    NULL /* url_request_context_getter */,
                                    std::vector<std::string>() /* scopes */,
                                    std::string() /* custom user agent */));
    runner_->Initialize();
    LOG(ERROR) << "Initialized.";
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<RequestSender> runner_;

  // Following members stores data returned with callbacks to be verified
  // by tests.
  scoped_ptr<base::Value> parse_json_result_;
  bool parse_json_callback_called_;
  GDataErrorCode get_data_result_error_;
  scoped_ptr<base::Value> get_data_result_value_;
  bool get_data_callback_called_;
};

TEST_F(BaseRequestsTest, ParseValidJson) {
  ParseJson(kValidJsonString,
            base::Bind(&BaseRequestsTest::ParseJsonCallback,
                       base::Unretained(this)));
  // Should wait for a blocking pool task, as the JSON parsing is done in the
  // blocking pool.
  test_util::RunBlockingPoolTask();

  ASSERT_TRUE(parse_json_callback_called_);
  ASSERT_TRUE(parse_json_result_.get());

  DictionaryValue* root_dict = NULL;
  ASSERT_TRUE(parse_json_result_->GetAsDictionary(&root_dict));

  int int_value = 0;
  ASSERT_TRUE(root_dict->GetInteger("test", &int_value));
  EXPECT_EQ(123, int_value);
}

TEST_F(BaseRequestsTest, ParseInvalidJson) {
  ParseJson(kInvalidJsonString,
            base::Bind(&BaseRequestsTest::ParseJsonCallback,
                       base::Unretained(this)));
  // Should wait for a blocking pool task, as the JSON parsing is done in the
  // blocking pool.
  test_util::RunBlockingPoolTask();

  ASSERT_TRUE(parse_json_callback_called_);
  ASSERT_FALSE(parse_json_result_.get());
}

TEST_F(BaseRequestsTest, GetDataRequestParseValidResponse) {
  FakeGetDataRequest* get_data_request =
      new FakeGetDataRequest(
          runner_.get(),
          base::Bind(&BaseRequestsTest::GetDataCallback,
                     base::Unretained(this)));

  get_data_request->ParseResponse(HTTP_SUCCESS, kValidJsonString);
  // Should wait for a blocking pool task, as the JSON parsing is done in the
  // blocking pool.
  test_util::RunBlockingPoolTask();

  ASSERT_TRUE(get_data_callback_called_);
  ASSERT_EQ(HTTP_SUCCESS, get_data_result_error_);
  ASSERT_TRUE(get_data_result_value_.get());
}

TEST_F(BaseRequestsTest, GetDataRequestParseInvalidResponse) {
  FakeGetDataRequest* get_data_request =
      new FakeGetDataRequest(
          runner_.get(),
          base::Bind(&BaseRequestsTest::GetDataCallback,
                     base::Unretained(this)));

  get_data_request->ParseResponse(HTTP_SUCCESS, kInvalidJsonString);
  // Should wait for a blocking pool task, as the JSON parsing is done in the
  // blocking pool.
  test_util::RunBlockingPoolTask();

  ASSERT_TRUE(get_data_callback_called_);
  ASSERT_EQ(GDATA_PARSE_ERROR, get_data_result_error_);
  ASSERT_FALSE(get_data_result_value_.get());
}

}  // namespace google_apis
