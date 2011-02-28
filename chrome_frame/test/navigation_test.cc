// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/scoped_comptr_win.h"
#include "base/test/test_file_util.h"
#include "base/win/windows_version.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/chrome_frame_ui_test_utils.h"
#include "chrome_frame/test/mock_ie_event_sink_actions.h"
#include "chrome_frame/test/mock_ie_event_sink_test.h"
#include "net/http/http_util.h"

// Needed for CreateFunctor.
#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

using testing::_;
using testing::InSequence;
using testing::StrEq;

namespace chrome_frame_test {

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
TEST_P(FullTabNavigationTest, TypeUrl) {
  MockAccEventObserver acc_observer;
  EXPECT_CALL(acc_observer, OnAccDocLoad(_)).Times(testing::AnyNumber());
  AccObjectMatcher address_matcher(L"Address*", L"editable text");
  AccObjectMatcher go_matcher(L"Go*", L"push button");

  ie_mock_.ExpectNavigation(IN_IE, GetSimplePageUrl());
  server_mock_.ExpectAndServeRequest(CFInvocation::None(), GetSimplePageUrl());
  // Enter the new url into the address bar.
  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          AccSetValueInBrowser(&ie_mock_, address_matcher, GetAnchorPageUrl(0)),
          AccWatchForOneValueChange(&acc_observer, address_matcher)));
  // Click the go button once the address has changed.
  EXPECT_CALL(acc_observer, OnAccValueChange(_, _, GetAnchorPageUrl(0)))
      .WillOnce(AccLeftClickInBrowser(&ie_mock_, go_matcher));

  bool in_cf = GetParam().invokes_cf();
  ie_mock_.ExpectNavigation(in_cf, GetAnchorPageUrl(0));
  server_mock_.ExpectAndServeRequest(GetParam(), GetAnchorPageUrl(0));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(0))))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// This tests navigation to a typed URL containing an fragment.
TEST_P(FullTabNavigationTest, TypeAnchorUrl) {
  MockAccEventObserver acc_observer;
  EXPECT_CALL(acc_observer, OnAccDocLoad(_)).Times(testing::AnyNumber());
  AccObjectMatcher address_matcher(L"Address*", L"editable text");
  AccObjectMatcher go_matcher(L"Go*", L"push button");

  ie_mock_.ExpectNavigation(IN_IE, GetSimplePageUrl());
  server_mock_.ExpectAndServeRequest(CFInvocation::None(), GetSimplePageUrl());

  // Enter the new url into the address bar.
  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          AccSetValueInBrowser(&ie_mock_, address_matcher, GetAnchorPageUrl(1)),
          AccWatchForOneValueChange(&acc_observer, address_matcher)));
  // Click the go button once the address has changed.
  EXPECT_CALL(acc_observer, OnAccValueChange(_, _, GetAnchorPageUrl(1)))
      .WillOnce(AccLeftClickInBrowser(&ie_mock_, go_matcher));

  bool in_cf = GetParam().invokes_cf();
  ie_mock_.ExpectNavigation(in_cf, GetAnchorPageUrl(1));
  server_mock_.ExpectAndServeRequest(GetParam(), GetAnchorPageUrl(1));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(1))))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Tests refreshing causes a page load.
TEST_P(FullTabNavigationTest, Refresh) {
  if (GetInstalledIEVersion() == IE_7) {
    LOG(ERROR) << "Test disabled for this configuration.";
    return;
  }
  bool in_cf = GetParam().invokes_cf();
  server_mock_.ExpectAndServeAnyRequests(GetParam());
  InSequence expect_in_sequence_for_scope;

  ie_mock_.ExpectNavigation(IN_IE, GetSimplePageUrl());
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetSimplePageUrl())))
      .WillOnce(DelayRefresh(&ie_mock_, &loop_, 0));

  if (in_cf) {
    EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetSimplePageUrl())))
        .WillOnce(CloseBrowserMock(&ie_mock_));
  } else {
    // For some reason IE still requests the resource again, but does not
    // trigger another load.
    EXPECT_CALL(server_mock_, Get(_, UrlPathEq(GetSimplePageUrl()), _))
        .WillOnce(CloseBrowserMock(&ie_mock_));
  }

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Test that multiple back and forward requests work.
TEST_P(FullTabNavigationTest, MultipleBackForward) {
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

  LaunchIENavigateAndLoop(page1,
                          kChromeFrameLongNavigationTimeoutInSeconds * 2);
}

