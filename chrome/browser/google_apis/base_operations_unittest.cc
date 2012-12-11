// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/google_apis/base_operations.h"
#include "chrome/browser/google_apis/operation_runner.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

// The class is used to test that the JSON parsing is done in the blocking
// pool, instead of UI thread.
class JsonParseTestGetDataOperation : public GetDataOperation {
 public:
  JsonParseTestGetDataOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const GetDataCallback& callback)
      : GetDataOperation(registry, url_request_context_getter, callback) {
  }

  virtual ~JsonParseTestGetDataOperation() {
  }

  void NotifyStart() {
    NotifyStartToOperationRegistry();
  }

 protected:
  // GetDataOperation overrides:
  virtual GURL GetURL() const OVERRIDE {
    // This method is never called because this test does not fetch json from
    // network.
    NOTREACHED();
    return GURL();
  }
};

}  // namespace

class BaseOperationsTest : public testing::Test {
 protected:
  BaseOperationsTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);
    runner_.reset(new OperationRunner(profile_.get(),
                                      NULL  /* url_request_context_getter */,
                                      std::vector<std::string>() /* scopes */,
                                      "" /* custom_user_agent*/));
    runner_->Initialize();
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<OperationRunner> runner_;
};

TEST_F(BaseOperationsTest, GetDataOperation_ParseValidJson) {
  scoped_ptr<base::Value> value;
  GDataErrorCode error = GDATA_OTHER_ERROR;
  JsonParseTestGetDataOperation* get_data =
      new JsonParseTestGetDataOperation(
          runner_->operation_registry(),
          NULL,  // request_context_getter.
          base::Bind(&test_util::CopyResultsFromGetDataCallback,
                     &error,
                     &value));
  get_data->NotifyStart();

  const std::string valid_json_str = "{ \"test\": 123 }";

  get_data->ParseResponse(HTTP_SUCCESS, valid_json_str);
  // Should wait for a blocking pool task, as the JSON parsing is done in the
  // blocking pool.
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(value.get());

  DictionaryValue* root_dict = NULL;
  ASSERT_TRUE(value->GetAsDictionary(&root_dict));

  int int_value = 0;
  ASSERT_TRUE(root_dict->GetInteger("test", &int_value));
  EXPECT_EQ(123, int_value);
}

TEST_F(BaseOperationsTest, GetDataOperation_ParseInvalidJson) {
  scoped_ptr<base::Value> value;
  GDataErrorCode error = GDATA_OTHER_ERROR;
  JsonParseTestGetDataOperation* get_data =
      new JsonParseTestGetDataOperation(
          runner_->operation_registry(),
          NULL,  // request_context_getter.
          base::Bind(&test_util::CopyResultsFromGetDataCallback,
                     &error,
                     &value));
  get_data->NotifyStart();

  const std::string invalid_json_str = "$$$";

  get_data->ParseResponse(HTTP_SUCCESS, invalid_json_str);
  test_util::RunBlockingPoolTask();

  // The parsing should fail.
  EXPECT_EQ(GDATA_PARSE_ERROR, error);
  ASSERT_FALSE(value.get());
}

}  // namespace google_apis
