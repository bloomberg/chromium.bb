// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_constants.h"
#include "chrome/browser/extensions/api/web_request/web_request_api_helpers.h"
#include "chrome/browser/extensions/extension_event_router_forwarder.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "net/base/auth.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace helpers = extension_web_request_api_helpers;
namespace keys = extension_web_request_api_constants;

using helpers::CalculateOnAuthRequiredDelta;
using helpers::CalculateOnBeforeRequestDelta;
using helpers::CalculateOnBeforeSendHeadersDelta;
using helpers::CalculateOnHeadersReceivedDelta;
using helpers::CharListToString;
using helpers::EventLogEntries;
using helpers::EventResponseDelta;
using helpers::EventResponseDeltas;
using helpers::EventResponseDeltas;
using helpers::InDecreasingExtensionInstallationTimeOrder;
using helpers::MergeCancelOfResponses;
using helpers::MergeOnBeforeRequestResponses;
using helpers::ResponseHeader;
using helpers::ResponseHeaders;
using helpers::StringToCharList;

namespace {
static void EventHandledOnIOThread(
    void* profile,
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64 request_id,
    ExtensionWebRequestEventRouter::EventResponse* response) {
  ExtensionWebRequestEventRouter::GetInstance()->OnEventHandled(
      profile, extension_id, event_name, sub_event_name, request_id,
      response);
}

// Searches |key| in |collection| by iterating over its elements and returns
// true if found.
template <typename Collection, typename Key>
bool Contains(const Collection& collection, const Key& key) {
  return std::find(collection.begin(), collection.end(), key) !=
      collection.end();
}

}  // namespace

// A mock event router that responds to events with a pre-arranged queue of
// Tasks.
class TestIPCSender : public IPC::Message::Sender {
 public:
  typedef std::list<linked_ptr<IPC::Message> > SentMessages;

  // Adds a Task to the queue. We will fire these in order as events are
  // dispatched.
  void PushTask(const base::Closure& task) {
    task_queue_.push(task);
  }

  size_t GetNumTasks() { return task_queue_.size(); }

  SentMessages::const_iterator sent_begin() const {
    return sent_messages_.begin();
  }

  SentMessages::const_iterator sent_end() const {
    return sent_messages_.end();
  }

 private:
  // IPC::Message::Sender
  virtual bool Send(IPC::Message* message) {
    EXPECT_EQ(ExtensionMsg_MessageInvoke::ID, message->type());

    EXPECT_FALSE(task_queue_.empty());
    MessageLoop::current()->PostTask(FROM_HERE, task_queue_.front());
    task_queue_.pop();

    sent_messages_.push_back(linked_ptr<IPC::Message>(message));
    return true;
  }

  std::queue<base::Closure> task_queue_;
  SentMessages sent_messages_;
};

class ExtensionWebRequestTest : public testing::Test {
 public:
  ExtensionWebRequestTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO, &message_loop_) {}

 protected:
  virtual void SetUp() OVERRIDE {
    event_router_ = new ExtensionEventRouterForwarder();
    enable_referrers_.Init(
        prefs::kEnableReferrers, profile_.GetTestingPrefService(), NULL);
    network_delegate_.reset(new ChromeNetworkDelegate(
        event_router_.get(), NULL, NULL, &profile_,
        CookieSettings::Factory::GetForProfile(&profile_), &enable_referrers_));
    context_.reset(new TestURLRequestContext());
    context_->set_network_delegate(network_delegate_.get());
  }

  MessageLoopForIO message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  TestingProfile profile_;
  TestDelegate delegate_;
  BooleanPrefMember enable_referrers_;
  TestIPCSender ipc_sender_;
  scoped_refptr<ExtensionEventRouterForwarder> event_router_;
  scoped_refptr<ExtensionInfoMap> extension_info_map_;
  scoped_ptr<ChromeNetworkDelegate> network_delegate_;
  scoped_ptr<TestURLRequestContext> context_;
};

// Tests that we handle disagreements among extensions about responses to
// blocking events (redirection) by choosing the response from the
// most-recently-installed extension.
TEST_F(ExtensionWebRequestTest, BlockingEventPrecedenceRedirect) {
  std::string extension1_id("1");
  std::string extension2_id("2");
  ExtensionWebRequestEventRouter::RequestFilter filter;
  const std::string kEventName(keys::kOnBeforeRequest);
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension1_id, extension1_id, kEventName, kEventName + "/1",
      filter, ExtensionWebRequestEventRouter::ExtraInfoSpec::BLOCKING,
      ipc_sender_factory.GetWeakPtr());
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension2_id, extension2_id, kEventName, kEventName + "/2",
      filter, ExtensionWebRequestEventRouter::ExtraInfoSpec::BLOCKING,
      ipc_sender_factory.GetWeakPtr());

  GURL redirect_url("about:redirected");
  GURL not_chosen_redirect_url("about:not_chosen");

  net::URLRequest request(GURL("about:blank"), &delegate_);
  request.set_context(context_.get());
  {
    // onBeforeRequest will be dispatched twice initially. The second response -
    // the redirect - should win, since it has a later |install_time|. The
    // redirect will dispatch another pair of onBeforeRequest. There, the first
    // response should win (later |install_time|).
    ExtensionWebRequestEventRouter::EventResponse* response = NULL;

    // Extension1 response. Arrives first, but ignored due to install_time.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension1_id, base::Time::FromDoubleT(1));
    response->new_url = not_chosen_redirect_url;
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension1_id, kEventName, kEventName + "/1",
            request.identifier(), response));

    // Extension2 response. Arrives second, and chosen because of install_time.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    response->new_url = redirect_url;
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension2_id, kEventName, kEventName + "/2",
            request.identifier(), response));

    // Extension2 response to the redirected URL. Arrives first, and chosen.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension2_id, kEventName, kEventName + "/2",
            request.identifier(), response));

    // Extension1 response to the redirected URL. Arrives second, and ignored.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension1_id, base::Time::FromDoubleT(1));
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension1_id, kEventName, kEventName + "/1",
            request.identifier(), response));

    request.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(!request.is_pending());
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request.status().status());
    EXPECT_EQ(0, request.status().error());
    EXPECT_EQ(redirect_url, request.url());
    EXPECT_EQ(2U, request.url_chain().size());
    EXPECT_EQ(0U, ipc_sender_.GetNumTasks());
  }

  // Now test the same thing but the extensions answer in reverse order.
  net::URLRequest request2(GURL("about:blank"), &delegate_);
  request2.set_context(context_.get());
  {
    ExtensionWebRequestEventRouter::EventResponse* response = NULL;

    // Extension2 response. Arrives first, and chosen because of install_time.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    response->new_url = redirect_url;
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension2_id, kEventName, kEventName + "/2",
            request2.identifier(), response));

    // Extension1 response. Arrives second, but ignored due to install_time.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension1_id, base::Time::FromDoubleT(1));
    response->new_url = not_chosen_redirect_url;
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension1_id, kEventName, kEventName + "/1",
            request2.identifier(), response));

    // Extension2 response to the redirected URL. Arrives first, and chosen.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension2_id, kEventName, kEventName + "/2",
            request2.identifier(), response));

    // Extension1 response to the redirected URL. Arrives second, and ignored.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension1_id, base::Time::FromDoubleT(1));
    ipc_sender_.PushTask(
        base::Bind(&EventHandledOnIOThread,
            &profile_, extension1_id, kEventName, kEventName + "/1",
            request2.identifier(), response));

    request2.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(!request2.is_pending());
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request2.status().status());
    EXPECT_EQ(0, request2.status().error());
    EXPECT_EQ(redirect_url, request2.url());
    EXPECT_EQ(2U, request2.url_chain().size());
    EXPECT_EQ(0U, ipc_sender_.GetNumTasks());
  }

  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      &profile_, extension1_id, kEventName + "/1");
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      &profile_, extension2_id, kEventName + "/2");
}

