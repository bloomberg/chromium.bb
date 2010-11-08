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
using testing::StrEq;
using testing::Eq;

// There is not much to test here since CFProxy is pretty dumb.
struct MockFactory : public ChromeProxyFactory {
  MOCK_METHOD0(CreateProxy, ChromeProxy*());
};

struct MockChromeProxyDelegate : public ChromeProxyDelegate {
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
  MOCK_METHOD3(HandleContextMenu, void(HANDLE menu_handle, int align_flags,
      const IPC::MiniContextMenuParams& params));
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

DISABLE_RUNNABLE_METHOD_REFCOUNT(SyncMsgSender);
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

  // Create some sync messages and their replies.
  AutomationMsg_InstallExtension m1(0, FilePath(L"c:\\awesome.x"), 0);
  AutomationMsg_CreateExternalTab m2(0, IPC::ExternalTabSettings(), 0, 0, 0, 0);
  scoped_ptr<IPC::Message> r1(CreateReply(&m1,
      AUTOMATION_MSG_EXTENSION_INSTALL_SUCCEEDED));
  scoped_ptr<IPC::Message> r2(CreateReply(&m2, (HWND)1, (HWND)2, kTabHandle,
                                          kSessionId));

  queue.QueueSyncMessage(&m1, &d1, NULL);
  queue.QueueSyncMessage(&m2, &d1, NULL);

