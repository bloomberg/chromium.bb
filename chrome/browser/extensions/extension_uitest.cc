// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/gfx/rect.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/automation/extension_automation_constants.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy_uitest.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"

namespace {

static const char kTestDirectorySimpleApiCall[] =
    "extensions/uitest/simple_api_call";
static const char kTestDirectoryRoundtripApiCall[] =
    "extensions/uitest/roundtrip_api_call";
static const char kTestDirectoryBrowserEvent[] =
    "extensions/uitest/event_sink";

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
template <class ParentTestType>
class ExtensionUITest : public ParentTestType {
 public:
  explicit ExtensionUITest(const std::string& extension_path) {
    FilePath filename(test_data_directory_);
    filename = filename.AppendASCII(extension_path);
    launch_arguments_.AppendSwitchWithValue(switches::kLoadExtension,
                                            filename.value());
    functions_enabled_.push_back("*");
  }

  void set_functions_enabled(
      const std::vector<std::string>& functions_enabled) {
    functions_enabled_ = functions_enabled;
  }

  void SetUp() {
    ParentTestType::SetUp();

    AutomationProxyForExternalTab* proxy =
        static_cast<AutomationProxyForExternalTab*>(automation());
    HWND external_tab_container = NULL;
    HWND tab_wnd = NULL;
    tab_ = proxy->CreateTabWithHostWindow(false,
        GURL(), &external_tab_container, &tab_wnd);

    tab_->SetEnableExtensionAutomation(functions_enabled_);
  }

  void TearDown() {
    tab_->SetEnableExtensionAutomation(std::vector<std::string>());

    AutomationProxyForExternalTab* proxy =
        static_cast<AutomationProxyForExternalTab*>(automation());
    proxy->DestroyHostWindow();
    proxy->WaitForTabCleanup(tab_, action_max_timeout_ms());
    EXPECT_FALSE(tab_->is_valid());
    tab_.release();

    ParentTestType::TearDown();
  }

  void TestWithURL(const GURL& url) {
    EXPECT_TRUE(tab_->is_valid());
    if (tab_) {
      AutomationProxyForExternalTab* proxy =
        static_cast<AutomationProxyForExternalTab*>(automation());

      // Enter a message loop to allow the tab to be created
      proxy->WaitForNavigation(2000);
      DoAdditionalPreNavigateSetup(tab_.get());

      // We explicitly do not make this a toolstrip in the extension manifest,
      // so that the test can control when it gets loaded, and so that we test
      // the intended behavior that tabs should be able to show extension pages
      // (useful for development etc.)
      tab_->NavigateInExternalTab(url, GURL());
      EXPECT_TRUE(proxy->WaitForMessage(action_max_timeout_ms()));
    }
  }

  // Override if you need additional stuff before we navigate the page.
  virtual void DoAdditionalPreNavigateSetup(TabProxy* tab) {
  }

 protected:
  // Extension API functions that we want to take over.  Defaults to all.
  std::vector<std::string> functions_enabled_;
  scoped_refptr<TabProxy> tab_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionUITest);
};

// For tests that only need to check for a single postMessage
// being received from the tab in Chrome.  These tests can send a message
// to the tab before receiving the new message, but there will not be
// a chance to respond by sending a message from the test to the tab after
// the postMessage is received.
typedef ExtensionUITest<ExternalTabTestType> SingleMessageExtensionUITest;

// A test that loads a basic extension that makes an API call that does
// not require a response.
class SimpleApiCallExtensionTest : public SingleMessageExtensionUITest {
 public:
  SimpleApiCallExtensionTest()
      : SingleMessageExtensionUITest(kTestDirectorySimpleApiCall) {
  }

  void SetUp() {
    // Set just this one function explicitly to be forwarded, as a test of
    // the selective forwarding.  The next test will leave the default to test
    // universal forwarding.
    functions_enabled_.clear();
    functions_enabled_.push_back("tabs.remove");
    SingleMessageExtensionUITest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleApiCallExtensionTest);
};

