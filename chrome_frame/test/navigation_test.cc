// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/scoped_comptr_win.h"
#include "base/win_util.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/mock_ie_event_sink_actions.h"
#include "chrome_frame/test/mock_ie_event_sink_test.h"

// Needed for CreateFunctor.
#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

using testing::_;
using testing::InSequence;
using testing::StrEq;

namespace chrome_frame_test {

const wchar_t tab_enter_keys[] = { VK_TAB, VK_RETURN, 0 };

// Test fixture for navigation-related tests. Each test is run thrice: IE, CF
// with meta tag invocation, and CF with http header invocation. This is
// accomplished by using gTest's parameterized test.
class FullTabNavigationTest
    : public MockIEEventSinkTest, public testing::TestWithParam<CFInvocation> {
 public:
  FullTabNavigationTest() {}
};

// Instantiate each test case. Instead of doing in one statement, it is split
// into three so gTest prints nicer names.
INSTANTIATE_TEST_CASE_P(IE, FullTabNavigationTest, testing::Values(
    CFInvocation(CFInvocation::NONE)));
INSTANTIATE_TEST_CASE_P(MetaTag, FullTabNavigationTest, testing::Values(
    CFInvocation(CFInvocation::META_TAG)));
INSTANTIATE_TEST_CASE_P(HttpHeader, FullTabNavigationTest, testing::Values(
    CFInvocation(CFInvocation::HTTP_HEADER)));

// This tests navigation to a typed URL.
TEST_P(FullTabNavigationTest, FLAKY_TypeUrl) {
  ie_mock_.ExpectNavigation(IN_IE, GetSimplePageUrl());
  server_mock_.ExpectAndServeRequest(CFInvocation::None(), GetSimplePageUrl());
  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          TypeUrlInAddressBar(&loop_, GetAnchorPageUrl(0), 2000)));

  bool in_cf = GetParam().invokes_cf();
  ie_mock_.ExpectNavigation(in_cf, GetAnchorPageUrl(0));
  server_mock_.ExpectAndServeRequest(GetParam(), GetAnchorPageUrl(0));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(0))))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// This tests navigation to a typed URL containing an fragment.
TEST_P(FullTabNavigationTest, FLAKY_TypeAnchorUrl) {
  if (IsIBrowserServicePatchEnabled()) {
    LOG(ERROR) << "Not running test. IBrowserServicePatch is in place.";
    return;
  }

  ie_mock_.ExpectNavigation(IN_IE, GetSimplePageUrl());
  server_mock_.ExpectAndServeRequest(CFInvocation::None(), GetSimplePageUrl());
  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          TypeUrlInAddressBar(&loop_, GetAnchorPageUrl(1), 2000)));

  bool in_cf = GetParam().invokes_cf();
  ie_mock_.ExpectNavigation(in_cf, GetAnchorPageUrl(1));
  server_mock_.ExpectAndServeRequest(GetParam(), GetAnchorPageUrl(1));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(1))))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Tests refreshing when a cached copy is available.