// Test that a request is canceled if this is requested by any extension
// regardless whether it is the extension with the highest precedence.
TEST_F(ExtensionWebRequestTest, BlockingEventPrecedenceCancel) {
  std::string extension1_id("1");
  std::string extension2_id("2");
  ExtensionWebRequestEventRouter::RequestFilter filter;
  const std::string kEventName(keys::kOnBeforeRequest);
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
    &profile_, extension1_id, extension1_id, kEventName, kEventName + "/1",
    filter, ExtensionWebRequestEventRouter::ExtraInfoSpec::BLOCKING,
    ipc_sender_factory.GetWeakPtr());
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
    &profile_, extension2_id, extension2_id, kEventName, kEventName + "/2",
    filter, ExtensionWebRequestEventRouter::ExtraInfoSpec::BLOCKING,
    ipc_sender_factory.GetWeakPtr());

  GURL request_url("about:blank");
  net::URLRequest request(request_url, &delegate_);
  request.set_context(context_.get());

  // onBeforeRequest will be dispatched twice. The second response -
  // the redirect - would win, since it has a later |install_time|, but
  // the first response takes precedence because cancel >> redirect.
  GURL redirect_url("about:redirected");
  ExtensionWebRequestEventRouter::EventResponse* response = NULL;

  // Extension1 response. Arrives first, would be ignored in principle due to
  // install_time but "cancel" always wins.
  response = new ExtensionWebRequestEventRouter::EventResponse(
      extension1_id, base::Time::FromDoubleT(1));
  response->cancel = true;
  ipc_sender_.PushTask(
      base::Bind(&EventHandledOnIOThread,
          &profile_, extension1_id, kEventName, kEventName + "/1",
          request.identifier(), response));

  // Extension2 response. Arrives second, but has higher precedence
  // due to its later install_time.
  response = new ExtensionWebRequestEventRouter::EventResponse(
      extension2_id, base::Time::FromDoubleT(2));
  response->new_url = redirect_url;
  ipc_sender_.PushTask(
      base::Bind(&EventHandledOnIOThread,
          &profile_, extension2_id, kEventName, kEventName + "/2",
          request.identifier(), response));

  request.Start();

  MessageLoop::current()->Run();

  EXPECT_TRUE(!request.is_pending());
  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, request.status().error());
  EXPECT_EQ(request_url, request.url());
  EXPECT_EQ(1U, request.url_chain().size());
  EXPECT_EQ(0U, ipc_sender_.GetNumTasks());

  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      &profile_, extension1_id, kEventName + "/1");
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      &profile_, extension2_id, kEventName + "/2");
}

TEST_F(ExtensionWebRequestTest, SimulateChancelWhileBlocked) {
  // We subscribe to OnBeforeRequest and OnErrorOccurred.
  // While the OnBeforeRequest handler is blocked, we cancel the request.
  // We verify that the response of the blocked OnBeforeRequest handler
  // is ignored.

  std::string extension_id("1");
  ExtensionWebRequestEventRouter::RequestFilter filter;

  // Subscribe to OnBeforeRequest and OnErrorOccurred.
  const std::string kEventName(keys::kOnBeforeRequest);
  const std::string kEventName2(keys::kOnErrorOccurred);
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
    &profile_, extension_id, extension_id, kEventName, kEventName + "/1",
    filter, ExtensionWebRequestEventRouter::ExtraInfoSpec::BLOCKING,
    ipc_sender_factory.GetWeakPtr());
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
    &profile_, extension_id, extension_id, kEventName2, kEventName2 + "/1",
    filter, 0, ipc_sender_factory.GetWeakPtr());

  GURL request_url("about:blank");
  net::URLRequest request(request_url, &delegate_);
  request.set_context(context_.get());

  ExtensionWebRequestEventRouter::EventResponse* response = NULL;

  // Extension response for the OnBeforeRequest handler. This should not be
  // processed because request is canceled before the handler responds.
  response = new ExtensionWebRequestEventRouter::EventResponse(
      extension_id, base::Time::FromDoubleT(1));
  GURL redirect_url("about:redirected");
  response->new_url = redirect_url;
  ipc_sender_.PushTask(
      base::Bind(&EventHandledOnIOThread,
          &profile_, extension_id, kEventName, kEventName + "/1",
          request.identifier(), response));

  // Extension response for OnErrorOccurred: Terminate the message loop.
  ipc_sender_.PushTask(
      base::Bind(&MessageLoop::PostTask,
                 base::Unretained(MessageLoop::current()),
                 FROM_HERE, MessageLoop::QuitClosure()
                 ));

  request.Start();
  // request.Start() will have submitted OnBeforeRequest by the time we cancel.
  request.Cancel();
  MessageLoop::current()->Run();

  EXPECT_TRUE(!request.is_pending());
  EXPECT_EQ(net::URLRequestStatus::CANCELED, request.status().status());
  EXPECT_EQ(net::ERR_ABORTED, request.status().error());
  EXPECT_EQ(request_url, request.url());
  EXPECT_EQ(1U, request.url_chain().size());
  EXPECT_EQ(0U, ipc_sender_.GetNumTasks());

  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      &profile_, extension_id, kEventName + "/1");
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
        &profile_, extension_id, kEventName2 + "/1");
}

struct HeaderModificationTest_Header {
  const char* name;
  const char* value;
};

struct HeaderModificationTest_Modification {
  enum Type {
    SET,
    REMOVE
  };

  int extension_id;
  Type type;
  const char* key;
  const char* value;
};

struct HeaderModificationTest {
  int before_size;
  HeaderModificationTest_Header before[10];
  int modification_size;
  HeaderModificationTest_Modification modification[10];
  int after_size;
  HeaderModificationTest_Header after[10];
};

class ExtensionWebRequestHeaderModificationTest :
    public testing::TestWithParam<HeaderModificationTest> {
 public:
  ExtensionWebRequestHeaderModificationTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO, &message_loop_) {}

 protected:
  virtual void SetUp() {
    event_router_ = new ExtensionEventRouterForwarder();
    enable_referrers_.Init(
        prefs::kEnableReferrers, profile_.GetTestingPrefService(), NULL);
    network_delegate_.reset(new ChromeNetworkDelegate(
        event_router_.get(), NULL, NULL, &profile_,
        CookieSettings::Factory::GetForProfile(&profile_), &enable_referrers_));
    context_.reset(new TestURLRequestContext());
    context_->set_network_delegate(network_delegate_.get());
  }

  MessageLoopForIO message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  TestingProfile profile_;
  TestDelegate delegate_;
  BooleanPrefMember enable_referrers_;
  TestIPCSender ipc_sender_;
  scoped_refptr<ExtensionEventRouterForwarder> event_router_;
  scoped_refptr<ExtensionInfoMap> extension_info_map_;
  scoped_ptr<ChromeNetworkDelegate> network_delegate_;
  scoped_ptr<TestURLRequestContext> context_;
};

