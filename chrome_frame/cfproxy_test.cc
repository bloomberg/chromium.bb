// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/common/automation_messages.h"
#include "chrome_frame/cfproxy_private.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"

using testing::_;
using testing::DoAll;
using testing::NotNull;
using testing::Return;
using testing::StrictMock;
using testing::InvokeWithoutArgs;
using testing::WithoutArgs;
using testing::CreateFunctor;
using testing::StrEq;
using testing::Eq;

// There is not much to test here since CFProxy is pretty dumb.
struct MockFactory : public ChromeProxyFactory {
  MOCK_METHOD0(CreateProxy, ChromeProxy*());
};

struct MockChromeProxyDelegate : public ChromeProxyDelegate {
  MOCK_METHOD1(OnMessageReceived, bool(const IPC::Message& message));
  MOCK_METHOD1(Connected, void(ChromeProxy* proxy));
  MOCK_METHOD2(PeerLost, void(ChromeProxy*, enum DisconnectReason reason));
  MOCK_METHOD0(Disconnected, void());
  MOCK_METHOD0(tab_handle, int());

  MOCK_METHOD5(Completed_CreateTab, void(bool success, HWND chrome_wnd,
      HWND tab_window, int tab_handle, int session_id));
  MOCK_METHOD5(Completed_ConnectToTab, void(bool success, HWND chrome_window,
      HWND tab_window, int tab_handle, int session_id));
  MOCK_METHOD2(Completed_Navigate, void(bool success,
      enum AutomationMsg_NavigationResponseValues res));

  // Network requests from Chrome.
  MOCK_METHOD2(Network_Start, void(int request_id,
      const AutomationURLRequest& request_info));
  MOCK_METHOD2(Network_Read, void(int request_id, int bytes_to_read));
  MOCK_METHOD2(Network_End, void(int request_id,
                                 const net::URLRequestStatus& s));
  MOCK_METHOD1(Network_DownloadInHost, void(int request_id));
  MOCK_METHOD2(GetCookies, void(const GURL& url, int cookie_id));
  MOCK_METHOD2(SetCookie, void(const GURL& url, const std::string& cookie));

  // Navigation progress notifications.
  MOCK_METHOD2(NavigationStateChanged, void(int flags,
      const NavigationInfo& nav_info));
  MOCK_METHOD1(UpdateTargetUrl, void(const std::wstring& url));
  MOCK_METHOD2(NavigationFailed, void(int error_code, const GURL& gurl));
  MOCK_METHOD1(DidNavigate, void(const NavigationInfo& navigation_info));
  MOCK_METHOD1(TabLoaded, void(const GURL& url));

  //
  MOCK_METHOD3(OpenURL, void(const GURL& url_to_open, const GURL& referrer,
      int open_disposition));
  MOCK_METHOD1(GoToHistoryOffset, void(int offset));
  MOCK_METHOD3(MessageToHost, void(const std::string& message,
      const std::string& origin, const std::string& target));

  // Misc. UI.
  MOCK_METHOD1(HandleAccelerator, void(const MSG& accel_message));
  MOCK_METHOD3(HandleContextMenu, void(HANDLE menu_handle, int align_flags,
      const MiniContextMenuParams& params));
  MOCK_METHOD1(TabbedOut, void(bool reverse));

  //
  MOCK_METHOD0(TabClosed, void());
  MOCK_METHOD1(AttachTab, void(const AttachExternalTabParams& attach_params));
};

struct MockSender : public IPC::Message::Sender {
  MOCK_METHOD1(Send, bool(IPC::Message* m));
};

struct MockCFProxyTraits : public CFProxyTraits {
  MOCK_METHOD2(DoCreateChannel, IPC::Message::Sender*(const std::string& id,
      IPC::Channel::Listener* l));
  MOCK_METHOD1(CloseChannel, void(IPC::Message::Sender* s));
  MOCK_METHOD1(LaunchApp, bool(const std::wstring& cmd_line));

  // Forward the CreateChannel to DoCreateChannel, but save the ipc_thread
  // and the listener (i.e. proxy implementation of Channel::Listener)
  virtual IPC::Message::Sender* CreateChannel(const std::string& id,
    IPC::Channel::Listener* l) {
      ipc_loop = MessageLoop::current();
      listener = l;
      return this->DoCreateChannel(id, l);
  }