// Test multiple back and forward operations among urls with anchors.
TEST_P(FullTabNavigationTest, BackForwardAnchor) {
  bool in_cf = GetParam().invokes_cf();
  ie_mock_.ExpectAnyNavigations();
  server_mock_.ExpectAndServeAnyRequests(GetParam());
  MockAccEventObserver acc_observer;

  // Navigate to anchor 1.
  // Back/Forward state at this point:
  // Back: 0
  // Forward: 0
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(0))))
      .Times(testing::AtMost(1));
  EXPECT_CALL(acc_observer, OnAccDocLoad(_))
      .WillOnce(AccDoDefaultAction(AccObjectMatcher(L"*1", L"link")))
      .WillRepeatedly(testing::Return());

  InSequence expect_in_sequence_for_scope;
  // Navigate to anchor 2 after the previous navigation is complete
  // Back/Forward state at this point:
  // Back: 1 (kAnchorUrl)
  // Forward: 0
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(1))))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          AccDoDefaultActionInRenderer(&ie_mock_,
                                       AccObjectMatcher(L"*2", L"link"))));

  // Navigate to anchor 3 after the previous navigation is complete
  // Back/Forward state at this point:
  // Back: 2 (kAnchorUrl, kAnchor1Url)
  // Forward: 0
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(2))))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          AccDoDefaultActionInRenderer(&ie_mock_,
                                       AccObjectMatcher(L"*3", L"link"))));

  // We will reach anchor 3 once the navigation is complete,
  // then go back to anchor 2
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 0
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(3))))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoBack(&ie_mock_, &loop_, 0)));

  // We will reach anchor 2 once the navigation is complete,
  // then go back to anchor 1
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 1 (kAnchor3Url)
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(2))))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoBack(&ie_mock_, &loop_, 0)));

  // We will reach anchor 1 once the navigation is complete,
  // now go forward to anchor 2
  // Back/Forward state at this point:
  // Back: 2 (kAnchorUrl, kAnchor1Url)
  // Forward: 2 (kAnchor2Url, kAnchor3Url)
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(1))))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoForward(&ie_mock_, &loop_, 0)));

  // We have reached anchor 2, go forward to anchor 3 again
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 1 (kAnchor3Url)
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(2))))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoForward(&ie_mock_, &loop_, 0)));

  // We have gone a few steps back and forward, this should be enough for now.
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(GetAnchorPageUrl(3))))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetAnchorPageUrl(0));
}

// Test that a user cannot navigate to a restricted site and that the security
// dialog appears.
TEST_P(FullTabNavigationTest, RestrictedSite) {
  // Add the server to restricted sites zone.
  ScopedComPtr<IInternetSecurityManager> security_manager;
  HRESULT hr = security_manager.CreateInstance(CLSID_InternetSecurityManager);
  ASSERT_HRESULT_SUCCEEDED(hr);
  hr = security_manager->SetZoneMapping(URLZONE_UNTRUSTED,
      GetTestUrl(L"").c_str(), SZM_CREATE);

  EXPECT_CALL(ie_mock_, OnFileDownload(_, _)).Times(testing::AnyNumber());
  server_mock_.ExpectAndServeAnyRequests(GetParam());

  MockWindowObserver win_observer_mock;

  // If the page is loaded in mshtml, then IE allows the page to be loaded
  // and just shows 'Restricted sites' in the status bar.
  if (!GetParam().invokes_cf()) {
    ie_mock_.ExpectNavigation(IN_IE, GetSimplePageUrl());
    EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(GetSimplePageUrl())))
        .Times(1)
        .WillOnce(CloseBrowserMock(&ie_mock_));
  } else {
    // If the page is being loaded in chrome frame then we will see
    // a security dialog.
    const char* kAlertDlgCaption = "Security Alert";
    win_observer_mock.WatchWindow(kAlertDlgCaption, "");

    EXPECT_CALL(ie_mock_, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
        testing::HasSubstr(GetSimplePageUrl())), _, _, _, _, _))
        .Times(testing::AtMost(2));

    EXPECT_CALL(ie_mock_, OnNavigateComplete2(_,
            testing::Field(&VARIANT::bstrVal, StrEq(GetSimplePageUrl()))))
        .Times(testing::AtMost(1));

    EXPECT_CALL(win_observer_mock, OnWindowOpen(_))
        .Times(1)
        .WillOnce(DoCloseWindow());
    EXPECT_CALL(win_observer_mock, OnWindowClose(_))
        .Times(1)
        .WillOnce(CloseBrowserMock(&ie_mock_));
  }

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
  MockAccEventObserver acc_observer;
  MockIEEventSink new_window_mock;
  ie_mock_.ExpectAnyNavigations();
  new_window_mock.ExpectAnyNavigations();
  server_mock_.ExpectAndServeAnyRequests(GetParam());

  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(), StrEq(parent_url)));
  EXPECT_CALL(acc_observer, OnAccDocLoad(_))
      .WillOnce(AccLeftClick(AccObjectMatcher()))
      .WillRepeatedly(testing::Return());

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