TEST_P(ExtensionWebRequestHeaderModificationTest, TestModifications) {
  std::string extension1_id("1");
  std::string extension2_id("2");
  std::string extension3_id("3");
  ExtensionWebRequestEventRouter::RequestFilter filter;
  const std::string kEventName(keys::kOnBeforeSendHeaders);
  base::WeakPtrFactory<TestIPCSender> ipc_sender_factory(&ipc_sender_);

  // Install two extensions that can modify headers. Extension 2 has
  // higher precedence than extension 1.
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension1_id, extension1_id, kEventName, kEventName + "/1",
      filter, ExtensionWebRequestEventRouter::ExtraInfoSpec::BLOCKING,
      ipc_sender_factory.GetWeakPtr());
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension2_id, extension2_id, kEventName, kEventName + "/2",
      filter, ExtensionWebRequestEventRouter::ExtraInfoSpec::BLOCKING,
      ipc_sender_factory.GetWeakPtr());

  // Install one extension that observes the final headers.
  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      &profile_, extension3_id, extension3_id, keys::kOnSendHeaders,
      std::string(keys::kOnSendHeaders) + "/3", filter,
      ExtensionWebRequestEventRouter::ExtraInfoSpec::REQUEST_HEADERS,
      ipc_sender_factory.GetWeakPtr());

  GURL request_url("http://doesnotexist/does_not_exist.html");
  net::URLRequest request(request_url, &delegate_);
  request.set_context(context_.get());

  // Initialize headers available before extensions are notified of the
  // onBeforeSendHeaders event.
  HeaderModificationTest test = GetParam();
  net::HttpRequestHeaders before_headers;
  for (int i = 0; i < test.before_size; ++i)
    before_headers.SetHeader(test.before[i].name, test.before[i].value);
  request.SetExtraRequestHeaders(before_headers);

  // Gather the modifications to the headers for the respective extensions.
  // We assume here that all modifications of one extension are listed
  // in a continuous block of |test.modifications_|.
  ExtensionWebRequestEventRouter::EventResponse* response = NULL;
  for (int i = 0; i < test.modification_size; ++i) {
    const HeaderModificationTest_Modification& mod = test.modification[i];
    if (response == NULL) {
      response = new ExtensionWebRequestEventRouter::EventResponse(
          mod.extension_id == 1 ? extension1_id : extension2_id,
          base::Time::FromDoubleT(mod.extension_id));
      response->request_headers.reset(new net::HttpRequestHeaders());
      response->request_headers->MergeFrom(request.extra_request_headers());
    }

    switch (mod.type) {
      case HeaderModificationTest_Modification::SET:
        response->request_headers->SetHeader(mod.key, mod.value);
        break;
      case HeaderModificationTest_Modification::REMOVE:
        response->request_headers->RemoveHeader(mod.key);
        break;
    }

    // Trigger the result when this is the last modification statement or
    // the block of modifications for the next extension starts.
    if (i+1 == test.modification_size ||
        mod.extension_id != test.modification[i+1].extension_id) {
      ipc_sender_.PushTask(
          base::Bind(&EventHandledOnIOThread,
              &profile_, mod.extension_id == 1 ? extension1_id : extension2_id,
              kEventName, kEventName + (mod.extension_id == 1 ? "/1" : "/2"),
              request.identifier(), response));
      response = NULL;
    }
  }

  // Don't do anything for the onSendHeaders message.
  ipc_sender_.PushTask(base::Bind(&base::DoNothing));

  // Note that we mess up the headers slightly:
  // request.Start() will first add additional headers (e.g. the User-Agent)
  // and then send an event to the extension. When we have prepared our
  // answers to the onBeforeSendHeaders events above, these headers did not
  // exists and are therefore not listed in the responses. This makes
  // them seem deleted.
  request.Start();
  MessageLoop::current()->Run();

  EXPECT_TRUE(!request.is_pending());
  // This cannot succeed as we send the request to a server that does not exist.
  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(request_url, request.url());
  EXPECT_EQ(1U, request.url_chain().size());
  EXPECT_EQ(0U, ipc_sender_.GetNumTasks());

  // Calculate the expected headers.
  net::HttpRequestHeaders expected_headers;
  for (int i = 0; i < test.after_size; ++i) {
    expected_headers.SetHeader(test.after[i].name,
                               test.after[i].value);
  }

  // Counter for the number of observed onSendHeaders events.
  int num_headers_observed = 0;

  // Search the onSendHeaders signal in the IPC messages and check that
  // it contained the correct headers.
  TestIPCSender::SentMessages::const_iterator i;
  for (i = ipc_sender_.sent_begin(); i != ipc_sender_.sent_end(); ++i) {
    IPC::Message* message = i->get();
    if (ExtensionMsg_MessageInvoke::ID != message->type())
      continue;
    ExtensionMsg_MessageInvoke::Param message_tuple;
    ExtensionMsg_MessageInvoke::Read(message, &message_tuple);
    ListValue& args = message_tuple.c;

    std::string event_name;
    if (!args.GetString(0, &event_name) ||
        event_name !=  std::string(keys::kOnSendHeaders) + "/3") {
      continue;
    }

    std::string event_arg_string;
    ASSERT_TRUE(args.GetString(1, &event_arg_string));

    scoped_ptr<Value> event_arg_value(
        JSONStringValueSerializer(event_arg_string).Deserialize(NULL, NULL));
    ASSERT_TRUE(event_arg_value.get() &&
                event_arg_value->IsType(Value::TYPE_LIST));

    DictionaryValue* event_arg_dict = NULL;
    ASSERT_TRUE(
        static_cast<ListValue*>(event_arg_value.get())->GetDictionary(
            0, &event_arg_dict));

    ListValue* request_headers = NULL;
    ASSERT_TRUE(event_arg_dict->GetList(keys::kRequestHeadersKey,
                                        &request_headers));

    net::HttpRequestHeaders observed_headers;
    for (size_t j = 0; j < request_headers->GetSize(); ++j) {
      DictionaryValue* header = NULL;
      ASSERT_TRUE(request_headers->GetDictionary(j, &header));
      std::string key;
      std::string value;
      ASSERT_TRUE(header->GetString(keys::kHeaderNameKey, &key));
      ASSERT_TRUE(header->GetString(keys::kHeaderValueKey, &value));
      observed_headers.SetHeader(key, value);
    }

    EXPECT_EQ(expected_headers.ToString(), observed_headers.ToString());
    ++num_headers_observed;
  }
  EXPECT_EQ(1, num_headers_observed);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      &profile_, extension1_id, kEventName + "/1");
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      &profile_, extension2_id, kEventName + "/2");
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      &profile_, extension3_id, std::string(keys::kOnSendHeaders) + "/3");
};

