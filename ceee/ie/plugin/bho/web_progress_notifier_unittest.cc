// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit test for WebProgressNotifier class.
#include "ceee/ie/plugin/bho/web_progress_notifier.h"

#include "ceee/ie/plugin/bho/web_browser_events_source.h"
#include "ceee/ie/plugin/bho/window_message_source.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/test_utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::_;
using testing::AddRef;
using testing::AnyNumber;
using testing::InSequence;
using testing::MockFunction;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::StrictMock;

class FakeWebBrowserEventsSource : public WebBrowserEventsSource {
 public:
  FakeWebBrowserEventsSource() : sink_(NULL) {}
  virtual ~FakeWebBrowserEventsSource() {
    EXPECT_EQ(NULL, sink_);
  }

  virtual void RegisterSink(Sink* sink) {
    ASSERT_TRUE(sink != NULL && sink_ == NULL);
    sink_ = sink;
  }

  virtual void UnregisterSink(Sink* sink) {
    ASSERT_TRUE(sink == sink_);
    sink_ = NULL;
  }

  void FireOnBeforeNavigate(IWebBrowser2* browser, BSTR url) {
    if (sink_ != NULL)
      sink_->OnBeforeNavigate(browser, url);
  }
  void FireOnDocumentComplete(IWebBrowser2* browser, BSTR url) {
    if (sink_ != NULL)
      sink_->OnDocumentComplete(browser, url);
  }
  void FireOnNavigateComplete(IWebBrowser2* browser, BSTR url) {
    if (sink_ != NULL)
      sink_->OnNavigateComplete(browser, url);
  }
  void FireOnNavigateError(IWebBrowser2* browser, BSTR url, long status_code) {
    if (sink_ != NULL)
      sink_->OnNavigateError(browser, url, status_code);
  }
  void FireOnNewWindow(BSTR url_context, BSTR url) {
    if (sink_ != NULL)
      sink_->OnNewWindow(url_context, url);
  }

 private:
  Sink* sink_;
};

class FakeWindowMessageSource : public WindowMessageSource {
 public:
  FakeWindowMessageSource() : sink_(NULL) {}
  virtual ~FakeWindowMessageSource() {
    EXPECT_EQ(NULL, sink_);
  }

  virtual void RegisterSink(Sink* sink) {
    ASSERT_TRUE(sink != NULL && sink_ == NULL);
    sink_ = sink;
  }

  virtual void UnregisterSink(Sink* sink) {
    ASSERT_TRUE(sink == sink_);
    sink_ = NULL;
  }

  void FireOnHandleMessage(MessageType type,
                           const MSG* message_info) {
    if (sink_ != NULL)
      sink_->OnHandleMessage(type, message_info);
  }

 private:
  Sink* sink_;
};

class TestWebProgressNotifier : public WebProgressNotifier {
 public:
  TestWebProgressNotifier()
      : mock_is_forward_back_(false),
        mock_in_onload_event_(false),
        mock_is_meta_refresh_(false),
        mock_has_potential_javascript_redirect_(false),
        mock_is_possible_user_action_in_content_window_(false),
        mock_is_possible_user_action_in_browser_ui_(false) {}

  virtual WebNavigationEventsFunnel* webnavigation_events_funnel() {
    return &mock_webnavigation_events_funnel_;
  }

  virtual WindowMessageSource* CreateWindowMessageSource() {
    return new FakeWindowMessageSource();
  }

  virtual bool IsForwardBack(BSTR /*url*/) { return mock_is_forward_back_; }
  virtual bool InOnLoadEvent(IWebBrowser2* /*browser*/) {
    return mock_in_onload_event_;
  }
  virtual bool IsMetaRefresh(IWebBrowser2* /*browser*/, BSTR /*url*/) {
    return mock_is_meta_refresh_;
  }
  virtual bool HasPotentialJavaScriptRedirect(IWebBrowser2* /*browser*/) {
    return mock_has_potential_javascript_redirect_;
  }
  virtual bool IsPossibleUserActionInContentWindow() {
    return mock_is_possible_user_action_in_content_window_;
  }
  virtual bool IsPossibleUserActionInBrowserUI() {
    return mock_is_possible_user_action_in_browser_ui_;
  }

  bool CallRealIsForwardBack(BSTR url) {
    return WebProgressNotifier::IsForwardBack(url);
  }

  TravelLogInfo& previous_travel_log_info() {
    return previous_travel_log_info_;
  }