// TODO(port) Should become portable once ExternalTabMessageLoop is ported.
#if defined(OS_WIN)
TEST_F(SimpleApiCallExtensionTest, RunTest) {
  namespace keys = extension_automation_constants;

  TestWithURL(GURL(
      "chrome-extension://pmgpglkggjdpkpghhdmbdhababjpcohk/test.html"));
  AutomationProxyForExternalTab* proxy =
      static_cast<AutomationProxyForExternalTab*>(automation());
  ASSERT_GT(proxy->messages_received(), 0);

  // Using EXPECT_TRUE rather than EXPECT_EQ as the compiler (VC++) isn't
  // finding the right match for EqHelper.
  EXPECT_TRUE(proxy->origin() == keys::kAutomationOrigin);
  EXPECT_TRUE(proxy->target() == keys::kAutomationRequestTarget);

  scoped_ptr<Value> message_value(base::JSONReader::Read(proxy->message(),
                                                         false));
  ASSERT_TRUE(message_value->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* message_dict =
      reinterpret_cast<DictionaryValue*>(message_value.get());
  std::string result;
  ASSERT_TRUE(message_dict->GetString(keys::kAutomationNameKey, &result));
  EXPECT_EQ(result, "tabs.remove");

  result = "";
  ASSERT_TRUE(message_dict->GetString(keys::kAutomationArgsKey, &result));
  EXPECT_NE(result, "");

  int callback_id = 0xBAADF00D;
  message_dict->GetInteger(keys::kAutomationRequestIdKey, &callback_id);
  EXPECT_NE(callback_id, 0xBAADF00D);

  bool has_callback = true;
  EXPECT_TRUE(message_dict->GetBoolean(keys::kAutomationHasCallbackKey,
                                       &has_callback));
  EXPECT_FALSE(has_callback);
}
#endif  // defined(OS_WIN)

// A base class for an automation proxy that checks several messages in
// a row.
class MultiMessageAutomationProxy : public AutomationProxyForExternalTab {
 public:
  explicit MultiMessageAutomationProxy(int execution_timeout)
      : AutomationProxyForExternalTab(execution_timeout) {
  }

  // Call when testing with the current tab is finished.
  void Quit() {
    PostQuitMessage(0);
  }

 protected:
  virtual void OnMessageReceived(const IPC::Message& msg) {
    IPC_BEGIN_MESSAGE_MAP(MultiMessageAutomationProxy, msg)
      IPC_MESSAGE_HANDLER(AutomationMsg_DidNavigate,
                          AutomationProxyForExternalTab::OnDidNavigate)
      IPC_MESSAGE_HANDLER(AutomationMsg_ForwardMessageToExternalHost,
                          OnForwardMessageToExternalHost)
    IPC_END_MESSAGE_MAP()
  }

  void OnForwardMessageToExternalHost(int handle,
                                      const std::string& message,
                                      const std::string& origin,
                                      const std::string& target) {
    messages_received_++;
    message_ = message;
    origin_ = origin;
    target_ = target;
    HandleMessageFromChrome();
  }

  // Override to do your custom checking and initiate any custom actions
  // needed in your particular unit test.
  virtual void HandleMessageFromChrome() = 0;
};

// This proxy is specific to RoundtripApiCallExtensionTest.
class RoundtripAutomationProxy : public MultiMessageAutomationProxy {
 public:
  explicit RoundtripAutomationProxy(int execution_timeout)
      : MultiMessageAutomationProxy(execution_timeout),
        tab_(NULL) {
  }

  // Must set before initiating test.
  TabProxy* tab_;

 protected:
  virtual void HandleMessageFromChrome() {
    namespace keys = extension_automation_constants;

    ASSERT_TRUE(tab_ != NULL);
    ASSERT_TRUE(messages_received_ == 1 || messages_received_ == 2);

    // Using EXPECT_TRUE rather than EXPECT_EQ as the compiler (VC++) isn't
    // finding the right match for EqHelper.
    EXPECT_TRUE(origin_ == keys::kAutomationOrigin);
    EXPECT_TRUE(target_ == keys::kAutomationRequestTarget);

    scoped_ptr<Value> message_value(base::JSONReader::Read(message_, false));
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

    if (messages_received_ == 1) {
      EXPECT_EQ(function_name, "tabs.getSelected");
      EXPECT_GE(request_id, 0);
      EXPECT_TRUE(has_callback);

      DictionaryValue response_dict;
      EXPECT_TRUE(response_dict.SetInteger(keys::kAutomationRequestIdKey,
                                           request_id));
      DictionaryValue tab_dict;
      EXPECT_TRUE(tab_dict.SetInteger(extension_tabs_module_constants::kIdKey,
                                      42));
      EXPECT_TRUE(tab_dict.SetInteger(
          extension_tabs_module_constants::kIndexKey, 1));
      EXPECT_TRUE(tab_dict.SetInteger(
          extension_tabs_module_constants::kWindowIdKey, 1));
      EXPECT_TRUE(tab_dict.SetBoolean(
          extension_tabs_module_constants::kSelectedKey, true));
      EXPECT_TRUE(tab_dict.SetString(
          extension_tabs_module_constants::kUrlKey, "http://www.google.com"));

      std::string tab_json;
      base::JSONWriter::Write(&tab_dict, false, &tab_json);

      EXPECT_TRUE(response_dict.SetString(keys::kAutomationResponseKey,
          tab_json));

      std::string response_json;
      base::JSONWriter::Write(&response_dict, false, &response_json);

      tab_->HandleMessageFromExternalHost(
          response_json,
          keys::kAutomationOrigin,
          keys::kAutomationResponseTarget);
    } else if (messages_received_ == 2) {
      EXPECT_EQ(function_name, "tabs.remove");
      EXPECT_FALSE(has_callback);

      std::string args;
      EXPECT_TRUE(request_dict->GetString(keys::kAutomationArgsKey, &args));
      EXPECT_NE(args.find("42"), -1);

      Quit();
    } else {
      Quit();
      FAIL();
    }
  }
};

class RoundtripApiCallExtensionTest
    : public ExtensionUITest<
                 CustomAutomationProxyTest<RoundtripAutomationProxy>> {
 public:
  RoundtripApiCallExtensionTest()
      : ExtensionUITest<
          CustomAutomationProxyTest<
              RoundtripAutomationProxy> >(kTestDirectoryRoundtripApiCall) {
  }

  void DoAdditionalPreNavigateSetup(TabProxy* tab) {
    RoundtripAutomationProxy* proxy =
      static_cast<RoundtripAutomationProxy*>(automation());
    proxy->tab_ = tab;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RoundtripApiCallExtensionTest);
};

// TODO(port) Should become portable once
// ExternalTabMessageLoop is ported.
#if defined(OS_WIN)
TEST_F(RoundtripApiCallExtensionTest, RunTest) {
  TestWithURL(GURL(
      "chrome-extension://ofoknjclcmghjfmbncljcnpjmfmldhno/test.html"));
  RoundtripAutomationProxy* proxy =
      static_cast<RoundtripAutomationProxy*>(automation());

  // Validation is done in the RoundtripAutomationProxy, so we just check
  // something basic here.
  EXPECT_EQ(proxy->messages_received(), 2);
}
#endif  // defined(OS_WIN)

// This proxy is specific to BrowserEventExtensionTest.
class BrowserEventAutomationProxy : public MultiMessageAutomationProxy {
 public:
  explicit BrowserEventAutomationProxy(int execution_timeout)
      : MultiMessageAutomationProxy(execution_timeout),
        tab_(NULL) {
  }

  // Must set before initiating test.
  TabProxy* tab_;

  // Counts the number of times we got a given event.
  std::map<std::string, int> event_count_;

  // Array containing the names of the events to fire to the extension.
  static const char* events_[];

 protected:
  // Process a message received from the test extension.
  virtual void HandleMessageFromChrome();

  // Fire an event of the given name to the test extension.
  void FireEvent(const char* event_name);
};

const char* BrowserEventAutomationProxy::events_[] = {
  // Window events.
  "[\"windows.onCreated\", \"[{'id':42,'focused':true,'top':0,'left':0,"
      "'width':100,'height':100}]\"]",

  "[\"windows.onRemoved\", \"[42]\"]",

  "[\"windows.onFocusChanged\", \"[42]\"]",

  // Tab events.
  "[\"tabs.onCreated\", \"[{'id\':42,'index':1,'windowId':1,"
      "'selected':true,'url':'http://www.google.com'}]\"]",

  "[\"tabs.onUpdated\", \"[42, {'status': 'complete',"
      "'url':'http://www.google.com'}, {'id\':42,'index':1,'windowId':1,"
      "'selected':true,'url':'http://www.google.com'}]\"]",

  "[\"tabs.onMoved\", \"[42, {'windowId':1,'fromIndex':1,'toIndex':2}]\"]",

  "[\"tabs.onSelectionChanged\", \"[42, {'windowId':1}]\"]",

  "[\"tabs.onAttached\", \"[42, {'newWindowId':1,'newPosition':1}]\"]",

  "[\"tabs.onDetached\", \"[43, {'oldWindowId':1,'oldPosition':1}]\"]",

  "[\"tabs.onRemoved\", \"[43]\"]",

  // Bookmark events.
  "[\"bookmarks.onCreated\", \"['42', {'id':'42','title':'foo',}]\"]",

  "[\"bookmarks.onRemoved\", \"['42', {'parentId':'2','index':1}]\"]",

  "[\"bookmarks.onChanged\", \"['42', {'title':'foo'}]\"]",

  "[\"bookmarks.onMoved\", \"['42', {'parentId':'2','index':1,"
      "'oldParentId':'3','oldIndex':2}]\"]",

  "[\"bookmarks.onChildrenReordered\", \"['32', "
      "{'childIds':['1', '2', '3']}]\"]"
};

void BrowserEventAutomationProxy::HandleMessageFromChrome() {
  namespace keys = extension_automation_constants;
  ASSERT_TRUE(tab_ != NULL);

  std::string message(message());
  std::string origin(origin());
  std::string target(target());

  ASSERT_TRUE(message.length() > 0);
  ASSERT_STREQ(keys::kAutomationOrigin, origin.c_str());

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
    ASSERT_STREQ(name.c_str(), "windows.getCurrent");

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
    for (int i = 0; i < arraysize(events_); ++i) {
      FireEvent(events_[i]);
    }
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
    ASSERT_TRUE(message_dict->GetString(L"data", &event_name));
    if (event_name == "\"ACK\"") {
      ASSERT_EQ(0, event_count_.size());
    } else {
      ++event_count_[event_name];
    }
  }
}