namespace {

void TestInitFromValue(const std::string& values, bool expected_return_code,
                       int expected_extra_info_spec) {
  // Create a ListValue of strings.
  std::vector<std::string> split_values;
  scoped_ptr<base::ListValue> list_value(new base::ListValue());
  size_t num_values = Tokenize(values, ",", &split_values);
  for (size_t i = 0; i < num_values ; ++i)
    list_value->Append(new base::StringValue(split_values[i]));
  int actual_info_spec;
  bool actual_return_code =
      ExtensionWebRequestEventRouter::ExtraInfoSpec::InitFromValue(
          *list_value, &actual_info_spec);
  EXPECT_EQ(expected_return_code, actual_return_code);
  if (expected_return_code)
    EXPECT_EQ(expected_extra_info_spec, actual_info_spec);
}

}
TEST_F(ExtensionWebRequestTest, InitFromValue) {
  TestInitFromValue("", true, 0);

  // Single valid values.
  TestInitFromValue(
      "requestHeaders",
      true,
      ExtensionWebRequestEventRouter::ExtraInfoSpec::REQUEST_HEADERS);
  TestInitFromValue(
      "responseHeaders",
      true,
      ExtensionWebRequestEventRouter::ExtraInfoSpec::RESPONSE_HEADERS);
  TestInitFromValue(
      "blocking",
      true,
      ExtensionWebRequestEventRouter::ExtraInfoSpec::BLOCKING);
  TestInitFromValue(
      "asyncBlocking",
      true,
      ExtensionWebRequestEventRouter::ExtraInfoSpec::ASYNC_BLOCKING);

  // Multiple valid values are bitwise-or'ed.
  TestInitFromValue(
      "requestHeaders,blocking",
      true,
      ExtensionWebRequestEventRouter::ExtraInfoSpec::REQUEST_HEADERS |
      ExtensionWebRequestEventRouter::ExtraInfoSpec::BLOCKING);

  // Any invalid values lead to a bad parse.
  TestInitFromValue("invalidValue", false, 0);
  TestInitFromValue("blocking,invalidValue", false, 0);
  TestInitFromValue("invalidValue1,invalidValue2", false, 0);

  // BLOCKING and ASYNC_BLOCKING are mutually exclusive.
  TestInitFromValue("blocking,asyncBlocking", false, 0);
}

namespace {

const HeaderModificationTest_Modification::Type SET =
    HeaderModificationTest_Modification::SET;
const HeaderModificationTest_Modification::Type REMOVE =
    HeaderModificationTest_Modification::REMOVE;

HeaderModificationTest kTests[] = {
  // Check that extension 2 always wins when settings the same header.
  {
    // Headers before test.
    2, { {"header1", "value1"},
         {"header2", "value2"} },
    // Modifications in test.
    2, { {1, SET, "header1", "foo"},
         {2, SET, "header1", "bar"} },
    // Headers after test.
    2, { {"header1", "bar"},
         {"header2", "value2"} }
  },
  // Same as before in reverse execution order.
  {
    // Headers before test.
    2, { {"header1", "value1"},
         {"header2", "value2"} },
    // Modifications in test.
    2, { {2, SET, "header1", "bar"},
         {1, SET, "header1", "foo"} },
    // Headers after test.
    2, { {"header1", "bar"},
         {"header2", "value2"} }
  },
  // Check that two extensions can modify different headers that do not
  // conflict.
  {
    // Headers before test.
    2, { {"header1", "value1"},
         {"header2", "value2"} },
    // Modifications in test.
    2, { {1, SET, "header1", "foo"},
         {2, SET, "header2", "bar"} },
    // Headers after test.
    2, { {"header1", "foo"},
         {"header2", "bar"} }
  },
  // Check insert/delete conflict.
  {
    // Headers before test.
    1, { {"header1", "value1"} },
    // Modifications in test.
    2, { {1, SET, "header1", "foo"},
         {2, REMOVE, "header1", NULL} },
    // Headers after test.
    0, { }
  },
  {
    // Headers before test.
    1, { {"header1", "value1"} },
    // Modifications in test.
    2, { {2, REMOVE, "header1", NULL},
         {1, SET, "header1", "foo"} },
    // Headers after test.
    0, {}
  },
  {
    // Headers before test.
    1, { {"header1", "value1"} },
    // Modifications in test.
    2, { {1, REMOVE, "header1", NULL},
         {2, SET, "header1", "foo"} },
    // Headers after test.
    1, { {"header1", "foo"} }
  },
  {
    // Headers before test.
    1, { {"header1", "value1"} },
    // Modifications in test.
    2, { {2, SET, "header1", "foo"},
         {1, REMOVE, "header1", NULL} },
    // Headers after test.
    1, { {"header1", "foo"} }
  },
  // Check that edits are atomic (i.e. either all edit requests of an
  // extension are executed or none).
  {
    // Headers before test.
    0, { },
    // Modifications in test.
    3, { {1, SET, "header1", "value1"},
         {1, SET, "header2", "value2"},
         {2, SET, "header1", "foo"} },
    // Headers after test.
    1, { {"header1", "foo"} } // set(header2) is ignored
  },
  // Check that identical edits do not conflict (set(header2) would be ignored
  // if set(header1) were considered a conflict).
  {
    // Headers before test.
    0, { },
    // Modifications in test.
    3, { {1, SET, "header1", "value2"},
         {1, SET, "header2", "foo"},
         {2, SET, "header1", "value2"} },
    // Headers after test.
    2, { {"header1", "value2"},
         {"header2", "foo"} }
  },
  // Check that identical deletes do not conflict (set(header2) would be ignored
  // if delete(header1) were considered a conflict).
  {
    // Headers before test.
    1, { {"header1", "value1"} },
    // Modifications in test.
    3, { {1, REMOVE, "header1", NULL},
         {1, SET, "header2", "foo"},
         {2, REMOVE, "header1", NULL} },
    // Headers after test.
    1, { {"header2", "foo"} }
  },
  // Check that setting a value to an identical value is not considered an
  // edit operation that can conflict.
  {
    // Headers before test.
    1, { {"header1", "value1"} },
    // Modifications in test.
    3, { {1, SET, "header1", "foo"},
         {1, SET, "header2", "bar"},
         {2, SET, "header1", "value1"} },
    // Headers after test.
    2, { {"header1", "foo"},
         {"header2", "bar"} }
  },
};

INSTANTIATE_TEST_CASE_P(
    ExtensionWebRequest,
    ExtensionWebRequestHeaderModificationTest,
    ::testing::ValuesIn(kTests));

} // namespace


TEST(ExtensionWebRequestHelpersTest,
     TestInDecreasingExtensionInstallationTimeOrder) {
  linked_ptr<EventResponseDelta> a(
      new EventResponseDelta("ext_1", base::Time::FromInternalValue(0)));
  linked_ptr<EventResponseDelta> b(
      new EventResponseDelta("ext_2", base::Time::FromInternalValue(1000)));
  EXPECT_FALSE(InDecreasingExtensionInstallationTimeOrder(a, a));
  EXPECT_FALSE(InDecreasingExtensionInstallationTimeOrder(a, b));
  EXPECT_TRUE(InDecreasingExtensionInstallationTimeOrder(b, a));
}

