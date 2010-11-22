// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for ApiDispatcher.

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "ceee/ie/broker/api_dispatcher.h"
#include "ceee/ie/broker/chrome_postman.h"
#include "ceee/testing/utils/test_utils.h"
#include "chrome/browser/automation/extension_automation_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {

namespace keys = extension_automation_constants;

using testing::_;
using testing::StrEq;
using testing::StrictMock;

MATCHER_P(ValuesEqual, value, "") {
  return arg.Equals(value);
}

class MockInvocation
    : public ApiDispatcher::Invocation {
 public:
  MOCK_METHOD2(Execute, void(const ListValue& args, int request_id));

  // A factory for the registry.
  static ApiDispatcher::Invocation* TestingInstance() {
    ApiDispatcher::Invocation* return_value = testing_instance_.release();
    testing_instance_.reset(new MockInvocation);
    return return_value;
  }

  static scoped_ptr<MockInvocation> testing_instance_;
};

scoped_ptr<MockInvocation> MockInvocation::testing_instance_;


TEST(ApiDispatcher, InvalidRegistrationNoCrash) {
  testing::LogDisabler no_dchecks;

  ApiDispatcher dispatcher;
  dispatcher.RegisterInvocation(NULL, NewApiInvocation<MockInvocation>);
  dispatcher.RegisterInvocation("hi", NULL);
}

TEST(ApiDispatcher, DuplicateRegistrationNoCrash) {
  testing::LogDisabler no_dchecks;

  ApiDispatcher dispatcher;
  dispatcher.RegisterInvocation("hi", NewApiInvocation<MockInvocation>);
  dispatcher.RegisterInvocation("hi", NewApiInvocation<MockInvocation>);
}

// Encodes a request in JSON.  Optionally skip encoding one of the required
// keys.
std::string MakeRequest(const char* function_name, const Value* args,
                        int request_id, const std::string& skip_key = "") {
  std::string json;
  DictionaryValue value;
  if (skip_key != keys::kAutomationNameKey)
    value.SetString(keys::kAutomationNameKey, function_name);
  if (skip_key != keys::kAutomationArgsKey) {
    std::string json;
    if (args) {
      base::JSONWriter::Write(args, false, &json);
    }
    value.SetString(keys::kAutomationArgsKey, json);
  }
  if (skip_key != keys::kAutomationRequestIdKey)
    value.SetInteger(keys::kAutomationRequestIdKey, request_id);
  base::JSONWriter::Write(&value, false, &json);
  return json;
}

static const int kRequestId = 42;

class ApiDispatcherTests : public testing::Test {
 public:
  virtual void SetUp() {
    // We must create the testing instance before the factory returns it
    // so that we can set expectations on it beforehand.
    MockInvocation::testing_instance_.reset(new MockInvocation);
  }
  virtual void TearDown() {
    // We don't want to wait for the static instance to get destroyed,
    // it will be too late and will look like we leaked.
    MockInvocation::testing_instance_.reset(NULL);
  }
};


TEST_F(ApiDispatcherTests, BasicDispatching) {
  testing::LogDisabler no_dchecks;

  scoped_ptr<ListValue> args1(new ListValue());
  args1->Append(Value::CreateStringValue("hello world"));
  scoped_ptr<ListValue> args2(new ListValue());
  args2->Append(Value::CreateIntegerValue(711));
  scoped_ptr<ListValue> args3(new ListValue());
  args3->Append(Value::CreateBooleanValue(true));
  DictionaryValue* dict_value = new DictionaryValue();
  dict_value->SetString("foo", "moo");
  dict_value->SetInteger("blat", 42);
  scoped_ptr<ListValue> args4(new ListValue());
  args4->Append(dict_value);

  ListValue* values[] = { args1.get(), args2.get(), args3.get(), args4.get() };

  for (int i = 0; i < arraysize(values); ++i) {
    ListValue* test_value = values[i];
    EXPECT_CALL(*MockInvocation::testing_instance_,
        Execute(ValuesEqual(test_value), kRequestId)).Times(1);

    ApiDispatcher dispatcher;
    dispatcher.RegisterInvocation("DoStuff", &MockInvocation::TestingInstance);
    std::string json = MakeRequest("DoStuff", test_value, kRequestId);
    dispatcher.HandleApiRequest(CComBSTR(json.c_str()), NULL);
  }
}

TEST_F(ApiDispatcherTests, ErrorHandlingDispatching) {
  testing::LogDisabler no_dchecks;

  ApiDispatcher dispatcher;
  dispatcher.RegisterInvocation("DoStuff", &MockInvocation::TestingInstance);

  std::string test_data[] = {
    MakeRequest("DoStuff", NULL, 42, keys::kAutomationNameKey),
    MakeRequest("DoStuff", NULL, 43, keys::kAutomationArgsKey),
    MakeRequest("DoStuff", NULL, 44, keys::kAutomationRequestIdKey),
    MakeRequest("NotRegistered", NULL, 45),
  };

  for (int i = 0; i < arraysize(test_data); ++i) {
    EXPECT_CALL(*MockInvocation::testing_instance_, Execute(_, _)).Times(0);
    CComBSTR json(test_data[i].c_str());
    dispatcher.HandleApiRequest(json, NULL);
  }
}

