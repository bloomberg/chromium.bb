// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/command_executor_impl.h"
#include "chrome/test/chromedriver/commands.h"
#include "chrome/test/chromedriver/element_commands.h"
#include "chrome/test/chromedriver/fake_session_accessor.h"
#include "chrome/test/chromedriver/session_commands.h"
#include "chrome/test/chromedriver/window_commands.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webdriver/atoms.h"

TEST(CommandsTest, GetStatus) {
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  std::string session_id;
  ASSERT_EQ(kOk, ExecuteGetStatus(params, "", &value, &session_id).code());
  base::DictionaryValue* dict;
  ASSERT_TRUE(value->GetAsDictionary(&dict));
  base::Value* unused;
  ASSERT_TRUE(dict->Get("os.name", &unused));
  ASSERT_TRUE(dict->Get("os.version", &unused));
  ASSERT_TRUE(dict->Get("os.arch", &unused));
  ASSERT_TRUE(dict->Get("build.version", &unused));
}

namespace {

Status ExecuteStubQuit(
    int* count,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* value,
    std::string* out_session_id) {
  if (*count == 0) {
    EXPECT_STREQ("id", session_id.c_str());
  } else {
    EXPECT_STREQ("id2", session_id.c_str());
  }
  (*count)++;
  return Status(kOk);
}

}  // namespace

TEST(CommandsTest, QuitAll) {
  SessionMap map;
  Session session("id");
  Session session2("id2");
  map.Set(session.id,
          scoped_refptr<SessionAccessor>(new FakeSessionAccessor(&session)));
  map.Set(session2.id,
          scoped_refptr<SessionAccessor>(new FakeSessionAccessor(&session2)));

  int count = 0;
  Command cmd = base::Bind(&ExecuteStubQuit, &count);
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  std::string session_id;
  Status status =
      ExecuteQuitAll(cmd, &map, params, "", &value, &session_id);
  ASSERT_EQ(kOk, status.code());
  ASSERT_FALSE(value.get());
  ASSERT_EQ(2, count);
}

TEST(CommandsTest, Quit) {
  SessionMap map;
  Session session("id", scoped_ptr<Chrome>(new StubChrome()));
  ASSERT_TRUE(session.thread.Start());
  scoped_refptr<FakeSessionAccessor> session_accessor(
      new FakeSessionAccessor(&session));
  map.Set(session.id, session_accessor);
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  std::string out_session_id;
  ASSERT_EQ(kOk,
            ExecuteSessionCommand(&map, base::Bind(ExecuteQuit, &map), params,
                                  session.id, &value, &out_session_id).code());
  ASSERT_FALSE(map.Has(session.id));
  ASSERT_TRUE(session_accessor->IsSessionDeleted());
  ASSERT_FALSE(value.get());
}

namespace {

class FailsToQuitChrome : public StubChrome {
 public:
  FailsToQuitChrome() {}
  virtual ~FailsToQuitChrome() {}

  // Overridden from Chrome:
  virtual Status Quit() OVERRIDE {
    return Status(kUnknownError);
  }
};

}  // namespace

TEST(CommandsTest, QuitFails) {
  SessionMap map;
  Session session("id", scoped_ptr<Chrome>(new FailsToQuitChrome()));
  ASSERT_TRUE(session.thread.Start());
  scoped_refptr<FakeSessionAccessor> session_accessor(
      new FakeSessionAccessor(&session));
  map.Set(session.id, session_accessor);
  base::DictionaryValue params;
  scoped_ptr<base::Value> value;
  std::string out_session_id;
  ASSERT_EQ(kUnknownError,
            ExecuteSessionCommand(&map, base::Bind(ExecuteQuit, &map), params,
                                  session.id, &value, &out_session_id).code());
  ASSERT_FALSE(map.Has(session.id));
  ASSERT_TRUE(session_accessor->IsSessionDeleted());
  ASSERT_FALSE(value.get());
}

namespace {

enum TestScenario {
  kElementExistsQueryOnce = 0,
  kElementExistsQueryTwice,
  kElementNotExistsQueryOnce,
  kElementExistsTimeout
};

class FindElementWebView : public StubWebView {
 public:
  FindElementWebView(bool only_one, TestScenario scenario)
      : StubWebView("1"), only_one_(only_one), scenario_(scenario),
        current_count_(0) {
    switch (scenario_) {
      case kElementExistsQueryOnce:
      case kElementExistsQueryTwice:
      case kElementExistsTimeout: {
        if (only_one_) {
          base::DictionaryValue element;
          element.SetString("ELEMENT", "1");
          result_.reset(element.DeepCopy());
        } else {
          base::DictionaryValue element1;
          element1.SetString("ELEMENT", "1");
          base::DictionaryValue element2;
          element2.SetString("ELEMENT", "2");
          base::ListValue list;
          list.Append(element1.DeepCopy());
          list.Append(element2.DeepCopy());
          result_.reset(list.DeepCopy());
        }
        break;
      }
      case kElementNotExistsQueryOnce: {
        if (only_one_)
          result_.reset(base::Value::CreateNullValue());
        else
          result_.reset(new base::ListValue());
        break;
      }
    }
  }
  virtual ~FindElementWebView() {}