// Tests that the parent window can successfully close its popup through
// the javascript close method.
TEST_P(FullTabNavigationTest, JavascriptWindowOpenCanClose) {
  // Please see http://code.google.com/p/chromium/issues/detail?id=60987
  // for more information on why this test is disabled for Vista with IE7.
  if (base::win::GetVersion() == base::win::VERSION_VISTA &&
      GetInstalledIEVersion() == IE_7) {
    LOG(INFO) << "Not running test on Vista with IE7";
    return;
  }

  std::wstring parent_url = GetTestUrl(L"window_open.html?simple.html");
  MockAccEventObserver acc_observer;
  MockIEEventSink new_window_mock;
  ie_mock_.ExpectAnyNavigations();
  new_window_mock.ExpectAnyNavigations();
  server_mock_.ExpectAndServeAnyRequests(GetParam());

  // Tell the page to open the popup. Some versions of IE will prevent a popup
  // unless a click is involved.
  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(), StrEq(parent_url)));
  EXPECT_CALL(acc_observer, OnAccDocLoad(_))
      .WillOnce(AccLeftClick(AccObjectMatcher()))
      .WillRepeatedly(testing::Return());

  ie_mock_.ExpectNewWindow(&new_window_mock);
  EXPECT_CALL(new_window_mock, OnLoad(_, StrEq(GetSimplePageUrl())))
      .Times(testing::AtMost(2))
      .WillOnce(PostCharMessageToRenderer(&ie_mock_, 'C'))  // close the popup
      .WillOnce(testing::Return());

  EXPECT_CALL(new_window_mock, OnQuit())
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIENavigateAndLoop(parent_url,
                          kChromeFrameLongNavigationTimeoutInSeconds * 2);
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
TEST_P(NavigationTransitionTest, JavascriptWindowOpen) {
  // Please see http://code.google.com/p/chromium/issues/detail?id=60987
  // for more information on why this test is disabled for Vista with IE7.
  if (base::win::GetVersion() == base::win::VERSION_VISTA &&
      GetInstalledIEVersion() == IE_7) {
    LOG(INFO) << "Not running test on Vista with IE7";
    return;
  }

  std::wstring parent_url = GetTestUrl(L"window_open.html?simple.html");
  std::wstring new_window_url = GetSimplePageUrl();
  MockAccEventObserver acc_observer;
  testing::StrictMock<MockIEEventSink> new_window_mock;

  ie_mock_.ExpectNavigation(page1_.invokes_cf(), parent_url);
  server_mock_.ExpectAndServeRequest(page1_, parent_url);
  EXPECT_CALL(ie_mock_, OnLoad(page1_.invokes_cf(), StrEq(parent_url)));
  // Tell the page to open the popup. Some versions of IE will prevent a popup
  // unless a click is involved.
  EXPECT_CALL(acc_observer, OnAccDocLoad(_))
      .WillOnce(AccLeftClick(AccObjectMatcher()))
      .WillRepeatedly(testing::Return());

  // If the parent window is in CF, the child should always load in CF since
  // the domain is the same.
  bool expect_cf = page1_.invokes_cf() || page2_.invokes_cf();
  ie_mock_.ExpectNewWindow(&new_window_mock);
  new_window_mock.ExpectJavascriptWindowOpenNavigation(page1_.invokes_cf(),
                                                       expect_cf,
                                                       new_window_url);
  server_mock_.ExpectAndServeRequest(page2_, new_window_url);
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
  server_mock_.ExpectAndServeRequest(page1_, redirect_url);
  EXPECT_CALL(ie_mock_, OnLoad(page1_.invokes_cf(), StrEq(redirect_url)))
      .WillOnce(VerifyAddressBarUrl(&ie_mock_));

  ie_mock_.ExpectNavigation(page2_.invokes_cf(), GetSimplePageUrl());
  server_mock_.ExpectAndServeRequest(page2_, GetSimplePageUrl());
  EXPECT_CALL(ie_mock_, OnLoad(page2_.invokes_cf(), StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          CloseBrowserMock(&ie_mock_)));

  LaunchIEAndNavigate(redirect_url);
}

// Test following a link.
TEST_P(NavigationTransitionTest, FollowLink) {
  if (page1_.invokes_cf() && page2_.invokes_cf()) {
    // For some reason IE 7 and 8 send two BeforeNavigate events for the second
    // page for this case. All versions do not send the OnLoad event for the
    // second page if both pages are renderered in CF.
    LOG(ERROR) << "Test disabled for this configuration.";
    return;
  }
  MockAccEventObserver acc_observer;
  EXPECT_CALL(acc_observer, OnAccDocLoad(_)).Times(testing::AnyNumber());

  ie_mock_.ExpectNavigation(page1_.invokes_cf(), GetLinkPageUrl());
  // Two requests are made when going from CF to IE, at least on Win7 IE8.
  EXPECT_CALL(server_mock_, Get(_, UrlPathEq(GetLinkPageUrl()), _))
      .Times(testing::Between(1, 2))
      .WillRepeatedly(SendResponse(&server_mock_, page1_));
  EXPECT_CALL(ie_mock_, OnLoad(page1_.invokes_cf(), StrEq(GetLinkPageUrl())));
  EXPECT_CALL(acc_observer, OnAccDocLoad(_))
      .WillOnce(AccDoDefaultAction(AccObjectMatcher(L"", L"link")))
      .RetiresOnSaturation();

  ie_mock_.ExpectNavigation(page2_.invokes_cf(), GetSimplePageUrl());
  server_mock_.ExpectAndServeRequest(page2_, GetSimplePageUrl());
  EXPECT_CALL(ie_mock_, OnLoad(page2_.invokes_cf(), StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          CloseBrowserMock(&ie_mock_)));

  LaunchIEAndNavigate(GetLinkPageUrl());
}

// gMock matcher which tests if a url is blank.
MATCHER(BlankUrl, "is \"\" or NULL") {
  return arg == NULL || wcslen(arg) == 0;
}

// Basic navigation test fixture which uses the MockIEEventSink. These tests
// are not parameterized.
class NavigationTest : public MockIEEventSinkTest, public testing::Test {
 public:
  NavigationTest() {}