  StrictMock<testing::MockWebNavigationEventsFunnel>
      mock_webnavigation_events_funnel_;
  bool mock_is_forward_back_;
  bool mock_in_onload_event_;
  bool mock_is_meta_refresh_;
  bool mock_has_potential_javascript_redirect_;
  bool mock_is_possible_user_action_in_content_window_;
  bool mock_is_possible_user_action_in_browser_ui_;
};

class WebProgressNotifierTestFixture : public testing::Test {
 protected:
  WebProgressNotifierTestFixture() : mock_web_browser_(NULL),
                                     mock_travel_log_stg_(NULL) {
  }

  virtual void SetUp() {
    CComObject<testing::MockIWebBrowser2>::CreateInstance(
        &mock_web_browser_);
    ASSERT_TRUE(mock_web_browser_ != NULL);
    web_browser_ = mock_web_browser_;

    CComObject<testing::MockITravelLogStg>::CreateInstance(
        &mock_travel_log_stg_);
    ASSERT_TRUE(mock_travel_log_stg_ != NULL);
    travel_log_stg_ = mock_travel_log_stg_;

    // We cannot use CopyInterfaceToArgument here because QueryService takes
    // void** as argument.
    EXPECT_CALL(*mock_web_browser_,
                QueryService(SID_STravelLogCursor, IID_ITravelLogStg,
                             NotNull()))
        .WillRepeatedly(DoAll(SetArgumentPointee<2>(travel_log_stg_.p),
                              AddRef(travel_log_stg_.p),
                              Return(S_OK)));

    web_browser_events_source_.reset(new FakeWebBrowserEventsSource());

    web_progress_notifier_.reset(new TestWebProgressNotifier());
    ASSERT_HRESULT_SUCCEEDED(web_progress_notifier_->Initialize(
        web_browser_events_source_.get(), NULL, reinterpret_cast<HWND>(1024),
        web_browser_));
  }

  virtual void TearDown() {
    web_progress_notifier_->TearDown();
    web_progress_notifier_.reset(NULL);
    web_browser_events_source_.reset(NULL);
    mock_web_browser_ = NULL;
    web_browser_.Release();
    mock_travel_log_stg_ = NULL;
    travel_log_stg_.Release();
  }

  void IgnoreCallsToEventsFunnelExceptOnCommitted() {
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnBeforeNavigate(_, _, _, _, _))
        .Times(AnyNumber());
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnBeforeRetarget(_, _, _, _))
        .Times(AnyNumber());
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCompleted(_, _, _, _))
        .Times(AnyNumber());
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnDOMContentLoaded(_, _, _, _))
        .Times(AnyNumber());
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnErrorOccurred(_, _, _, _, _))
        .Times(AnyNumber());
  }

  void FireNavigationEvents(IWebBrowser2* browser, BSTR url) {
    web_browser_events_source_->FireOnBeforeNavigate(browser, url);
    web_browser_events_source_->FireOnNavigateComplete(browser, url);
    web_browser_events_source_->FireOnDocumentComplete(browser, url);
  }

  HRESULT CreateMockWebBrowser(IWebBrowser2** web_browser) {
    EXPECT_TRUE(web_browser != NULL);
    CComObject<testing::MockIWebBrowser2>* mock_web_browser = NULL;
    CComObject<testing::MockIWebBrowser2>::CreateInstance(&mock_web_browser);
    EXPECT_TRUE(mock_web_browser != NULL);

    *web_browser = mock_web_browser;
    (*web_browser)->AddRef();
    return S_OK;
  }

  CComObject<testing::MockIWebBrowser2>* mock_web_browser_;
  CComPtr<IWebBrowser2> web_browser_;
  CComObject<testing::MockITravelLogStg>* mock_travel_log_stg_;
  CComPtr<ITravelLogStg> travel_log_stg_;
  scoped_ptr<FakeWebBrowserEventsSource> web_browser_events_source_;
  scoped_ptr<TestWebProgressNotifier> web_progress_notifier_;
};

