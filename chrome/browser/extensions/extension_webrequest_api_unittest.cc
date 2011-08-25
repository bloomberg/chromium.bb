// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "base/file_util.h"
#include "base/path_service.h"

#include "chrome/browser/extensions/extension_event_router_forwarder.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "chrome/browser/extensions/extension_webrequest_api_constants.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_browser_process_test.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/common/json_value_serializer.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_webrequest_api_constants;

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
}  // namespace

// A mock event router that responds to events with a pre-arranged queue of
// Tasks.
class TestIPCSender : public IPC::Message::Sender {
 public:
  typedef std::list<linked_ptr<IPC::Message> > SentMessages;

  // Adds a Task to the queue. We will fire these in order as events are
  // dispatched.
  void PushTask(Task* task) {
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

  std::queue<Task*> task_queue_;
  SentMessages sent_messages_;
};

class ExtensionWebRequestTest : public TestingBrowserProcessTest {
 protected:
  virtual void SetUp() {
    event_router_ = new ExtensionEventRouterForwarder();
    enable_referrers_.Init(
        prefs::kEnableReferrers, profile_.GetTestingPrefService(), NULL);
    network_delegate_.reset(new ChromeNetworkDelegate(
        event_router_.get(), NULL, &profile_, &enable_referrers_));
    context_ = new TestURLRequestContext();
    context_->set_network_delegate(network_delegate_.get());
  }

  MessageLoopForIO io_loop_;
  TestingProfile profile_;
  TestDelegate delegate_;
  BooleanPrefMember enable_referrers_;
  TestIPCSender ipc_sender_;
  scoped_refptr<ExtensionEventRouterForwarder> event_router_;
  scoped_refptr<ExtensionInfoMap> extension_info_map_;
  scoped_ptr<ChromeNetworkDelegate> network_delegate_;
  scoped_refptr<TestURLRequestContext> context_;
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
  request.set_context(context_);
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
        NewRunnableFunction(&EventHandledOnIOThread,
            &profile_, extension1_id, kEventName, kEventName + "/1",
            request.identifier(), response));

    // Extension2 response. Arrives second, and chosen because of install_time.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    response->new_url = redirect_url;
    ipc_sender_.PushTask(
        NewRunnableFunction(&EventHandledOnIOThread,
            &profile_, extension2_id, kEventName, kEventName + "/2",
            request.identifier(), response));

