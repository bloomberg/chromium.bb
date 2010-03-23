// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome_frame/test/automation_client_mock.h"

#include "base/callback.h"
#include "net/base/net_errors.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"

#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

using testing::CreateFunctor;
using testing::_;

template <> struct RunnableMethodTraits<ProxyFactory::LaunchDelegate> {
  void RetainCallee(ProxyFactory::LaunchDelegate* obj) {}
  void ReleaseCallee(ProxyFactory::LaunchDelegate* obj) {}
};

template <> struct RunnableMethodTraits<ChromeFrameAutomationClient> {
  void RetainCallee(ChromeFrameAutomationClient* obj) {}
  void ReleaseCallee(ChromeFrameAutomationClient* obj) {}
};

template <> struct RunnableMethodTraits<chrome_frame_test::TimedMsgLoop> {
  void RetainCallee(chrome_frame_test::TimedMsgLoop* obj) {}
  void ReleaseCallee(chrome_frame_test::TimedMsgLoop* obj) {}
};

void MockProxyFactory::GetServerImpl(ChromeFrameAutomationProxy* pxy,
                                     void* proxy_id,
                                     AutomationLaunchResult result,
                                     LaunchDelegate* d,
                                     const ChromeFrameLaunchParams& params,
                                     void** automation_server_id) {
  *automation_server_id = proxy_id;
  Task* task = NewRunnableMethod(d,
      &ProxyFactory::LaunchDelegate::LaunchComplete, pxy, result);
  loop_->PostDelayedTask(FROM_HERE, task,
                         params.automation_server_launch_timeout/2);
}

void CFACMockTest::SetAutomationServerOk() {
  EXPECT_CALL(factory_, GetAutomationServer(testing::NotNull(),
              testing::Field(&ChromeFrameLaunchParams::profile_name,
                             testing::StrEq(profile_)),
              testing::NotNull()))
    .Times(1)
    .WillOnce(testing::Invoke(CreateFunctor(&factory_,
                                            &MockProxyFactory::GetServerImpl,
                                            get_proxy(), id_,
                                            AUTOMATION_SUCCESS)));

  EXPECT_CALL(factory_, ReleaseAutomationServer(testing::Eq(id_))).Times(1);
}

void CFACMockTest::Set_CFD_LaunchFailed(AutomationLaunchResult result) {
  EXPECT_CALL(cfd_, OnAutomationServerLaunchFailed(testing::Eq(result),
                                                   testing::_))
      .Times(1)
      .WillOnce(QUIT_LOOP(loop_));
}

MATCHER_P(MsgType, msg_type, "IPC::Message::type()") {
  const IPC::Message& m = arg;
  return (m.type() == msg_type);
}

MATCHER_P(EqNavigationInfoUrl, url, "IPC::NavigationInfo matcher") {
  if (url.is_valid() && url != arg.url)
    return false;
  // TODO(stevet): other members
  return true;
}

// Could be implemented as MockAutomationProxy member (we have WithArgs<>!)
ACTION_P3(HandleCreateTab, tab_handle, external_tab_container, tab_wnd) {
  // arg0 - message
  // arg1 - callback
  // arg2 - key
  CallbackRunner<Tuple3<HWND, HWND, int> >* c =
      reinterpret_cast<CallbackRunner<Tuple3<HWND, HWND, int> >*>(arg1);
  c->Run(external_tab_container, tab_wnd, tab_handle);
  delete c;
  delete arg0;
}