TEST(ExtensionWebRequestHelpersTest, TestStringToCharList) {
  ListValue list_value;
  list_value.Append(Value::CreateIntegerValue('1'));
  list_value.Append(Value::CreateIntegerValue('2'));
  list_value.Append(Value::CreateIntegerValue('3'));
  list_value.Append(Value::CreateIntegerValue(0xFE));
  list_value.Append(Value::CreateIntegerValue(0xD1));

  unsigned char char_value[] = {'1', '2', '3', 0xFE, 0xD1};
  std::string string_value(reinterpret_cast<char *>(char_value), 5);

  scoped_ptr<ListValue> converted_list(StringToCharList(string_value));
  EXPECT_TRUE(list_value.Equals(converted_list.get()));

  std::string converted_string;
  EXPECT_TRUE(CharListToString(&list_value, &converted_string));
  EXPECT_EQ(string_value, converted_string);
}

TEST(ExtensionWebRequestHelpersTest, TestCalculateOnBeforeRequestDelta) {
  const bool cancel = true;
  const GURL localhost("http://localhost");
  scoped_ptr<EventResponseDelta> delta(
      CalculateOnBeforeRequestDelta("extid", base::Time::Now(),
          cancel, localhost));
  ASSERT_TRUE(delta.get());
  EXPECT_TRUE(delta->cancel);
  EXPECT_EQ(localhost, delta->new_url);
}

TEST(ExtensionWebRequestHelpersTest, TestCalculateOnBeforeSendHeadersDelta) {
  const bool cancel = true;
  std::string value;
  net::HttpRequestHeaders old_headers;
  old_headers.AddHeadersFromString("key1: value1\r\n"
                                   "key2: value2\r\n");

  // Test adding a header.
  net::HttpRequestHeaders new_headers_added;
  new_headers_added.AddHeadersFromString("key1: value1\r\n"
                                         "key3: value3\r\n"
                                         "key2: value2\r\n");
  scoped_ptr<EventResponseDelta> delta_added(
      CalculateOnBeforeSendHeadersDelta("extid", base::Time::Now(), cancel,
          &old_headers, &new_headers_added));
  ASSERT_TRUE(delta_added.get());
  EXPECT_TRUE(delta_added->cancel);
  ASSERT_TRUE(delta_added->modified_request_headers.GetHeader("key3", &value));
  EXPECT_EQ("value3", value);

  // Test deleting a header.
  net::HttpRequestHeaders new_headers_deleted;
  new_headers_deleted.AddHeadersFromString("key1: value1\r\n");
  scoped_ptr<EventResponseDelta> delta_deleted(
      CalculateOnBeforeSendHeadersDelta("extid", base::Time::Now(), cancel,
          &old_headers, &new_headers_deleted));
  ASSERT_TRUE(delta_deleted.get());
  ASSERT_EQ(1u, delta_deleted->deleted_request_headers.size());
  ASSERT_EQ("key2", delta_deleted->deleted_request_headers.front());

  // Test modifying a header.
  net::HttpRequestHeaders new_headers_modified;
  new_headers_modified.AddHeadersFromString("key1: value1\r\n"
                                            "key2: value3\r\n");
  scoped_ptr<EventResponseDelta> delta_modified(
      CalculateOnBeforeSendHeadersDelta("extid", base::Time::Now(), cancel,
          &old_headers, &new_headers_modified));
  ASSERT_TRUE(delta_modified.get());
  EXPECT_TRUE(delta_modified->deleted_request_headers.empty());
  ASSERT_TRUE(
      delta_modified->modified_request_headers.GetHeader("key2", &value));
  EXPECT_EQ("value3", value);

  // Test modifying a header if extension author just appended a new (key,
  // value) pair with a key that existed before. This is incorrect
  // usage of the API that shall be handled gracefully.
  net::HttpRequestHeaders new_headers_modified2;
  new_headers_modified2.AddHeadersFromString("key1: value1\r\n"
                                             "key2: value2\r\n"
                                             "key2: value3\r\n");
  scoped_ptr<EventResponseDelta> delta_modified2(
      CalculateOnBeforeSendHeadersDelta("extid", base::Time::Now(), cancel,
          &old_headers, &new_headers_modified));
  ASSERT_TRUE(delta_modified2.get());
  EXPECT_TRUE(delta_modified2->deleted_request_headers.empty());
  ASSERT_TRUE(
      delta_modified2->modified_request_headers.GetHeader("key2", &value));
  EXPECT_EQ("value3", value);
}

TEST(ExtensionWebRequestHelpersTest, TestCalculateOnHeadersReceivedDelta) {
  const bool cancel = true;
  char base_headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n"
      "Key2: Value2\r\n"
      "Key3: Value3\r\n"
      "\r\n";
  scoped_refptr<net::HttpResponseHeaders> base_headers(
      new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(
            base_headers_string, sizeof(base_headers_string))));

  ResponseHeaders new_headers;
  new_headers.push_back(ResponseHeader("kEy1", "Value1"));  // Unchanged
  new_headers.push_back(ResponseHeader("Key2", "Value1"));  // Modified
  // Key3 is deleted
  new_headers.push_back(ResponseHeader("Key4", "Value4"));  // Added

  scoped_ptr<EventResponseDelta> delta(
      CalculateOnHeadersReceivedDelta("extid", base::Time::Now(), cancel,
          base_headers, &new_headers));
  ASSERT_TRUE(delta.get());
  EXPECT_TRUE(delta->cancel);
  EXPECT_EQ(2u, delta->added_response_headers.size());
  EXPECT_TRUE(Contains(delta->added_response_headers,
                       ResponseHeader("Key2", "Value1")));
  EXPECT_TRUE(Contains(delta->added_response_headers,
                       ResponseHeader("Key4", "Value4")));
  EXPECT_EQ(2u, delta->deleted_response_headers.size());
  EXPECT_TRUE(Contains(delta->deleted_response_headers,
                        ResponseHeader("Key2", "Value2")));
  EXPECT_TRUE(Contains(delta->deleted_response_headers,
                        ResponseHeader("Key3", "Value3")));
}

TEST(ExtensionWebRequestHelpersTest, TestCalculateOnAuthRequiredDelta) {
  const bool cancel = true;

  string16 username = ASCIIToUTF16("foo");
  string16 password = ASCIIToUTF16("bar");
  scoped_ptr<net::AuthCredentials> credentials(
      new net::AuthCredentials(username, password));

  scoped_ptr<EventResponseDelta> delta(
      CalculateOnAuthRequiredDelta("extid", base::Time::Now(), cancel,
          &credentials));
  ASSERT_TRUE(delta.get());
  EXPECT_TRUE(delta->cancel);
  ASSERT_TRUE(delta->auth_credentials.get());
  EXPECT_EQ(username, delta->auth_credentials->username());
  EXPECT_EQ(password, delta->auth_credentials->password());
}

TEST(ExtensionWebRequestHelpersTest, TestMergeCancelOfResponses) {
  EventResponseDeltas deltas;
  EventLogEntries event_log;
  bool canceled = false;

  // Single event that does not cancel.
  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(1000)));
  d1->cancel = false;
  deltas.push_back(d1);
  MergeCancelOfResponses(deltas, &canceled, &event_log);
  EXPECT_FALSE(canceled);
  EXPECT_TRUE(event_log.empty());

  // Second event that cancels the request
  linked_ptr<EventResponseDelta> d2(
      new EventResponseDelta("extid2", base::Time::FromInternalValue(500)));
  d2->cancel = true;
  deltas.push_back(d2);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  MergeCancelOfResponses(deltas, &canceled, &event_log);
  EXPECT_TRUE(canceled);
  EXPECT_EQ(1u, event_log.size());
}

TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeRequestResponses) {
  EventResponseDeltas deltas;
  EventLogEntries event_log;
  std::set<std::string> conflicting_extensions;
  GURL effective_new_url;

  // No redirect
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(0)));
  deltas.push_back(d0);
  MergeOnBeforeRequestResponses(
      deltas, &effective_new_url, &conflicting_extensions, &event_log);
  EXPECT_TRUE(effective_new_url.is_empty());

  // Single redirect.
  GURL new_url_1("http://foo.com");
  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(1000)));
  d1->new_url = GURL(new_url_1);
  deltas.push_back(d1);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  event_log.clear();
  MergeOnBeforeRequestResponses(
      deltas, &effective_new_url, &conflicting_extensions, &event_log);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_TRUE(conflicting_extensions.empty());
  EXPECT_EQ(1u, event_log.size());

  // Ignored redirect (due to precedence).
  GURL new_url_2("http://bar.com");
  linked_ptr<EventResponseDelta> d2(
      new EventResponseDelta("extid2", base::Time::FromInternalValue(500)));
  d2->new_url = GURL(new_url_2);
  deltas.push_back(d2);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  event_log.clear();
  MergeOnBeforeRequestResponses(
      deltas, &effective_new_url, &conflicting_extensions, &event_log);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_EQ(1u, conflicting_extensions.size());
  EXPECT_TRUE(ContainsKey(conflicting_extensions, "extid2"));
  EXPECT_EQ(2u, event_log.size());

  // Overriding redirect.
  GURL new_url_3("http://baz.com");
  linked_ptr<EventResponseDelta> d3(
      new EventResponseDelta("extid3", base::Time::FromInternalValue(1500)));
  d3->new_url = GURL(new_url_3);
  deltas.push_back(d3);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  conflicting_extensions.clear();
  event_log.clear();
  MergeOnBeforeRequestResponses(
      deltas, &effective_new_url, &conflicting_extensions, &event_log);
  EXPECT_EQ(new_url_3, effective_new_url);
  EXPECT_EQ(2u, conflicting_extensions.size());
  EXPECT_TRUE(ContainsKey(conflicting_extensions, "extid1"));
  EXPECT_TRUE(ContainsKey(conflicting_extensions, "extid2"));
  EXPECT_EQ(3u, event_log.size());

  // Check that identical redirects don't cause a conflict.
  linked_ptr<EventResponseDelta> d4(
      new EventResponseDelta("extid4", base::Time::FromInternalValue(2000)));
  d4->new_url = GURL(new_url_3);
  deltas.push_back(d4);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  conflicting_extensions.clear();
  event_log.clear();
  MergeOnBeforeRequestResponses(
      deltas, &effective_new_url, &conflicting_extensions, &event_log);
  EXPECT_EQ(new_url_3, effective_new_url);
  EXPECT_EQ(2u, conflicting_extensions.size());
  EXPECT_TRUE(ContainsKey(conflicting_extensions, "extid1"));
  EXPECT_TRUE(ContainsKey(conflicting_extensions, "extid2"));
  EXPECT_EQ(4u, event_log.size());
}

// This tests that we can redirect to data:// urls, which is considered
// a kind of cancelling requests.
TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeRequestResponses2) {
  EventResponseDeltas deltas;
  EventLogEntries event_log;
  std::set<std::string> conflicting_extensions;
  GURL effective_new_url;

  // Single redirect.
  GURL new_url_0("http://foo.com");
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(2000)));
  d0->new_url = GURL(new_url_0);
  deltas.push_back(d0);
  MergeOnBeforeRequestResponses(
      deltas, &effective_new_url, &conflicting_extensions, &event_log);
  EXPECT_EQ(new_url_0, effective_new_url);

  // Cancel request by redirecting to a data:// URL. This shall override
  // the other redirect but not cause any conflict warnings.
  GURL new_url_1("data://foo");
  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(1500)));
  d1->new_url = GURL(new_url_1);
  deltas.push_back(d1);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  event_log.clear();
  MergeOnBeforeRequestResponses(
      deltas, &effective_new_url, &conflicting_extensions, &event_log);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_TRUE(conflicting_extensions.empty());
  EXPECT_EQ(1u, event_log.size());

  // Cancel request by redirecting to the same data:// URL. This shall
  // not create any conflicts as it is in line with d1.
  GURL new_url_2("data://foo");
  linked_ptr<EventResponseDelta> d2(
      new EventResponseDelta("extid2", base::Time::FromInternalValue(1000)));
  d2->new_url = GURL(new_url_2);
  deltas.push_back(d2);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  event_log.clear();
  MergeOnBeforeRequestResponses(
      deltas, &effective_new_url, &conflicting_extensions, &event_log);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_TRUE(conflicting_extensions.empty());
  EXPECT_EQ(2u, event_log.size());

  // Cancel redirect by redirecting to a different data:// URL. This needs
  // to create a conflict.
  GURL new_url_3("data://something_totally_different");
  linked_ptr<EventResponseDelta> d3(
      new EventResponseDelta("extid3", base::Time::FromInternalValue(500)));
  d3->new_url = GURL(new_url_3);
  deltas.push_back(d3);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  event_log.clear();
  MergeOnBeforeRequestResponses(
      deltas, &effective_new_url, &conflicting_extensions, &event_log);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_EQ(1u, conflicting_extensions.size());
  EXPECT_TRUE(ContainsKey(conflicting_extensions, "extid3"));
  EXPECT_EQ(3u, event_log.size());
}

// This tests that we can redirect to about:blank, which is considered
// a kind of cancelling requests.
TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeRequestResponses3) {
  EventResponseDeltas deltas;
  EventLogEntries event_log;
  std::set<std::string> conflicting_extensions;
  GURL effective_new_url;

  // Single redirect.
  GURL new_url_0("http://foo.com");
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(2000)));
  d0->new_url = GURL(new_url_0);
  deltas.push_back(d0);
  MergeOnBeforeRequestResponses(
      deltas, &effective_new_url, &conflicting_extensions, &event_log);
  EXPECT_EQ(new_url_0, effective_new_url);

  // Cancel request by redirecting to about:blank. This shall override
  // the other redirect but not cause any conflict warnings.
  GURL new_url_1("about:blank");
  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(1500)));
  d1->new_url = GURL(new_url_1);
  deltas.push_back(d1);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  event_log.clear();
  MergeOnBeforeRequestResponses(
      deltas, &effective_new_url, &conflicting_extensions, &event_log);
  EXPECT_EQ(new_url_1, effective_new_url);
  EXPECT_TRUE(conflicting_extensions.empty());
  EXPECT_EQ(1u, event_log.size());
}