TEST_P(FullTabNavigationTest, FLAKY_RefreshWithCachedCopy) {
  if (GetInstalledIEVersion() == IE_7) {
    LOG(ERROR) << "Test disabled for this configuration.";
    return;
  }
  bool in_cf = GetParam().invokes_cf();
  ie_mock_.ExpectAnyNavigations();
  InSequence expect_in_scope_for_sequence;

  testing::Cardinality initial_req_cardinality = testing::Exactly(1);
  // TODO(kkania): Remove this allowance for double request on meta tag when no
  // longer necessary.
  if (GetParam().type() == CFInvocation::META_TAG)
    initial_req_cardinality = testing::Between(1, 2);
  EXPECT_CALL(server_mock_, Get(_, UrlPathEq(GetSimplePageUrl()), _))
      .Times(initial_req_cardinality)
      .WillRepeatedly(SendAllowCacheResponse(&server_mock_, GetParam()));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetSimplePageUrl())))
        .WillOnce(testing::DoAll(
            SetFocusToRenderer(&ie_mock_),
            DelayRefresh(&ie_mock_, &loop_, 0)));

  if (in_cf) {
    // For some reason IE7 requests the resource again.
    if (GetInstalledIEVersion() == IE_7) {
      server_mock_.ExpectAndServeRequest(GetParam(), GetSimplePageUrl());
    }
    EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetSimplePageUrl())))
        .WillOnce(testing::DoAll(
            SetFocusToRenderer(&ie_mock_),
            CloseBrowserMock(&ie_mock_)));
  } else {
    // For some reason IE still requests the resource again, but does not
    // trigger another load.
    EXPECT_CALL(server_mock_, Get(_, UrlPathEq(GetSimplePageUrl()), _))
        .WillOnce(CloseBrowserMock(&ie_mock_));
  }

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Test that multiple back and forward requests work.
TEST_P(FullTabNavigationTest, FLAKY_MultipleBackForward) {
  std::wstring page1 = GetSimplePageUrl();
  std::wstring page2 = GetLinkPageUrl();
  std::wstring page3 = GetAnchorPageUrl(0);
  bool in_cf = GetParam().invokes_cf();
  server_mock_.ExpectAndServeAnyRequests(GetParam());
  InSequence expect_in_sequence_for_scope;

  // Navigate to url 2 after the previous navigation is complete.
  ie_mock_.ExpectNavigation(in_cf, page1);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(page1)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          Navigate(&ie_mock_, page2)));

  // Navigate to url 3 after the previous navigation is complete.
  ie_mock_.ExpectNavigation(in_cf, page2);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(page2)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          Navigate(&ie_mock_, page3)));

  // We have reached url 3 and have two back entries for url 1 & 2.
  // Go back to url 2 now.
  ie_mock_.ExpectNavigation(in_cf, page3);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(page3)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoBack(&ie_mock_, &loop_, 0)));

  // We have reached url 2 and have 1 back & 1 forward entries for url 1 & 3.
  // Go back to url 1 now.
  ie_mock_.ExpectNavigation(in_cf, page2);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(page2)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoBack(&ie_mock_, &loop_, 0)));

  // We have reached url 1 and have 0 back & 2 forward entries for url 2 & 3.
  // Go forward to url 2 now.
  ie_mock_.ExpectNavigation(in_cf, page1);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(page1)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoForward(&ie_mock_, &loop_, 0)));

  // We have reached url 2 and have 1 back & 1 forward entries for url 1 & 3.
  // Go forward to url 3 now.
  ie_mock_.ExpectNavigation(in_cf, page2);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(page2)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoForward(&ie_mock_, &loop_, 0)));

  // We have reached url 2 and have 1 back & 1 forward entries for url 1 & 3.
  ie_mock_.ExpectNavigation(in_cf, page3);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(page3)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          CloseBrowserMock(&ie_mock_)));

  LaunchIEAndNavigate(page1);
}

