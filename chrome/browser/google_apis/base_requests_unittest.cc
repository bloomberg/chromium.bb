// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/base_requests.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/google_apis/dummy_auth_service.h"
#include "chrome/browser/google_apis/request_sender.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kValidJsonString[] = "{ \"test\": 123 }";
const char kInvalidJsonString[] = "$$$";

class FakeGetDataRequest : public GetDataRequest {
 public:
  explicit FakeGetDataRequest(RequestSender* sender,
                              const GetDataCallback& callback)
      : GetDataRequest(sender, callback) {
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
  virtual void SetUp() OVERRIDE {
    sender_.reset(new RequestSender(new DummyAuthService,
                                    NULL /* url_request_context_getter */,
                                    message_loop_.message_loop_proxy(),
                                    std::string() /* custom user agent */));
  }

  base::MessageLoop message_loop_;
  scoped_ptr<RequestSender> sender_;
};

TEST_F(BaseRequestsTest, ParseValidJson) {
  scoped_ptr<base::Value> json;
  ParseJson(message_loop_.message_loop_proxy(),
            kValidJsonString,
            base::Bind(test_util::CreateCopyResultCallback(&json)));
  base::RunLoop().RunUntilIdle();

  DictionaryValue* root_dict = NULL;
  ASSERT_TRUE(json);
  ASSERT_TRUE(json->GetAsDictionary(&root_dict));

  int int_value = 0;
  ASSERT_TRUE(root_dict->GetInteger("test", &int_value));
  EXPECT_EQ(123, int_value);
}

TEST_F(BaseRequestsTest, ParseInvalidJson) {
  // Initialize with a valid pointer to verify that null is indeed assigned.
  scoped_ptr<base::Value> json(base::Value::CreateNullValue());
  ParseJson(message_loop_.message_loop_proxy(),
            kInvalidJsonString,
            base::Bind(test_util::CreateCopyResultCallback(&json)));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(json);
}

TEST_F(BaseRequestsTest, GetDataRequestParseValidResponse) {
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> value;
  FakeGetDataRequest* get_data_request =
      new FakeGetDataRequest(
          sender_.get(),
          base::Bind(test_util::CreateCopyResultCallback(&error, &value)));

  get_data_request->ParseResponse(HTTP_SUCCESS, kValidJsonString);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(value);
}

TEST_F(BaseRequestsTest, GetDataRequestParseInvalidResponse) {
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> value;
  FakeGetDataRequest* get_data_request =
      new FakeGetDataRequest(
          sender_.get(),
          base::Bind(test_util::CreateCopyResultCallback(&error, &value)));

  get_data_request->ParseResponse(HTTP_SUCCESS, kInvalidJsonString);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_PARSE_ERROR, error);
  EXPECT_FALSE(value);
}

}  // namespace google_apis
