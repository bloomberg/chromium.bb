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
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/cast_channel.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/capturing_net_log.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cast_channel =  extensions::api::cast_channel;
using cast_channel::CastSocket;
using cast_channel::ChannelInfo;
using cast_channel::MessageInfo;

using ::testing::A;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace {

const char kTestExtensionId[] = "ddchlicdkolnonkihahngkmmmjnjlkkf";
const char kTestUrl[] = "cast://192.168.1.1:8009";

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

class MockCastSocket : public CastSocket {
 public:
  explicit MockCastSocket(CastSocket::Delegate* delegate,
                          net::NetLog* net_log)
      : CastSocket(kTestExtensionId, GURL(kTestUrl), delegate, net_log) {}
  virtual ~MockCastSocket() {}

  virtual bool CalledOnValidThread() const OVERRIDE {
    // Always return true in testing.
    return true;
  }

  MOCK_CONST_METHOD1(FillChannelInfo, void(ChannelInfo*));
  MOCK_METHOD1(Connect, void(const net::CompletionCallback& callback));
  MOCK_METHOD2(SendMessage, void(const MessageInfo& message,
                                 const net::CompletionCallback& callback));
  MOCK_METHOD1(Close, void(const net::CompletionCallback& callback));
};

}  // namespace

class CastChannelAPITest : public ExtensionApiTest {
 public:
  CastChannelAPITest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kWhitelistedExtensionID, kTestExtensionId);
  }

  void SetUpMockCastSocket() {
    extensions::CastChannelAPI* api = GetApi();
    mock_cast_socket_ = new MockCastSocket(api, &capturing_net_log_);
    // Transfers ownership of the socket.
    api->SetSocketForTest(
        make_scoped_ptr<CastSocket>(mock_cast_socket_).Pass());
  }

  extensions::CastChannelAPI* GetApi() {
    return extensions::CastChannelAPI::Get(profile());
  }

  static void FillChannelInfoForOpenState(ChannelInfo* channel_info) {
    channel_info->channel_id = 1;
    channel_info->url = kTestUrl;
    channel_info->ready_state = cast_channel::READY_STATE_OPEN;
    channel_info->error_state = cast_channel::CHANNEL_ERROR_NONE;
  }

  static void FillChannelInfoForClosedState(ChannelInfo* channel_info) {
    channel_info->channel_id = 1;
    channel_info->url = kTestUrl;
    channel_info->ready_state = cast_channel::READY_STATE_CLOSED;
    channel_info->error_state = cast_channel::CHANNEL_ERROR_NONE;
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
  ChannelInfo channel_info;
  net::CapturingNetLog capturing_net_log_;
};

// TODO(munjal): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestOpenSendClose DISABLED_TestOpenSendClose
#else
#define MAYBE_TestOpenSendClose TestOpenSendClose
#endif
// Test loading extension, opening a channel, adding a listener,
// writing, reading, and closing.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestOpenSendClose) {
  SetUpMockCastSocket();

  EXPECT_CALL(*mock_cast_socket_, Connect(_))
      .WillOnce(InvokeCompletionCallback<0>(net::OK));
  EXPECT_CALL(*mock_cast_socket_, FillChannelInfo(_))
      .WillOnce(Invoke(FillChannelInfoForOpenState))
      .WillOnce(Invoke(FillChannelInfoForOpenState))
      .WillOnce(Invoke(FillChannelInfoForClosedState));
  EXPECT_CALL(*mock_cast_socket_, SendMessage(A<const MessageInfo&>(), _)).
      WillOnce(InvokeCompletionCallback<1>(net::OK));
  EXPECT_CALL(*mock_cast_socket_, Close(_)).
      WillOnce(InvokeCompletionCallback<0>(net::OK));

  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_send_close.html"));
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

  EXPECT_CALL(*mock_cast_socket_, Connect(_))
      .WillOnce(InvokeCompletionCallback<0>(net::OK));
  EXPECT_CALL(*mock_cast_socket_, FillChannelInfo(_))
      .WillOnce(Invoke(FillChannelInfoForOpenState))
      .WillOnce(Invoke(FillChannelInfoForOpenState))
      .WillOnce(Invoke(FillChannelInfoForOpenState))
      .WillOnce(Invoke(FillChannelInfoForClosedState));
  EXPECT_CALL(*mock_cast_socket_, Close(_)).
      WillOnce(InvokeCompletionCallback<0>(net::OK));

  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_receive_close.html"));

  ResultCatcher catcher;
  CallOnMessage("some-message");
  CallOnMessage("some-message");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