// Test multiple back and forward operations among urls with anchors.
// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_P(FullTabNavigationTest, FLAKY_BackForwardAnchor) {
  bool in_cf = GetParam().invokes_cf();
  if (!in_cf) {
    LOG(ERROR) << "Test not yet implemented.";
    return;
  }
  server_mock_.ExpectAndServeAnyRequests(GetParam());
  InSequence expect_in_sequence_for_scope;

  // Navigate to anchor 1:
  // - First set focus to chrome renderer window
  //   Call WebBrowserEventSink::SetFocusToRenderer only once
  //   in the beginning. Calling it again will change focus from the
  //   current location to an element near the simulated mouse click.
  // - Then send keyboard input of TAB + ENTER to cause navigation.
  //   It's better to send input as PostDelayedTask since the Activex
  //   message loop_ on the other side might be blocked when we get
  //   called in Onload.
  // Back/Forward state at this point:
  // Back: 0
  // Forward: 0
  ie_mock_.ExpectNavigation(in_cf, GetAnchorPageUrl(0));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(0))))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          DelaySendString(&loop_, 500, tab_enter_keys)));

  // Navigate to anchor 2 after the previous navigation is complete
  // Back/Forward state at this point:
  // Back: 1 (kAnchorUrl)
  // Forward: 0
  ie_mock_.ExpectInPageNavigation(in_cf, GetAnchorPageUrl(1));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(1))))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelaySendString(&loop_, 500, tab_enter_keys)));

  // Navigate to anchor 3 after the previous navigation is complete
  // Back/Forward state at this point:
  // Back: 2 (kAnchorUrl, kAnchor1Url)
  // Forward: 0
  ie_mock_.ExpectInPageNavigation(in_cf, GetAnchorPageUrl(2));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(2))))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelaySendString(&loop_, 500, tab_enter_keys)));

  // We will reach anchor 3 once the navigation is complete,
  // then go back to anchor 2
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 0
  ie_mock_.ExpectInPageNavigation(in_cf, GetAnchorPageUrl(3));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(3))))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoBack(&ie_mock_, &loop_, 0)));

  // We will reach anchor 2 once the navigation is complete,
  // then go back to anchor 1
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 1 (kAnchor3Url)
  ie_mock_.ExpectNavigation(in_cf, GetAnchorPageUrl(2));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(2))))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoBack(&ie_mock_, &loop_, 0)));

  // We will reach anchor 1 once the navigation is complete,
  // now go forward to anchor 2
  // Back/Forward state at this point:
  // Back: 2 (kAnchorUrl, kAnchor1Url)
  // Forward: 2 (kAnchor2Url, kAnchor3Url)
  ie_mock_.ExpectNavigation(in_cf, GetAnchorPageUrl(1));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(1))))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoForward(&ie_mock_, &loop_, 0)));

  // We have reached anchor 2, go forward to anchor 3 again
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 1 (kAnchor3Url)
  ie_mock_.ExpectNavigation(in_cf, GetAnchorPageUrl(2));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(2))))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoForward(&ie_mock_, &loop_, 0)));

  // We have gone a few steps back and forward, this should be enough for now.
  ie_mock_.ExpectNavigation(in_cf, GetAnchorPageUrl(3));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(3))))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetAnchorPageUrl(0));
}

// Test that a user cannot navigate to a restricted site and that the security
// dialog appears.
TEST_P(FullTabNavigationTest, FLAKY_RestrictedSite) {
  if (!GetParam().invokes_cf() || GetInstalledIEVersion() == IE_8) {
    // Test has been disabled on IE8 bot because it hangs at times.
    // http://crbug.com/47596
    LOG(ERROR) << "Test disabled for this configuration.";
    return;
  }
  if (IsIBrowserServicePatchEnabled()) {
    LOG(ERROR) << "Not running test. IBrowserServicePatch is in place.";
    return;
  }
  MockWindowObserver win_observer_mock;
  ScopedComPtr<IInternetSecurityManager> security_manager;
  HRESULT hr = security_manager.CreateInstance(CLSID_InternetSecurityManager);
  ASSERT_HRESULT_SUCCEEDED(hr);
  // Add the server to restricted sites zone.
  hr = security_manager->SetZoneMapping(URLZONE_UNTRUSTED,
      GetTestUrl(L"").c_str(), SZM_CREATE);

  EXPECT_CALL(ie_mock_, OnFileDownload(_, _))
      .Times(testing::AnyNumber());
  server_mock_.ExpectAndServeAnyRequests(GetParam());

  ProtocolPatchMethod patch_method = GetPatchMethod();

  const wchar_t* kDialogClass = L"#32770";
  const char* kAlertDlgCaption = "Security Alert";

  EXPECT_CALL(ie_mock_, OnBeforeNavigate2(_,
      testing::Field(&VARIANT::bstrVal,
      testing::StrCaseEq(GetSimplePageUrl())), _, _, _, _, _))
      .Times(1)
      .WillOnce(WatchWindow(&win_observer_mock, kDialogClass));

  if (patch_method == PATCH_METHOD_INET_PROTOCOL) {
    EXPECT_CALL(ie_mock_, OnBeforeNavigate2(_,
        testing::Field(&VARIANT::bstrVal,
        testing::HasSubstr(L"res://")), _, _, _, _, _))
        .Times(testing::AtMost(1));
  }

  EXPECT_CALL(ie_mock_, OnNavigateComplete2(_,
      testing::Field(&VARIANT::bstrVal, StrEq(GetSimplePageUrl()))))
      .Times(testing::AtMost(1));

  EXPECT_CALL(win_observer_mock, OnWindowDetected(_, StrEq(kAlertDlgCaption)))
      .Times(1)
      .WillOnce(testing::DoAll(
          DoCloseWindow(),
          CloseBrowserMock(&ie_mock_)));

  LaunchIEAndNavigate(GetSimplePageUrl());

  ASSERT_HRESULT_SUCCEEDED(security_manager->SetZoneMapping(URLZONE_UNTRUSTED,
      GetTestUrl(L"").c_str(), SZM_DELETE));
}