TEST(ExtensionWebRequestHelpersTest, TestMergeOnBeforeSendHeadersResponses) {
  net::HttpRequestHeaders base_headers;
  base_headers.AddHeaderFromString("key1: value 1");
  base_headers.AddHeaderFromString("key2: value 2");
  EventLogEntries event_log;
  std::set<std::string> conflicting_extensions;
  std::string header_value;
  EventResponseDeltas deltas;

  // Check that we can handle not changing the headers.
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(2500)));
  deltas.push_back(d0);
  net::HttpRequestHeaders headers0;
  headers0.MergeFrom(base_headers);
  MergeOnBeforeSendHeadersResponses(
      deltas, &headers0, &conflicting_extensions, &event_log);
  ASSERT_TRUE(headers0.GetHeader("key1", &header_value));
  EXPECT_EQ("value 1", header_value);
  ASSERT_TRUE(headers0.GetHeader("key2", &header_value));
  EXPECT_EQ("value 2", header_value);
  EXPECT_EQ(0u, conflicting_extensions.size());
  EXPECT_EQ(0u, event_log.size());

  // Delete, modify and add a header.
  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(2000)));
  d1->deleted_request_headers.push_back("key1");
  d1->modified_request_headers.AddHeaderFromString("key2: value 3");
  d1->modified_request_headers.AddHeaderFromString("key3: value 3");
  deltas.push_back(d1);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  net::HttpRequestHeaders headers1;
  headers1.MergeFrom(base_headers);
  MergeOnBeforeSendHeadersResponses(
      deltas, &headers1, &conflicting_extensions, &event_log);
  EXPECT_FALSE(headers1.HasHeader("key1"));
  ASSERT_TRUE(headers1.GetHeader("key2", &header_value));
  EXPECT_EQ("value 3", header_value);
  ASSERT_TRUE(headers1.GetHeader("key3", &header_value));
  EXPECT_EQ("value 3", header_value);
  EXPECT_EQ(0u, conflicting_extensions.size());
  EXPECT_EQ(1u, event_log.size());

  // Check that conflicts are atomic, i.e. if one header modification
  // collides all other conflicts of the same extension are declined as well.
  linked_ptr<EventResponseDelta> d2(
      new EventResponseDelta("extid2", base::Time::FromInternalValue(1500)));
  // This one conflicts:
  d2->modified_request_headers.AddHeaderFromString("key3: value 0");
  d2->modified_request_headers.AddHeaderFromString("key4: value 4");
  deltas.push_back(d2);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  conflicting_extensions.clear();
  event_log.clear();
  net::HttpRequestHeaders headers2;
  headers2.MergeFrom(base_headers);
  MergeOnBeforeSendHeadersResponses(
      deltas, &headers2, &conflicting_extensions, &event_log);
  EXPECT_FALSE(headers2.HasHeader("key1"));
  ASSERT_TRUE(headers2.GetHeader("key2", &header_value));
  EXPECT_EQ("value 3", header_value);
  ASSERT_TRUE(headers2.GetHeader("key3", &header_value));
  EXPECT_EQ("value 3", header_value);
  EXPECT_FALSE(headers2.HasHeader("key4"));
  EXPECT_EQ(1u, conflicting_extensions.size());
  EXPECT_TRUE(ContainsKey(conflicting_extensions, "extid2"));
  EXPECT_EQ(2u, event_log.size());

  // Check that identical modifications don't conflict and operations
  // can be merged.
  linked_ptr<EventResponseDelta> d3(
      new EventResponseDelta("extid3", base::Time::FromInternalValue(1000)));
  d3->deleted_request_headers.push_back("key1");
  d3->modified_request_headers.AddHeaderFromString("key2: value 3");
  d3->modified_request_headers.AddHeaderFromString("key5: value 5");
  deltas.push_back(d3);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  conflicting_extensions.clear();
  event_log.clear();
  net::HttpRequestHeaders headers3;
  headers3.MergeFrom(base_headers);
  MergeOnBeforeSendHeadersResponses(
      deltas, &headers3, &conflicting_extensions, &event_log);
  EXPECT_FALSE(headers3.HasHeader("key1"));
  ASSERT_TRUE(headers3.GetHeader("key2", &header_value));
  EXPECT_EQ("value 3", header_value);
  ASSERT_TRUE(headers3.GetHeader("key3", &header_value));
  EXPECT_EQ("value 3", header_value);
  ASSERT_TRUE(headers3.GetHeader("key5", &header_value));
  EXPECT_EQ("value 5", header_value);
  EXPECT_EQ(1u, conflicting_extensions.size());
  EXPECT_TRUE(ContainsKey(conflicting_extensions, "extid2"));
  EXPECT_EQ(3u, event_log.size());
}

TEST(ExtensionWebRequestHelpersTest, TestMergeOnHeadersReceivedResponses) {
    EventLogEntries event_log;
    std::set<std::string> conflicting_extensions;
    std::string header_value;
    EventResponseDeltas deltas;

  char base_headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n"
      "Key2: Value2\r\n"
      "\r\n";
  scoped_refptr<net::HttpResponseHeaders> base_headers(
      new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(
            base_headers_string, sizeof(base_headers_string))));

  // Check that we can handle if not touching the response headers.
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(3000)));
  deltas.push_back(d0);
  scoped_refptr<net::HttpResponseHeaders> new_headers0;
  MergeOnHeadersReceivedResponses(
        deltas, base_headers.get(), &new_headers0, &conflicting_extensions,
        &event_log);
  EXPECT_FALSE(new_headers0.get());
  EXPECT_EQ(0u, conflicting_extensions.size());
  EXPECT_EQ(0u, event_log.size());

  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(2000)));
  d1->deleted_response_headers.push_back(ResponseHeader("KEY1", "Value1"));
  d1->deleted_response_headers.push_back(ResponseHeader("KEY2", "Value2"));
  d1->added_response_headers.push_back(ResponseHeader("Key2", "Value3"));
  deltas.push_back(d1);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  conflicting_extensions.clear();
  event_log.clear();
  scoped_refptr<net::HttpResponseHeaders> new_headers1;
  MergeOnHeadersReceivedResponses(
        deltas, base_headers.get(), &new_headers1, &conflicting_extensions,
        &event_log);
  ASSERT_TRUE(new_headers1.get());
  std::multimap<std::string, std::string> expected1;
  expected1.insert(std::pair<std::string, std::string>("Key2", "Value3"));
  void* iter = NULL;
  std::string name;
  std::string value;
  std::multimap<std::string, std::string> actual1;
  while (new_headers1->EnumerateHeaderLines(&iter, &name, &value)) {
    actual1.insert(std::pair<std::string, std::string>(name, value));
  }
  EXPECT_EQ(expected1, actual1);
  EXPECT_EQ(0u, conflicting_extensions.size());
  EXPECT_EQ(1u, event_log.size());

  // Check that we replace response headers only once.
  linked_ptr<EventResponseDelta> d2(
      new EventResponseDelta("extid2", base::Time::FromInternalValue(1500)));
  // Note that we use a different capitalization of KeY2. This should not
  // matter.
  d2->deleted_response_headers.push_back(ResponseHeader("KeY2", "Value2"));
  d2->added_response_headers.push_back(ResponseHeader("Key2", "Value4"));
  deltas.push_back(d2);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  conflicting_extensions.clear();
  event_log.clear();
  scoped_refptr<net::HttpResponseHeaders> new_headers2;
  MergeOnHeadersReceivedResponses(
        deltas, base_headers.get(), &new_headers2, &conflicting_extensions,
        &event_log);
  ASSERT_TRUE(new_headers2.get());
  iter = NULL;
  std::multimap<std::string, std::string> actual2;
  while (new_headers2->EnumerateHeaderLines(&iter, &name, &value)) {
    actual2.insert(std::pair<std::string, std::string>(name, value));
  }
  EXPECT_EQ(expected1, actual2);
  EXPECT_EQ(1u, conflicting_extensions.size());
  EXPECT_TRUE(ContainsKey(conflicting_extensions, "extid2"));
  EXPECT_EQ(2u, event_log.size());
}