TEST_F(WebProgressNotifierTestFixture, FireEvents) {
  MockFunction<void(int check_point)> check;
  {
    InSequence sequence;
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnBeforeNavigate(_, _, _, _, _));
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnBeforeRetarget(_, _, _, _));
    EXPECT_CALL(check, Call(2));
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCommitted(_, _, _, _, _, _));
    EXPECT_CALL(check, Call(3));
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCompleted(_, _, _, _));
    EXPECT_CALL(check, Call(4));
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnErrorOccurred(_, _, _, _, _));

    web_browser_events_source_->FireOnBeforeNavigate(
        web_browser_, CComBSTR(L"http://www.google.com/"));
    check.Call(1);
    web_browser_events_source_->FireOnNewWindow(
        CComBSTR(L"http://www.google.com/"),
        CComBSTR(L"http://mail.google.com/"));
    check.Call(2);
    web_browser_events_source_->FireOnNavigateComplete(
        web_browser_, CComBSTR(L"http://www.google.com/"));
    check.Call(3);
    web_browser_events_source_->FireOnDocumentComplete(
        web_browser_, CComBSTR(L"http://www.google.com/"));
    check.Call(4);
    web_browser_events_source_->FireOnNavigateError(
        web_browser_, CComBSTR(L"http://www.google.com/"), 400);
  }
}

TEST_F(WebProgressNotifierTestFixture, FilterAbnormalWebBrowserEvents) {
  MockFunction<void(int check_point)> check;
  {
    InSequence sequence;
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnBeforeNavigate(_, _, _, _, _));
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCommitted(_, _, _, _, _, _));
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCompleted(_, _, _, _));
  }

  web_browser_events_source_->FireOnBeforeNavigate(
      web_browser_, CComBSTR(L"http://www.google.com/"));
  // Undesired event.
  web_browser_events_source_->FireOnDocumentComplete(
      web_browser_, CComBSTR(L"http://www.google.com/"));
  // Undesired event.
  web_browser_events_source_->FireOnNavigateComplete(
      web_browser_, CComBSTR(L"http://www.google.com/"));
  web_browser_events_source_->FireOnNavigateComplete(
      web_browser_, CComBSTR(L"http://mail.google.com/"));
  web_browser_events_source_->FireOnDocumentComplete(
      web_browser_, CComBSTR(L"http://mail.google.com/"));
}

TEST_F(WebProgressNotifierTestFixture, TestFrameId) {
  int subframe_id_1 = -1;
  int current_frame_id = -1;

  CComPtr<IWebBrowser2> subframe_1;
  ASSERT_HRESULT_SUCCEEDED(CreateMockWebBrowser(&subframe_1));

  CComPtr<IWebBrowser2> subframe_2;
  ASSERT_HRESULT_SUCCEEDED(CreateMockWebBrowser(&subframe_2));

  MockFunction<void(int check_point)> check;
  {
    InSequence sequence;
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnBeforeNavigate(_, _, _, _, _))
        .WillOnce(DoAll(SaveArg<2>(&current_frame_id),
                        Return(S_OK)));
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCommitted(_, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<2>(&current_frame_id),
                        Return(S_OK)));
    EXPECT_CALL(check, Call(2));
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnBeforeNavigate(_, _, _, _, _))
        .WillOnce(DoAll(SaveArg<2>(&current_frame_id),
                        Return(S_OK)));
    EXPECT_CALL(check, Call(3));
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCommitted(_, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<2>(&current_frame_id),
                        Return(S_OK)));
    EXPECT_CALL(check, Call(4));
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnBeforeNavigate(_, _, _, _, _))
        .WillOnce(DoAll(SaveArg<2>(&current_frame_id),
                        Return(S_OK)));

    web_browser_events_source_->FireOnBeforeNavigate(
        web_browser_, CComBSTR(L"http://www.google.com/"));
    // The main frame has 0 as frame ID.
    EXPECT_EQ(0, current_frame_id);
    check.Call(1);

    current_frame_id = -1;
    web_browser_events_source_->FireOnNavigateComplete(
        web_browser_, CComBSTR(L"http://www.google.com/"));
    // The main frame has 0 as frame ID.
    EXPECT_EQ(0, current_frame_id);
    check.Call(2);

    current_frame_id = -1;
    web_browser_events_source_->FireOnBeforeNavigate(
        subframe_1, CComBSTR(L"http://www.google.com/"));
    subframe_id_1 = current_frame_id;
    // A subframe should not have 0 as frame ID.
    EXPECT_NE(0, subframe_id_1);
    check.Call(3);

    current_frame_id = -1;
    web_browser_events_source_->FireOnNavigateComplete(
        subframe_1, CComBSTR(L"http://www.google.com/"));
    // The frame ID of a subframe remains the same.
    EXPECT_EQ(subframe_id_1, current_frame_id);
    check.Call(4);

    current_frame_id = -1;
    web_browser_events_source_->FireOnBeforeNavigate(
        subframe_2, CComBSTR(L"http://www.google.com/"));
    // Different subframes have different frame IDs.
    EXPECT_NE(subframe_id_1, current_frame_id);
  }
}