// This test checks if window.open calls with target blank issued for a
// different domain make it back to IE instead of completing the navigation
// within Chrome. We validate this by initiating a navigation to a non existent
// url which ensures we would get an error during navigation.
// Marking this disabled as it leaves behind Chrome processes, at least on
// IE 6 XP (http://crbug.com/48732).
TEST_P(FullTabNavigationTest, DISABLED_JavascriptWindowOpenDifferentDomain) {
  if (!GetParam().invokes_cf() || GetInstalledIEVersion() == IE_7) {
    LOG(ERROR) << "Test disabled for this configuration.";
    return;
  }
  std::wstring parent_url =
      GetTestUrl(L"window_open.html?http://www.nonexistent.com");
  MockIEEventSink new_window_mock;
  ie_mock_.ExpectAnyNavigations();
  new_window_mock.ExpectAnyNavigations();
  server_mock_.ExpectAndServeAnyRequests(GetParam());

  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(), StrEq(parent_url)))
      .WillOnce(DelaySendDoubleClick(&ie_mock_, &loop_, 0, 10, 10));

  ie_mock_.ExpectNewWindow(&new_window_mock);

  EXPECT_CALL(new_window_mock, OnNavigateError(_, _, _, _, _))
      .Times(1)
      .WillOnce(CloseBrowserMock(&new_window_mock));

  EXPECT_CALL(new_window_mock, OnLoad(_, _))
      .Times(testing::AtMost(1));

  EXPECT_CALL(new_window_mock, OnQuit())
      .Times(1)
      .WillOnce(CloseBrowserMock(&ie_mock_));

  // OnNavigateError can take a long time to fire.
  LaunchIENavigateAndLoop(parent_url,
                          kChromeFrameLongNavigationTimeoutInSeconds * 4);
  ASSERT_TRUE(new_window_mock.event_sink()->web_browser2() != NULL);
}

// Tests that a page that calls window.open can then close the popup.
// Marking this test as FLAKY initially as it relies on getting focus and user
// input which don't work correctly at times.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_P(FullTabNavigationTest, FLAKY_JavascriptWindowOpenCanClose) {
  std::wstring parent_url =
      GetTestUrl(L"window_open.html?simple.html");
  MockIEEventSink new_window_mock;
  ie_mock_.ExpectAnyNavigations();
  new_window_mock.ExpectAnyNavigations();
  server_mock_.ExpectAndServeAnyRequests(GetParam());

  // Sending 'A' to the renderer should cause window.open then window.close.
  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(), StrEq(parent_url)))
      .WillOnce(DelaySendDoubleClick(&ie_mock_, &loop_, 0, 10, 10));

  ie_mock_.ExpectNewWindow(&new_window_mock);

  EXPECT_CALL(new_window_mock, OnLoad(_, StrEq(GetSimplePageUrl())))
      .Times(testing::AtMost(2))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          DelaySendChar(&loop_, 500, 'C', simulate_input::NONE)))
      .WillOnce(testing::Return());  // just to stop a gmock warning

  EXPECT_CALL(new_window_mock, OnQuit())
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(parent_url);
}

