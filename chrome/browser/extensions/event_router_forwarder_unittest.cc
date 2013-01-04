// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/event_router_forwarder.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/system_monitor/system_monitor.h"
#include "base/test/thread_test_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace extensions {

namespace {

const char kEventName[] = "event_name";
const char kExt[] = "extension";

class MockEventRouterForwarder : public EventRouterForwarder {
 public:
  MOCK_METHOD5(CallEventRouter,
      void(Profile*, const std::string&, const std::string&, Profile*,
           const GURL&));

  virtual void CallEventRouter(
      Profile* profile, const std::string& extension_id,
      const std::string& event_name, scoped_ptr<base::ListValue> event_args,
      Profile* restrict_to_profile, const GURL& event_url) {
    CallEventRouter(profile, extension_id, event_name,
                             restrict_to_profile, event_url);
  }

 protected:
  virtual ~MockEventRouterForwarder() {}
};

static void BroadcastEventToRenderers(EventRouterForwarder* event_router,
                                      const std::string& event_name,
                                      const GURL& url) {
  scoped_ptr<ListValue> args(new ListValue());
  event_router->BroadcastEventToRenderers(event_name, args.Pass(), url);
}

static void DispatchEventToRenderers(EventRouterForwarder* event_router,
                                     const std::string& event_name,
                                     void* profile,
                                     bool use_profile_to_restrict_events,
                                     const GURL& url) {
  scoped_ptr<ListValue> args(new ListValue());
  event_router->DispatchEventToRenderers(event_name, args.Pass(), profile,
                                         use_profile_to_restrict_events, url);
}

static void BroadcastEventToExtension(EventRouterForwarder* event_router,
                                      const std::string& extension,
                                      const std::string& event_name,
                                      const GURL& url) {
  scoped_ptr<ListValue> args(new ListValue());
  event_router->BroadcastEventToExtension(extension, event_name, args.Pass(),
                                          url);
}

static void DispatchEventToExtension(EventRouterForwarder* event_router,
                                     const std::string& extension,
                                     const std::string& event_name,
                                     void* profile,
                                     bool use_profile_to_restrict_events,
                                     const GURL& url) {
  scoped_ptr<ListValue> args(new ListValue());
  event_router->DispatchEventToExtension(
      extension, event_name, args.Pass(), profile,
      use_profile_to_restrict_events, url);
}

}  // namespace

class EventRouterForwarderTest : public testing::Test {
 protected:
  EventRouterForwarderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO),
        profile_manager_(
            TestingBrowserProcess::GetGlobal()) {
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
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  TestingProfileManager profile_manager_;
  scoped_ptr<base::SystemMonitor> dummy;
  // Profiles are weak pointers, owned by ProfileManager in |browser_process_|.
  TestingProfile* profile1_;
  TestingProfile* profile2_;
};

TEST_F(EventRouterForwarderTest, BroadcastRendererUI) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  GURL url;
  EXPECT_CALL(*event_router,
      CallEventRouter(profile1_, "", kEventName, profile1_, url));
  EXPECT_CALL(*event_router,
      CallEventRouter(profile2_, "", kEventName, profile2_, url));
  BroadcastEventToRenderers(event_router.get(), kEventName, url);
}

TEST_F(EventRouterForwarderTest, BroadcastRendererUIIncognito) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  using ::testing::_;
  GURL url;
  Profile* incognito = CreateIncognitoProfile(profile1_);
  EXPECT_CALL(*event_router,
      CallEventRouter(profile1_, "", kEventName, profile1_, url));
  EXPECT_CALL(*event_router,
      CallEventRouter(incognito, _, _, _, _)).Times(0);
  EXPECT_CALL(*event_router,
      CallEventRouter(profile2_, "", kEventName, profile2_, url));
  BroadcastEventToRenderers(event_router.get(), kEventName, url);
}

