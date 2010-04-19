// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>

#include "base/string_util.h"
#include "chrome_frame/http_negotiate.h"
#include "chrome_frame/html_utils.h"
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
        static_cast<IHttpNegotiate*>(&test_http)))[kBeginningTransactionIndex]);

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
    { L"Accept: */*\r\n\r\n",
      L"",
      cf_ua + L"\r\n\r\n",
      S_OK },
    { L"Accept: */*\r\n\r\n",
      L"",
      L"",
      E_OUTOFMEMORY },
    { L"",
      L"Accept: */*\r\n\r\n",
      L"Accept: */*\r\n" + cf_ua + L"\r\n\r\n",
      S_OK },
    { L"User-Agent: Bingo/1.0\r\n\r\n",
      L"",
      L"User-Agent: Bingo/1.0 " + cf_tag + L"\r\n\r\n",
      S_OK },
    { L"User-Agent: NotMe/1.0\r\n\r\n",
      L"User-Agent: MeMeMe/1.0\r\n\r\n",
      L"User-Agent: MeMeMe/1.0 " + cf_tag + L"\r\n\r\n",
      S_OK },
    { L"",
      L"User-Agent: MeMeMe/1.0\r\n\r\n",
      L"User-Agent: MeMeMe/1.0 " + cf_tag + L"\r\n\r\n",
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