// Parameter for tests using the NavigationTransitionTest fixture. Includes two
// pages, each with their own possible CF invocation.
struct NavigationTransitionTestParameter {
  NavigationTransitionTestParameter(CFInvocation::Type type1,
                                    CFInvocation::Type type2) {
    page1_ = CFInvocation(type1);
    page2_ = CFInvocation(type2);
  }
  CFInvocation page1_;
  CFInvocation page2_;
};

// Parameterized test fixture for tests which test navigation transitions
// between two pages.
class NavigationTransitionTest
    : public MockIEEventSinkTest,
      public testing::TestWithParam<NavigationTransitionTestParameter> {
 public:
  NavigationTransitionTest() {}

  virtual void SetUp() {
    page1_ = GetParam().page1_;
    page2_ = GetParam().page2_;
  }

 protected:
  CFInvocation page1_;
  CFInvocation page2_;
};

// This instantiates each parameterized test with some of the different CF
// invocation methods.
// TODO(kkania): Do not allow these tests to be cache the pages. This is used
// currently because otherwise the meta tags can cause double requests. Change
// ExpectAndServeRequestAllowCache to ExpectAndServeRequest when allowed.
INSTANTIATE_TEST_CASE_P(
    IEToIE,
    NavigationTransitionTest,
    testing::Values(NavigationTransitionTestParameter(
        CFInvocation::NONE, CFInvocation::NONE)));
INSTANTIATE_TEST_CASE_P(
    IEToMetaTag,
    NavigationTransitionTest,
    testing::Values(NavigationTransitionTestParameter(
        CFInvocation::NONE, CFInvocation::META_TAG)));
INSTANTIATE_TEST_CASE_P(
    IEToHttpHeader,
    NavigationTransitionTest,
    testing::Values(NavigationTransitionTestParameter(
        CFInvocation::NONE, CFInvocation::HTTP_HEADER)));
INSTANTIATE_TEST_CASE_P(
    CFToCF,
    NavigationTransitionTest,
    testing::Values(NavigationTransitionTestParameter(
        CFInvocation::META_TAG, CFInvocation::META_TAG)));
INSTANTIATE_TEST_CASE_P(
    CFToIE,
    NavigationTransitionTest,
    testing::Values(NavigationTransitionTestParameter(
        CFInvocation::META_TAG, CFInvocation::NONE)));

// Test window.open calls.
// Marking this test as FLAKY initially as it relies on getting focus and user
// input which don't work correctly at times.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_P(NavigationTransitionTest, FLAKY_JavascriptWindowOpen) {
  std::wstring parent_url = GetTestUrl(L"window_open.html?simple.html");
  std::wstring new_window_url = GetSimplePageUrl();
  testing::StrictMock<MockIEEventSink> new_window_mock;

  ie_mock_.ExpectNavigation(page1_.invokes_cf(), parent_url);
  server_mock_.ExpectAndServeRequestAllowCache(page1_, parent_url);
  EXPECT_CALL(ie_mock_, OnLoad(page1_.invokes_cf(), StrEq(parent_url)))
      .WillOnce(DelaySendDoubleClick(&ie_mock_, &loop_, 0, 10, 10));

  // If the parent window is in CF, the child will load in CF regardless of
  // whether the child page invokes CF.
  bool expect_cf = page1_.invokes_cf() || page2_.invokes_cf();
  ie_mock_.ExpectNewWindow(&new_window_mock);
  new_window_mock.ExpectJavascriptWindowOpenNavigation(page1_.invokes_cf(),
                                                       expect_cf,
                                                       new_window_url);
  EXPECT_CALL(server_mock_, Get(_, UrlPathEq(new_window_url), _))
      .Times(testing::AtMost(1))
      .WillOnce(SendAllowCacheResponse(&server_mock_, page2_));
  EXPECT_CALL(new_window_mock, OnLoad(expect_cf, StrEq(new_window_url)))
      .WillOnce(testing::DoAll(
          ValidateWindowSize(&new_window_mock, 10, 10, 250, 250),
          CloseBrowserMock(&new_window_mock)));

  EXPECT_CALL(new_window_mock, OnQuit())
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIENavigateAndLoop(parent_url,
                          kChromeFrameLongNavigationTimeoutInSeconds * 2);
}