  void TestDisAllowedUrl(const wchar_t* url) {
    // If a navigation fails then IE issues a navigation to an interstitial
    // page. Catch this to track navigation errors as the NavigateError
    // notification does not seem to fire reliably.
    EXPECT_CALL(ie_mock_, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                            StrEq(url)),
                                            _, _, _, _, _));
    EXPECT_CALL(ie_mock_, OnLoad(IN_IE, BlankUrl()))
        .Times(testing::AtMost(1));
    EXPECT_CALL(ie_mock_, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                            testing::StartsWith(L"res:")),
                                            _, _, _, _, _));
    EXPECT_CALL(ie_mock_, OnFileDownload(VARIANT_TRUE, _))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return());
    EXPECT_CALL(ie_mock_, OnNavigateComplete2(_,
                                              testing::Field(&VARIANT::bstrVal,
                                              StrEq(url))));
    // Although we expect a load event for this, we should never receive a
    // corresponding GET request.
    EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(url)))
        .WillOnce(CloseBrowserMock(&ie_mock_));

    LaunchIEAndNavigate(url);
  }

};

// Test navigation to a disallowed gcf: url with file scheme.
TEST_F(NavigationTest, GcfProtocol1) {
  // Make sure that we are not accidently enabling gcf protocol.
  SetConfigBool(kAllowUnsafeURLs, false);
  TestDisAllowedUrl(L"gcf:file:///C:/");
}

// Test navigation to a disallowed gcf: url with http scheme.
TEST_F(NavigationTest, GcfProtocol2) {
  // Make sure that we are not accidently enabling gcf protocol.
  SetConfigBool(kAllowUnsafeURLs, false);
  TestDisAllowedUrl(L"gcf:http://www.google.com");
}

// Test navigation to a disallowed gcf: url with https scheme.
TEST_F(NavigationTest, GcfProtocol3) {
  // Make sure that we are not accidently enabling gcf protocol.
  SetConfigBool(kAllowUnsafeURLs, false);
  TestDisAllowedUrl(L"gcf:https://www.google.com");
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

TEST_P(FullTabNavigationTest, FormPostBackForward) {
  bool in_cf = GetParam().invokes_cf();
  // Navigate to the form-get.html page:
  // - First set focus to chrome renderer window
  // - Send over a character to the window.
  // - This should initiate a form post which eventually navigates to the
  //   action.html page.
  // Navigate backwards from the action.html page and then navigate forward
  // from the form-get.html page.
  std::wstring kFormPostUrl = GetTestUrl(L"form-get.html");
  std::wstring kFormPostActionUrl =
      GetTestUrl(L"action.html?field1=a&field2=b&submit=Submit");

  MockAccEventObserver acc_observer;
  server_mock_.ExpectAndServeAnyRequests(GetParam());

  EXPECT_CALL(acc_observer, OnAccDocLoad(_))
      .Times(testing::AtLeast(1))
      .WillOnce(AccDoDefaultAction(AccObjectMatcher(L"Submit")))
      .WillRepeatedly(testing::Return());

  InSequence expect_in_sequence_for_scope;

  ie_mock_.ExpectNavigation(in_cf, kFormPostUrl);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(kFormPostUrl)));

  ie_mock_.ExpectNavigationOptionalBefore(in_cf, kFormPostActionUrl);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(kFormPostActionUrl)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoBack(&ie_mock_, &loop_, 0)));

  ie_mock_.ExpectNavigation(in_cf, kFormPostUrl);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(kFormPostUrl)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          DelayGoForward(&ie_mock_, &loop_, 0)));

  ie_mock_.ExpectNavigationOptionalBefore(in_cf, kFormPostActionUrl);
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

  ie_mock_.ExpectNavigationOptionalBefore(in_cf, kUnloadEventMainUrl);
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(kUnloadEventMainUrl)));

  EXPECT_CALL(ie_mock_, OnMessage(_, _, _))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(kUnloadEventTestUrl);
}

// Fixture for ChromeFrame download tests.
class FullTabDownloadTest
    : public MockIEEventSinkTest, public testing::TestWithParam<CFInvocation> {
 public:
  FullTabDownloadTest() {}
};

void SaveOwnerWindow(HWND* owner_window, HWND window) {
  *owner_window = GetWindow(window, GW_OWNER);
}

void CloseWindow(HWND* window) {
  if (window)
    PostMessage(*window, WM_CLOSE, 0, 0);
}

