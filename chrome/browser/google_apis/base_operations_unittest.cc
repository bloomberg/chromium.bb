// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/base_operations.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/google_apis/operation_runner.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kValidJsonString[] = "{ \"test\": 123 }";
const char kInvalidJsonString[] = "$$$";

class FakeGetDataOperation : public GetDataOperation {
 public:
  explicit FakeGetDataOperation(OperationRegistry* registry,
                                const GetDataCallback& callback)
      : GetDataOperation(registry, NULL, callback) {
  }

  virtual ~FakeGetDataOperation() {
  }

  void NotifyStart() {
    NotifyStartToOperationRegistry();
  }

 protected:
  virtual GURL GetURL() const OVERRIDE {
    NOTREACHED();  // This method is not called in tests.
    return GURL();
  }
};

}  // namespace

class BaseOperationsTest : public testing::Test {
 public:
  BaseOperationsTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        parse_json_callback_called_(false),
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
    runner_.reset(new OperationRunner(profile_.get(),
                                      NULL /* url_request_context_getter */,
                                      std::vector<std::string>() /* scopes */,
                                      std::string() /* custom user agent */));
    runner_->Initialize();
    LOG(ERROR) << "Initialized.";
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<OperationRunner> runner_;

  // Following members stores data returned with callbacks to be verified
  // by tests.
  scoped_ptr<base::Value> parse_json_result_;
  bool parse_json_callback_called_;
  GDataErrorCode get_data_result_error_;
  scoped_ptr<base::Value> get_data_result_value_;
  bool get_data_callback_called_;
};

TEST_F(BaseOperationsTest, ParseValidJson) {
  ParseJson(kValidJsonString,
            base::Bind(&BaseOperationsTest::ParseJsonCallback,
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

TEST_F(BaseOperationsTest, ParseInvalidJson) {
  ParseJson(kInvalidJsonString,
            base::Bind(&BaseOperationsTest::ParseJsonCallback,
                       base::Unretained(this)));
  // Should wait for a blocking pool task, as the JSON parsing is done in the
  // blocking pool.
  test_util::RunBlockingPoolTask();

  ASSERT_TRUE(parse_json_callback_called_);
  ASSERT_FALSE(parse_json_result_.get());
}

TEST_F(BaseOperationsTest, GetDataOperationParseValidResponse) {
  FakeGetDataOperation* get_data_operation =
      new FakeGetDataOperation(
          runner_->operation_registry(),
          base::Bind(&BaseOperationsTest::GetDataCallback,
                     base::Unretained(this)));
  get_data_operation->NotifyStart();

  get_data_operation->ParseResponse(HTTP_SUCCESS, kValidJsonString);
  // Should wait for a blocking pool task, as the JSON parsing is done in the
  // blocking pool.
  test_util::RunBlockingPoolTask();

  ASSERT_TRUE(get_data_callback_called_);
  ASSERT_EQ(HTTP_SUCCESS, get_data_result_error_);
  ASSERT_TRUE(get_data_result_value_.get());
}

TEST_F(BaseOperationsTest, GetDataOperationParseInvalidResponse) {
  FakeGetDataOperation* get_data_operation =
      new FakeGetDataOperation(
          runner_->operation_registry(),
          base::Bind(&BaseOperationsTest::GetDataCallback,
                     base::Unretained(this)));
  get_data_operation->NotifyStart();

  get_data_operation->ParseResponse(HTTP_SUCCESS, kInvalidJsonString);
  // Should wait for a blocking pool task, as the JSON parsing is done in the
  // blocking pool.
  test_util::RunBlockingPoolTask();

  ASSERT_TRUE(get_data_callback_called_);
  ASSERT_EQ(GDATA_PARSE_ERROR, get_data_result_error_);
  ASSERT_FALSE(get_data_result_value_.get());
}

}  // namespace google_apis