// Test redirection with window.location in Javascript.
// Disabled because crashes IE occasionally: http://crbug.com/48849.
TEST_P(NavigationTransitionTest, DISABLED_JavascriptRedirection) {
  std::wstring redirect_url = GetTestUrl(L"javascript_redirect.html");

  ie_mock_.ExpectNavigation(page1_.invokes_cf(), redirect_url);
  server_mock_.ExpectAndServeRequestAllowCache(page1_, redirect_url);
  EXPECT_CALL(ie_mock_, OnLoad(page1_.invokes_cf(), StrEq(redirect_url)))
      .WillOnce(VerifyAddressBarUrl(&ie_mock_));

  ie_mock_.ExpectNavigation(page2_.invokes_cf(), GetSimplePageUrl());
  server_mock_.ExpectAndServeRequestAllowCache(page2_, GetSimplePageUrl());
  EXPECT_CALL(ie_mock_, OnLoad(page2_.invokes_cf(), StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          CloseBrowserMock(&ie_mock_)));

  LaunchIEAndNavigate(redirect_url);
}

// Test following a link by TAB + ENTER.
TEST_P(NavigationTransitionTest, FLAKY_FollowLink) {
  if (page1_.invokes_cf() && page2_.invokes_cf() &&
      GetInstalledIEVersion() > IE_6) {
    // For some reason IE 7 and 8 send two BeforeNavigate events for the second
    // page for this case.
    LOG(ERROR) << "Test disabled for this configuration.";
    return;
  }
  ie_mock_.ExpectNavigation(page1_.invokes_cf(), GetLinkPageUrl());
  server_mock_.ExpectAndServeRequestAllowCache(page1_, GetLinkPageUrl());
  EXPECT_CALL(ie_mock_, OnLoad(page1_.invokes_cf(), StrEq(GetLinkPageUrl())))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          DelaySendChar(&loop_, 500, VK_TAB, simulate_input::NONE),
          DelaySendChar(&loop_, 1000, VK_RETURN, simulate_input::NONE)));

  ie_mock_.ExpectNavigation(page2_.invokes_cf(), GetSimplePageUrl());
  server_mock_.ExpectAndServeRequestAllowCache(page2_, GetSimplePageUrl());
  EXPECT_CALL(ie_mock_, OnLoad(page2_.invokes_cf(), StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          CloseBrowserMock(&ie_mock_)));

  LaunchIEAndNavigate(GetLinkPageUrl());
}

// Basic navigation test fixture which uses the MockIEEventSink. These tests
// are not parameterized.
class NavigationTest : public MockIEEventSinkTest, public testing::Test {
 public:
  NavigationTest() {}
};

// gMock matcher which tests if a url is blank.
MATCHER(BlankUrl, "is \"\" or NULL") {
  return arg == NULL || wcslen(arg) == 0;
}

// Test navigation to a disallowed url.
TEST_F(NavigationTest, DisallowedUrl) {
  // If a navigation fails then IE issues a navigation to an interstitial
  // page. Catch this to track navigation errors as the NavigateError
  // notification does not seem to fire reliably.
  const wchar_t disallowed_url[] = L"gcf:file:///C:/";

  EXPECT_CALL(ie_mock_, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          StrEq(disallowed_url)),
                                          _, _, _, _, _));
  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, BlankUrl()))
      .Times(testing::AtMost(1));
  EXPECT_CALL(ie_mock_, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StartsWith(L"res:")),
                                          _, _, _, _, _));
  EXPECT_CALL(ie_mock_, OnFileDownload(VARIANT_TRUE, _))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return());
  EXPECT_CALL(ie_mock_, OnNavigateComplete2(_, testing::Field(&VARIANT::bstrVal,
                                            StrEq(disallowed_url))));
  // Although we expect a load event for this, we should never receive a
  // corresponding GET request.
  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(disallowed_url)))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(disallowed_url);
}