// See bug http://crbug.com/36694
// This test does the following:-
// Navigates IE to a URL which in ChromeFrame.
// Performs a top level form post in the document
// In response to the POST we send over an attachment via the
// content-disposition header.
// IE brings up a file open dialog in this context.
// We bring up the Save dialog via accessibility and save the file
// and validate that all is well.
TEST_F(FullTabDownloadTest, CF_DownloadFileFromPost) {
  // Please see http://code.google.com/p/chromium/issues/detail?id=60987
  // for more information on why this test is disabled for Vista with IE7.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    if (GetInstalledIEVersion() == IE_7) {
      LOG(INFO) << "Not running test on Vista with IE7";
      return;
    } else if (GetInstalledIEVersion() == IE_9) {
      LOG(INFO) << "Not running test on Vista/Windows 7 with IE9";
      return;
    }
  }

  chrome_frame_test::MockWindowObserver download_watcher;
  download_watcher.WatchWindow("File Download", "");

  chrome_frame_test::MockWindowObserver save_dialog_watcher;
  save_dialog_watcher.WatchWindow("Save As", "");

  testing::StrictMock<MockIEEventSink> download_window_mock;

  EXPECT_CALL(server_mock_, Get(_, StrEq(L"/post_source.html"), _)).WillOnce(
    SendFast(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n",
      "<html>"
      "<head><meta http-equiv=\"x-ua-compatible\" content=\"chrome=1\" />"
      " <script type=\"text/javascript\">"
      " function onLoad() {"
      " document.getElementById(\"myform\").submit();}</script></head>"
      " <body onload=\"setTimeout(onLoad, 2000);\">"
      " <form id=\"myform\" action=\"post_target.html\" method=\"POST\">"
      "</form></body></html>"));

  EXPECT_CALL(server_mock_, Post(_, StrEq(L"/post_target.html"), _))
    .Times(2)
    .WillRepeatedly(
      SendFast(
        "HTTP/1.1 200 OK\r\n"
        "content-disposition: attachment;filename=\"hello.txt\"\r\n"
        "Content-Type: application/text\r\n"
        "Cache-Control: private\r\n",
        "hello"));

  // If you want to debug this action then you may need to
  // SendMessage(parent_window, WM_NCACTIVATE, TRUE, 0);
  // SendMessage(parent_window, WM_COMMAND, MAKEWPARAM(0x114B, BN_CLICKED),
  //             control_window);
  // For the uninitiated, please debug IEFrame!CDialogActivateGuard::*
  EXPECT_CALL(download_watcher, OnWindowOpen(_))
      .Times(2)
      .WillOnce(DelayAccDoDefaultAction(
          AccObjectMatcher(L"Save", L"push button"),
          1000))
      .WillOnce(testing::Return());

  EXPECT_CALL(download_watcher, OnWindowClose(_))
      .Times(testing::AnyNumber());

  std::wstring src_url = server_mock_.Resolve(L"/post_source.html");
  std::wstring tgt_url = server_mock_.Resolve(L"/post_target.html");

  EXPECT_CALL(ie_mock_, OnFileDownload(_, _)).Times(testing::AnyNumber());

  EXPECT_CALL(ie_mock_, OnBeforeNavigate2(_,
                              testing::Field(&VARIANT::bstrVal,
                              StrEq(src_url)), _, _, _, _, _));
  EXPECT_CALL(ie_mock_, OnNavigateComplete2(_,
                              testing::Field(&VARIANT::bstrVal,
                              StrEq(src_url))));
  EXPECT_CALL(ie_mock_, OnLoad(true, StrEq(src_url)))
      .Times(testing::AnyNumber());

  ie_mock_.ExpectNewWindow(&download_window_mock);
  EXPECT_CALL(ie_mock_, OnLoadError(StrEq(tgt_url)))
      .Times(testing::AnyNumber());

  EXPECT_CALL(download_window_mock, OnFileDownload(_, _))
    .Times(testing::AnyNumber());
  EXPECT_CALL(download_window_mock, OnLoadError(StrEq(tgt_url)))
    .Times(testing::AtMost(1));
  EXPECT_CALL(download_window_mock, OnBeforeNavigate2(_,
                              testing::Field(&VARIANT::bstrVal,
                              StrEq(tgt_url)), _, _, _, _, _));
  EXPECT_CALL(download_window_mock, OnLoad(false, _));
  EXPECT_CALL(download_window_mock, OnQuit()).Times(testing::AtMost(1));

  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFile(&temp_file_path));
  file_util::DieFileDie(temp_file_path, false);

  temp_file_path = temp_file_path.ReplaceExtension(L"txt");
  file_util::DieFileDie(temp_file_path, false);

  AccObjectMatcher file_name_box(L"File name:", L"editable text");

  HWND owner_window = NULL;

  EXPECT_CALL(save_dialog_watcher, OnWindowOpen(_))
        .WillOnce(testing::DoAll(
            testing::Invoke(testing::CreateFunctor(
                SaveOwnerWindow, &owner_window)),
            AccSendCharMessage(file_name_box, L'a'),
            AccSetValue(file_name_box, temp_file_path.value()),
            AccDoDefaultAction(AccObjectMatcher(L"Save", L"push button"))));

  EXPECT_CALL(save_dialog_watcher, OnWindowClose(_))
        .WillOnce(testing::DoAll(
            WaitForFileSave(temp_file_path, 2000),
            testing::InvokeWithoutArgs(
                testing::CreateFunctor(CloseWindow, &owner_window)),
            CloseBrowserMock(&ie_mock_)));
  LaunchIENavigateAndLoop(src_url, kChromeFrameLongNavigationTimeoutInSeconds);

  std::string data;
  EXPECT_TRUE(file_util::ReadFileToString(temp_file_path, &data));
  EXPECT_EQ("hello", data);
  file_util::DieFileDie(temp_file_path, false);
}

// Test fixture for testing if http header works for supported content types
class HttpHeaderTest : public MockIEEventSinkTest, public testing::Test {
 public:
  HttpHeaderTest() {}

  void HeaderTestWithData(const char* content_type, const char* data) {
    const wchar_t* relative_url = L"/header_test";
    const char* kHeaderFormat =
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: %s\r\n"
        "X-UA-Compatible: chrome=1\r\n";
    std::string header = StringPrintf(kHeaderFormat, content_type);
    std::wstring url = server_mock_.Resolve(relative_url);
    EXPECT_CALL(server_mock_, Get(_, StrEq(relative_url), _))
        .WillRepeatedly(SendFast(header, data));

    InSequence expect_in_sequence_for_scope;

    ie_mock_.ExpectNavigation(IN_CF, url);
    EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(url)))
        .WillOnce(CloseBrowserMock(&ie_mock_));

    LaunchIEAndNavigate(url);
  }
};