// We mock ChromeFrameDelegate only. The rest is with real AutomationProxy
TEST(CFACWithChrome, CreateTooFast) {
  MockCFDelegate cfd;
  chrome_frame_test::TimedMsgLoop loop;
  int timeout = 0;  // Chrome cannot send Hello message so fast.
  const std::wstring profile = L"Adam.N.Epilinter";

  scoped_refptr<ChromeFrameAutomationClient> client;
  client = new ChromeFrameAutomationClient();

  EXPECT_CALL(cfd, OnAutomationServerLaunchFailed(AUTOMATION_TIMEOUT, _))
      .Times(1)
      .WillOnce(QUIT_LOOP(loop));

  ChromeFrameLaunchParams cfp = {
    timeout,
    GURL(),
    GURL(),
    profile,
    L"",
    false,
    false,
    false
  };
  EXPECT_TRUE(client->Initialize(&cfd, cfp));
  loop.RunFor(10);
  client->Uninitialize();
}

// This test may fail if Chrome take more that 10 seconds (timeout var) to
// launch. In this case GMock shall print something like "unexpected call to
// OnAutomationServerLaunchFailed". I'm yet to find out how to specify
// that this is an unexpected call, and still to execute and action.
TEST(CFACWithChrome, CreateNotSoFast) {
  MockCFDelegate cfd;
  chrome_frame_test::TimedMsgLoop loop;
  const std::wstring profile = L"Adam.N.Epilinter";
  int timeout = 10000;

  scoped_refptr<ChromeFrameAutomationClient> client;
  client = new ChromeFrameAutomationClient;

  EXPECT_CALL(cfd, OnAutomationServerReady())
      .Times(1)
      .WillOnce(QUIT_LOOP(loop));

  EXPECT_CALL(cfd, OnAutomationServerLaunchFailed(_, _))
      .Times(0);

  ChromeFrameLaunchParams cfp = {
    timeout,
    GURL(),
    GURL(),
    profile,
    L"",
    false,
    false,
    false
  };
  EXPECT_TRUE(client->Initialize(&cfd, cfp));

  loop.RunFor(11);
  client->Uninitialize();
  client = NULL;
}

TEST(CFACWithChrome, NavigateOk) {
  MockCFDelegate cfd;
  chrome_frame_test::TimedMsgLoop loop;
  const std::wstring profile = L"Adam.N.Epilinter";
  const std::string url = "about:version";
  int timeout = 10000;

  scoped_refptr<ChromeFrameAutomationClient> client;
  client = new ChromeFrameAutomationClient;

  EXPECT_CALL(cfd, OnAutomationServerReady())
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
          client.get(), &ChromeFrameAutomationClient::InitiateNavigation,
          url, std::string(), false))));

  EXPECT_CALL(cfd, GetBounds(_)).Times(testing::AnyNumber());

  EXPECT_CALL(cfd, OnNavigationStateChanged(_, _))
      .Times(testing::AnyNumber());

  {
    testing::InSequence s;

    EXPECT_CALL(cfd, OnDidNavigate(_, EqNavigationInfoUrl(GURL())))
        .Times(1);

    EXPECT_CALL(cfd, OnUpdateTargetUrl(_, _)).Times(1);

    EXPECT_CALL(cfd, OnLoad(_, _))
        .Times(1)
        .WillOnce(QUIT_LOOP(loop));
  }

  ChromeFrameLaunchParams cfp = {
    timeout,
    GURL(),
    GURL(),
    profile,
    L"",
    false,
    false,
    false
  };
  EXPECT_TRUE(client->Initialize(&cfd, cfp));
  loop.RunFor(10);
  client->Uninitialize();
  client = NULL;
}

