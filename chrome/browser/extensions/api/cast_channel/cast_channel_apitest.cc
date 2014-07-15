// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/api/cast_channel/cast_channel_api.h"
#include "chrome/browser/extensions/api/cast_channel/cast_socket.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/api/cast_channel.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/switches.h"
#include "net/base/capturing_net_log.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"

namespace cast_channel =  extensions::api::cast_channel;
using cast_channel::CastSocket;
using cast_channel::ChannelError;
using cast_channel::MessageInfo;
using cast_channel::ReadyState;

using ::testing::_;
using ::testing::A;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::Return;

namespace {

const char kTestExtensionId[] = "ddchlicdkolnonkihahngkmmmjnjlkkf";

static void FillMessageInfo(MessageInfo* message_info,
                            const std::string& message) {
  message_info->namespace_ = "foo";
  message_info->source_id = "src";
  message_info->destination_id = "dest";
  message_info->data.reset(new base::StringValue(message));
}

ACTION_TEMPLATE(InvokeCompletionCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(result)) {
  ::std::tr1::get<k>(args).Run(result);
}

ACTION_P2(InvokeDelegateOnError, api_test, api) {
  api_test->CallOnError(api);
}

class MockCastSocket : public CastSocket {
 public:
  explicit MockCastSocket(CastSocket::Delegate* delegate,
                          net::IPEndPoint ip_endpoint,
                          net::NetLog* net_log)
    : CastSocket(kTestExtensionId, ip_endpoint,
                 cast_channel::CHANNEL_AUTH_TYPE_SSL, delegate, net_log) {}
  virtual ~MockCastSocket() {}

  virtual bool CalledOnValidThread() const OVERRIDE {
    // Always return true in testing.
    return true;
  }

  MOCK_METHOD1(Connect, void(const net::CompletionCallback& callback));
  MOCK_METHOD2(SendMessage, void(const MessageInfo& message,
                                 const net::CompletionCallback& callback));
  MOCK_METHOD1(Close, void(const net::CompletionCallback& callback));
  MOCK_CONST_METHOD0(ready_state, cast_channel::ReadyState());
  MOCK_CONST_METHOD0(error_state, cast_channel::ChannelError());
};

}  // namespace

class CastChannelAPITest : public ExtensionApiTest {
 public:
  CastChannelAPITest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        kTestExtensionId);
  }

  void SetUpMockCastSocket() {
    extensions::CastChannelAPI* api = GetApi();
    net::IPAddressNumber ip_number;
    net::ParseIPLiteralToNumber("192.168.1.1", &ip_number);
    net::IPEndPoint ip_endpoint(ip_number, 8009);
    mock_cast_socket_ = new MockCastSocket(api, ip_endpoint,
                                           &capturing_net_log_);
    // Transfers ownership of the socket.
    api->SetSocketForTest(
        make_scoped_ptr<CastSocket>(mock_cast_socket_).Pass());

    // Set expectations on error_state().
    EXPECT_CALL(*mock_cast_socket_, error_state())
      .WillRepeatedly(Return(cast_channel::CHANNEL_ERROR_NONE));
  }

  extensions::CastChannelAPI* GetApi() {
    return extensions::CastChannelAPI::Get(profile());
  }

  void CallOnError(extensions::CastChannelAPI* api) {
    api->OnError(mock_cast_socket_,
                 cast_channel::CHANNEL_ERROR_CONNECT_ERROR);
  }

 protected:
  void CallOnMessage(const std::string& message) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&CastChannelAPITest::DoCallOnMessage, this,
                   GetApi(), mock_cast_socket_, message));
  }

  void DoCallOnMessage(extensions::CastChannelAPI* api,
                       MockCastSocket* cast_socket,
                       const std::string& message) {
    MessageInfo message_info;
    FillMessageInfo(&message_info, message);
    api->OnMessage(cast_socket, message_info);
  }

  MockCastSocket* mock_cast_socket_;
  net::CapturingNetLog capturing_net_log_;
};