const char* kXmlContent =
  "<tree>"
    "<node href=\"root.htm\" text=\"Root\">"
      "<node href=\"child1.htm\" text=\"Child 1\" />"
      "<node href=\"child2.htm\" text=\"Child 2\" />"
    "</node>"
  "</tree>";

TEST_F(HttpHeaderTest, ApplicationXhtml) {
  HeaderTestWithData("application/xhtml+xml", kXmlContent);
}

TEST_F(HttpHeaderTest, ApplicationXml) {
  HeaderTestWithData("application/xml", kXmlContent);
}

TEST_F(HttpHeaderTest, TextXml) {
  HeaderTestWithData("text/xml", kXmlContent);
}

const char* kImageSvg =
  "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" "
      "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">"
  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100%\" height=\"100%\">"
    "<rect height=\"100\" width=\"300\" "
        "style=\"fill:rgb(0,0,255);stroke-width:2;\"/>"
  "</svg>";

TEST_F(HttpHeaderTest, DISABLED_ImageSvg) {
  HeaderTestWithData("image/svg", kImageSvg);
}

TEST_F(HttpHeaderTest, ImageSvgXml) {
  HeaderTestWithData("image/svg+xml", kImageSvg);
}

// Tests refreshing causes a page load.
TEST_P(FullTabNavigationTest, RefreshContents) {
  bool in_cf = GetParam().invokes_cf();
  if (!in_cf) {
    VLOG(1) << "Disabled for this configuration";
    return;
  }

  const char kHeaders[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
                          "X-UA-Compatible: chrome=1\r\n";

  const char kBody[] =  "<html><body>Hi there. Got new content?"
                        "</body></html>";

  std::wstring src_url = server_mock_.Resolve(L"/refresh_src.html");

  EXPECT_CALL(server_mock_, Get(_, StrEq(L"/refresh_src.html"), _))
      .Times(2)
      .WillOnce(SendFast(kHeaders, kBody))
      .WillOnce(testing::DoAll(
          SendFast(kHeaders, kBody),
          DelayCloseBrowserMock(&loop_, 4000, &ie_mock_)));

  EXPECT_CALL(ie_mock_, OnFileDownload(_, _)).Times(testing::AnyNumber());

  EXPECT_CALL(ie_mock_,
              OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                                  StrEq(src_url)),
                                _, _, _, _, _));
  EXPECT_CALL(ie_mock_,
              OnNavigateComplete2(_, testing::Field(&VARIANT::bstrVal,
                                                    StrEq(src_url))));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(src_url)))
      .Times(2)
      .WillOnce(DelayRefresh(&ie_mock_, &loop_, 50))
      .WillOnce(testing::Return());

  LaunchIENavigateAndLoop(src_url,
                          kChromeFrameVeryLongNavigationTimeoutInSeconds);
}

class FullTabSeleniumTest
    : public MockIEEventSinkTest, public testing::TestWithParam<CFInvocation> {
 public:
  FullTabSeleniumTest()
      : MockIEEventSinkTest(1337, L"127.0.0.1", GetSeleniumTestFolder()) {}
};

ACTION(VerifySeleniumCoreTestResults) {
  int num_tests = 0;
  int failed_tests = 0;

  swscanf(arg0, L"%d/%d", &num_tests, &failed_tests);

  // Currently we run total 505 tests and 8 steps fail.
  // TODO(amit): send results as JSON, diagnose and eliminate failures.
  EXPECT_LE(failed_tests, 15) << "Expected failures: " << 15 <<
      " Actual failures: " << failed_tests;
  EXPECT_GE(num_tests, 500) << "Expected to run: " << 500 << " tests." <<
      " Actual number of tests run: " << num_tests;
}

