// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>

#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "chrome_frame/http_negotiate.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/registry_list_preferences_holder.h"
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

  string16 cf_ua(
      ASCIIToWide(http_utils::GetDefaultUserAgentHeaderWithCFTag()));
  string16 cf_tag(
      ASCIIToWide(http_utils::GetChromeFrameUserAgent()));

  EXPECT_NE(string16::npos, cf_ua.find(L"chromeframe/"));

  struct TestCase {
    const string16 original_headers_;
    const string16 delegate_additional_;
    const string16 expected_additional_;
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
      EXPECT_EQ(test.expected_additional_, string16(additional));
      ::CoTaskMemFree(additional);
    }
  }
}

TEST_F(HttpNegotiateTest, BeginningTransactionUARemoval) {
  static const int kBeginningTransactionIndex = 3;
  CComObjectStackEx<TestHttpNegotiate> test_http;
  IHttpNegotiate_BeginningTransaction_Fn original =
      reinterpret_cast<IHttpNegotiate_BeginningTransaction_Fn>(
          (*reinterpret_cast<void***>(
              static_cast<IHttpNegotiate*>(
                  &test_http)))[kBeginningTransactionIndex]);

  string16 nocf_ua(
      ASCIIToWide(http_utils::RemoveChromeFrameFromUserAgentValue(
          http_utils::GetDefaultUserAgentHeaderWithCFTag())));
  string16 cf_ua(
      ASCIIToWide(http_utils::AddChromeFrameToUserAgentValue(
          WideToASCII(nocf_ua))));

  EXPECT_EQ(string16::npos, nocf_ua.find(L"chromeframe/"));
  EXPECT_NE(string16::npos, cf_ua.find(L"chromeframe/"));

  string16 ua_url(L"www.withua.com");
  string16 no_ua_url(L"www.noua.com");

  RegistryListPreferencesHolder& ua_holder =
      GetUserAgentPreferencesHolderForTesting();
  ua_holder.AddStringForTesting(no_ua_url);

  struct TestCase {
    const string16 url_;
    const string16 original_headers_;
    const string16 delegate_additional_;
    const string16 expected_additional_;
  } test_cases[] = {
    { ua_url,
      L"",
      L"Accept: */*\r\n" + cf_ua + L"\r\n",
      L"Accept: */*\r\n" + cf_ua + L"\r\n" },
    { ua_url,
      L"",
      L"Accept: */*\r\n" + nocf_ua + L"\r\n",
      L"Accept: */*\r\n" + cf_ua + L"\r\n" },
    { no_ua_url,
      L"",
      L"Accept: */*\r\n" + cf_ua + L"\r\n",
      L"Accept: */*\r\n" + nocf_ua + L"\r\n" },
    { no_ua_url,
      L"",
      L"Accept: */*\r\n" + nocf_ua + L"\r\n",
      L"Accept: */*\r\n" + nocf_ua + L"\r\n" },
  };

  for (int i = 0; i < arraysize(test_cases); ++i) {
    TestCase& test = test_cases[i];
    wchar_t* additional = NULL;
    test_http.beginning_transaction_ret_ = S_OK;
    test_http.additional_headers_ = test.delegate_additional_.c_str();
    HttpNegotiatePatch::BeginningTransaction(original, &test_http,
        test.url_.c_str(), test.original_headers_.c_str(), 0,
        &additional);
    EXPECT_TRUE(additional != NULL);

    if (additional) {
      // Check against the expected additional headers.
      EXPECT_EQ(test.expected_additional_, string16(additional))
          << "Iteration: " << i;
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

  const string16& last_status_text() const {
    return status_text_;
  }

 protected:
  ULONG status_;
  string16 status_text_;
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