  void Verify(const std::string& expected_frame,
              const base::ListValue* expected_args,
              const base::Value* actrual_result) {
    EXPECT_EQ(expected_frame, frame_);
    std::string function;
    if (only_one_)
      function = webdriver::atoms::asString(webdriver::atoms::FIND_ELEMENT);
    else
      function = webdriver::atoms::asString(webdriver::atoms::FIND_ELEMENTS);
    EXPECT_EQ(function, function_);
    ASSERT_TRUE(args_.get());
    EXPECT_TRUE(expected_args->Equals(args_.get()));
    ASSERT_TRUE(actrual_result);
    EXPECT_TRUE(result_->Equals(actrual_result));
  }

  // Overridden from WebView:
  virtual Status CallFunction(const std::string& frame,
                              const std::string& function,
                              const base::ListValue& args,
                              scoped_ptr<base::Value>* result) OVERRIDE {
    ++current_count_;
    if (scenario_ == kElementExistsTimeout ||
        (scenario_ == kElementExistsQueryTwice && current_count_ == 1)) {
        // Always return empty result when testing timeout.
        if (only_one_)
          result->reset(base::Value::CreateNullValue());
        else
          result->reset(new base::ListValue());
    } else {
      switch (scenario_) {
        case kElementExistsQueryOnce:
        case kElementNotExistsQueryOnce: {
          EXPECT_EQ(1, current_count_);
          break;
        }
        case kElementExistsQueryTwice: {
          EXPECT_EQ(2, current_count_);
          break;
        }
        default: {
          break;
        }
      }

      result->reset(result_->DeepCopy());
      frame_ = frame;
      function_ = function;
      args_.reset(args.DeepCopy());
    }
    return Status(kOk);
  }

 private:
  bool only_one_;
  TestScenario scenario_;
  int current_count_;
  std::string frame_;
  std::string function_;
  scoped_ptr<base::ListValue> args_;
  scoped_ptr<base::Value> result_;
};

}  // namespace

TEST(CommandsTest, SuccessfulFindElement) {
  FindElementWebView web_view(true, kElementExistsQueryTwice);
  Session session("id");
  session.implicit_wait = 1000;
  session.SwitchToSubFrame("frame_id1", "");
  base::DictionaryValue params;
  params.SetString("using", "id");
  params.SetString("value", "a");
  scoped_ptr<base::Value> result;
  ASSERT_EQ(kOk,
            ExecuteFindElement(1, &session, &web_view, params, &result).code());
  base::DictionaryValue param;
  param.SetString("id", "a");
  base::ListValue expected_args;
  expected_args.Append(param.DeepCopy());
  web_view.Verify("frame_id1", &expected_args, result.get());
}

TEST(CommandsTest, FailedFindElement) {
  FindElementWebView web_view(true, kElementNotExistsQueryOnce);
  Session session("id");
  base::DictionaryValue params;
  params.SetString("using", "id");
  params.SetString("value", "a");
  scoped_ptr<base::Value> result;
  ASSERT_EQ(kNoSuchElement,
            ExecuteFindElement(1, &session, &web_view, params, &result).code());
}

TEST(CommandsTest, SuccessfulFindElements) {
  FindElementWebView web_view(false, kElementExistsQueryTwice);
  Session session("id");
  session.implicit_wait = 1000;
  session.SwitchToSubFrame("frame_id2", "");
  base::DictionaryValue params;
  params.SetString("using", "name");
  params.SetString("value", "b");
  scoped_ptr<base::Value> result;
  ASSERT_EQ(
      kOk,
      ExecuteFindElements(1, &session, &web_view, params, &result).code());
  base::DictionaryValue param;
  param.SetString("name", "b");
  base::ListValue expected_args;
  expected_args.Append(param.DeepCopy());
  web_view.Verify("frame_id2", &expected_args, result.get());
}

TEST(CommandsTest, FailedFindElements) {
  Session session("id");
  FindElementWebView web_view(false, kElementNotExistsQueryOnce);
  base::DictionaryValue params;
  params.SetString("using", "id");
  params.SetString("value", "a");
  scoped_ptr<base::Value> result;
  ASSERT_EQ(
      kOk,
      ExecuteFindElements(1, &session, &web_view, params, &result).code());
  base::ListValue* list;
  ASSERT_TRUE(result->GetAsList(&list));
  ASSERT_EQ(0U, list->GetSize());
}