    // Extension2 response to the redirected URL. Arrives first, and chosen.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    ipc_sender_.PushTask(
        NewRunnableFunction(&EventHandledOnIOThread,
            &profile_, extension2_id, kEventName, kEventName + "/2",
            request.identifier(), response));

    // Extension1 response to the redirected URL. Arrives second, and ignored.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension1_id, base::Time::FromDoubleT(1));
    ipc_sender_.PushTask(
        NewRunnableFunction(&EventHandledOnIOThread,
            &profile_, extension1_id, kEventName, kEventName + "/1",
            request.identifier(), response));

    request.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(!request.is_pending());
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request.status().status());
    EXPECT_EQ(0, request.status().os_error());
    EXPECT_EQ(redirect_url, request.url());
    EXPECT_EQ(2U, request.url_chain().size());
    EXPECT_EQ(0U, ipc_sender_.GetNumTasks());
  }

  // Now test the same thing but the extensions answer in reverse order.
  net::URLRequest request2(GURL("about:blank"), &delegate_);
  request2.set_context(context_);
  {
    ExtensionWebRequestEventRouter::EventResponse* response = NULL;

    // Extension2 response. Arrives first, and chosen because of install_time.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    response->new_url = redirect_url;
    ipc_sender_.PushTask(
        NewRunnableFunction(&EventHandledOnIOThread,
            &profile_, extension2_id, kEventName, kEventName + "/2",
            request2.identifier(), response));

    // Extension1 response. Arrives second, but ignored due to install_time.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension1_id, base::Time::FromDoubleT(1));
    response->new_url = not_chosen_redirect_url;
    ipc_sender_.PushTask(
        NewRunnableFunction(&EventHandledOnIOThread,
            &profile_, extension1_id, kEventName, kEventName + "/1",
            request2.identifier(), response));

    // Extension2 response to the redirected URL. Arrives first, and chosen.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension2_id, base::Time::FromDoubleT(2));
    ipc_sender_.PushTask(
        NewRunnableFunction(&EventHandledOnIOThread,
            &profile_, extension2_id, kEventName, kEventName + "/2",
            request2.identifier(), response));

    // Extension1 response to the redirected URL. Arrives second, and ignored.
    response = new ExtensionWebRequestEventRouter::EventResponse(
        extension1_id, base::Time::FromDoubleT(1));
    ipc_sender_.PushTask(
        NewRunnableFunction(&EventHandledOnIOThread,
            &profile_, extension1_id, kEventName, kEventName + "/1",
            request2.identifier(), response));

    request2.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(!request2.is_pending());
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request2.status().status());
    EXPECT_EQ(0, request2.status().os_error());
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
  request.set_context(context_);

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
      NewRunnableFunction(&EventHandledOnIOThread,
          &profile_, extension1_id, kEventName, kEventName + "/1",
          request.identifier(), response));

  // Extension2 response. Arrives second, but has higher precedence
  // due to its later install_time.
  response = new ExtensionWebRequestEventRouter::EventResponse(
      extension2_id, base::Time::FromDoubleT(2));
  response->new_url = redirect_url;
  ipc_sender_.PushTask(
      NewRunnableFunction(&EventHandledOnIOThread,
          &profile_, extension2_id, kEventName, kEventName + "/2",
          request.identifier(), response));

  request.Start();
  MessageLoop::current()->Run();

  EXPECT_TRUE(!request.is_pending());
  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE, request.status().os_error());
  EXPECT_EQ(request_url, request.url());
  EXPECT_EQ(1U, request.url_chain().size());
  EXPECT_EQ(0U, ipc_sender_.GetNumTasks());

  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      &profile_, extension1_id, kEventName + "/1");
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      &profile_, extension2_id, kEventName + "/2");
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
 protected:
  virtual void SetUp() {
    event_router_ = new ExtensionEventRouterForwarder();
    enable_referrers_.Init(
        prefs::kEnableReferrers, profile_.GetTestingPrefService(), NULL);
    network_delegate_.reset(new ChromeNetworkDelegate(
        event_router_.get(), NULL, &profile_, &enable_referrers_));
    context_ = new TestURLRequestContext();
    context_->set_network_delegate(network_delegate_.get());
  }

  ScopedTestingBrowserProcess browser_process_;
  MessageLoopForIO io_loop_;
  TestingProfile profile_;
  TestDelegate delegate_;
  BooleanPrefMember enable_referrers_;
  TestIPCSender ipc_sender_;
  scoped_refptr<ExtensionEventRouterForwarder> event_router_;
  scoped_refptr<ExtensionInfoMap> extension_info_map_;
  scoped_ptr<ChromeNetworkDelegate> network_delegate_;
  scoped_refptr<TestURLRequestContext> context_;
};

class DoNothingTask : public Task {
  virtual ~DoNothingTask() {};
  virtual void Run() {};
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
  request.set_context(context_);

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
          NewRunnableFunction(&EventHandledOnIOThread,
              &profile_, mod.extension_id == 1 ? extension1_id : extension2_id,
              kEventName, kEventName + (mod.extension_id == 1 ? "/1" : "/2"),
              request.identifier(), response));
      response = NULL;
    }
  }

  // Don't do anything for the onSendHeaders message.
  ipc_sender_.PushTask(new DoNothingTask);

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
    ASSERT_TRUE(event_arg_value.get());
    ListValue* list = event_arg_value->AsList();
    ASSERT_TRUE(list);

    DictionaryValue* event_arg_dict = NULL;
    ASSERT_TRUE(list->GetDictionary(0, &event_arg_dict));

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