TEST_F(WebProgressNotifierTestFixture, TestTransitionClientRedirect) {
  IgnoreCallsToEventsFunnelExceptOnCommitted();

  CComBSTR url(L"http://www.google.com");
  CComBSTR javascript_url(L"javascript:someScript();");
  const char* link = "link";
  const char* redirect_javascript = "client_redirect|redirect_javascript";
  const char* redirect_onload = "client_redirect|redirect_onload";
  const char* redirect_meta_refresh = "client_redirect|redirect_meta_refresh";
  MockFunction<void(int check_point)> check;
  {
    InSequence sequence;
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCommitted(_, _, _, StrEq(link), StrEq(redirect_javascript),
                            _));
    EXPECT_CALL(check, Call(1));

    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCommitted(_, _, _, StrEq(link), StrEq(redirect_javascript),
                            _));
    EXPECT_CALL(check, Call(2));

    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCommitted(_, _, _, _, _, _));
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCommitted(_, _, _, StrEq(link), StrEq(redirect_javascript),
                            _));
    EXPECT_CALL(check, Call(3));

    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCommitted(_, _, _, StrEq(link), StrEq(redirect_onload), _));
    EXPECT_CALL(check, Call(4));

    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCommitted(_, _, _, StrEq(link), StrEq(redirect_meta_refresh),
                            _));
  }

  // If target URL starts with "javascript:" and no other signals are found,
  // consider the transition as JavaScript redirect.
  web_browser_events_source_->FireOnBeforeNavigate(web_browser_,
                                                   javascript_url);
  web_browser_events_source_->FireOnNavigateComplete(web_browser_,
                                                     javascript_url);
  check.Call(1);

  // If DocumentComplete hasn't been received for the previous navigation, and
  // no other signals are found, consider the transition as JavaScript redirect.
  FireNavigationEvents(web_browser_, url);
  check.Call(2);

  web_progress_notifier_->mock_has_potential_javascript_redirect_ = true;
  FireNavigationEvents(web_browser_, url);
  // If JavaScript code to navigate the page is found on the previous page, and
  // no other signals are found, consider the transition as JavaScript redirect.
  FireNavigationEvents(web_browser_, url);
  check.Call(3);

  // If currently we are in the onload event handler, consider the transition as
  // onload redirect.
  web_progress_notifier_->mock_has_potential_javascript_redirect_ = false;
  web_progress_notifier_->mock_in_onload_event_ = true;
  FireNavigationEvents(web_browser_, url);
  check.Call(4);

  // If the previous page has <meta http-equiv="refresh"> tag, consider the
  // transition as meta-refresh redirect.
  web_progress_notifier_->mock_in_onload_event_ = false;
  web_progress_notifier_->mock_is_meta_refresh_ = true;
  FireNavigationEvents(web_browser_, url);
}

TEST_F(WebProgressNotifierTestFixture, TestTransitionUserAction) {
  IgnoreCallsToEventsFunnelExceptOnCommitted();

  CComBSTR url(L"http://www.google.com");
  const char* link = "link";
  const char* typed = "typed";
  const char* no_qualifier = "";
  MockFunction<void(int check_point)> check;
  {
    InSequence sequence;
    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCommitted(_, _, _, StrEq(link), StrEq(no_qualifier), _));
    EXPECT_CALL(check, Call(1));

    EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
                OnCommitted(_, _, _, StrEq(typed), StrEq(no_qualifier), _));
  }

  // User actions override JavaScript redirect signals, so setting the following
  // flag should have no effect.
  web_progress_notifier_->mock_has_potential_javascript_redirect_ = true;

  // If there is user action in the content window, consider the transition as
  // link.
  web_progress_notifier_->mock_is_possible_user_action_in_content_window_ =
      true;
  FireNavigationEvents(web_browser_, url);
  check.Call(1);

  // If there is user action in the browser UI, consider the transition as
  // typed.
  web_progress_notifier_->mock_is_possible_user_action_in_content_window_ =
      false;
  web_progress_notifier_->mock_is_possible_user_action_in_browser_ui_ = true;
  FireNavigationEvents(web_browser_, url);
}

