// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/xmpp_connection.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/weak_ptr.h"
#include "jingle/notifier/base/weak_xmpp_client.h"
#include "talk/xmpp/prexmppauth.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace buzz {
class CaptchaChallenge;
class Jid;
}  // namespace buzz

namespace talk_base {
class CryptString;
class SocketAddress;
class Task;
}  // namespace talk_base

namespace notifier {

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

class MockPreXmppAuth : public buzz::PreXmppAuth {
 public:
  virtual ~MockPreXmppAuth() {}

  MOCK_METHOD2(ChooseBestSaslMechanism,
               std::string(const std::vector<std::string>&, bool));
  MOCK_METHOD1(CreateSaslMechanism,
               buzz::SaslMechanism*(const std::string&));
  MOCK_METHOD4(StartPreXmppAuth,
               void(const buzz::Jid&,
                    const talk_base::SocketAddress&,
                    const talk_base::CryptString&,
                    const std::string&));
  MOCK_CONST_METHOD0(IsAuthDone, bool());
  MOCK_CONST_METHOD0(IsAuthorized, bool());
  MOCK_CONST_METHOD0(HadError, bool());
  MOCK_CONST_METHOD0(GetError, int());
  MOCK_CONST_METHOD0(GetCaptchaChallenge, buzz::CaptchaChallenge());
  MOCK_CONST_METHOD0(GetAuthCookie, std::string());
};

class MockXmppConnectionDelegate : public XmppConnection::Delegate {
 public:
  virtual ~MockXmppConnectionDelegate() {}

  MOCK_METHOD1(OnConnect, void(base::WeakPtr<talk_base::Task>));
  MOCK_METHOD3(OnError,
               void(buzz::XmppEngine::Error, int, const buzz::XmlElement*));
};

class XmppConnectionTest : public testing::Test {
 protected:
  XmppConnectionTest() : mock_pre_xmpp_auth_(new MockPreXmppAuth()) {}

  virtual ~XmppConnectionTest() {}

  virtual void TearDown() {
    // Clear out any messages posted by XmppConnection's destructor.
    message_loop_.RunAllPending();
  }