  testing::InSequence s;
  EXPECT_CALL(d1, Completed_InstallExtension(true,
      AUTOMATION_MSG_EXTENSION_INSTALL_SUCCEEDED, NULL));
  EXPECT_CALL(d1, Completed_CreateTab(true, (HWND)1, (HWND)2, kTabHandle,
                                      kSessionId));

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

MATCHER_P(EqNavigationInfo, ni, "") {
  return arg.navigation_type == ni.navigation_type &&
      arg.relative_offset == ni.relative_offset &&
      arg.navigation_index == ni.navigation_index &&
      arg.title == ni.title &&
      arg.url == ni.url &&
      arg.referrer == ni.referrer &&
      arg.security_style == ni.security_style &&
      arg.displayed_insecure_content == ni.displayed_insecure_content &&
      arg.ran_insecure_content == ni.ran_insecure_content;
}

MATCHER_P(EqMSG, msg, "") {
  return arg.hwnd == msg.hwnd &&
      arg.message == msg.message &&
      arg.wParam == msg.wParam &&
      arg.lParam == msg.lParam &&
      arg.time == msg.time &&
      arg.pt.x == msg.pt.x &&
      arg.pt.y == msg.pt.y;
}

MATCHER_P(EqContextMenuParam, p, "") {
  return arg.screen_x == p.screen_x &&
    arg.screen_y == p.screen_y &&
    arg.link_url == p.link_url &&
    arg.unfiltered_link_url == p.unfiltered_link_url &&
    arg.src_url == p.src_url &&
    arg.page_url == p.page_url &&
    arg.frame_url == p.frame_url;
}

MATCHER_P(EqURLRequest, p, "") {
  return arg.url == p.url &&
      arg.method == p.method &&
      arg.referrer == p.referrer &&
      arg.extra_request_headers == p.extra_request_headers &&
      // TODO(stoyan): scoped_refptr<net::UploadData> upload_data;
      arg.resource_type == p.resource_type;
}


MATCHER_P(EqAttachExternalTab, p, "") {
  return arg.cookie == p.cookie &&
      arg.url == p.url &&
      arg.dimensions == p.dimensions &&
      arg.disposition == p.disposition &&
      arg.user_gesture == p.user_gesture &&
      arg.profile_name == p.profile_name;
}

TEST(Deserialize, DispatchTabMessage) {
  testing::InSequence s;
  StrictMock<MockChromeProxyDelegate> delegate;
  GURL url("http://destination");
  GURL ref("http://referer");

  // Tuple3<int, int, IPC::NavigationInfo>
  int flags = 2;
  IPC::NavigationInfo ni = {2, 3, 4, L"title", url,
      ref, SECURITY_STYLE_AUTHENTICATION_BROKEN, true, true};
  AutomationMsg_NavigationStateChanged m1(0, 1, flags, ni);
  EXPECT_CALL(delegate, NavigationStateChanged(flags, EqNavigationInfo(ni)));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m1));

  // Tuple2<int, std::wstring>
  AutomationMsg_UpdateTargetUrl m2(0, 1, L"hello");
  EXPECT_CALL(delegate, UpdateTargetUrl(StrEq(L"hello")));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m2));

  // Tuple2<int, MSG>
  MSG wnd_msg = {0, WM_DESTROY, 1, 9, 0x5671, { 11, 12 }};
  AutomationMsg_HandleAccelerator m3(0, 1, wnd_msg);
  EXPECT_CALL(delegate, HandleAccelerator(EqMSG(wnd_msg)));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m3));

  // Tuple2<int, bool>
  AutomationMsg_TabbedOut m4(0, 1, true);
  EXPECT_CALL(delegate, TabbedOut(true));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m4));


  // Tuple4<int, GURL, GURL, int>
  AutomationMsg_OpenURL m5(0, 1, url, ref, 4);
  EXPECT_CALL(delegate, OpenURL(url, ref, 4));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m5));

  // Tuple3<int, int, GURL>
  AutomationMsg_NavigationFailed m6(0, 1, 2, url);
  EXPECT_CALL(delegate, NavigationFailed(2, url));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m6));


  // Tuple2<int, IPC::NavigationInfo>
  AutomationMsg_DidNavigate m7(0, 1, ni);
  EXPECT_CALL(delegate, DidNavigate(EqNavigationInfo(ni)));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m7));


  // Tuple2<int, GURL>
  AutomationMsg_TabLoaded m8(0, 1, url);
  EXPECT_CALL(delegate, TabLoaded(url));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m8));

  // Tuple4<int, string, string, string>
  std::string msg("Load oranges barrels");
  std::string origin("Brothers Karamazov");
  std::string target("Alexander Ivanovich");
  AutomationMsg_ForwardMessageToExternalHost m9(0, 1, msg, origin, target);
  EXPECT_CALL(delegate, MessageToHost(msg, origin, target));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m9));

  // Tuple4<int, HANDLE, int, IPC::ContextMenuParams>
  IPC::MiniContextMenuParams ctxmenu = { 711, 512, GURL("http://link_src"),
      GURL("http://unfiltered_link_url"), GURL("http://src_url"),
      GURL("http://page_url"), GURL("http://frame_url") };
  AutomationMsg_ForwardContextMenuToExternalHost m10(0, 1, HANDLE(7), 4,
                                                     ctxmenu);
  EXPECT_CALL(delegate, HandleContextMenu(HANDLE(7), 4,
                                          EqContextMenuParam(ctxmenu)));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m10));

  // Tuple3<int, int, IPC::AutomationURLRequest>
  IPC::AutomationURLRequest url_request = {"url", "post", "referer",
      "extra_headers", 0, 3 };
  AutomationMsg_RequestStart m11(0, 1, 7, url_request);
  EXPECT_CALL(delegate, Network_Start(7, EqURLRequest(url_request)));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m11));

  // Tuple3<int, int, int>
  AutomationMsg_RequestRead m12(0, 1, 7, 16384);
  EXPECT_CALL(delegate, Network_Read(7, 16384));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m12));

  // Tuple3<int, int, URLRequestStatus>
  AutomationMsg_RequestEnd m13(0, 1, 7, URLRequestStatus());
  EXPECT_CALL(delegate, Network_End(7, _));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m13));

  // Tuple2<int, int>
  AutomationMsg_DownloadRequestInHost m14(0, 1, 7);
  EXPECT_CALL(delegate, Network_DownloadInHost(7));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m14));

  // Tuple3<int, GURL, string>
  AutomationMsg_SetCookieAsync m15(0, 1, url, "cake=big");
  EXPECT_CALL(delegate, SetCookie(url, "cake=big"));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m15));

  // Tuple2<int, IPC::AttachExternalTabParams>
  IPC::AttachExternalTabParams ext_tab = { 0xFEDCBA0987654321i64, url,
      gfx::Rect(6, 9, 123, 999), 1, false, "theprofile" };
  AutomationMsg_AttachExternalTab m16(0, 1, ext_tab);
  EXPECT_CALL(delegate, AttachTab(EqAttachExternalTab(ext_tab)));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m16));

  // Tuple2<int, int>
  AutomationMsg_RequestGoToHistoryEntryOffset m17(0, 1, -4);
  EXPECT_CALL(delegate, GoToHistoryOffset(-4));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m17));

  // Tuple3<int, GURL, int>
  AutomationMsg_GetCookiesFromHost m18(0, 1, url, 903);
  EXPECT_CALL(delegate, GetCookies(url, 903));
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m18));

  AutomationMsg_CloseExternalTab m19(0, 1);
  EXPECT_CALL(delegate, TabClosed());
  EXPECT_TRUE(DispatchTabMessageToDelegate(&delegate, m19));
}
