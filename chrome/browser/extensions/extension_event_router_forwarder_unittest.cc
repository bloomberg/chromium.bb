// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_event_router_forwarder.h"

#include "base/message_loop.h"
#include "base/system_monitor/system_monitor.h"
#include "base/test/thread_test_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kEventName[] = "event_name";
const char kEventArgs[] = "event_args";
const char kExt[] = "extension";

class MockExtensionEventRouterForwarder : public ExtensionEventRouterForwarder {
 public:
  virtual ~MockExtensionEventRouterForwarder() {}

  MOCK_METHOD6(CallExtensionEventRouter,
      void(Profile*, const std::string&, const std::string&, const std::string&,
           Profile*, const GURL&));
};

}  // namespace

class ExtensionEventRouterForwarderTest : public testing::Test {
 protected:
  ExtensionEventRouterForwarderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO),
        profile_manager_(
            static_cast<TestingBrowserProcess*>(g_browser_process)) {
#if defined(OS_MACOSX)
    base::SystemMonitor::AllocateSystemIOPorts();
#endif
    dummy.reset(new base::SystemMonitor);
  }

  virtual void SetUp() {
    ASSERT_TRUE(profile_manager_.SetUp());

    // Inject a BrowserProcess with a ProfileManager.
    ASSERT_TRUE(io_thread_.Start());

    profile1_ = profile_manager_.CreateTestingProfile("one");
    profile2_ = profile_manager_.CreateTestingProfile("two");
  }

  TestingProfile* CreateIncognitoProfile(TestingProfile* base) {
    TestingProfile* incognito = new TestingProfile;  // Owned by |base|.
    incognito->set_incognito(true);
    base->SetOffTheRecordProfile(incognito);
    return incognito;
  }

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;
  TestingProfileManager profile_manager_;
  scoped_ptr<base::SystemMonitor> dummy;
  // Profiles are weak pointers, owned by ProfileManager in |browser_process_|.
  TestingProfile* profile1_;
  TestingProfile* profile2_;
};

TEST_F(ExtensionEventRouterForwarderTest, BroadcastRendererUI) {
  scoped_refptr<MockExtensionEventRouterForwarder> event_router(
      new MockExtensionEventRouterForwarder);
  GURL url;
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile1_, "", kEventName, kEventArgs, profile1_, url));
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile2_, "", kEventName, kEventArgs, profile2_, url));
  event_router->BroadcastEventToRenderers(kEventName, kEventArgs, url);
}

TEST_F(ExtensionEventRouterForwarderTest, BroadcastRendererUIIncognito) {
  scoped_refptr<MockExtensionEventRouterForwarder> event_router(
      new MockExtensionEventRouterForwarder);
  using ::testing::_;
  GURL url;
  Profile* incognito = CreateIncognitoProfile(profile1_);
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile1_, "", kEventName, kEventArgs, profile1_, url));
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(incognito, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile2_, "", kEventName, kEventArgs, profile2_, url));
  event_router->BroadcastEventToRenderers(kEventName, kEventArgs, url);
}

// This is the canonical test for passing control flow from the IO thread
// to the UI thread. Repeating this for all public functions of
// ExtensionEventRouterForwarder would not increase coverage.
TEST_F(ExtensionEventRouterForwarderTest, BroadcastRendererIO) {
  scoped_refptr<MockExtensionEventRouterForwarder> event_router(
      new MockExtensionEventRouterForwarder);
  GURL url;
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile1_, "", kEventName, kEventArgs, profile1_, url));
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile2_, "", kEventName, kEventArgs, profile2_, url));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          event_router.get(),
          &MockExtensionEventRouterForwarder::BroadcastEventToRenderers,
          std::string(kEventName), std::string(kEventArgs), url));

  // Wait for IO thread's message loop to be processed
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));
  ASSERT_TRUE(helper->Run());

  MessageLoop::current()->RunAllPending();
}