TEST(CommandsTest, SuccessfulFindChildElement) {
  FindElementWebView web_view(true, kElementExistsQueryTwice);
  Session session("id");
  session.implicit_wait = 1000;
  session.SwitchToSubFrame("frame_id3", "");
  base::DictionaryValue params;
  params.SetString("using", "tag name");
  params.SetString("value", "div");
  std::string element_id = "1";
  scoped_ptr<base::Value> result;
  ASSERT_EQ(
      kOk,
      ExecuteFindChildElement(
          1, &session, &web_view, element_id, params, &result).code());
  base::DictionaryValue locator_param;
  locator_param.SetString("tag name", "div");
  base::DictionaryValue root_element_param;
  root_element_param.SetString("ELEMENT", element_id);
  base::ListValue expected_args;
  expected_args.Append(locator_param.DeepCopy());
  expected_args.Append(root_element_param.DeepCopy());
  web_view.Verify("frame_id3", &expected_args, result.get());
}

TEST(CommandsTest, FailedFindChildElement) {
  Session session("id");
  FindElementWebView web_view(true, kElementNotExistsQueryOnce);
  base::DictionaryValue params;
  params.SetString("using", "id");
  params.SetString("value", "a");
  std::string element_id = "1";
  scoped_ptr<base::Value> result;
  ASSERT_EQ(
      kNoSuchElement,
      ExecuteFindChildElement(
          1, &session, &web_view, element_id, params, &result).code());
}

TEST(CommandsTest, SuccessfulFindChildElements) {
  FindElementWebView web_view(false, kElementExistsQueryTwice);
  Session session("id");
  session.implicit_wait = 1000;
  session.SwitchToSubFrame("frame_id4", "");
  base::DictionaryValue params;
  params.SetString("using", "class name");
  params.SetString("value", "c");
  std::string element_id = "1";
  scoped_ptr<base::Value> result;
  ASSERT_EQ(
      kOk,
      ExecuteFindChildElements(
          1, &session, &web_view, element_id, params, &result).code());
  base::DictionaryValue locator_param;
  locator_param.SetString("class name", "c");
  base::DictionaryValue root_element_param;
  root_element_param.SetString("ELEMENT", element_id);
  base::ListValue expected_args;
  expected_args.Append(locator_param.DeepCopy());
  expected_args.Append(root_element_param.DeepCopy());
  web_view.Verify("frame_id4", &expected_args, result.get());
}

TEST(CommandsTest, FailedFindChildElements) {
  Session session("id");
  FindElementWebView web_view(false, kElementNotExistsQueryOnce);
  base::DictionaryValue params;
  params.SetString("using", "id");
  params.SetString("value", "a");
  std::string element_id = "1";
  scoped_ptr<base::Value> result;
  ASSERT_EQ(
      kOk,
      ExecuteFindChildElements(
          1, &session, &web_view, element_id, params, &result).code());
  base::ListValue* list;
  ASSERT_TRUE(result->GetAsList(&list));
  ASSERT_EQ(0U, list->GetSize());
}

TEST(CommandsTest, TimeoutInFindElement) {
  Session session("id");
  FindElementWebView web_view(true, kElementExistsTimeout);
  session.implicit_wait = 2;
  base::DictionaryValue params;
  params.SetString("using", "id");
  params.SetString("value", "a");
  params.SetString("id", "1");
  scoped_ptr<base::Value> result;
  ASSERT_EQ(kNoSuchElement,
            ExecuteFindElement(1, &session, &web_view, params, &result).code());
}

namespace {

class ErrorCallFunctionWebView : public StubWebView {
 public:
  explicit ErrorCallFunctionWebView(StatusCode code)
      : StubWebView("1"), code_(code) {}
  virtual ~ErrorCallFunctionWebView() {}

  // Overridden from WebView:
  virtual Status CallFunction(const std::string& frame,
                              const std::string& function,
                              const base::ListValue& args,
                              scoped_ptr<base::Value>* result) OVERRIDE {
    return Status(code_);
  }

 private:
  StatusCode code_;
};

}  // namespace

TEST(CommandsTest, ErrorFindElement) {
  Session session("id");
  ErrorCallFunctionWebView web_view(kUnknownError);
  base::DictionaryValue params;
  params.SetString("using", "id");
  params.SetString("value", "a");
  scoped_ptr<base::Value> value;
  ASSERT_EQ(kUnknownError,
            ExecuteFindElement(1, &session, &web_view, params, &value).code());
  ASSERT_EQ(kUnknownError,
            ExecuteFindElements(1, &session, &web_view, params, &value).code());
}

TEST(CommandsTest, ErrorFindChildElement) {
  Session session("id");
  ErrorCallFunctionWebView web_view(kStaleElementReference);
  base::DictionaryValue params;
  params.SetString("using", "id");
  params.SetString("value", "a");
  std::string element_id = "1";
  scoped_ptr<base::Value> result;
  ASSERT_EQ(
      kStaleElementReference,
      ExecuteFindChildElement(
          1, &session, &web_view, element_id, params, &result).code());
  ASSERT_EQ(
      kStaleElementReference,
      ExecuteFindChildElements(
          1, &session, &web_view, element_id, params, &result).code());
}
