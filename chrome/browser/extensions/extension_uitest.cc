// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "chrome/browser/automation/extension_automation_constants.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/automation/automation_proxy_uitest.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"
#include "ui/gfx/rect.h"

namespace {

using testing::_;
using testing::CreateFunctor;
using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::SaveArg;
using testing::WithArgs;

using ui_test_utils::TimedMessageLoopRunner;

static const char kTestDirectorySimpleApiCall[] =
    "extensions/uitest/simple_api_call";
static const char kTestDirectoryRoundtripApiCall[] =
    "extensions/uitest/roundtrip_api_call";
static const char kTestDirectoryBrowserEvent[] =
    "extensions/uitest/event_sink";

// TODO(port): http://crbug.com/45766
#if defined(OS_WIN)

// Base class to test extensions almost end-to-end by including browser
// startup, manifest parsing, and the actual process model in the
// equation.  This would also let you write UITests that test individual
// Chrome Extensions as running in Chrome.  Takes over implementation of
// extension API calls so that behavior can be tested deterministically
// through code, instead of having to contort the browser into a state
// suitable for testing.
//
// By default, makes Chrome forward all Chrome Extension API function calls
// via the automation interface. To override this, call set_functions_enabled()
// with a list of function names that should be forwarded,
class ExtensionUITest : public ExternalTabUITest {
 public:
   explicit ExtensionUITest(const std::string& extension_path)
      : loop_(MessageLoop::current()) {
    FilePath filename = test_data_directory_.AppendASCII(extension_path);
    launch_arguments_.AppendSwitchPath(switches::kLoadExtension, filename);
    functions_enabled_.push_back("*");
  }

  void set_functions_enabled(
      const std::vector<std::string>& functions_enabled) {
    functions_enabled_ = functions_enabled;
  }

  void SetUp() {
    ExternalTabUITest::SetUp();
    // TabProxy::NavigateInExternalTab is a sync call and can cause a deadlock
    // if host window is visible.
    mock_->host_window_style_ &= ~WS_VISIBLE;
    tab_ = mock_->CreateTabWithUrl(GURL());
    tab_->SetEnableExtensionAutomation(functions_enabled_);
  }

  void TearDown() {
    tab_->SetEnableExtensionAutomation(std::vector<std::string>());
    tab_ = NULL;
    EXPECT_TRUE(mock_->HostWindowExists()) <<
        "You shouldn't DestroyHostWindow yourself, or extension automation "
        "won't be correctly reset. Just exit your message loop.";
    mock_->DestroyHostWindow();
    ExternalTabUITest::TearDown();
  }

 protected:
  // Extension API functions that we want to take over.  Defaults to all.
  std::vector<std::string> functions_enabled_;

  // Message loop for running the async bits of your test.
  TimedMessageLoopRunner loop_;

  // The external tab.
  scoped_refptr<TabProxy> tab_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionUITest);
};

// A test that loads a basic extension that makes an API call that does
// not require a response.
class ExtensionTestSimpleApiCall : public ExtensionUITest {
 public:
  ExtensionTestSimpleApiCall()
      : ExtensionUITest(kTestDirectorySimpleApiCall) {
  }

  void SetUp() {
    // Set just this one function explicitly to be forwarded, as a test of
    // the selective forwarding.  The next test will leave the default to test
    // universal forwarding.
    functions_enabled_.clear();
    functions_enabled_.push_back("tabs.remove");
    ExtensionUITest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionTestSimpleApiCall);
};

// Flaky: http://crbug.com/44599
TEST_F(ExtensionTestSimpleApiCall, FLAKY_RunTest) {
  namespace keys = extension_automation_constants;

  ASSERT_THAT(mock_, testing::NotNull());
  EXPECT_CALL(*mock_, OnDidNavigate(_)).Times(1);
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _))
      .Times(testing::AnyNumber());

  std::string message_received;
  EXPECT_CALL(*mock_, OnForwardMessageToExternalHost(
      _, keys::kAutomationOrigin, keys::kAutomationRequestTarget))
      .WillOnce(DoAll(
        SaveArg<0>(&message_received),
        InvokeWithoutArgs(
            CreateFunctor(&loop_, &TimedMessageLoopRunner::Quit))));

  EXPECT_CALL(*mock_, HandleClosed(_));

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab_->NavigateInExternalTab(
      GURL("chrome-extension://pmgpglkggjdpkpghhdmbdhababjpcohk/test.html"),
      GURL("")));

  loop_.RunFor(TestTimeouts::action_max_timeout_ms());
  ASSERT_FALSE(message_received.empty());

  scoped_ptr<Value> message_value(base::JSONReader::Read(message_received,
                                                         false));
  ASSERT_TRUE(message_value->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* message_dict =
      reinterpret_cast<DictionaryValue*>(message_value.get());
  std::string result;
  ASSERT_TRUE(message_dict->GetString(keys::kAutomationNameKey, &result));
  EXPECT_EQ("tabs.remove", result);

  result.clear();
  ASSERT_TRUE(message_dict->GetString(keys::kAutomationArgsKey, &result));
  EXPECT_FALSE(result.empty());

  int callback_id = 123456789;
  message_dict->GetInteger(keys::kAutomationRequestIdKey, &callback_id);
  EXPECT_NE(callback_id, 123456789);

  bool has_callback = true;
  EXPECT_TRUE(message_dict->GetBoolean(keys::kAutomationHasCallbackKey,
                                       &has_callback));
  EXPECT_FALSE(has_callback);
  DictionaryValue* associated_tab = NULL;
  EXPECT_TRUE(message_dict->GetDictionary(keys::kAutomationTabJsonKey,
      &associated_tab));
  std::string associated_tab_url;
  EXPECT_TRUE(associated_tab->GetString(
      extension_tabs_module_constants::kUrlKey, &associated_tab_url));
  EXPECT_EQ("chrome-extension://pmgpglkggjdpkpghhdmbdhababjpcohk/test.html",
            associated_tab_url);
}