  // Simulate some activity in the IPC thread.
  // You may find API_FIRE_XXXX macros (see below) handy instead.
  void FireConnect(base::TimeDelta t) {
    ASSERT_TRUE(ipc_loop != NULL);
    ipc_loop->PostDelayedTask(
        FROM_HERE, base::Bind(&IPC::Channel::Listener::OnChannelConnected,
                              base::Unretained(listener), 0),
        t.InMilliseconds());
  }

  void FireError(base::TimeDelta t) {
    ASSERT_TRUE(ipc_loop != NULL);
    ipc_loop->PostDelayedTask(
        FROM_HERE, base::Bind(&IPC::Channel::Listener::OnChannelError,
                              base::Unretained(listener)),
        t.InMilliseconds());
  }

  void FireMessage(const IPC::Message& m, base::TimeDelta t) {
    ASSERT_TRUE(ipc_loop != NULL);
    ipc_loop->PostDelayedTask(
        FROM_HERE,
        base::IgnoreReturn<bool>(
            base::Bind(&IPC::Channel::Listener::OnMessageReceived,
                       base::Unretained(listener), m)),
        t.InMilliseconds());
  }

  MockCFProxyTraits() : ipc_loop(NULL) {}
  MockSender sender;
 private:
  MessageLoop* ipc_loop;
  IPC::Channel::Listener* listener;
};

// Handy macros when we want so simulate something on the IPC thread.
#define API_FIRE_CONNECT(api, t)  InvokeWithoutArgs(CreateFunctor(&api, \
                                       &MockCFProxyTraits::FireConnect, t))
#define API_FIRE_ERROR(api, t) InvokeWithoutArgs(CreateFunctor(&api, \
                                       &MockCFProxyTraits::FireError, t))
#define API_FIRE_MESSAGE(api, t) InvokeWithoutArgs(CreateFunctor(&api, \
                                       &MockCFProxyTraits::FireMessage, t))

TEST(ChromeProxy, DelegateAddRemove) {
  StrictMock<MockCFProxyTraits> api;
  StrictMock<MockChromeProxyDelegate> delegate;
  StrictMock<MockFactory> factory;    // to be destroyed before other mocks
  CFProxy* proxy = new CFProxy(&api);

  EXPECT_CALL(factory, CreateProxy()).WillOnce(Return(proxy));
  EXPECT_CALL(api, DoCreateChannel(_, proxy)).WillOnce(Return(&api.sender));
  EXPECT_CALL(api, LaunchApp(_)).WillOnce(Return(true));
  EXPECT_CALL(api, CloseChannel(&api.sender));

  EXPECT_CALL(delegate, tab_handle()).WillRepeatedly(Return(0));
  EXPECT_CALL(delegate, Disconnected());

  ProxyParams params;
  params.profile = "Adam N. Epilinter";
  params.timeout = base::TimeDelta::FromSeconds(4);
  factory.GetProxy(&delegate, params);
  factory.ReleaseProxy(&delegate, params.profile);
}

// Not very useful test. Just for illustration. :)
TEST(ChromeProxy, SharedProxy) {
  base::WaitableEvent done1(false, false);
  base::WaitableEvent done2(false, false);
  StrictMock<MockCFProxyTraits> api;
  StrictMock<MockChromeProxyDelegate> delegate1;
  StrictMock<MockChromeProxyDelegate> delegate2;
  StrictMock<MockFactory> factory;
  CFProxy* proxy = new CFProxy(&api);

  EXPECT_CALL(factory, CreateProxy()).WillOnce(Return(proxy));
  EXPECT_CALL(api, DoCreateChannel(_, proxy)).WillOnce(Return(&api.sender));
  EXPECT_CALL(api, LaunchApp(_)).WillOnce(DoAll(
      API_FIRE_CONNECT(api, base::TimeDelta::FromMilliseconds(150)),
      Return(true)));
  EXPECT_CALL(api, CloseChannel(&api.sender));

  EXPECT_CALL(delegate1, tab_handle()).WillRepeatedly(Return(0));
  EXPECT_CALL(delegate2, tab_handle()).WillRepeatedly(Return(0));

  EXPECT_CALL(delegate1, Connected(proxy))
      .WillOnce(InvokeWithoutArgs(&done1, &base::WaitableEvent::Signal));
  EXPECT_CALL(delegate2, Connected(proxy))
      .WillOnce(InvokeWithoutArgs(&done2, &base::WaitableEvent::Signal));

  ProxyParams params;
  params.profile = "Adam N. Epilinter";
  params.timeout = base::TimeDelta::FromSeconds(4);

  factory.GetProxy(&delegate1, params);
  params.timeout = base::TimeDelta::FromSeconds(2);
  factory.GetProxy(&delegate2, params);

  EXPECT_TRUE(done1.TimedWait(base::TimeDelta::FromSeconds(1)));
  EXPECT_TRUE(done2.TimedWait(base::TimeDelta::FromSeconds(1)));

  EXPECT_CALL(delegate2, Disconnected());
  EXPECT_CALL(delegate1, Disconnected());

  factory.ReleaseProxy(&delegate2, params.profile);
  factory.ReleaseProxy(&delegate1, params.profile);
}

