// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/file_path.h"
#include "base/waitable_event.h"
#include "chrome_frame/cfproxy_private.h"
#include "chrome/test/automation/automation_messages.h"
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

// There is not much to test here since CFProxy is pretty dumb.
struct MockFactory : public ChromeProxyFactory {
  MOCK_METHOD0(CreateProxy, ChromeProxy*());
};

struct MockChromeProxyDelegate : public ChromeProxyDelegate {
  MOCK_METHOD1(Connected, void(ChromeProxy* proxy));
  MOCK_METHOD2(PeerLost, void(ChromeProxy*, enum DisconnectReason reason));
  MOCK_METHOD0(Disconnected, void());
  MOCK_METHOD0(tab_handle, int());

  MOCK_METHOD4(Completed_CreateTab, void(bool success, HWND chrome_wnd,
      HWND tab_window, int tab_handle));
  MOCK_METHOD4(Completed_ConnectToTab, void(bool success, HWND chrome_window,
      HWND tab_window, int tab_handle));
  MOCK_METHOD2(Completed_Navigate, void(bool success,
      enum AutomationMsg_NavigationResponseValues res));
  MOCK_METHOD3(Completed_InstallExtension, void(bool success,
      enum AutomationMsg_ExtensionResponseValues res, SyncMessageContext* ctx));
  MOCK_METHOD3(Completed_LoadExpandedExtension, void(bool success,
      enum AutomationMsg_ExtensionResponseValues res, SyncMessageContext* ctx));
  MOCK_METHOD2(Completed_GetEnabledExtensions, void(bool success,
      const std::vector<FilePath>* v));

  // Network requests from Chrome.
  MOCK_METHOD2(Network_Start, void(int request_id,
      const IPC::AutomationURLRequest& request_info));
  MOCK_METHOD2(Network_Read, void(int request_id, int bytes_to_read));
  MOCK_METHOD2(Network_End, void(int request_id, const URLRequestStatus& s));
  MOCK_METHOD1(Network_DownloadInHost, void(int request_id));
  MOCK_METHOD2(GetCookies, void(const GURL& url, int cookie_id));
  MOCK_METHOD2(SetCookie, void(const GURL& url, const std::string& cookie));

  // Navigation progress notifications.
  MOCK_METHOD2(NavigationStateChanged, void(int flags,
      const IPC::NavigationInfo& nav_info));
  MOCK_METHOD1(UpdateTargetUrl, void(const std::wstring& url));
  MOCK_METHOD2(NavigationFailed, void(int error_code, const GURL& gurl));
  MOCK_METHOD1(DidNavigate, void(const IPC::NavigationInfo& navigation_info));
  MOCK_METHOD1(TabLoaded, void(const GURL& url));

  //
  MOCK_METHOD3(OpenURL, void(const GURL& url_to_open, const GURL& referrer,
      int open_disposition));
  MOCK_METHOD1(GoToHistoryOffset, void(int offset));
  MOCK_METHOD3(MessageToHost, void(const std::string& message,
      const std::string& origin, const std::string& target));

  // Misc. UI.
  MOCK_METHOD1(HandleAccelerator, void(const MSG& accel_message));
  MOCK_METHOD1(TabbedOut, void(bool reverse));

  //
  MOCK_METHOD0(TabClosed, void());
  MOCK_METHOD1(AttachTab,
      void(const IPC::AttachExternalTabParams& attach_params));
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
    ipc_loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(listener,
        &IPC::Channel::Listener::OnChannelConnected, 0), t.InMilliseconds());
  }

  void FireError(base::TimeDelta t) {
    ASSERT_TRUE(ipc_loop != NULL);
    ipc_loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(listener,
        &IPC::Channel::Listener::OnChannelError), t.InMilliseconds());
  }

  void FireMessage(const IPC::Message& m, base::TimeDelta t) {
    ASSERT_TRUE(ipc_loop != NULL);
    ipc_loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(listener,
        &IPC::Channel::Listener::OnMessageReceived, m), t.InMilliseconds());
  }

  MockCFProxyTraits() : ipc_loop(NULL) {}
  MockSender sender;
 private:
  MessageLoop* ipc_loop;
  IPC::Channel::Listener* listener;
};

// Handy macros when we want so similate something on the IPC thread.
#define API_FIRE_CONNECT(api, t)  InvokeWithoutArgs(CreateFunctor(&api, \
                                       &MockCFProxyTraits::FireConnect, t))
#define API_FIRE_ERROR(api, t) InvokeWithoutArgs(CreateFunctor(&api, \
                                       &MockCFProxyTraits::FireError, t))
#define API_FIRE_MESSAGE(api, t) InvokeWithoutArgs(CreateFunctor(&api, \
                                       &MockCFProxyTraits::FireMessage, t))
DISABLE_RUNNABLE_METHOD_REFCOUNT(IPC::Channel::Listener);

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
  StrictMock<MockFactory> factory;
  StrictMock<MockCFProxyTraits> api;
  StrictMock<MockChromeProxyDelegate> delegate;
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
}  // namespace

DISABLE_RUNNABLE_METHOD_REFCOUNT(SyncMsgSender);
TEST(SyncMsgSender, Deserialize) {
  // Note the ipc thread is not actually needed, but we try to be close
  // to real-world conditions - that SyncMsgSender works from multiple threads.
  base::Thread ipc("ipc");
  ipc.StartWithOptions(base::Thread::Options(MessageLoop::TYPE_IO, 0));

  StrictMock<MockChromeProxyDelegate> d1;
  TabsMap tab2delegate;
  SyncMsgSender queue(&tab2delegate);

  // Create some sync messages and their replies.
  AutomationMsg_InstallExtension m1(0, FilePath(L"c:\\awesome.x"), 0);
  AutomationMsg_CreateExternalTab m2(0, IPC::ExternalTabSettings(), 0, 0, 0);
  scoped_ptr<IPC::Message> r1(CreateReply(&m1,
      AUTOMATION_MSG_EXTENSION_INSTALL_SUCCEEDED));
  scoped_ptr<IPC::Message> r2(CreateReply(&m2, (HWND)1, (HWND)2, 6));

  queue.QueueSyncMessage(&m1, &d1, NULL);
  queue.QueueSyncMessage(&m2, &d1, NULL);

  testing::InSequence s;
  EXPECT_CALL(d1, Completed_InstallExtension(true,
      AUTOMATION_MSG_EXTENSION_INSTALL_SUCCEEDED, NULL));
  EXPECT_CALL(d1, Completed_CreateTab(true, (HWND)1, (HWND)2, 6));

  // Execute replies in a worker thread.
  ipc.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(&queue,
      &SyncMsgSender::OnReplyReceived, r1.get()));
  ipc.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(&queue,
      &SyncMsgSender::OnReplyReceived, r2.get()));
  ipc.Stop();

  // Expect that tab 6 has been associated with the delegate.
  EXPECT_EQ(&d1, tab2delegate[6]);
}

TEST(SyncMsgSender, OnChannelClosed) {
  // TODO(stoyan): implement.
}