// Tests refreshing causes a page load.
TEST_F(FullTabSeleniumTest, Core) {
  // Please see http://code.google.com/p/chromium/issues/detail?id=60987
  // for more information on why this test is disabled for Vista with IE7.
  if (base::win::GetVersion() == base::win::VERSION_VISTA &&
      GetInstalledIEVersion() == IE_7) {
    LOG(INFO) << "Not running test on Vista with IE7";
    return;
  }

  server_mock_.ExpectAndServeAnyRequests(CFInvocation::HttpHeader());
  std::wstring url = GetTestUrl(L"core/TestRunner.html");

  // Expectations for TestRunner.html
  EXPECT_CALL(ie_mock_, OnFileDownload(_, _)).Times(testing::AnyNumber());
  EXPECT_CALL(ie_mock_, OnBeforeNavigate2(_,
                              testing::Field(&VARIANT::bstrVal,
                              testing::StartsWith(url)), _, _, _, _, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(ie_mock_, OnNavigateComplete2(_,
                              testing::Field(&VARIANT::bstrVal,
                              testing::StartsWith(url))))
      .Times(testing::AnyNumber());
  EXPECT_CALL(ie_mock_, OnLoad(true, testing::StartsWith(url)))
      .Times(testing::AnyNumber());

  // Expectation for cookie test
  EXPECT_CALL(ie_mock_, OnLoadError(testing::StartsWith(url)))
    .Times(testing::AtMost(3));

  // Expectations for popups
  std::wstring attach_url_prefix = GetTestUrl(L"?attach_external_tab&");
  EXPECT_CALL(ie_mock_, OnNewWindow3(_, _, _, _,
                            testing::StartsWith(attach_url_prefix)))
      .Times(testing::AnyNumber());
  EXPECT_CALL(ie_mock_, OnNewBrowserWindow(_,
                            testing::StartsWith(attach_url_prefix)))
      .Times(testing::AnyNumber());

  // At the end the tests will post us a message.  See _onTestSuiteComplete in
  // ...\src\data\selenium_core\core\scripts\selenium-testrunner.js
  EXPECT_CALL(ie_mock_, OnMessage(_, _, _))
      .WillOnce(testing::DoAll(VerifySeleniumCoreTestResults(),
                               CloseBrowserMock(&ie_mock_)));

  // Selenium tests take longer to finish, lets give it 2 mins.
  const int kSeleniumTestTimeout = 120;
  LaunchIENavigateAndLoop(url, kSeleniumTestTimeout);
}

// See bug http://code.google.com/p/chromium/issues/detail?id=64901
// This test does the following:-
// Navigates IE to a non ChromeFrame URL.
// Performs a top level form post in the document
// In response to the POST send over a html document containing a meta tag
// This would cause IE to switch to ChromeFrame.
// Refresh the page in ChromeFrame.
// This should bring up a confirmation dialog which we hit yes on. This should
// reissue the top level post request in response to which the html content
// containing the meta tag is sent again.
TEST_F(FullTabDownloadTest, TopLevelPostReissueFromChromeFramePage) {
  chrome_frame_test::MockWindowObserver post_reissue_watcher;
  post_reissue_watcher.WatchWindow("Confirm Form Resubmission", "");

  EXPECT_CALL(server_mock_, Get(_, StrEq(L"/post_source.html"), _))
    .WillOnce(SendFast(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n",
        "<html>"
        "<head>"
        " <script type=\"text/javascript\">"
        " function onLoad() {"
        " document.getElementById(\"myform\").submit();}</script></head>"
        " <body onload=\"setTimeout(onLoad, 2000);\">"
        " <form id=\"myform\" action=\"post_target.html\" method=\"POST\">"
        "</form></body></html>"));

  EXPECT_CALL(server_mock_, Post(_, StrEq(L"/post_target.html"), _))
    .Times(2)
    .WillRepeatedly(
        SendFast(
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/html\r\n",
          "<html>"
          "<head><meta http-equiv=\"x-ua-compatible\" content=\"chrome=1\" />"
          "</head>"
          "<body> Target page in ChromeFrame </body>"
          "</html>"));

  EXPECT_CALL(post_reissue_watcher, OnWindowOpen(_))
      .WillOnce(DelayAccDoDefaultAction(
          AccObjectMatcher(L"Yes", L"push button"),
          1000));

  EXPECT_CALL(post_reissue_watcher, OnWindowClose(_));

  std::wstring src_url = server_mock_.Resolve(L"/post_source.html");
  std::wstring tgt_url = server_mock_.Resolve(L"/post_target.html");

  EXPECT_CALL(ie_mock_, OnFileDownload(_, _)).Times(testing::AnyNumber());

  EXPECT_CALL(ie_mock_, OnBeforeNavigate2(_,
                              testing::Field(&VARIANT::bstrVal,
                              StrEq(src_url)), _, _, _, _, _));
  EXPECT_CALL(ie_mock_, OnNavigateComplete2(_,
                              testing::Field(&VARIANT::bstrVal,
                              StrEq(src_url))));
  EXPECT_CALL(ie_mock_, OnLoad(false, StrEq(src_url)));

  EXPECT_CALL(ie_mock_, OnLoad(true, StrEq(tgt_url)))
      .Times(testing::Between(1,2))
      .WillOnce(DelayRefresh(&ie_mock_, &loop_, 50))
      .WillOnce(testing::Return());

  EXPECT_CALL(ie_mock_, OnBeforeNavigate2(_,
                              testing::Field(&VARIANT::bstrVal,
                              StrEq(tgt_url)), _, _, _, _, _))
      .Times(2);

  EXPECT_CALL(ie_mock_, OnNavigateComplete2(_,
                              testing::Field(&VARIANT::bstrVal,
                              StrEq(tgt_url))))
      .Times(2)
      .WillOnce(DelayCloseBrowserMock(&loop_, 4000, &ie_mock_))
      .WillOnce(testing::Return());

  LaunchIENavigateAndLoop(src_url,
                          kChromeFrameVeryLongNavigationTimeoutInSeconds);
}

MATCHER_P(UserAgentHeaderMatcher, ua_string, "") {
  std::string headers = arg.headers();
  StringToUpperASCII(&headers);

  std::string ua_string_to_search = ua_string;
  StringToUpperASCII(&ua_string_to_search);

  net::HttpUtil::HeadersIterator it(headers.begin(), headers.end(),
                                    "\r\n");
  while (it.GetNext()) {
    if (lstrcmpiA(it.name().c_str(), "User-Agent") == 0) {
      if (it.values().find(ua_string_to_search) != std::string::npos)
        return true;
    }
  }
  return false;
}

// Tests refreshing causes a page load and that the chrome frame user agent
// string is appended to the UA in the incoming top level HTTP requests.
TEST_P(FullTabNavigationTest, RefreshContentsUATest) {
  const char kBody[] = "<html><head></head>"
                       "<body>Hi there. Got new content?"
                       "</body></html>";

  std::string headers = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
  bool in_cf = GetParam().invokes_cf();
  if (in_cf) {
    headers.append("X-UA-Compatible: chrome=1\r\n");
  } else {
    if (GetInstalledIEVersion() == IE_9) {
      LOG(ERROR) << "Test disabled for IE9";
      return;
    }
  }

  EXPECT_CALL(server_mock_, Get(_, testing::StrCaseEq(L"/favicon.ico"), _))
      .Times(testing::AtMost(2))
      .WillRepeatedly(SendFast("HTTP/1.1 404 Not Found", ""));

  std::wstring src_url = server_mock_.Resolve(L"/refresh_src.html");

  EXPECT_CALL(server_mock_, Get(_, StrEq(L"/refresh_src.html"),
                                UserAgentHeaderMatcher("chromeframe")))
      .Times(2)
      .WillOnce(SendFast(headers, kBody))
      .WillOnce(testing::DoAll(
          SendFast(headers, kBody),
          DelayCloseBrowserMock(&loop_, 4000, &ie_mock_)));

  EXPECT_CALL(ie_mock_, OnFileDownload(_, _)).Times(testing::AnyNumber());

  EXPECT_CALL(ie_mock_,
              OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                                  StrEq(src_url)),
                                _, _, _, _, _));
  EXPECT_CALL(ie_mock_,
              OnNavigateComplete2(_, testing::Field(&VARIANT::bstrVal,
                                                    StrEq(src_url))));
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(src_url)))
      .Times(testing::Between(1, 2))
      .WillOnce(DelayRefresh(&ie_mock_, &loop_, 50))
      .WillOnce(testing::Return());

  LaunchIENavigateAndLoop(src_url,
                          kChromeFrameVeryLongNavigationTimeoutInSeconds);
}

