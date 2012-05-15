// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/communicator/single_login_attempt.h"

#include <cstddef>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "jingle/notifier/base/const_communicator.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/communicator/login_settings.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppengine.h"

namespace buzz {
class XmppTaskParentInterface;
}  // namespace buzz

namespace notifier {

namespace {

enum DelegateState { IDLE, CONNECTED, NEED_RECONNECT, REDIRECTED };

class FakeDelegate : public SingleLoginAttempt::Delegate {
 public:
  FakeDelegate() : state_(IDLE) {}

  void OnConnect(base::WeakPtr<buzz::XmppTaskParentInterface> base_task) {
    state_ = CONNECTED;
    base_task_ = base_task;
  }

  virtual void OnNeedReconnect() {
    state_ = NEED_RECONNECT;
  }

  virtual void OnRedirect(const ServerInformation& redirect_server) OVERRIDE {
    state_ = REDIRECTED;
    redirect_server_ = redirect_server;
  }

  DelegateState state() const { return state_; }

  base::WeakPtr<buzz::XmppTaskParentInterface> base_task() const {
    return base_task_;
  }

  const ServerInformation& redirect_server() const {
    return redirect_server_;
  }

 private:
  DelegateState state_;
  base::WeakPtr<buzz::XmppTaskParentInterface> base_task_;
  ServerInformation redirect_server_;
};

class SingleLoginAttemptTest : public ::testing::Test {
 protected:
  SingleLoginAttemptTest()
      : login_settings_(
          buzz::XmppClientSettings(),
          new TestURLRequestContextGetter(base::MessageLoopProxy::current()),
          ServerList(
              1,
              ServerInformation(
                  net::HostPortPair("example.com", 100), SUPPORTS_SSLTCP)),
          false /* try_ssltcp_first */,
          "auth_mechanism"),
        attempt_(login_settings_, &fake_delegate_) {}

  virtual void TearDown() OVERRIDE {
    message_loop_.RunAllPending();
  }

  void FireRedirect(buzz::XmlElement* redirect_error) {
    attempt_.OnError(buzz::XmppEngine::ERROR_STREAM, 0, redirect_error);
  }

 private:
  MessageLoop message_loop_;
  const LoginSettings login_settings_;