// A test that loads a basic extension that makes an API call that does
// not require a response.
class ExtensionTestRoundtripApiCall : public ExtensionUITest {
public:
  ExtensionTestRoundtripApiCall()
    : ExtensionUITest(kTestDirectoryRoundtripApiCall),
      messages_received_(0) {
  }

  void SetUp() {
    // Set just this one function explicitly to be forwarded, as a test of
    // the selective forwarding.  The next test will leave the default to test
    // universal forwarding.
    functions_enabled_.clear();
    functions_enabled_.push_back("tabs.getSelected");
    functions_enabled_.push_back("tabs.remove");
    ExtensionUITest::SetUp();
  }

  void CheckAndSendResponse(const std::string& message) {
    namespace keys = extension_automation_constants;
    ++messages_received_;

    ASSERT_TRUE(tab_ != NULL);
    ASSERT_TRUE(messages_received_ == 1 || messages_received_ == 2);

    scoped_ptr<Value> message_value(base::JSONReader::Read(message, false));
    ASSERT_TRUE(message_value->IsType(Value::TYPE_DICTIONARY));
    DictionaryValue* request_dict =
      static_cast<DictionaryValue*>(message_value.get());
    std::string function_name;
    ASSERT_TRUE(request_dict->GetString(keys::kAutomationNameKey,
      &function_name));
    int request_id = -2;
    EXPECT_TRUE(request_dict->GetInteger(keys::kAutomationRequestIdKey,
      &request_id));
    bool has_callback = false;
    EXPECT_TRUE(request_dict->GetBoolean(keys::kAutomationHasCallbackKey,
      &has_callback));
    // The API requests in this extension come from the background page, so
    // the tab is not set.
    DictionaryValue* associated_tab = NULL;
    EXPECT_FALSE(request_dict->GetDictionary(keys::kAutomationTabJsonKey,
      &associated_tab));

    if (messages_received_ == 1) {
      EXPECT_EQ("tabs.getSelected", function_name);
      EXPECT_GE(request_id, 0);
      EXPECT_TRUE(has_callback);

      DictionaryValue response_dict;
      response_dict.SetInteger(keys::kAutomationRequestIdKey, request_id);
      DictionaryValue tab_dict;
      tab_dict.SetInteger(extension_tabs_module_constants::kIdKey, 42);
      tab_dict.SetInteger(extension_tabs_module_constants::kIndexKey, 1);
      tab_dict.SetInteger(extension_tabs_module_constants::kWindowIdKey, 1);
      tab_dict.SetBoolean(extension_tabs_module_constants::kSelectedKey, true);
      tab_dict.SetBoolean(extension_tabs_module_constants::kIncognitoKey,
                          false);
      tab_dict.SetString(extension_tabs_module_constants::kUrlKey,
                         "http://www.google.com");

      std::string tab_json;
      base::JSONWriter::Write(&tab_dict, false, &tab_json);

      response_dict.SetString(keys::kAutomationResponseKey, tab_json);

      std::string response_json;
      base::JSONWriter::Write(&response_dict, false, &response_json);

      tab_->HandleMessageFromExternalHost(
        response_json,
        keys::kAutomationOrigin,
        keys::kAutomationResponseTarget);
    } else if (messages_received_ == 2) {
      EXPECT_EQ("tabs.remove", function_name);
      EXPECT_FALSE(has_callback);

      std::string args;
      EXPECT_TRUE(request_dict->GetString(keys::kAutomationArgsKey, &args));
      EXPECT_NE(std::string::npos, args.find("42"));
      loop_.Quit();
    } else {
      FAIL();
      loop_.Quit();
    }
  }

private:
  int messages_received_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionTestRoundtripApiCall);
};