// Link navigations in the same domain specified with the noreferrer flag
// should be opened in the host browser.
TEST_F(FullTabNavigationTest, JavascriptWindowOpenNoReferrerOpensInHost) {
  // Please see http://code.google.com/p/chromium/issues/detail?id=60987
  // for more information on why this test is disabled for Vista with IE7.
  if (base::win::GetVersion() == base::win::VERSION_VISTA &&
      GetInstalledIEVersion() == IE_7) {
    LOG(INFO) << "Not running test on Vista with IE7";
    return;
  }

  MockAccEventObserver acc_observer;

  testing::StrictMock<MockIEEventSink> new_window_mock;
  testing::StrictMock<MockIEEventSink>
      no_referrer_target_opener_window_mock;

  std::wstring initial_url =
      GetTestUrl(L"window_open.html?open_href_target_no_referrer.html");

  std::wstring parent_url = GetTestUrl(
      L"open_href_target_no_referrer.html");

  std::wstring new_window_url = GetSimplePageUrl();

  ie_mock_.ExpectNavigation(false, initial_url);
  EXPECT_CALL(ie_mock_, OnLoad(false, StrEq(initial_url)));

  EXPECT_CALL(acc_observer, OnAccDocLoad(_))
      .WillOnce(AccLeftClick(AccObjectMatcher()))
      .WillRepeatedly(testing::Return());

  ie_mock_.ExpectNewWindow(&no_referrer_target_opener_window_mock);

  no_referrer_target_opener_window_mock.ExpectNavigation(true, parent_url);

  server_mock_.ExpectAndServeRequest(CFInvocation::MetaTag(), parent_url);
  server_mock_.ExpectAndServeRequest(CFInvocation::None(), new_window_url);
  server_mock_.ExpectAndServeRequest(CFInvocation::None(), initial_url);

  EXPECT_CALL(no_referrer_target_opener_window_mock,
      OnLoad(false, StrEq(parent_url)))
      .Times(testing::AnyNumber());

  EXPECT_CALL(no_referrer_target_opener_window_mock,
      OnLoad(true, StrEq(parent_url)))
      .WillOnce(DelayAccDoDefaultActionInRenderer(
          &no_referrer_target_opener_window_mock,
          AccObjectMatcher(L"", L"link"), 1000));

  // The parent window is in CF and opens a child window with the no referrer
  // flag in which case it should open in IE.
  no_referrer_target_opener_window_mock.ExpectNewWindow(&new_window_mock);
  new_window_mock.ExpectNavigation(false, new_window_url);

  EXPECT_CALL(new_window_mock, OnFileDownload(_, _))
      .Times(testing::AnyNumber());

  EXPECT_CALL(new_window_mock,
              OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                              testing::HasSubstr(L"attach")),
                                _, _, _, _, _));
  EXPECT_CALL(new_window_mock,
              OnNavigateComplete2(_, testing::Field(&VARIANT::bstrVal,
                                              testing::HasSubstr(L"attach"))))
      .Times(testing::AtMost(1));

  EXPECT_CALL(new_window_mock, OnLoad(false, StrEq(new_window_url)))
      .WillOnce(CloseBrowserMock(&new_window_mock));

  EXPECT_CALL(new_window_mock, OnQuit())
      .WillOnce(CloseBrowserMock(
          &no_referrer_target_opener_window_mock));

  EXPECT_CALL(no_referrer_target_opener_window_mock, OnQuit())
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIENavigateAndLoop(initial_url,
                          kChromeFrameLongNavigationTimeoutInSeconds);
}

}  // namespace chrome_frame_test