// TODO(munjal): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestOpenSendClose DISABLED_TestOpenSendClose
#else
#define MAYBE_TestOpenSendClose TestOpenSendClose
#endif
// Test loading extension, opening a channel with ConnectInfo, adding a
// listener, writing, reading, and closing.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestOpenSendClose) {
  SetUpMockCastSocket();

  {
    InSequence dummy;
    EXPECT_CALL(*mock_cast_socket_, Connect(_))
        .WillOnce(InvokeCompletionCallback<0>(net::OK));
    EXPECT_CALL(*mock_cast_socket_, ready_state())
        .WillOnce(Return(cast_channel::READY_STATE_OPEN));
    EXPECT_CALL(*mock_cast_socket_, SendMessage(A<const MessageInfo&>(), _))
        .WillOnce(InvokeCompletionCallback<1>(net::OK));
    EXPECT_CALL(*mock_cast_socket_, ready_state())
        .WillOnce(Return(cast_channel::READY_STATE_OPEN));
    EXPECT_CALL(*mock_cast_socket_, Close(_))
        .WillOnce(InvokeCompletionCallback<0>(net::OK));
    EXPECT_CALL(*mock_cast_socket_, ready_state())
        .WillOnce(Return(cast_channel::READY_STATE_CLOSED));
  }

  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_send_close.html"));
}

// TODO(munjal): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestOpenSendCloseWithUrl DISABLED_TestOpenSendCloseWithUrl
#else
#define MAYBE_TestOpenSendCloseWithUrl TestOpenSendCloseWithUrl
#endif
// Test loading extension, opening a channel with a URL, adding a listener,
// writing, reading, and closing.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestOpenSendCloseWithUrl) {
  SetUpMockCastSocket();

  {
    InSequence dummy;
    EXPECT_CALL(*mock_cast_socket_, Connect(_))
        .WillOnce(InvokeCompletionCallback<0>(net::OK));
    EXPECT_CALL(*mock_cast_socket_, ready_state())
        .WillOnce(Return(cast_channel::READY_STATE_OPEN));
    EXPECT_CALL(*mock_cast_socket_, SendMessage(A<const MessageInfo&>(), _))
        .WillOnce(InvokeCompletionCallback<1>(net::OK));
    EXPECT_CALL(*mock_cast_socket_, ready_state())
        .WillOnce(Return(cast_channel::READY_STATE_OPEN));
    EXPECT_CALL(*mock_cast_socket_, Close(_))
        .WillOnce(InvokeCompletionCallback<0>(net::OK));
    EXPECT_CALL(*mock_cast_socket_, ready_state())
        .WillOnce(Return(cast_channel::READY_STATE_CLOSED));
  }

  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_send_close_url.html"));
}

// TODO(munjal): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestOpenReceiveClose DISABLED_TestOpenReceiveClose
#else
#define MAYBE_TestOpenReceiveClose TestOpenReceiveClose
#endif
// Test loading extension, opening a channel, adding a listener,
// writing, reading, and closing.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestOpenReceiveClose) {
  SetUpMockCastSocket();

  {
    InSequence dummy;
    EXPECT_CALL(*mock_cast_socket_, Connect(_))
        .WillOnce(InvokeCompletionCallback<0>(net::OK));
    EXPECT_CALL(*mock_cast_socket_, ready_state())
        .Times(3)
        .WillRepeatedly(Return(cast_channel::READY_STATE_OPEN));
    EXPECT_CALL(*mock_cast_socket_, Close(_))
        .WillOnce(InvokeCompletionCallback<0>(net::OK));
    EXPECT_CALL(*mock_cast_socket_, ready_state())
        .WillOnce(Return(cast_channel::READY_STATE_CLOSED));
  }

  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_receive_close.html"));

  ResultCatcher catcher;
  CallOnMessage("some-message");
  CallOnMessage("some-message");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// TODO(munjal): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
// Flaky on mac: crbug.com/393969
#if (defined(OS_WIN) && !defined(NDEBUG)) || defined(OS_MACOSX)
#define MAYBE_TestOpenError DISABLED_TestOpenError
#else
#define MAYBE_TestOpenError TestOpenError
#endif
// Test the case when socket open results in an error.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestOpenError) {
  SetUpMockCastSocket();

  EXPECT_CALL(*mock_cast_socket_, Connect(_))
      .WillOnce(DoAll(
          InvokeDelegateOnError(this, GetApi()),
          InvokeCompletionCallback<0>(net::ERR_FAILED)));
  EXPECT_CALL(*mock_cast_socket_, ready_state())
      .WillRepeatedly(Return(cast_channel::READY_STATE_CLOSED));
  EXPECT_CALL(*mock_cast_socket_, Close(_));

  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_error.html"));

  ResultCatcher catcher;
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