// This is flaky on XP Tests (dbg)(2) bot.
// http://code.google.com/p/chromium/issues/detail?id=44650
TEST_F(ExtensionTestRoundtripApiCall, FLAKY_RunTest) {
  namespace keys = extension_automation_constants;

  ASSERT_THAT(mock_, testing::NotNull());
  EXPECT_CALL(*mock_, OnDidNavigate(_)).Times(1);
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnLoad(_)).Times(testing::AnyNumber());

  EXPECT_CALL(*mock_, OnForwardMessageToExternalHost(
    _, keys::kAutomationOrigin, keys::kAutomationRequestTarget))
    .Times(2)
    .WillRepeatedly(WithArgs<0>(Invoke(
        CreateFunctor(this,
            &ExtensionTestRoundtripApiCall::CheckAndSendResponse))));

  EXPECT_CALL(*mock_, HandleClosed(_));

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab_->NavigateInExternalTab(
    GURL("chrome-extension://ofoknjclcmghjfmbncljcnpjmfmldhno/test.html"),
    GURL("")));

  // CheckAndSendResponse (called by OnForwardMessageToExternalHost)
  // will end the loop once it has received both of our expected messages.
  loop_.RunFor(TestTimeouts::action_max_timeout_ms());
}

class ExtensionTestBrowserEvents : public ExtensionUITest {
 public:
  ExtensionTestBrowserEvents()
    : ExtensionUITest(kTestDirectoryBrowserEvent),
      response_count_(0) {
  }

  void SetUp() {
    // Set just this one function explicitly to be forwarded, as a test of
    // the selective forwarding.  The next test will leave the default to test
    // universal forwarding.
    functions_enabled_.clear();
    functions_enabled_.push_back("windows.getCurrent");
    ExtensionUITest::SetUp();
  }

  // Fire an event of the given name to the test extension.
  void FireEvent(const char* event_message) {
    // JSON doesn't allow single quotes.
    std::string event_message_strict(event_message);
    ReplaceSubstringsAfterOffset(&event_message_strict, 0, "\'", "\\\"");

    namespace keys = extension_automation_constants;

    ASSERT_TRUE(tab_ != NULL);

    tab_->HandleMessageFromExternalHost(
        event_message_strict,
        keys::kAutomationOrigin,
        keys::kAutomationBrowserEventRequestTarget);
  }

  void HandleMessageFromChrome(const std::string& message,
                               const std::string& target);

 protected:
  // Counts the number of times we got a given event.
  std::map<std::string, int> event_count_;

  // Array containing the names of the events to fire to the extension.
  static const char* events_[];

  // Number of events extension has told us it received.
  int response_count_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTestBrowserEvents);
};

const char* ExtensionTestBrowserEvents::events_[] = {
  // Window events.
  "[\"windows.onCreated\", \"[{'id':42,'focused':true,'top':0,'left':0,"
      "'width':100,'height':100,'incognito':false,'type':'normal'}]\"]",

  "[\"windows.onRemoved\", \"[42]\"]",

  "[\"windows.onFocusChanged\", \"[42]\"]",

  // Tab events.
  "[\"tabs.onCreated\", \"[{'id':42,'index':1,'windowId':1,"
      "'selected':true,'url':'http://www.google.com','incognito':false}]\"]",

  "[\"tabs.onUpdated\", \"[42, {'status': 'complete',"
      "'url':'http://www.google.com'}, {'id':42,'index':1,'windowId':1,"
      "'selected':true,'url':'http://www.google.com','incognito':false}]\"]",

  "[\"tabs.onMoved\", \"[42, {'windowId':1,'fromIndex':1,'toIndex':2}]\"]",

  "[\"tabs.onSelectionChanged\", \"[42, {'windowId':1}]\"]",

  "[\"tabs.onAttached\", \"[42, {'newWindowId':1,'newPosition':1}]\"]",

  "[\"tabs.onDetached\", \"[43, {'oldWindowId':1,'oldPosition':1}]\"]",

  "[\"tabs.onRemoved\", \"[43]\"]",

  // Bookmark events.
  "[\"bookmarks.onCreated\", \"['42', {'id':'42','title':'foo'}]\"]",

  "[\"bookmarks.onRemoved\", \"['42', {'parentId':'2','index':1}]\"]",

  "[\"bookmarks.onChanged\", \"['42', {'title':'foo'}]\"]",

  "[\"bookmarks.onMoved\", \"['42', {'parentId':'2','index':1,"
      "'oldParentId':'3','oldIndex':2}]\"]",

  "[\"bookmarks.onChildrenReordered\", \"['32', "
      "{'childIds':['1', '2', '3']}]\"]"
};