void BrowserEventAutomationProxy::FireEvent(const char* event) {
  namespace keys = extension_automation_constants;

  // Build the event message to send to the extension.  The only important
  // part is the name, as the payload is not used by the test extension.
  std::string message;
  message += event;

  tab_->HandleMessageFromExternalHost(
      message,
      keys::kAutomationOrigin,
      keys::kAutomationBrowserEventRequestTarget);
}

class BrowserEventExtensionTest
    : public ExtensionUITest<
                 CustomAutomationProxyTest<BrowserEventAutomationProxy>> {
 public:
  BrowserEventExtensionTest()
      : ExtensionUITest<
          CustomAutomationProxyTest<
              BrowserEventAutomationProxy> >(kTestDirectoryBrowserEvent) {
  }

  void DoAdditionalPreNavigateSetup(TabProxy* tab) {
    BrowserEventAutomationProxy* proxy =
      static_cast<BrowserEventAutomationProxy*>(automation());
    proxy->tab_ = tab;
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(BrowserEventExtensionTest);
};

// TODO(port) Should become portable once
// ExternalTabMessageLoop is ported.
#if defined(OS_WIN)
TEST_F(BrowserEventExtensionTest, RunTest) {
  // This test loads an HTML file that tries to add listeners to a bunch of
  // chrome.* events and upon adding a listener it posts the name of the event
  // to the automation layer, which we'll count to make sure the events work.
  TestWithURL(GURL(
      "chrome-extension://ofoknjclcmghjfmbncljcnpjmfmldhno/test.html"));
  BrowserEventAutomationProxy* proxy =
      static_cast<BrowserEventAutomationProxy*>(automation());

  // If this assert hits and the actual size is 0 then you need to look at:
  // src\chrome\test\data\extensions\uitest\event_sink\test.html and see if
  // all the events we are attaching to are valid. Also compare the list against
  // the event_names_ string array above.
  EXPECT_EQ(arraysize(BrowserEventAutomationProxy::events_),
            proxy->event_count_.size());
  for (std::map<std::string, int>::iterator i = proxy->event_count_.begin();
      i != proxy->event_count_.end(); ++i) {
    const std::pair<std::string, int>& value = *i;
    ASSERT_EQ(1, value.second);
  }
}
#endif  // defined(OS_WIN)

}  // namespace
