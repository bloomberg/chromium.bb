// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>

#include "base/scoped_comptr_win.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/urlmon_moniker.h"
#include "chrome_frame/test/urlmon_moniker_tests.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::WithArg;
using testing::WithArgs;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::Eq;

class UrlmonMonikerTest : public testing::Test {
 protected:
  UrlmonMonikerTest() {
  }
};

// Tests the ReadStreamCache class by writing content into a stream object
// and verify that reading that stream through the ReadStreamCache class
// reads the correct content and also verifies that ReadStreamCache caches
// all the reads.
TEST_F(UrlmonMonikerTest, ReadStreamCache) {
  CComObjectStackEx<ReadStreamCache> read_stream;
  EXPECT_EQ(NULL, read_stream.cache());

  ScopedComPtr<IStream> test_stream;
  ::CreateStreamOnHGlobal(NULL, TRUE, test_stream.Receive());
  EXPECT_TRUE(NULL != test_stream);
  const char test_string[] = "ReadStreamCacheTest";
  DWORD written;
  EXPECT_HRESULT_SUCCEEDED(test_stream->Write(test_string, sizeof(test_string),
                                              &written));
  EXPECT_HRESULT_SUCCEEDED(RewindStream(test_stream));

  read_stream.SetDelegate(test_stream);

  char buffer[0xff];
  DWORD read = 0;
  EXPECT_HRESULT_SUCCEEDED(read_stream.Read(buffer, sizeof(buffer), &read));
  EXPECT_EQ(read, sizeof(test_string));
  EXPECT_EQ(read_stream.GetCacheSize(), sizeof(test_string));
  read_stream.RewindCache();
  IStream* cache = read_stream.cache();
  EXPECT_TRUE(NULL != cache);
  if (cache) {
    read = 0;
    EXPECT_HRESULT_SUCCEEDED(cache->Read(buffer, sizeof(buffer), &read));
    EXPECT_EQ(read, sizeof(test_string));
    EXPECT_EQ(0, memcmp(test_string, buffer, sizeof(test_string)));
  }
}

// Verifies that we can override bind results by using the SimpleBindingImpl
// class.
TEST_F(UrlmonMonikerTest, SimpleBindingImpl1) {
  CComObjectStackEx<SimpleBindingImpl> test;
  ScopedComPtr<IBinding> binding;
  binding.QueryFrom(&test);
  EXPECT_TRUE(binding != NULL);
  test.OverrideBindResults(E_INVALIDARG);
  DWORD hr = E_UNEXPECTED;
  EXPECT_HRESULT_SUCCEEDED(binding->GetBindResult(NULL, &hr, NULL, NULL));
  EXPECT_EQ(E_INVALIDARG, hr);
  test.OverrideBindResults(E_ACCESSDENIED);
  // {1AF15145-104B-4bd8-AA4F-97CEFD40D370} - just something non-null.
  GUID guid = { 0x1af15145, 0x104b, 0x4bd8,
      { 0xaa, 0x4f, 0x97, 0xce, 0xfd, 0x40, 0xd3, 0x70 } };
  EXPECT_HRESULT_SUCCEEDED(binding->GetBindResult(&guid, &hr, NULL, NULL));
  EXPECT_EQ(E_ACCESSDENIED, hr);
  EXPECT_TRUE(guid == GUID_NULL);
}

// Tests the SimpleBindingImpl class with a delegate.  Verifies that the
// delegate gets called and also that we can override the bind results.
TEST_F(UrlmonMonikerTest, SimpleBindingImpl2) {
  CComObjectStackEx<MockBindingImpl> mock;
  CComObjectStackEx<SimpleBindingImpl> test;

  EXPECT_CALL(mock, QueryService(_, _, _))
      .WillOnce(Return(E_ACCESSDENIED));
  EXPECT_CALL(mock, GetBindResult(_, _, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(E_ACCESSDENIED),
                            Return(S_OK)));
  EXPECT_CALL(mock, Abort())
      .WillOnce(Return(E_ACCESSDENIED));

  ScopedComPtr<IServiceProvider> svc;
  svc.QueryFrom(test.GetUnknown());
  EXPECT_TRUE(svc == NULL);

  test.SetDelegate(&mock);

  // Now we should have access to IServiceProvider
  svc.QueryFrom(test.GetUnknown());
  EXPECT_TRUE(svc != NULL);

  ScopedComPtr<IUnknown> unk;
  EXPECT_EQ(E_ACCESSDENIED, svc->QueryService(GUID_NULL, IID_NULL,
      reinterpret_cast<void**>(unk.Receive())));

  // Call through to the mock's GetBindResult implementation.
  DWORD result;
  test.GetBindResult(NULL, &result, NULL, NULL);
  EXPECT_TRUE(result == E_ACCESSDENIED);
  // Let the binding override the result code.
  test.OverrideBindResults(INET_E_TERMINATED_BIND);
  test.GetBindResult(NULL, &result, NULL, NULL);
  EXPECT_TRUE(result == INET_E_TERMINATED_BIND);

  EXPECT_EQ(E_ACCESSDENIED, test.Abort());
}

