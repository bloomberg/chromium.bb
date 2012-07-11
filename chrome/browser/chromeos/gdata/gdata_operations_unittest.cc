// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/gdata_operations.h"
#include "chrome/browser/chromeos/gdata/gdata_operation_runner.h"
#include "chrome/browser/chromeos/gdata/gdata_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {

namespace {

class JsonParseTestGetDataOperation : public GetDataOperation {
 public:
  JsonParseTestGetDataOperation(GDataOperationRegistry* registry,
                                Profile* profile,
                                const GetDataCallback& callback)
      : GetDataOperation(registry, profile, callback) {
  }

  virtual ~JsonParseTestGetDataOperation() {
  }

  void NotifyStart() {
    NotifyStartToOperationRegistry();
  }

  void NotifySuccess() {
    NotifySuccessToOperationRegistry();
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

class GDataOperationsTest : public testing::Test {
 protected:
  GDataOperationsTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);
    runner_.reset(new GDataOperationRunner(profile_.get()));
    runner_->Initialize();
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<GDataOperationRunner> runner_;
};

void GetDataOperationParseJsonCallback(GDataErrorCode* error_out,
                                       scoped_ptr<base::Value>* value_out,
                                       GDataErrorCode error_in,
                                       scoped_ptr<base::Value> value_in) {
  value_out->swap(value_in);
  *error_out = error_in;
}


TEST_F(GDataOperationsTest, GetDataOperationParseJson) {

  scoped_ptr<base::Value> value;
  GDataErrorCode error;
  gdata::GetDataCallback cb = base::Bind(&GetDataOperationParseJsonCallback,
                                         &error,
                                         &value);
  JsonParseTestGetDataOperation* getData =
      new JsonParseTestGetDataOperation(runner_->operation_registry(),
                                        profile_.get(),
                                        cb);
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
    message_loop_.RunAllPending();

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
    EXPECT_TRUE(dict->Remove("foo", &dict_literals[0]));
    EXPECT_TRUE(dict->Remove("bar", &dict_literals[1]));
    EXPECT_TRUE(dict->Remove("baz", &dict_strings[0]));
    EXPECT_TRUE(dict->Remove("moo", &dict_strings[1]));
    ASSERT_EQ(2u, list->GetSize());
    EXPECT_TRUE(list->Remove(0, &list_values[0]));
    EXPECT_TRUE(list->Remove(0, &list_values[1]));
  }

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
    message_loop_.RunAllPending();

    ASSERT_TRUE(value.get() == NULL);
    EXPECT_EQ(GDATA_PARSE_ERROR, error);
  }

  getData->NotifySuccess();
}

} // namespace gdata