// NOTE: This test is currently disabled as we haven't finished implementing
// support for this yet.  The test (as written) works fine for IE.  CF might
// have a different set of requirements once we fully support this and hence
// the test might need some refining before being enabled.
TEST_F(NavigationTest, DISABLED_DownloadInNewWindow) {
  MockIEEventSink new_window_mock;
  std::wstring kDownloadFromNewWin =
      GetTestUrl(L"full_tab_download_from_new_window.html");

  ie_mock_.ExpectNavigation(IN_CF, kDownloadFromNewWin);

  EXPECT_CALL(ie_mock_, OnNewWindow3(_, _, _, _, _));

  EXPECT_CALL(ie_mock_, OnNewBrowserWindow(_, _))
      .WillOnce(testing::WithArgs<0>(testing::Invoke(testing::CreateFunctor(
          &new_window_mock, &MockIEEventSink::Attach))));
  EXPECT_CALL(new_window_mock, OnBeforeNavigate2(_, _, _, _, _, _, _));

  EXPECT_CALL(new_window_mock, OnFileDownload(VARIANT_FALSE, _))
          .Times(2)
          .WillRepeatedly(CloseBrowserMock(&new_window_mock));

  EXPECT_CALL(new_window_mock, OnNavigateComplete2(_, _));

  EXPECT_CALL(new_window_mock, OnQuit()).WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(kDownloadFromNewWin);
}

TEST_P(FullTabNavigationTest, FLAKY_FormPostBackForward) {
  bool in_cf = GetParam().invokes_cf();
  // Navigate to the form-get.html page:
  // - First set focus to chrome renderer window
  // - Send over a character to the window.
  // - This should initiate a form post which eventually navigates to the
  //   action.html page.
  // Navigate backwards from the action.html page and then navigate forward
  // from the form-get.html page.

  std::wstring kFormPostUrl =
      GetTestUrl(L"form-get.html");

  std::wstring kFormPostActionUrl =
      GetTestUrl(L"action.html?field1=a&field2=b&submit=Submit");

  server_mock_.ExpectAndServeAnyRequests(GetParam());
  InSequence expect_in_sequence_for_scope;

  ie_mock_.ExpectNavigation(in_cf, kFormPostUrl);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(kFormPostUrl)))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          DelaySendChar(&loop_, 500, 'C', simulate_input::NONE)));

  ie_mock_.ExpectNavigation(in_cf, kFormPostActionUrl);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(kFormPostActionUrl)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoBack(&ie_mock_, &loop_, 0)));

  ie_mock_.ExpectNavigation(in_cf, kFormPostUrl);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(kFormPostUrl)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoForward(&ie_mock_, &loop_, 0)));

  ie_mock_.ExpectNavigation(in_cf, kFormPostActionUrl);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(kFormPostActionUrl)))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(kFormPostUrl);
}

TEST_P(FullTabNavigationTest, CF_UnloadEventTest) {
  bool in_cf = GetParam().invokes_cf();
  if (!in_cf) {
    LOG(ERROR) << "Test not yet implemented.";
    return;
  }

  std::wstring kUnloadEventTestUrl =
      GetTestUrl(L"fulltab_before_unload_event_test.html");

  std::wstring kUnloadEventMainUrl =
      GetTestUrl(L"fulltab_before_unload_event_main.html");

  server_mock_.ExpectAndServeAnyRequests(GetParam());
  InSequence expect_in_sequence_for_scope;

  ie_mock_.ExpectNavigation(in_cf, kUnloadEventTestUrl);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(kUnloadEventTestUrl)));

  ie_mock_.ExpectNavigation(in_cf, kUnloadEventMainUrl);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(kUnloadEventMainUrl)));

  EXPECT_CALL(ie_mock_, OnMessage(_, _, _))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(kUnloadEventTestUrl);
}

}  // namespace chrome_frame_test