TEST_F(ExtensionEventRouterForwarderTest, UnicastRendererUIRestricted) {
  scoped_refptr<MockExtensionEventRouterForwarder> event_router(
      new MockExtensionEventRouterForwarder);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile1_, "", kEventName, kEventArgs, profile1_, url));
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(profile2_, _, _, _, _, _)).Times(0);
  event_router->DispatchEventToRenderers(kEventName, kEventArgs,
                                         profile1_, true, url);
}

TEST_F(ExtensionEventRouterForwarderTest,
       UnicastRendererUIRestrictedIncognito1) {
  scoped_refptr<MockExtensionEventRouterForwarder> event_router(
      new MockExtensionEventRouterForwarder);
  Profile* incognito = CreateIncognitoProfile(profile1_);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile1_, "", kEventName, kEventArgs, profile1_, url));
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(incognito, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(profile2_, _, _, _, _, _)).Times(0);
  event_router->DispatchEventToRenderers(kEventName, kEventArgs,
                                         profile1_, true, url);
}

TEST_F(ExtensionEventRouterForwarderTest,
       UnicastRendererUIRestrictedIncognito2) {
  scoped_refptr<MockExtensionEventRouterForwarder> event_router(
      new MockExtensionEventRouterForwarder);
  Profile* incognito = CreateIncognitoProfile(profile1_);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(profile1_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          incognito, "", kEventName, kEventArgs, incognito, url));
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(profile2_, _, _, _, _, _)).Times(0);
  event_router->DispatchEventToRenderers(kEventName, kEventArgs,
                                         incognito, true, url);
}

TEST_F(ExtensionEventRouterForwarderTest, UnicastRendererUIUnrestricted) {
  scoped_refptr<MockExtensionEventRouterForwarder> event_router(
      new MockExtensionEventRouterForwarder);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile1_, "", kEventName, kEventArgs, NULL, url));
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(profile2_, _, _, _, _, _)).Times(0);
  event_router->DispatchEventToRenderers(kEventName, kEventArgs,
                                         profile1_, false, url);
}

TEST_F(ExtensionEventRouterForwarderTest,
       UnicastRendererUIUnrestrictedIncognito) {
  scoped_refptr<MockExtensionEventRouterForwarder> event_router(
      new MockExtensionEventRouterForwarder);
  Profile* incognito = CreateIncognitoProfile(profile1_);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile1_, "", kEventName, kEventArgs, NULL, url));
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(incognito, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(profile2_, _, _, _, _, _)).Times(0);
  event_router->DispatchEventToRenderers(kEventName, kEventArgs,
                                         profile1_, false, url);
}

TEST_F(ExtensionEventRouterForwarderTest, BroadcastExtensionUI) {
  scoped_refptr<MockExtensionEventRouterForwarder> event_router(
      new MockExtensionEventRouterForwarder);
  GURL url;
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile1_, kExt, kEventName, kEventArgs, profile1_, url));
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile2_, kExt, kEventName, kEventArgs, profile2_, url));
  event_router->BroadcastEventToExtension(kExt, kEventName, kEventArgs, url);
}

TEST_F(ExtensionEventRouterForwarderTest, UnicastExtensionUIRestricted) {
  scoped_refptr<MockExtensionEventRouterForwarder> event_router(
      new MockExtensionEventRouterForwarder);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile1_, kExt, kEventName, kEventArgs, profile1_, url));
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(profile2_, _, _, _, _, _)).Times(0);
  event_router->DispatchEventToExtension(kExt, kEventName, kEventArgs,
                                         profile1_, true, url);
}

TEST_F(ExtensionEventRouterForwarderTest, UnicastExtensionUIUnrestricted) {
  scoped_refptr<MockExtensionEventRouterForwarder> event_router(
      new MockExtensionEventRouterForwarder);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(
          profile1_, kExt, kEventName, kEventArgs, NULL, url));
  EXPECT_CALL(*event_router,
      CallExtensionEventRouter(profile2_, _, _, _, _, _)).Times(0);
  event_router->DispatchEventToExtension(kExt, kEventName, kEventArgs,
                                         profile1_, false, url);
}
