// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/mock_ie_event_sink_test.h"

#include <sstream>

#include "base/scoped_variant_win.h"
#include "base/utf_string_conversions.h"
#include "chrome_frame/test/mock_ie_event_sink_actions.h"

// Needed for CreateFunctor.
#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

using testing::_;
using testing::Cardinality;
using testing::Exactly;
using testing::ExpectationSet;
using testing::InSequence;
using testing::StrCaseEq;

namespace chrome_frame_test {

// MockIEEventSink methods
void MockIEEventSink::OnDocumentComplete(IDispatch* dispatch, VARIANT* url) {
  if (!event_sink_->IsCFRendering()) {
    HWND renderer_window = event_sink_->GetRendererWindowSafe();
    if (renderer_window) {
      ::NotifyWinEvent(IA2_EVENT_DOCUMENT_LOAD_COMPLETE,
                       renderer_window,
                       OBJID_CLIENT, 0L);
    } else {
      DLOG(INFO) << "Browser does not have renderer window";
    }
    OnLoad(IN_IE, V_BSTR(url));
  }
}

ExpectationSet MockIEEventSink::ExpectNavigationCardinality(
    const std::wstring& url, Cardinality before_cardinality,
    Cardinality complete_cardinality) {
  ExpectationSet navigation;
  if (url.empty()) {
    navigation += EXPECT_CALL(*this, OnBeforeNavigate2(_, _, _, _, _, _, _))
        .Times(before_cardinality).RetiresOnSaturation();
  } else {
    navigation += EXPECT_CALL(*this, OnBeforeNavigate2(_,
                              testing::Field(&VARIANT::bstrVal,
                              StrCaseEq(url)), _, _, _, _, _))
        .Times(before_cardinality).RetiresOnSaturation();
  }

  // Hack: OnFileDownload may occur zero or once (for reasons not understood)
  // before each OnNavigateComplete2 which causes problems for tests expecting
  // things in sequence. To redress this, expectations which allow multiple
  // calls are split into expect statements expecting exactly one call or at
  // most one call.
  // TODO(kkania): Consider avoiding this problem by creating a mock without
  // the OnFileDownload call or by removing the dependency of some tests on
  // InSequence.
  DLOG_IF(WARNING, complete_cardinality.ConservativeUpperBound() > 1000)
      << "Cardinality upper bound may be too great to be split up into single "
      << "expect statements. If you do not require this navigation to be in "
      << "sequence, do not call this method.";
  int call_count = 0;
  InSequence expect_in_sequence_for_scope;
  while (!complete_cardinality.IsSaturatedByCallCount(call_count)) {
    navigation += EXPECT_CALL(*this, OnFileDownload(_, _))
        .Times(testing::AtMost(1))
        .RetiresOnSaturation();

    Cardinality split_complete_cardinality = testing::Exactly(1);
    if (complete_cardinality.IsSatisfiedByCallCount(call_count))
      split_complete_cardinality = testing::AtMost(1);

    if (url.empty()) {
      navigation += EXPECT_CALL(*this, OnNavigateComplete2(_, _))
          .Times(split_complete_cardinality)
          .RetiresOnSaturation();
    } else {
      navigation += EXPECT_CALL(*this, OnNavigateComplete2(_,
                                testing::Field(&VARIANT::bstrVal,
                                StrCaseEq(url))))
          .Times(split_complete_cardinality)
          .RetiresOnSaturation();
    }
    call_count++;
  }
  return navigation;
}

void MockIEEventSink::ExpectNavigation(bool is_cf, const std::wstring& url) {
  InSequence expect_in_sequence_for_scope;
  if (is_cf) {
    ExpectNavigationCardinality(url, Exactly(1), testing::Between(1, 2));
  } else {
    ExpectNavigationCardinality(url, Exactly(1), Exactly(1));
  }
}

void MockIEEventSink::ExpectNavigationOptionalBefore(bool is_cf,
                                                     const std::wstring& url) {
  InSequence expect_in_sequence_for_scope;
  if (is_cf && GetInstalledIEVersion() == IE_6) {
    ExpectNavigationCardinality(url, testing::AtMost(1),
                                testing::Between(1, 2));
  } else {
    ExpectNavigation(is_cf, url);
  }
}

void MockIEEventSink::ExpectJavascriptWindowOpenNavigation(
    bool parent_cf, bool new_window_cf, const std::wstring& url) {
  if (parent_cf) {
    InSequence expect_in_sequence_for_scope;
    ExpectNavigation(IN_CF, L"");
    ExpectNavigationCardinality(L"", testing::AtMost(1),
                                testing::Between(1, 2));
  } else {
    if (new_window_cf) {
      ExpectNavigationCardinality(url, testing::AtMost(1), testing::AtMost(1));
      // Sometimes an extra load occurs here for some reason.
      EXPECT_CALL(*this, OnLoad(IN_IE, StrCaseEq(url)))
          .Times(testing::AtMost(1));
      ExpectNavigationCardinality(url, testing::AtMost(1),
                                  testing::Between(1, 2));
    } else {
      ExpectNavigation(IN_IE, url);
    }
  }
}

void MockIEEventSink::ExpectNewWindow(MockIEEventSink* new_window_mock) {
  DCHECK(new_window_mock);

  EXPECT_CALL(*this, OnNewWindow3(_, _, _, _, _));
  EXPECT_CALL(*this, OnNewBrowserWindow(_, _))
      .WillOnce(testing::WithArgs<0>(testing::Invoke(testing::CreateFunctor(
          new_window_mock, &MockIEEventSink::Attach))));
}

void MockIEEventSink::ExpectAnyNavigations() {
  EXPECT_CALL(*this, OnBeforeNavigate2(_, _, _, _, _, _, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*this, OnFileDownload(VARIANT_TRUE, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*this, OnNavigateComplete2(_, _))
      .Times(testing::AnyNumber());
}

void MockIEEventSink::ExpectDocumentReadystate(int ready_state) {
  ScopedComPtr<IWebBrowser2> browser(event_sink_->web_browser2());
  EXPECT_TRUE(browser != NULL);
  if (browser) {
    ScopedComPtr<IDispatch> document;
    browser->get_Document(document.Receive());
    EXPECT_TRUE(document != NULL);
    if (document) {
      DISPPARAMS params = { 0 };
      ScopedVariant result;
      EXPECT_HRESULT_SUCCEEDED(document->Invoke(DISPID_READYSTATE, IID_NULL,
          LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params,
          result.Receive(), NULL, NULL));
      EXPECT_EQ(VT_I4, result.type());
      if (result.type() == VT_I4) {
        EXPECT_EQ(ready_state, static_cast<int>(V_I4(&result)));
      }
    }
  }
}

// MockIEEventSinkTest methods
MockIEEventSinkTest::MockIEEventSinkTest() : server_mock_(1337, L"127.0.0.1",
                                                          GetTestDataFolder()) {
  EXPECT_CALL(server_mock_, Get(_, StrCaseEq(L"/favicon.ico"), _))
      .WillRepeatedly(SendFast("HTTP/1.1 404 Not Found", ""));
}

void MockIEEventSinkTest::LaunchIEAndNavigate(const std::wstring& url) {
  LaunchIENavigateAndLoop(url, kChromeFrameLongNavigationTimeoutInSeconds);
}

void MockIEEventSinkTest::LaunchIENavigateAndLoop(const std::wstring& url,
                                                  int timeout) {
  EXPECT_CALL(ie_mock_, OnQuit())
      .WillOnce(QUIT_LOOP(loop_));
  HRESULT hr = ie_mock_.event_sink()->LaunchIEAndNavigate(url, &ie_mock_);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(ie_mock_.event_sink()->web_browser2() != NULL);
  loop_.RunFor(timeout);
}

FilePath MockIEEventSinkTest::GetTestFilePath(
    const std::wstring& relative_path) {
  return server_mock_.root_dir().Append(relative_path);
}

std::wstring MockIEEventSinkTest::GetTestUrl(
    const std::wstring& relative_path) {
  return server_mock_.Resolve(relative_path.c_str());
}

}  // namespace chrome_frame_test