class MockChromePostman : public ChromePostman {
 public:
  MOCK_METHOD2(PostMessage, void(BSTR, BSTR));
};

TEST(ApiDispatcher, PostResult) {
  testing::LogDisabler no_dchecks;

  // Simply instantiating the postman makes it the single instance
  // available to all clients.
  CComObjectStackEx< StrictMock< MockChromePostman > > postman;

  // We use different (numbered) result instances, because they are not
  // meant to post more than once (and we validate that too).
  ApiDispatcher::InvocationResult result1(kRequestId);

  // First check that we can correctly create an error response.
  static const char* kError = "error";
  DictionaryValue expected_value;
  expected_value.SetInteger(keys::kAutomationRequestIdKey, kRequestId);
  expected_value.SetString(keys::kAutomationErrorKey, kError);

  std::string expected_response;
  base::JSONWriter::Write(&expected_value, false, &expected_response);

  CComBSTR request_name(keys::kAutomationResponseTarget);
  EXPECT_CALL(postman, PostMessage(
      StrEq(CComBSTR(expected_response.c_str()).m_str),
      StrEq(request_name.m_str))).Times(1);
  result1.PostError(kError);

  // PostResult and PostError must not call the postman anymore,
  // and StrictMock will validate it.
  result1.PostResult();
  result1.PostError(kError);

  // Now check that we can create an empty response when there is no value.
  expected_value.Remove(keys::kAutomationErrorKey, NULL);
  expected_value.SetString(keys::kAutomationResponseKey, "");
  base::JSONWriter::Write(&expected_value, false, &expected_response);

  EXPECT_CALL(postman, PostMessage(
      StrEq(CComBSTR(expected_response.c_str()).m_str),
      StrEq(request_name.m_str))).Times(1);
  ApiDispatcher::InvocationResult result2(kRequestId);
  result2.PostResult();
  // These must not get to PostMan, and StrictMock will validate it.
  result2.PostError("");
  result2.PostResult();

  // And now check that we can create an full response.
  ApiDispatcher::InvocationResult result3(kRequestId);
  result3.set_value(Value::CreateIntegerValue(42));
  std::string result_str;
  base::JSONWriter::Write(result3.value(), false, &result_str);

  expected_value.SetString(keys::kAutomationResponseKey, result_str);
  base::JSONWriter::Write(&expected_value, false, &expected_response);

  EXPECT_CALL(postman, PostMessage(
      StrEq(CComBSTR(expected_response.c_str()).m_str),
      StrEq(request_name.m_str))).Times(1);
  result3.PostResult();
  // These must not get to PostMan, and StrictMock will validate it.
  result3.PostResult();
  result3.PostError("");
}

bool EventHandler1(const std::string& input_args, std::string* converted_args,
                   ApiDispatcher* dispatcher) {
  EXPECT_STREQ(input_args.c_str(), "EventHandler1Args");
  EXPECT_TRUE(converted_args != NULL);
  *converted_args = "EventHandler1ConvertedArgs";
  return true;
}

bool EventHandler2(const std::string& input_args, std::string* converted_args,
                   ApiDispatcher* dispatcher) {
  EXPECT_STREQ(input_args.c_str(), "EventHandler2Args");
  EXPECT_TRUE(converted_args != NULL);
  *converted_args = "EventHandler2ConvertedArgs";
  return true;
}

bool EventHandler3(const std::string& input_args, std::string* converted_args,
                   ApiDispatcher* dispatcher) {
  return false;
}

TEST(ApiDispatcher, PermanentEventHandler) {
  testing::LogDisabler no_dchecks;

  // Simply instantiating the postman makes it the single instance
  // available to all clients.
  CComObjectStackEx< StrictMock< MockChromePostman > > postman;
  ApiDispatcher dispatcher;
  dispatcher.RegisterPermanentEventHandler("Event1", EventHandler1);
  dispatcher.RegisterPermanentEventHandler("Event2", EventHandler2);
  dispatcher.RegisterPermanentEventHandler("Event3", EventHandler3);

  ListValue message1;
  message1.Append(Value::CreateStringValue("Event1"));
  message1.Append(Value::CreateStringValue("EventHandler1ConvertedArgs"));
  std::string message1_str;
  base::JSONWriter::Write(&message1, false, &message1_str);
  CComBSTR request_name(keys::kAutomationBrowserEventRequestTarget);
  EXPECT_CALL(postman, PostMessage(StrEq(CComBSTR(message1_str.c_str()).m_str),
                                   StrEq(request_name.m_str))).Times(1);
  dispatcher.FireEvent("Event1", "EventHandler1Args");

  ListValue message2;
  message2.Append(Value::CreateStringValue("Event2"));
  message2.Append(Value::CreateStringValue("EventHandler2ConvertedArgs"));
  std::string message2_str;
  base::JSONWriter::Write(&message2, false, &message2_str);
  EXPECT_CALL(postman, PostMessage(StrEq(CComBSTR(message2_str.c_str()).m_str),
                                   StrEq(request_name.m_str))).Times(1);
  dispatcher.FireEvent("Event2", "EventHandler2Args");

  // There shouldn't be a post when the event handler returns false.
  dispatcher.FireEvent("Event3", "");
}

// TODO(mad@chromium.org): Add tests for the EphemeralEventHandlers.

}  // namespace