TEST(CFACWithChrome, NavigateFailed) {
  MockCFDelegate cfd;
  chrome_frame_test::TimedMsgLoop loop;
  const std::wstring profile = L"Adam.N.Epilinter";
  const std::string url = "http://127.0.0.3:65412/";
  const URLRequestStatus connection_failed(URLRequestStatus::FAILED,
                                           net::ERR_INVALID_URL);

  scoped_refptr<ChromeFrameAutomationClient> client;
  client = new ChromeFrameAutomationClient;
  cfd.SetRequestDelegate(client);

  EXPECT_CALL(cfd, OnAutomationServerReady())
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
          client.get(), &ChromeFrameAutomationClient::InitiateNavigation,
          url, std::string(), false))));

  EXPECT_CALL(cfd, GetBounds(_)).Times(testing::AnyNumber());
  EXPECT_CALL(cfd, OnNavigationStateChanged(_, _)).Times(testing::AnyNumber());

  EXPECT_CALL(cfd, OnRequestStart(_, _, _))
      // Often there's another request for the error page
      .Times(testing::Between(1, 2))
      .WillRepeatedly(testing::WithArgs<1>(testing::Invoke(CreateFunctor(&cfd,
          &MockCFDelegate::Reply, connection_failed))));

  EXPECT_CALL(cfd, OnUpdateTargetUrl(_, _)).Times(testing::AnyNumber());
  EXPECT_CALL(cfd, OnLoad(_, _)).Times(testing::AtMost(1));

  EXPECT_CALL(cfd, OnNavigationFailed(_, _, GURL(url)))
      .Times(1)
      .WillOnce(QUIT_LOOP_SOON(loop, 2));

  ChromeFrameLaunchParams cfp = {
    10000,
    GURL(),
    GURL(),
    profile,
    L"",
    false,
    false,
    false
  };
  EXPECT_TRUE(client->Initialize(&cfd, cfp));

  loop.RunFor(10);
  client->Uninitialize();
  client = NULL;
}

TEST_F(CFACMockTest, MockedCreateTabOk) {
  int timeout = 500;
  CreateTab();
  SetAutomationServerOk();

  EXPECT_CALL(proxy_, server_version()).Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(""));

  // We need some valid HWNDs, when responding to CreateExternalTab
  HWND h1 = ::GetDesktopWindow();
  HWND h2 = ::GetDesktopWindow();
  EXPECT_CALL(proxy_, SendAsAsync(testing::Property(&IPC::SyncMessage::type,
                                 AutomationMsg_CreateExternalTab__ID),
                                 testing::NotNull(), _))
      .Times(1)
      .WillOnce(HandleCreateTab(tab_handle_, h1, h2));

  EXPECT_CALL(proxy_, CreateTabProxy(testing::Eq(tab_handle_)))
      .WillOnce(testing::Return(tab_));

  EXPECT_CALL(cfd_, OnAutomationServerReady())
      .WillOnce(QUIT_LOOP(loop_));

  EXPECT_CALL(proxy_, CancelAsync(_)).Times(testing::AnyNumber());

  // Here we go!
  ChromeFrameLaunchParams cfp = {
    timeout,
    GURL(),
    GURL(),
    profile_,
    L"",
    false,
    false,
    false
  };
  EXPECT_TRUE(client_->Initialize(&cfd_, cfp));
  loop_.RunFor(10);
  client_->Uninitialize();
}

TEST_F(CFACMockTest, MockedCreateTabFailed) {
  HWND null_wnd = NULL;
  SetAutomationServerOk();

  EXPECT_CALL(proxy_, server_version()).Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(""));

  EXPECT_CALL(proxy_, SendAsAsync(testing::Property(&IPC::SyncMessage::type,
                                  AutomationMsg_CreateExternalTab__ID),
                                  testing::NotNull(), _))
      .Times(1)
      .WillOnce(HandleCreateTab(tab_handle_, null_wnd, null_wnd));

  EXPECT_CALL(proxy_, CreateTabProxy(_)).Times(0);

  EXPECT_CALL(proxy_, CancelAsync(_)).Times(testing::AnyNumber());

  Set_CFD_LaunchFailed(AUTOMATION_CREATE_TAB_FAILED);

  // Here we go!
  ChromeFrameLaunchParams cfp = {
    timeout_,
    GURL(),
    GURL(),
    profile_,
    L"",
    false,
    false,
    false
  };
  EXPECT_TRUE(client_->Initialize(&cfd_, cfp));
  loop_.RunFor(4);
  client_->Uninitialize();
}