// This is the canonical test for passing control flow from the IO thread
// to the UI thread. Repeating this for all public functions of
// EventRouterForwarder would not increase coverage.
TEST_F(EventRouterForwarderTest, BroadcastRendererIO) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  GURL url;
  EXPECT_CALL(*event_router,
      CallEventRouter(profile1_, "", kEventName, profile1_, url));
  EXPECT_CALL(*event_router,
      CallEventRouter(profile2_, "", kEventName, profile2_, url));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(
          &BroadcastEventToRenderers, base::Unretained(event_router.get()),
          kEventName, url));

  // Wait for IO thread's message loop to be processed
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));
  ASSERT_TRUE(helper->Run());

  MessageLoop::current()->RunUntilIdle();
}

TEST_F(EventRouterForwarderTest, UnicastRendererUIRestricted) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallEventRouter(profile1_, "", kEventName, profile1_, url));
  EXPECT_CALL(*event_router,
      CallEventRouter(profile2_, _, _, _, _)).Times(0);
  DispatchEventToRenderers(event_router.get(), kEventName, profile1_, true,
                           url);
}

TEST_F(EventRouterForwarderTest, UnicastRendererUIRestrictedIncognito1) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  Profile* incognito = CreateIncognitoProfile(profile1_);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallEventRouter(profile1_, "", kEventName, profile1_, url));
  EXPECT_CALL(*event_router,
      CallEventRouter(incognito, _, _, _, _)).Times(0);
  EXPECT_CALL(*event_router,
      CallEventRouter(profile2_, _, _, _, _)).Times(0);
  DispatchEventToRenderers(event_router.get(), kEventName, profile1_, true,
                           url);
}

TEST_F(EventRouterForwarderTest, UnicastRendererUIRestrictedIncognito2) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  Profile* incognito = CreateIncognitoProfile(profile1_);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallEventRouter(profile1_, _, _, _, _)).Times(0);
  EXPECT_CALL(*event_router,
      CallEventRouter(incognito, "", kEventName, incognito, url));
  EXPECT_CALL(*event_router,
      CallEventRouter(profile2_, _, _, _, _)).Times(0);
  DispatchEventToRenderers(event_router.get(), kEventName, incognito, true,
                           url);
}

TEST_F(EventRouterForwarderTest, UnicastRendererUIUnrestricted) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallEventRouter(profile1_, "", kEventName, NULL, url));
  EXPECT_CALL(*event_router,
      CallEventRouter(profile2_, _, _, _, _)).Times(0);
  DispatchEventToRenderers(event_router.get(), kEventName, profile1_, false,
                           url);
}

TEST_F(EventRouterForwarderTest, UnicastRendererUIUnrestrictedIncognito) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  Profile* incognito = CreateIncognitoProfile(profile1_);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallEventRouter(profile1_, "", kEventName, NULL, url));
  EXPECT_CALL(*event_router,
      CallEventRouter(incognito, _, _, _, _)).Times(0);
  EXPECT_CALL(*event_router,
      CallEventRouter(profile2_, _, _, _, _)).Times(0);
  DispatchEventToRenderers(event_router.get(), kEventName, profile1_, false,
                           url);
}

TEST_F(EventRouterForwarderTest, BroadcastExtensionUI) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  GURL url;
  EXPECT_CALL(*event_router,
      CallEventRouter(profile1_, kExt, kEventName, profile1_, url));
  EXPECT_CALL(*event_router,
      CallEventRouter(profile2_, kExt, kEventName, profile2_, url));
  BroadcastEventToExtension(event_router.get(), kExt, kEventName, url);
}

TEST_F(EventRouterForwarderTest, UnicastExtensionUIRestricted) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallEventRouter(profile1_, kExt, kEventName, profile1_, url));
  EXPECT_CALL(*event_router,
      CallEventRouter(profile2_, _, _, _, _)).Times(0);
  DispatchEventToExtension(event_router.get(), kExt, kEventName, profile1_,
                           true, url);
}

TEST_F(EventRouterForwarderTest, UnicastExtensionUIUnrestricted) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router,
      CallEventRouter(profile1_, kExt, kEventName, NULL, url));
  EXPECT_CALL(*event_router,
      CallEventRouter(profile2_, _, _, _, _)).Times(0);
  DispatchEventToExtension(event_router.get(), kExt, kEventName, profile1_,
                           false, url);
}

}  // namespace extensions