// Check that we do not delete too much
TEST(ExtensionWebRequestHelpersTest,
     TestMergeOnHeadersReceivedResponsesDeletion) {
    EventLogEntries event_log;
    std::set<std::string> conflicting_extensions;
    std::string header_value;
    EventResponseDeltas deltas;

  char base_headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n"
      "Key1: Value2\r\n"
      "Key1: Value3\r\n"
      "Key2: Value4\r\n"
      "\r\n";
  scoped_refptr<net::HttpResponseHeaders> base_headers(
      new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(
            base_headers_string, sizeof(base_headers_string))));

  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(2000)));
  d1->deleted_response_headers.push_back(ResponseHeader("KEY1", "Value2"));
  deltas.push_back(d1);
  scoped_refptr<net::HttpResponseHeaders> new_headers1;
  MergeOnHeadersReceivedResponses(
        deltas, base_headers.get(), &new_headers1, &conflicting_extensions,
        &event_log);
  ASSERT_TRUE(new_headers1.get());
  std::multimap<std::string, std::string> expected1;
  expected1.insert(std::pair<std::string, std::string>("Key1", "Value1"));
  expected1.insert(std::pair<std::string, std::string>("Key1", "Value3"));
  expected1.insert(std::pair<std::string, std::string>("Key2", "Value4"));
  void* iter = NULL;
  std::string name;
  std::string value;
  std::multimap<std::string, std::string> actual1;
  while (new_headers1->EnumerateHeaderLines(&iter, &name, &value)) {
    actual1.insert(std::pair<std::string, std::string>(name, value));
  }
  EXPECT_EQ(expected1, actual1);
  EXPECT_EQ(0u, conflicting_extensions.size());
  EXPECT_EQ(1u, event_log.size());
}

TEST(ExtensionWebRequestHelpersTest, TestMergeOnAuthRequiredResponses) {
  EventLogEntries event_log;
  std::set<std::string> conflicting_extensions;
  EventResponseDeltas deltas;
  string16 username = ASCIIToUTF16("foo");
  string16 password = ASCIIToUTF16("bar");
  string16 password2 = ASCIIToUTF16("baz");

  // Check that we can handle if not returning credentials.
  linked_ptr<EventResponseDelta> d0(
      new EventResponseDelta("extid0", base::Time::FromInternalValue(3000)));
  deltas.push_back(d0);
  net::AuthCredentials auth0;
  bool credentials_set = MergeOnAuthRequiredResponses(
        deltas, &auth0, &conflicting_extensions, &event_log);
  EXPECT_FALSE(credentials_set);
  EXPECT_TRUE(auth0.Empty());
  EXPECT_EQ(0u, conflicting_extensions.size());
  EXPECT_EQ(0u, event_log.size());

  // Check that we can set AuthCredentials.
  linked_ptr<EventResponseDelta> d1(
      new EventResponseDelta("extid1", base::Time::FromInternalValue(2000)));
  d1->auth_credentials.reset(new net::AuthCredentials(username, password));
  deltas.push_back(d1);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  conflicting_extensions.clear();
  event_log.clear();
  net::AuthCredentials auth1;
  credentials_set = MergeOnAuthRequiredResponses(
        deltas, &auth1, &conflicting_extensions, &event_log);
  EXPECT_TRUE(credentials_set);
  EXPECT_FALSE(auth1.Empty());
  EXPECT_EQ(username, auth1.username());
  EXPECT_EQ(password, auth1.password());
  EXPECT_EQ(0u, conflicting_extensions.size());
  EXPECT_EQ(1u, event_log.size());

  // Check that we set AuthCredentials only once.
  linked_ptr<EventResponseDelta> d2(
      new EventResponseDelta("extid2", base::Time::FromInternalValue(1500)));
  d2->auth_credentials.reset(new net::AuthCredentials(username, password2));
  deltas.push_back(d2);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  conflicting_extensions.clear();
  event_log.clear();
  net::AuthCredentials auth2;
  credentials_set = MergeOnAuthRequiredResponses(
        deltas, &auth2, &conflicting_extensions, &event_log);
  EXPECT_TRUE(credentials_set);
  EXPECT_FALSE(auth2.Empty());
  EXPECT_EQ(username, auth1.username());
  EXPECT_EQ(password, auth1.password());
  EXPECT_EQ(1u, conflicting_extensions.size());
  EXPECT_TRUE(ContainsKey(conflicting_extensions, "extid2"));
  EXPECT_EQ(2u, event_log.size());

  // Check that we can set identical AuthCredentials twice without causing
  // a conflict.
  linked_ptr<EventResponseDelta> d3(
      new EventResponseDelta("extid3", base::Time::FromInternalValue(1000)));
  d3->auth_credentials.reset(new net::AuthCredentials(username, password));
  deltas.push_back(d3);
  deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
  conflicting_extensions.clear();
  event_log.clear();
  net::AuthCredentials auth3;
  credentials_set = MergeOnAuthRequiredResponses(
        deltas, &auth3, &conflicting_extensions, &event_log);
  EXPECT_TRUE(credentials_set);
  EXPECT_FALSE(auth3.Empty());
  EXPECT_EQ(username, auth1.username());
  EXPECT_EQ(password, auth1.password());
  EXPECT_EQ(1u, conflicting_extensions.size());
  EXPECT_TRUE(ContainsKey(conflicting_extensions, "extid2"));
  EXPECT_EQ(3u, event_log.size());
}

TEST(ExtensionWebRequestHelpersTest, TestHideRequestForURL) {
  const char* sensitive_urls[] = {
      "http://www.google.com/chrome",
      "https://www.google.com/chrome",
      "http://www.google.com/chrome/foobar",
      "https://www.google.com/chrome/foobar",
      "http://chrome.google.com",
      "https://chrome.google.com",
      "http://client2.google.com",
      "https://client2.google.com",
      // No http version of webstore.
      "https://chrome.google.com/webstore",
      "http://clients2.google.com/service/update2/crx",
      "https://clients2.google.com/service/update2/crx",
      "http://www.gstatic.com/chrome/extensions/blacklist",
      "https://www.gstatic.com/chrome/extensions/blacklist",
      "notregisteredscheme://www.foobar.com"
  };
  const char* non_sensitive_urls[] = {
      "http://www.google.com/"
  };
  for (size_t i = 0; i < arraysize(sensitive_urls); ++i) {
    EXPECT_TRUE(helpers::HideRequestForURL(GURL(sensitive_urls[i])))
        << sensitive_urls[i];
  }
  for (size_t i = 0; i < arraysize(non_sensitive_urls); ++i) {
    EXPECT_FALSE(helpers::HideRequestForURL(GURL(non_sensitive_urls[i])))
        << non_sensitive_urls[i];
  }
}