TEST(ChromeProxy, LaunchTimeout) {
  base::WaitableEvent done(true, false);
  StrictMock<MockCFProxyTraits> api;
  StrictMock<MockChromeProxyDelegate> delegate;
  StrictMock<MockFactory> factory;
  CFProxy* proxy = new CFProxy(&api);

  EXPECT_CALL(delegate, tab_handle()).WillRepeatedly(Return(0));
  EXPECT_CALL(factory, CreateProxy()).WillOnce(Return(proxy));
  EXPECT_CALL(api, DoCreateChannel(_, proxy)).WillOnce(Return(&api.sender));
  EXPECT_CALL(api, LaunchApp(_)).WillOnce(Return(true));
  EXPECT_CALL(api, CloseChannel(&api.sender));

  EXPECT_CALL(delegate, PeerLost(_,
                      ChromeProxyDelegate::CHROME_EXE_LAUNCH_TIMEOUT))
      .WillOnce(InvokeWithoutArgs(&done, &base::WaitableEvent::Signal));
  ProxyParams params;
  params.profile = "Adam N. Epilinter";
  params.timeout = base::TimeDelta::FromMilliseconds(300);
  factory.GetProxy(&delegate, params);
  EXPECT_TRUE(done.TimedWait(base::TimeDelta::FromSeconds(1)));

  EXPECT_CALL(delegate, Disconnected());
  factory.ReleaseProxy(&delegate, params.profile);
}

TEST(ChromeProxy, LaunchChrome) {
  base::WaitableEvent connected(false, false);
  StrictMock<MockChromeProxyDelegate> delegate;
  ChromeProxyFactory factory;

  ProxyParams params;
  params.profile = "Adam N. Epilinter";
  params.timeout = base::TimeDelta::FromSeconds(10);

  EXPECT_CALL(delegate, tab_handle()).WillRepeatedly(Return(0));
  EXPECT_CALL(delegate, Connected(NotNull()))
    .WillOnce(InvokeWithoutArgs(&connected, &base::WaitableEvent::Signal));

  factory.GetProxy(&delegate, params);
  EXPECT_TRUE(connected.TimedWait(base::TimeDelta::FromSeconds(15)));

  EXPECT_CALL(delegate, Disconnected());
  factory.ReleaseProxy(&delegate, params.profile);
}