// Tests the RequestData class.  Content is fed to the object via OnX methods
// and then we verify that the cached content is correct by calling
// FireHttpNegotiateEvents and see if the notifications contain the correct
// content.
TEST_F(UrlmonMonikerTest, RequestHeaders) {
  scoped_refptr<RequestData> test(new RequestData());
  test->Initialize(NULL);

  const wchar_t url[] = L"http://www.chromium.org";
  const wchar_t begin_headers[] = L"Cookie: endilega\r\n";
  const wchar_t request_headers[] = L"GET / HTTP/1.0\r\nHost: cough\r\n\rn";
  const wchar_t response_headers[] = L"HTTP 200 OK\r\nHave-a: good-day\r\n\r\n";
  const wchar_t additional_headers[] = L"Referer: http://foo.com\r\n";

  // Null pointers should be ignored.
  RequestHeaders* headers = test->headers();
  EXPECT_TRUE(NULL != headers);
  headers->OnBeginningTransaction(NULL, NULL, NULL);
  headers->OnBeginningTransaction(url, begin_headers, additional_headers);
  headers->OnResponse(500, NULL, NULL);
  headers->OnResponse(200, response_headers, request_headers);

  CComObjectStackEx<MockHttpNegotiateImpl> mock;
  EXPECT_CALL(mock, BeginningTransaction(StrEq(url), StrEq(begin_headers),
                                         _, _))
      .WillOnce(Return(S_OK));
  EXPECT_CALL(mock, OnResponse(Eq(200), StrEq(response_headers),
                               StrEq(request_headers), _))
      .WillOnce(Return(S_OK));

  EXPECT_EQ(0, headers->request_url().compare(url));

  headers->FireHttpNegotiateEvents(&mock);
}

// Tests the HTML content portion of the RequestData class.
// Here we provide content in the form of a stream object and call
// OnDataAvailable to make the object cache the contents.
// Then we fetch the cached content stream by calling
// GetResetCachedContentStream and verify that it's contents are correct.
// In order to also test when data is cached outside of OnDataAvailable
// calls, we also call DelegateDataRead.  In this test we simply use the
// original content again which will make the end results that the cached
// content should be double the test content.
TEST_F(UrlmonMonikerTest, RequestDataContent) {
  scoped_refptr<RequestData> test(new RequestData());
  test->Initialize(NULL);

  CComObjectStackEx<MockBindStatusCallbackImpl> mock;

  ScopedComPtr<IStream> data;
  ::CreateStreamOnHGlobal(NULL, TRUE, data.Receive());
  const char content[] = "<html><head>"
      "<meta http-equiv=\"X-UA-Compatible\" content=\"chrome=1\" />"
      "</head><body>Test HTML content</body></html>";
  data->Write(content, sizeof(content) - 1, NULL);
  STATSTG stat = {0};
  data->Stat(&stat, STATFLAG_NONAME);
  DCHECK(stat.cbSize.LowPart == (sizeof(content) - 1));
  RewindStream(data);

  FORMATETC format = {0};
  format.cfFormat = ::RegisterClipboardFormat(L"application/chromepage");
  format.tymed = TYMED_ISTREAM;
  STGMEDIUM storage = {0};
  storage.tymed = TYMED_ISTREAM;
  storage.pstm = data;

  DWORD flags = BSCF_FIRSTDATANOTIFICATION | BSCF_LASTDATANOTIFICATION |
                BSCF_DATAFULLYAVAILABLE;

  EXPECT_CALL(mock, OnDataAvailable(Eq(flags), Eq(stat.cbSize.LowPart), _, _))
      .WillOnce(DoAll(
          WithArgs<3>(testing::Invoke(
              &MockBindStatusCallbackImpl::ReadAllData)),
          Return(S_OK)));

  size_t bytes_read = 0;
  test->DelegateDataRead(&mock, flags, stat.cbSize.LowPart, &format, &storage,
      &bytes_read);

  DCHECK(bytes_read == stat.cbSize.LowPart);
  DCHECK(test->GetCachedContentSize() == bytes_read);

  // Also test that the CacheAll method appends the stream.
  RewindStream(data);
  test->CacheAll(data);
  DCHECK(test->GetCachedContentSize() == (bytes_read * 2));

  ScopedComPtr<IStream> cache;
  EXPECT_HRESULT_SUCCEEDED(test->GetResetCachedContentStream(cache.Receive()));
  if (cache) {
    char buffer[0xffff];
    DCHECK((bytes_read * 2) <= sizeof(buffer));
    DWORD read = 0;
    cache->Read(buffer, sizeof(buffer), &read);
    EXPECT_EQ(read, bytes_read * 2);
    EXPECT_EQ(0, memcmp(content, buffer, sizeof(content) - 1));
    EXPECT_EQ(0, memcmp(content, buffer + sizeof(content) - 1,
                        sizeof(content) - 1));
  }
}

