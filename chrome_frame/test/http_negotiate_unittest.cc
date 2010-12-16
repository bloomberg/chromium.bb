// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "chrome_frame/http_negotiate.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/utils.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

class TestInternetProtocolSink
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IInternetProtocolSink {
 public:
  TestInternetProtocolSink() : status_(0) {
    // Create an instance of IE to fullfill the requirements of being able
    // to detect whether a sub-frame or top-frame is being loaded (see
    // IsSubFrameRequest) and to be able to mark an IBrowserService
    // implementation as a target for CF navigation.
    HRESULT hr = browser_.CreateInstance(CLSID_InternetExplorer);
    CHECK(SUCCEEDED(hr));
    if (SUCCEEDED(hr)) {
      browser_->Navigate(base::win::ScopedBstr(L"about:blank"),
                         NULL, NULL, NULL, NULL);
    }
  }

  ~TestInternetProtocolSink() {
    if (browser_)
      browser_->Quit();
  }

BEGIN_COM_MAP(TestInternetProtocolSink)
  COM_INTERFACE_ENTRY(IInternetProtocolSink)
  COM_INTERFACE_ENTRY_AGGREGATE(IID_IServiceProvider, browser_)
END_COM_MAP()

  // IInternetProtocolSink.
  STDMETHOD(Switch)(PROTOCOLDATA* data) {
    NOTREACHED();
    return S_OK;
  }

  STDMETHOD(ReportProgress)(ULONG status, LPCWSTR text) {
    status_ = status;
    status_text_ = text ? text : L"";
    return S_OK;
  }

  STDMETHOD(ReportData)(DWORD bscf, ULONG progress, ULONG progress_max) {
    NOTREACHED();
    return S_OK;
  }

  STDMETHOD(ReportResult)(HRESULT hr, DWORD err, LPCWSTR result) {
    NOTREACHED();
    return S_OK;
  }

  ULONG last_status() const {
    return status_;
  }

  const std::wstring& last_status_text() const {
    return status_text_;
  }

 protected:
  ULONG status_;
  std::wstring status_text_;
  base::win::ScopedComPtr<IWebBrowser2> browser_;
};

using testing::AllOf;
using testing::ContainsRegex;
using testing::HasSubstr;

TEST(AppendUserAgent, Append) {
  EXPECT_THAT(AppendCFUserAgentString(NULL, NULL),
      testing::ContainsRegex("User-Agent:.+chromeframe.+\r\n"));

  // Check Http headers are reasonably parsed.
  EXPECT_THAT(AppendCFUserAgentString(L"Bad User-Agent: Age Tuners;\r\n", NULL),
      AllOf(ContainsRegex("User-Agent:.+chromeframe.+\r\n"),
            testing::Not(testing::HasSubstr("Age Tuners"))));

  // Honor headers User-Agent, if additional headers does not specify one.
  EXPECT_THAT(AppendCFUserAgentString(L"User-Agent: A Tense Rug;\r\n", NULL),
      ContainsRegex("User-Agent: A Tense Rug; chromeframe.+\r\n"));

  // Honor additional headers User-Agent.
  EXPECT_THAT(AppendCFUserAgentString(L"User-Agent: Near Guest;\r\n",
                                      L"User-Agent: Rat see Gun;\r\n"),
      ContainsRegex("User-Agent: Rat see Gun; chromeframe.+\r\n"));

  // Check additional headers are preserved.
  EXPECT_THAT(AppendCFUserAgentString(NULL,
            L"Authorization: A Zoo That I Ruin\r\n"
            L"User-Agent: Get a Nurse;\r\n"
            L"Accept-Language: Cleanup a Cat Egg\r\n"),
      AllOf(ContainsRegex("User-Agent: Get a Nurse; chromeframe.+\r\n"),
            HasSubstr("Authorization: A Zoo That I Ruin\r\n"),
            HasSubstr("Accept-Language: Cleanup a Cat Egg\r\n")));
}