// Test that a channel error results in Completed_XYZ(false, ) called if
// the synchronious XYZ message has been sent.
TEST(ChromeProxy, ChannelError) {
  base::WaitableEvent connected(false, false);
  StrictMock<MockCFProxyTraits> api;
  StrictMock<MockChromeProxyDelegate> delegate;
  StrictMock<MockFactory> factory;
  CFProxy* proxy = new CFProxy(&api);

  ProxyParams params;
  params.profile = "Adam N. Epilinter";
  params.timeout = base::TimeDelta::FromMilliseconds(300);

  testing::InSequence s;

  EXPECT_CALL(factory, CreateProxy()).WillOnce(Return(proxy));
  EXPECT_CALL(api, DoCreateChannel(_, proxy)).WillOnce(Return(&api.sender));
  EXPECT_CALL(api, LaunchApp(_)).WillOnce(DoAll(
      API_FIRE_CONNECT(api, base::TimeDelta::FromMilliseconds(10)),
      Return(true)));
  EXPECT_CALL(delegate, Connected(proxy))
      .WillOnce(DoAll(
          InvokeWithoutArgs(CreateFunctor(proxy, &ChromeProxy::ConnectTab,
                                          &delegate, HWND(6), 512)),
          InvokeWithoutArgs(&connected, &base::WaitableEvent::Signal)));

  EXPECT_CALL(api.sender, Send(_));
  EXPECT_CALL(delegate, Completed_ConnectToTab(false, _, _, _, _));
  EXPECT_CALL(api, CloseChannel(&api.sender));
  EXPECT_CALL(delegate, PeerLost(_, ChromeProxyDelegate::CHANNEL_ERROR));

  factory.GetProxy(&delegate, params);
  EXPECT_TRUE(connected.TimedWait(base::TimeDelta::FromSeconds(15)));
  // Simulate a channel error.
  api.FireError(base::TimeDelta::FromMilliseconds(0));

  // Expectations when the Proxy is destroyed.
  EXPECT_CALL(delegate, tab_handle()).WillOnce(Return(0));
  EXPECT_CALL(delegate, Disconnected());
  factory.ReleaseProxy(&delegate, params.profile);
}
///////////////////////////////////////////////////////////////////////////////
namespace {
template <typename M, typename A>
inline IPC::Message* CreateReply(M* m, const A& a) {
  IPC::Message* r = IPC::SyncMessage::GenerateReply(m);
  if (r) {
    M::WriteReplyParams(r, a);
  }
  return r;
}

template <typename M, typename A, typename B>
inline IPC::Message* CreateReply(M* m, const A& a, const B& b) {
  IPC::Message* r = IPC::SyncMessage::GenerateReply(m);
  if (r) {
    M::WriteReplyParams(r, a, b);
  }
  return r;
}

template <typename M, typename A, typename B, typename C>
inline IPC::Message* CreateReply(M* m, const A& a, const B& b, const C& c) {
  IPC::Message* r = IPC::SyncMessage::GenerateReply(m);
  if (r) {
    M::WriteReplyParams(r, a, b, c);
  }
  return r;
}

template <typename M, typename A, typename B, typename C, typename D>
inline IPC::Message* CreateReply(M* m, const A& a, const B& b, const C& c,
                                 const D& d) {
  IPC::Message* r = IPC::SyncMessage::GenerateReply(m);
  if (r) {
    M::WriteReplyParams(r, a, b, c, d);
  }
  return r;
}}  // namespace

TEST(SyncMsgSender, Deserialize) {
  // Note the ipc thread is not actually needed, but we try to be close
  // to real-world conditions - that SyncMsgSender works from multiple threads.
  base::Thread ipc("ipc");
  ipc.StartWithOptions(base::Thread::Options(MessageLoop::TYPE_IO, 0));

  StrictMock<MockChromeProxyDelegate> d1;
  TabsMap tab2delegate;
  SyncMsgSender queue(&tab2delegate);

  const int kTabHandle = 6;
  const int kSessionId = 8;

  // Create a sync message and its reply.
  AutomationMsg_CreateExternalTab m(ExternalTabSettings(), 0, 0, 0, 0);
  scoped_ptr<IPC::Message> r(CreateReply(&m, (HWND)1, (HWND)2, kTabHandle,
                                         kSessionId));

  queue.QueueSyncMessage(&m, &d1, NULL);

  testing::InSequence s;
  EXPECT_CALL(d1, Completed_CreateTab(true, (HWND)1, (HWND)2, kTabHandle,
                                      kSessionId));

  // Execute replies in a worker thread.
  ipc.message_loop()->PostTask(
      FROM_HERE,
      base::IgnoreReturn<bool>(base::Bind(&SyncMsgSender::OnReplyReceived,
                                          base::Unretained(&queue), r.get())));
  ipc.Stop();

  // Expect that tab 6 has been associated with the delegate.
  EXPECT_EQ(&d1, tab2delegate[6]);
}

TEST(SyncMsgSender, OnChannelClosed) {
  // TODO(stoyan): implement.
}
