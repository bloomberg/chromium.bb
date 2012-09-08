// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/drive_test_util.h"
#include "chrome/browser/chromeos/gdata/gdata_operations.h"
#include "chrome/browser/chromeos/gdata/operation_runner.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {

namespace {

class JsonParseTestGetDataOperation : public GetDataOperation {
 public:
  JsonParseTestGetDataOperation(OperationRegistry* registry,
                                const GetDataCallback& callback)
      : GetDataOperation(registry, callback) {
  }

  virtual ~JsonParseTestGetDataOperation() {
  }

  void NotifyStart() {
    NotifyStartToOperationRegistry();
  }

  void NotifySuccess() {
    NotifySuccessToOperationRegistry();
  }

  void NotifyFailure() {
    NotifyFinish(OperationRegistry::OPERATION_FAILED);
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

void GetDataOperationParseJsonCallback(GDataErrorCode* error_out,
                                       scoped_ptr<base::Value>* value_out,
                                       GDataErrorCode error_in,
                                       scoped_ptr<base::Value> value_in) {
  value_out->swap(value_in);
  *error_out = error_in;
}

}  // namespace

class GDataOperationsTest : public testing::Test {
 protected:
  GDataOperationsTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);
    runner_.reset(new OperationRunner(profile_.get(),
                                      std::vector<std::string>()));
    runner_->Initialize();
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<OperationRunner> runner_;
};

TEST_F(GDataOperationsTest, GetDataOperationParseJson) {
  scoped_ptr<base::Value> value;
  GDataErrorCode error;
  gdata::GetDataCallback cb = base::Bind(&GetDataOperationParseJsonCallback,
                                         &error,
                                         &value);
  JsonParseTestGetDataOperation* getData =
      new JsonParseTestGetDataOperation(runner_->operation_registry(), cb);
  getData->NotifyStart();

  // Parses a valid json string.
  {
    std::string valid_json_str =
        "{"
        "  \"test\": {"
        "    \"foo\": true,"
        "    \"bar\": 3.14,"
        "    \"baz\": \"bat\","
        "    \"moo\": \"cow\""
        "  },"
        "  \"list\": ["
        "    \"a\","
        "    \"b\""
        "  ]"
        "}";

    getData->ParseResponse(HTTP_SUCCESS, valid_json_str);
    test_util::RunBlockingPoolTask();

    EXPECT_EQ(HTTP_SUCCESS, error);
    ASSERT_TRUE(value.get());

    DictionaryValue* root_dict = NULL;
    ASSERT_TRUE(value->GetAsDictionary(&root_dict));

    DictionaryValue* dict = NULL;
    ListValue* list = NULL;
    ASSERT_TRUE(root_dict->GetDictionary("test", &dict));
    ASSERT_TRUE(root_dict->GetList("list", &list));

    Value* dict_literals[2] = {0};
    Value* dict_strings[2] = {0};
    Value* list_values[2] = {0};
    EXPECT_TRUE(dict->Get("foo", &dict_literals[0]));
    EXPECT_TRUE(dict->Get("bar", &dict_literals[1]));
    EXPECT_TRUE(dict->Get("baz", &dict_strings[0]));
    EXPECT_TRUE(dict->Get("moo", &dict_strings[1]));
    ASSERT_EQ(2u, list->GetSize());
    EXPECT_TRUE(list->Get(0, &list_values[0]));
    EXPECT_TRUE(list->Get(1, &list_values[1]));

    bool b = false;
    double d = 0;
    std::string s;
    EXPECT_TRUE(dict_literals[0]->GetAsBoolean(&b));
    EXPECT_TRUE(b);
    EXPECT_TRUE(dict_literals[1]->GetAsDouble(&d));
    EXPECT_EQ(3.14, d);
    EXPECT_TRUE(dict_strings[0]->GetAsString(&s));
    EXPECT_EQ("bat", s);
    EXPECT_TRUE(dict_strings[1]->GetAsString(&s));
    EXPECT_EQ("cow", s);
    EXPECT_TRUE(list_values[0]->GetAsString(&s));
    EXPECT_EQ("a", s);
    EXPECT_TRUE(list_values[1]->GetAsString(&s));
    EXPECT_EQ("b", s);
  }
}

TEST_F(GDataOperationsTest, GetDataOperationParseInvalidJson) {
  scoped_ptr<base::Value> value;
  GDataErrorCode error;
  gdata::GetDataCallback cb = base::Bind(&GetDataOperationParseJsonCallback,
                                         &error,
                                         &value);
  JsonParseTestGetDataOperation* getData =
      new JsonParseTestGetDataOperation(runner_->operation_registry(), cb);
  getData->NotifyStart();

  // Parses an invalid json string.
  {
    std::string invalid_json_str =
        "/* hogehoge *"
        "  \"test\": {"
        "    \"moo\": \"cow"
        "  "
        "  \"list\": ["
        "    \"foo\","
        "    \"bar\""
        "  ]";

    getData->ParseResponse(HTTP_SUCCESS, invalid_json_str);
    test_util::RunBlockingPoolTask();

    EXPECT_EQ(GDATA_PARSE_ERROR, error);
    ASSERT_TRUE(value.get() == NULL);
  }
}

}  // namespace gdata