TEST_F(WebProgressNotifierTestFixture, TestTransitionForwardBack) {
  IgnoreCallsToEventsFunnelExceptOnCommitted();

  CComBSTR url(L"http://www.google.com");
  const char* auto_bookmark = "auto_bookmark";
  const char* forward_back = "forward_back";

  EXPECT_CALL(web_progress_notifier_->mock_webnavigation_events_funnel_,
              OnCommitted(_, _, _, StrEq(auto_bookmark), StrEq(forward_back),
                          _));

  // Forward/back overrides other signals, so setting the following flags
  // should have no effect.
  web_progress_notifier_->mock_is_possible_user_action_in_content_window_ =
      true;
  web_progress_notifier_->mock_is_possible_user_action_in_browser_ui_ =
      true;
  web_progress_notifier_->mock_in_onload_event_ = true;
  web_progress_notifier_->mock_is_meta_refresh_ = true;

  // If the current navigation doesn't cause browsing history to change,
  // consider the transition as forward/back.
  web_progress_notifier_->mock_is_forward_back_ = true;
  FireNavigationEvents(web_browser_, url);
}

TEST_F(WebProgressNotifierTestFixture, TestDetectingForwardBack) {
  TLENUMF back_fore = TLEF_RELATIVE_BACK | TLEF_RELATIVE_FORE |
                      TLEF_INCLUDE_UNINVOKEABLE;
  TLENUMF fore = TLEF_RELATIVE_FORE | TLEF_INCLUDE_UNINVOKEABLE;
  MockFunction<void(int check_point)> check;
  {
    InSequence sequence;

    EXPECT_CALL(*mock_travel_log_stg_, GetCount(back_fore, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(5),
                        Return(S_OK)));
    EXPECT_CALL(*mock_travel_log_stg_, GetCount(fore, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(0),
                        Return(S_OK)));
    EXPECT_CALL(check, Call(1));

    EXPECT_CALL(*mock_travel_log_stg_, GetCount(back_fore, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(6),
                        Return(S_OK)));
    EXPECT_CALL(*mock_travel_log_stg_, GetCount(fore, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(0),
                        Return(S_OK)));
    EXPECT_CALL(check, Call(2));

    EXPECT_CALL(*mock_travel_log_stg_, GetCount(back_fore, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(6),
                        Return(S_OK)));
    EXPECT_CALL(*mock_travel_log_stg_, GetCount(fore, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(3),
                        Return(S_OK)));
    EXPECT_CALL(check, Call(3));

    EXPECT_CALL(*mock_travel_log_stg_, GetCount(back_fore, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(6),
                        Return(S_OK)));
    EXPECT_CALL(*mock_travel_log_stg_, GetCount(fore, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(0),
                        Return(S_OK)));
    EXPECT_CALL(check, Call(4));

    EXPECT_CALL(*mock_travel_log_stg_, GetCount(back_fore, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(6),
                        Return(S_OK)));
    EXPECT_CALL(*mock_travel_log_stg_, GetCount(fore, NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<1>(0),
                        Return(S_OK)));
  }

  CComBSTR google_url(L"http://www.google.com/");
  CComBSTR gmail_url(L"http://mail.google.com/");

  // Test recording of the previous travel log info.
  EXPECT_FALSE(web_progress_notifier_->CallRealIsForwardBack(google_url));
  EXPECT_EQ(6, web_progress_notifier_->previous_travel_log_info().length);
  EXPECT_EQ(0, web_progress_notifier_->previous_travel_log_info().position);
  EXPECT_EQ(google_url,
            web_progress_notifier_->previous_travel_log_info().newest_url);
  check.Call(1);

  // If the length of the forward/back list has changed, the navigation is not
  // forward/back.
  EXPECT_FALSE(web_progress_notifier_->CallRealIsForwardBack(google_url));
  check.Call(2);

  // If the length of the forward/back list remains the same, and there are
  // entries in the forward list, the navigation is forward/back.
  EXPECT_TRUE(web_progress_notifier_->CallRealIsForwardBack(gmail_url));
  EXPECT_NE(gmail_url,
            web_progress_notifier_->previous_travel_log_info().newest_url);
  check.Call(3);

  // If the length of the forward/back list remains the same, and the URL of the
  // last entry in the list remains the same, the navigation is forward/back.
  EXPECT_TRUE(web_progress_notifier_->CallRealIsForwardBack(google_url));
  check.Call(4);

  // If the length of the forward/back list remains the same, but the URL of the
  // last entry in the list has changed, the navigation is not forward/back.
  EXPECT_FALSE(web_progress_notifier_->CallRealIsForwardBack(gmail_url));
}

}  // namespace
