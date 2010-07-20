// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>

#include "base/scoped_bstr_win.h"
#include "base/scoped_comptr_win.h"
#include "base/string_util.h"
#include "chrome_frame/http_negotiate.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/utils.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

class HttpNegotiateTest : public testing::Test {
 protected:
  HttpNegotiateTest() {
  }
};

class TestHttpNegotiate
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IHttpNegotiate {
 public:
  TestHttpNegotiate()
      : beginning_transaction_ret_(S_OK), additional_headers_(NULL) {
  }

BEGIN_COM_MAP(TestHttpNegotiate)
  COM_INTERFACE_ENTRY(IHttpNegotiate)
END_COM_MAP()
  STDMETHOD(BeginningTransaction)(LPCWSTR url, LPCWSTR headers,  // NOLINT
                                  DWORD reserved,  // NOLINT
                                  LPWSTR* additional_headers) {  // NOLINT
    if (additional_headers_) {
      int len = lstrlenW(additional_headers_);
      len++;
      *additional_headers = reinterpret_cast<wchar_t*>(
          ::CoTaskMemAlloc(len * sizeof(wchar_t)));
      lstrcpyW(*additional_headers, additional_headers_);
    }
    return beginning_transaction_ret_;
  }

  STDMETHOD(OnResponse)(DWORD response_code, LPCWSTR response_header,
                        LPCWSTR request_header,
                        LPWSTR* additional_request_headers) {
    return S_OK;
  }

  HRESULT beginning_transaction_ret_;
  const wchar_t* additional_headers_;
};

TEST_F(HttpNegotiateTest, BeginningTransaction) {
  static const int kBeginningTransactionIndex = 3;
  CComObjectStackEx<TestHttpNegotiate> test_http;
  IHttpNegotiate_BeginningTransaction_Fn original =
      reinterpret_cast<IHttpNegotiate_BeginningTransaction_Fn>(
          (*reinterpret_cast<void***>(
              static_cast<IHttpNegotiate*>(
                  &test_http)))[kBeginningTransactionIndex]);

  std::wstring cf_ua(
      ASCIIToWide(http_utils::GetDefaultUserAgentHeaderWithCFTag()));
  std::wstring cf_tag(
      ASCIIToWide(http_utils::GetChromeFrameUserAgent()));

  EXPECT_NE(std::wstring::npos, cf_ua.find(cf_tag));

  struct TestCase {
    const std::wstring original_headers_;
    const std::wstring delegate_additional_;
    const std::wstring expected_additional_;
    HRESULT delegate_return_value_;
  } test_cases[] = {
    { L"Accept: */*\r\n",
      L"",
      cf_ua + L"\r\n",
      S_OK },
    { L"Accept: */*\r\n",
      L"",
      L"",
      E_OUTOFMEMORY },
    { L"",
      L"Accept: */*\r\n",
      L"Accept: */*\r\n" + cf_ua + L"\r\n",
      S_OK },
    { L"User-Agent: Bingo/1.0\r\n",
      L"",
      L"User-Agent: Bingo/1.0 " + cf_tag + L"\r\n",
      S_OK },
    { L"User-Agent: NotMe/1.0\r\n",
      L"User-Agent: MeMeMe/1.0\r\n",
      L"User-Agent: MeMeMe/1.0 " + cf_tag + L"\r\n",
      S_OK },
    { L"",
      L"User-Agent: MeMeMe/1.0\r\n",
      L"User-Agent: MeMeMe/1.0 " + cf_tag + L"\r\n",
      S_OK },
  };

  for (int i = 0; i < arraysize(test_cases); ++i) {
    TestCase& test = test_cases[i];
    wchar_t* additional = NULL;
    test_http.beginning_transaction_ret_ = test.delegate_return_value_;
    test_http.additional_headers_ = test.delegate_additional_.c_str();
    HttpNegotiatePatch::BeginningTransaction(original, &test_http,
        L"http://www.google.com", test.original_headers_.c_str(), 0,
        &additional);
    EXPECT_TRUE(additional != NULL);

    if (additional) {
      // Check against the expected additional headers.
      EXPECT_EQ(test.expected_additional_, std::wstring(additional));
      ::CoTaskMemFree(additional);
    }
  }
}

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
      browser_->Navigate(ScopedBstr(L"about:blank"), NULL, NULL, NULL, NULL);
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
  ScopedComPtr<IWebBrowser2> browser_;
};

TEST_F(HttpNegotiateTest, ReportProgress) {
  if (chrome_frame_test::GetInstalledIEVersion() == IE_6) {
    DLOG(INFO) << "Not running test for IE6";
    return;
  }
  static const int kReportProgressIndex = 4;
  CComObjectStackEx<TestInternetProtocolSink> test_sink;
  IInternetProtocolSink_ReportProgress_Fn original =
      reinterpret_cast<IInternetProtocolSink_ReportProgress_Fn>(
          (*reinterpret_cast<void***>(
              static_cast<IInternetProtocolSink*>(
                  &test_sink)))[kReportProgressIndex]);

  struct TestCase {
    bool mark_browser_;
    const ULONG status_;
    const std::wstring status_text_;
    const std::wstring expected_status_text_;
  } test_cases[] = {
    // Cases where we could switch the mime type.
    { true,
      BINDSTATUS_MIMETYPEAVAILABLE,
      L"text/html",
      kChromeMimeType },
    { false,
      BINDSTATUS_MIMETYPEAVAILABLE,
      L"text/html",
      L"text/html" },
    { true,
      BINDSTATUS_VERIFIEDMIMETYPEAVAILABLE,
      L"text/html",
      kChromeMimeType },
    { false,
      BINDSTATUS_VERIFIEDMIMETYPEAVAILABLE,
      L"text/html",
      L"text/html" },
    { true,
      LOCAL_BINDSTATUS_SERVER_MIMETYPEAVAILABLE,
      L"text/html",
      kChromeMimeType },
    { false,
      LOCAL_BINDSTATUS_SERVER_MIMETYPEAVAILABLE,
      L"text/html",
      L"text/html" },

    // Cases where we mark the browser but can't switch due to mime type.
    { true,
      BINDSTATUS_MIMETYPEAVAILABLE,
      L"application/pdf",
      L"application/pdf" },
    { true,
      BINDSTATUS_VERIFIEDMIMETYPEAVAILABLE,
      L"application/pdf",
      L"application/pdf" },
    { true,
      LOCAL_BINDSTATUS_SERVER_MIMETYPEAVAILABLE,
      L"application/pdf",
      L"application/pdf" },

    // Cases where we should do nothing.
    { false,
      BINDSTATUS_CONNECTING,
      L"foo",
      L"foo" },
    { false,
      BINDSTATUS_UPLOADINGDATA,
      L"bar",
      L"bar" },
  };

  ScopedComPtr<IBrowserService> browser;
  EXPECT_HRESULT_SUCCEEDED(GetBrowserServiceFromProtocolSink(&test_sink,
      browser.Receive()));

  for (int i = 0; i < arraysize(test_cases); ++i) {
    TestCase& test = test_cases[i];
    if (test.mark_browser_) {
      MarkBrowserOnThreadForCFNavigation(browser);
    }

    HRESULT hr = HttpNegotiatePatch::ReportProgress(original, &test_sink,
        test.status_, test.status_text_.c_str());
    EXPECT_EQ(S_OK, hr);
    // The TLS flag should always be cleared.
    EXPECT_FALSE(CheckForCFNavigation(browser, false));
    EXPECT_EQ(test.expected_status_text_, test_sink.last_status_text());
    EXPECT_EQ(test.status_, test_sink.last_status());
  }
}

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