  // Needed by XmppConnection.
  MessageLoop message_loop_;
  MockXmppConnectionDelegate mock_xmpp_connection_delegate_;
  scoped_ptr<MockPreXmppAuth> mock_pre_xmpp_auth_;
};

TEST_F(XmppConnectionTest, CreateDestroy) {
  XmppConnection xmpp_connection(buzz::XmppClientSettings(),
                                 &mock_xmpp_connection_delegate_, NULL);
}

TEST_F(XmppConnectionTest, ImmediateFailure) {
  // ChromeAsyncSocket::Connect() will always return false since we're
  // not setting a valid host, but this gets bubbled up as ERROR_NONE
  // due to XmppClient's inconsistent error-handling.
  EXPECT_CALL(mock_xmpp_connection_delegate_,
              OnError(buzz::XmppEngine::ERROR_NONE, 0, NULL));

  XmppConnection xmpp_connection(buzz::XmppClientSettings(),
                                 &mock_xmpp_connection_delegate_, NULL);

  // We need to do this *before* |xmpp_connection| gets destroyed or
  // our delegate won't be called.
  message_loop_.RunAllPending();
}

TEST_F(XmppConnectionTest, PreAuthFailure) {
  EXPECT_CALL(*mock_pre_xmpp_auth_, StartPreXmppAuth(_, _, _, _));
  EXPECT_CALL(*mock_pre_xmpp_auth_, IsAuthDone()).WillOnce(Return(true));
  EXPECT_CALL(*mock_pre_xmpp_auth_, IsAuthorized()).WillOnce(Return(false));
  EXPECT_CALL(*mock_pre_xmpp_auth_, HadError()).WillOnce(Return(true));
  EXPECT_CALL(*mock_pre_xmpp_auth_, GetError()).WillOnce(Return(5));

  EXPECT_CALL(mock_xmpp_connection_delegate_,
              OnError(buzz::XmppEngine::ERROR_AUTH, 5, NULL));

  XmppConnection xmpp_connection(
      buzz::XmppClientSettings(), &mock_xmpp_connection_delegate_,
      mock_pre_xmpp_auth_.release());

  // We need to do this *before* |xmpp_connection| gets destroyed or
  // our delegate won't be called.
  message_loop_.RunAllPending();
}

TEST_F(XmppConnectionTest, FailureAfterPreAuth) {
  EXPECT_CALL(*mock_pre_xmpp_auth_, StartPreXmppAuth(_, _, _, _));
  EXPECT_CALL(*mock_pre_xmpp_auth_, IsAuthDone()).WillOnce(Return(true));
  EXPECT_CALL(*mock_pre_xmpp_auth_, IsAuthorized()).WillOnce(Return(true));
  EXPECT_CALL(*mock_pre_xmpp_auth_, GetAuthCookie()).WillOnce(Return(""));

  EXPECT_CALL(mock_xmpp_connection_delegate_,
              OnError(buzz::XmppEngine::ERROR_NONE, 0, NULL));

  XmppConnection xmpp_connection(
      buzz::XmppClientSettings(), &mock_xmpp_connection_delegate_,
      mock_pre_xmpp_auth_.release());

  // We need to do this *before* |xmpp_connection| gets destroyed or
  // our delegate won't be called.
  message_loop_.RunAllPending();
}

TEST_F(XmppConnectionTest, RaisedError) {
  EXPECT_CALL(mock_xmpp_connection_delegate_,
              OnError(buzz::XmppEngine::ERROR_NONE, 0, NULL));

  XmppConnection xmpp_connection(buzz::XmppClientSettings(),
                                 &mock_xmpp_connection_delegate_, NULL);

  xmpp_connection.weak_xmpp_client_->
      SignalStateChange(buzz::XmppEngine::STATE_CLOSED);
}

TEST_F(XmppConnectionTest, Connect) {
  base::WeakPtr<talk_base::Task> weak_ptr;
  EXPECT_CALL(mock_xmpp_connection_delegate_, OnConnect(_)).
      WillOnce(SaveArg<0>(&weak_ptr));

  {
    XmppConnection xmpp_connection(buzz::XmppClientSettings(),
                                   &mock_xmpp_connection_delegate_, NULL);

    xmpp_connection.weak_xmpp_client_->
        SignalStateChange(buzz::XmppEngine::STATE_OPEN);
    EXPECT_EQ(xmpp_connection.weak_xmpp_client_.get(), weak_ptr.get());
  }

  EXPECT_EQ(NULL, weak_ptr.get());
}

TEST_F(XmppConnectionTest, MultipleConnect) {
  EXPECT_DEBUG_DEATH({
    base::WeakPtr<talk_base::Task> weak_ptr;
    EXPECT_CALL(mock_xmpp_connection_delegate_, OnConnect(_)).
        WillOnce(SaveArg<0>(&weak_ptr));

    XmppConnection xmpp_connection(buzz::XmppClientSettings(),
                                   &mock_xmpp_connection_delegate_, NULL);

    xmpp_connection.weak_xmpp_client_->
        SignalStateChange(buzz::XmppEngine::STATE_OPEN);
    for (int i = 0; i < 3; ++i) {
      xmpp_connection.weak_xmpp_client_->
          SignalStateChange(buzz::XmppEngine::STATE_OPEN);
    }

    EXPECT_EQ(xmpp_connection.weak_xmpp_client_.get(), weak_ptr.get());
  }, "more than once");
}

TEST_F(XmppConnectionTest, ConnectThenError) {
  base::WeakPtr<talk_base::Task> weak_ptr;
  EXPECT_CALL(mock_xmpp_connection_delegate_, OnConnect(_)).
      WillOnce(SaveArg<0>(&weak_ptr));
  EXPECT_CALL(mock_xmpp_connection_delegate_,
              OnError(buzz::XmppEngine::ERROR_NONE, 0, NULL));

  XmppConnection xmpp_connection(buzz::XmppClientSettings(),
                                 &mock_xmpp_connection_delegate_, NULL);

  xmpp_connection.weak_xmpp_client_->
      SignalStateChange(buzz::XmppEngine::STATE_OPEN);
  EXPECT_EQ(xmpp_connection.weak_xmpp_client_.get(), weak_ptr.get());

  xmpp_connection.weak_xmpp_client_->
      SignalStateChange(buzz::XmppEngine::STATE_CLOSED);
  EXPECT_EQ(NULL, weak_ptr.get());
}

}  // namespace notifier