void ExtensionTestBrowserEvents::HandleMessageFromChrome(
    const std::string& message,
    const std::string& target) {
  namespace keys = extension_automation_constants;
  ASSERT_TRUE(tab_ != NULL);

  ASSERT_FALSE(message.empty());

  if (target == keys::kAutomationRequestTarget) {
    // This should be a request for the current window.  We don't need to
    // respond, as this is used only as an indication that the extension
    // page is now loaded.
    scoped_ptr<Value> message_value(base::JSONReader::Read(message, false));
    ASSERT_TRUE(message_value->IsType(Value::TYPE_DICTIONARY));
    DictionaryValue* message_dict =
        reinterpret_cast<DictionaryValue*>(message_value.get());

    std::string name;
    ASSERT_TRUE(message_dict->GetString(keys::kAutomationNameKey, &name));
    ASSERT_EQ("windows.getCurrent", name);

    // Send an OpenChannelToExtension message to chrome. Note: the JSON
    // reader expects quoted property keys.  See the comment in
    // TEST_F(BrowserEventExtensionTest, RunTest) to understand where the
    // extension Id comes from.
    tab_->HandleMessageFromExternalHost(
        "{\"rqid\":0, \"extid\": \"ofoknjclcmghjfmbncljcnpjmfmldhno\","
        " \"connid\": 1}",
        keys::kAutomationOrigin,
        keys::kAutomationPortRequestTarget);
  } else if (target == keys::kAutomationPortResponseTarget) {
    // This is a response to the open channel request.  This means we know
    // that the port is ready to send us messages.  Fire all the events now.
    for (size_t i = 0; i < arraysize(events_); ++i)
      FireEvent(events_[i]);
  } else if (target == keys::kAutomationPortRequestTarget) {
    // This is the test extension calling us back.  Make sure its telling
    // us that it received an event.  We do this by checking to see if the
    // message is a simple string of one of the event names that is fired.
    //
    // There is a special message "ACK" which means that the extension
    // received the port connection.  This is not an event response and
    // should happen before all events.
    scoped_ptr<Value> message_value(base::JSONReader::Read(message, false));
    ASSERT_TRUE(message_value->IsType(Value::TYPE_DICTIONARY));
    DictionaryValue* message_dict =
        reinterpret_cast<DictionaryValue*>(message_value.get());

    std::string event_name;
    message_dict->GetString(keys::kAutomationMessageDataKey, &event_name);
    if (event_name == "\"ACK\"") {
      ASSERT_EQ(0, event_count_.size());
    } else if (event_name.empty()) {
      // This must be the post disconnect.
      int request_id = -1;
      message_dict->GetInteger(keys::kAutomationRequestIdKey, &request_id);
      ASSERT_EQ(keys::CHANNEL_CLOSED, request_id);
    } else {
      ++event_count_[event_name];
    }

    // Check if we're done.
    if (event_count_.size() == arraysize(events_)) {
      loop_.Quit();
    }
  }
}

// Flaky, http://crbug.com/37554.
TEST_F(ExtensionTestBrowserEvents, FLAKY_RunTest) {
  // This test loads an HTML file that tries to add listeners to a bunch of
  // chrome.* events and upon adding a listener it posts the name of the event
  // to the automation layer, which we'll count to make sure the events work.
  namespace keys = extension_automation_constants;

  ASSERT_THAT(mock_, testing::NotNull());
  EXPECT_CALL(*mock_, OnDidNavigate(_)).Times(1);
  EXPECT_CALL(*mock_, OnNavigationStateChanged(_, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_, OnLoad(_)).Times(testing::AnyNumber());

  EXPECT_CALL(*mock_, OnForwardMessageToExternalHost(
    _, keys::kAutomationOrigin, _))
    .WillRepeatedly(WithArgs<0, 2>(Invoke(
        CreateFunctor(this,
            &ExtensionTestBrowserEvents::HandleMessageFromChrome))));

  EXPECT_CALL(*mock_, HandleClosed(_));

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab_->NavigateInExternalTab(
    GURL("chrome-extension://ofoknjclcmghjfmbncljcnpjmfmldhno/test.html"),
    GURL("")));

  // HandleMessageFromChrome (called by OnForwardMessageToExternalHost) ends
  // the loop when we've received the number of response messages we expect.
  loop_.RunFor(TestTimeouts::action_max_timeout_ms());

  // If this assert hits and the actual size is 0 then you need to look at:
  // src\chrome\test\data\extensions\uitest\event_sink\test.html and see if
  // all the events we are attaching to are valid. Also compare the list against
  // the event_names_ string array above.
  ASSERT_EQ(arraysize(events_), event_count_.size());
  for (std::map<std::string, int>::iterator i = event_count_.begin();
      i != event_count_.end(); ++i) {
    const std::pair<std::string, int>& value = *i;
    EXPECT_EQ(1, value.second);
  }
}
#endif  // defined(OS_WIN)

}  // namespace