 protected:
  SingleLoginAttempt attempt_;
  FakeDelegate fake_delegate_;
  FakeBaseTask fake_base_task_;
};

// Fire OnConnect and make sure the base task gets passed to the
// delegate properly.
TEST_F(SingleLoginAttemptTest, Basic) {
  attempt_.OnConnect(fake_base_task_.AsWeakPtr());
  EXPECT_EQ(CONNECTED, fake_delegate_.state());
  EXPECT_EQ(fake_base_task_.AsWeakPtr().get(),
            fake_delegate_.base_task().get());
}

// Fire OnErrors and make sure the delegate gets the OnNeedReconnect()
// event.
TEST_F(SingleLoginAttemptTest, Error) {
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(IDLE, fake_delegate_.state());
    attempt_.OnError(buzz::XmppEngine::ERROR_NONE, 0, NULL);
  }
  EXPECT_EQ(NEED_RECONNECT, fake_delegate_.state());
}

// Fire OnErrors but replace the last one with OnConnect, and make
// sure the delegate still gets the OnConnect message.
TEST_F(SingleLoginAttemptTest, ErrorThenSuccess) {
  attempt_.OnError(buzz::XmppEngine::ERROR_NONE, 0, NULL);
  attempt_.OnConnect(fake_base_task_.AsWeakPtr());
  EXPECT_EQ(CONNECTED, fake_delegate_.state());
  EXPECT_EQ(fake_base_task_.AsWeakPtr().get(),
            fake_delegate_.base_task().get());
}

buzz::XmlElement* MakeRedirectError(const std::string& redirect_server) {
  buzz::XmlElement* stream_error =
      new buzz::XmlElement(buzz::QN_STREAM_ERROR, true);
  stream_error->AddElement(
      new buzz::XmlElement(buzz::QN_XSTREAM_SEE_OTHER_HOST, true));
  buzz::XmlElement* text =
      new buzz::XmlElement(buzz::QN_XSTREAM_TEXT, true);
  stream_error->AddElement(text);
  text->SetBodyText(redirect_server);
  return stream_error;
}

// Fire a redirect and make sure the delegate gets the proper redirect
// server info.
TEST_F(SingleLoginAttemptTest, Redirect) {
  const ServerInformation redirect_server(
      net::HostPortPair("example.com", 1000),
      SUPPORTS_SSLTCP);

  scoped_ptr<buzz::XmlElement> redirect_error(
      MakeRedirectError(redirect_server.server.ToString()));
  FireRedirect(redirect_error.get());

  EXPECT_EQ(REDIRECTED, fake_delegate_.state());
  EXPECT_TRUE(fake_delegate_.redirect_server().Equals(redirect_server));
}

// Fire a redirect with the host only and make sure the delegate gets
// the proper redirect server info with the default XMPP port.
TEST_F(SingleLoginAttemptTest, RedirectHostOnly) {
  const ServerInformation redirect_server(
      net::HostPortPair("example.com", kDefaultXmppPort),
      SUPPORTS_SSLTCP);

  scoped_ptr<buzz::XmlElement> redirect_error(
      MakeRedirectError(redirect_server.server.host()));
  FireRedirect(redirect_error.get());

  EXPECT_EQ(REDIRECTED, fake_delegate_.state());
  EXPECT_TRUE(fake_delegate_.redirect_server().Equals(redirect_server));
}

// Fire a redirect with a zero port and make sure the delegate gets
// the proper redirect server info with the default XMPP port.
TEST_F(SingleLoginAttemptTest, RedirectZeroPort) {
  const ServerInformation redirect_server(
      net::HostPortPair("example.com", kDefaultXmppPort),
      SUPPORTS_SSLTCP);

  scoped_ptr<buzz::XmlElement> redirect_error(
      MakeRedirectError(redirect_server.server.host() + ":0"));
  FireRedirect(redirect_error.get());

  EXPECT_EQ(REDIRECTED, fake_delegate_.state());
  EXPECT_TRUE(fake_delegate_.redirect_server().Equals(redirect_server));
}

// Fire a redirect with an invalid port and make sure the delegate
// gets the proper redirect server info with the default XMPP port.
TEST_F(SingleLoginAttemptTest, RedirectInvalidPort) {
  const ServerInformation redirect_server(
      net::HostPortPair("example.com", kDefaultXmppPort),
      SUPPORTS_SSLTCP);

  scoped_ptr<buzz::XmlElement> redirect_error(
      MakeRedirectError(redirect_server.server.host() + ":invalidport"));
  FireRedirect(redirect_error.get());

  EXPECT_EQ(REDIRECTED, fake_delegate_.state());
  EXPECT_TRUE(fake_delegate_.redirect_server().Equals(redirect_server));
}

// Fire an empty redirect and make sure the delegate does not get a
// redirect.
TEST_F(SingleLoginAttemptTest, RedirectEmpty) {
  scoped_ptr<buzz::XmlElement> redirect_error(MakeRedirectError(""));
  FireRedirect(redirect_error.get());
  EXPECT_EQ(IDLE, fake_delegate_.state());
}

// Fire a redirect with a missing text element and make sure the
// delegate does not get a redirect.
TEST_F(SingleLoginAttemptTest, RedirectMissingText) {
  scoped_ptr<buzz::XmlElement> redirect_error(MakeRedirectError(""));
  redirect_error->RemoveChildAfter(redirect_error->FirstChild());
  FireRedirect(redirect_error.get());
  EXPECT_EQ(IDLE, fake_delegate_.state());
}

// Fire a redirect with a missing see-other-host element and make sure
// the delegate does not get a redirect.
TEST_F(SingleLoginAttemptTest, RedirectMissingSeeOtherHost) {
  scoped_ptr<buzz::XmlElement> redirect_error(MakeRedirectError(""));
  redirect_error->RemoveChildAfter(NULL);
  FireRedirect(redirect_error.get());
  EXPECT_EQ(IDLE, fake_delegate_.state());
}

}  // namespace

}  // namespace notifier
