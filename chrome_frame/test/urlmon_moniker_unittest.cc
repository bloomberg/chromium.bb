// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/win/scoped_comptr.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/urlmon_moniker_tests.h"
#include "chrome_frame/urlmon_bind_status_callback.h"

using chrome_frame_test::ScopedVirtualizeHklmAndHkcu;
using testing::Return;
using testing::Eq;

class MonikerPatchTest : public testing::Test {
 protected:
  MonikerPatchTest() {
  }

  virtual void SetUp() {
    DeleteAllSingletons();
    PathService::Get(base::DIR_SOURCE_ROOT, &test_file_path_);
    test_file_path_ = test_file_path_.Append(FILE_PATH_LITERAL("chrome_frame"))
        .Append(FILE_PATH_LITERAL("test"))
        .Append(FILE_PATH_LITERAL("data"));
  }

  bool ReadFileAsString(const wchar_t* file_name, std::string* file_contents) {
    EXPECT_TRUE(file_name);
    base::FilePath file_path = test_file_path_.Append(file_name);
    return file_util::ReadFileToString(file_path, file_contents);
  }

  static bool StringToStream(const std::string& data, IStream** ret) {
    EXPECT_TRUE(!data.empty());

    base::win::ScopedComPtr<IStream> stream;
    HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, stream.Receive());
    EXPECT_HRESULT_SUCCEEDED(hr);
    if (FAILED(hr)) {
      return false;
    }

    DWORD written = 0;
    hr = stream->Write(data.c_str(), data.size(), &written);
    EXPECT_HRESULT_SUCCEEDED(hr);
    EXPECT_EQ(data.size(), written);

    bool result = false;
    if (SUCCEEDED(hr)) {
      RewindStream(stream);
      *ret = stream.Detach();
      result = true;
    }

    return result;
  }

  base::FilePath test_file_path_;
  ScopedVirtualizeHklmAndHkcu virtualized_registry_;
};

// Tests the CacheStream class by writing content into a stream object
// and verify that reading that stream back
TEST_F(MonikerPatchTest, CacheStream) {
  const char data[] = "ReadStreamCacheTest";
  char ret[2 * sizeof(data)] = {0};
  DWORD read = 0;

  // Test 1: empty stream reads nothing
  CComObjectStackEx<CacheStream> cache_stream1;
  EXPECT_EQ(S_FALSE, cache_stream1.Read(ret, sizeof(ret), &read));
  EXPECT_EQ(0, read);

  // Test 2: Read from initialized cache
  CComObjectStackEx<CacheStream> cache_stream2;
  cache_stream2.Initialize(data, sizeof(data), false);
  EXPECT_HRESULT_SUCCEEDED(cache_stream2.Read(ret, sizeof(ret), &read));
  EXPECT_EQ(sizeof(data), read);
  EXPECT_EQ(std::string(data), std::string(ret));

  read = 0;
  EXPECT_EQ(E_PENDING, cache_stream2.Read(ret, sizeof(ret), &read));
  EXPECT_EQ(0, read);
}

ACTION_P3(ReadStream, buffer, size, ret) {
  EXPECT_EQ(TYMED_ISTREAM, arg3->tymed);
  *ret = arg3->pstm->Read(buffer, *size, size);
}

// Tests the implementation of BSCBFeedData to feed data to the
// specified IBindStatusCallback
TEST_F(MonikerPatchTest, BSCBFeedData) {
  CComObjectStackEx<MockBindStatusCallbackImpl> mock;
  const char data[] = "ReadStreamCacheTest";
  const DWORD size = sizeof(data);
  const CLIPFORMAT cf = 0xd0d0;
  const DWORD flags = BSCF_FIRSTDATANOTIFICATION | BSCF_LASTDATANOTIFICATION;
  const DWORD kArbitraryreadSize = 0xdeadbabe;

  char read_buffer1[size] = {0}, read_buffer2[size] = {0};
  DWORD read_size1 = size, read_size2 = kArbitraryreadSize;
  HRESULT ret1 = E_FAIL, ret2 = E_FAIL;

  EXPECT_CALL(mock, OnDataAvailable(flags, size,
                    testing::Field(&FORMATETC::cfFormat, cf),
                    testing::Field(&STGMEDIUM::tymed, TYMED_ISTREAM)))
      .WillOnce(testing::DoAll(
          ReadStream(read_buffer1, &read_size1, &ret1),
          ReadStream(read_buffer2, &read_size2, &ret2),
          Return(S_OK)));

  EXPECT_HRESULT_SUCCEEDED(CacheStream::BSCBFeedData(&mock, data, size, cf,
                                                     flags, false));

  EXPECT_HRESULT_SUCCEEDED(ret1);
  EXPECT_STREQ(data, read_buffer1);
  EXPECT_EQ(size, read_size1);

  EXPECT_EQ(E_PENDING, ret2);
  EXPECT_STREQ("", read_buffer2);
  EXPECT_EQ(kArbitraryreadSize, read_size2);
}

const wchar_t kSmallHtmlMetaTag[] = L"sub_frame1.html";
const wchar_t kSmallHtmlNoMetaTag[] = L"host_browser.html";

// Test various aspects of the SniffData class
TEST_F(MonikerPatchTest, SniffDataMetaTag) {
  std::string small_html_meta_tag, small_html_no_meta_tag;
  ASSERT_TRUE(ReadFileAsString(kSmallHtmlMetaTag, &small_html_meta_tag));
  ASSERT_TRUE(ReadFileAsString(kSmallHtmlNoMetaTag, &small_html_no_meta_tag));

  base::win::ScopedComPtr<IStream> stream_with_meta, stream_no_meta;
  ASSERT_TRUE(StringToStream(small_html_meta_tag, stream_with_meta.Receive()));
  ASSERT_TRUE(StringToStream(small_html_no_meta_tag,
                             stream_no_meta.Receive()));

  // Initialize 2 sniffers 1 with meta tag and 1 without.
  SniffData sniffer1, sniffer2;
  EXPECT_HRESULT_SUCCEEDED(sniffer1.InitializeCache(std::wstring()));
  EXPECT_HRESULT_SUCCEEDED(sniffer2.InitializeCache(std::wstring()));
  EXPECT_HRESULT_SUCCEEDED(sniffer1.ReadIntoCache(stream_with_meta, true));
  EXPECT_HRESULT_SUCCEEDED(sniffer2.ReadIntoCache(stream_no_meta, true));

  // Verify renderer type and size read.
  EXPECT_TRUE(sniffer1.is_chrome());
  EXPECT_EQ(SniffData::OTHER, sniffer2.renderer_type());
  EXPECT_EQ(small_html_meta_tag.size(), sniffer1.size());
  EXPECT_EQ(small_html_no_meta_tag.size(), sniffer2.size());
}

// Now test how the data is fed back the the bind status callback.
// case 1: callback reads data in 1 read
TEST_F(MonikerPatchTest, SniffDataPlayback1) {
  std::string small_html_meta_tag;
  base::win::ScopedComPtr<IStream> stream_with_meta;
  SniffData sniffer;

  EXPECT_HRESULT_SUCCEEDED(sniffer.InitializeCache(std::wstring()));
  ASSERT_TRUE(ReadFileAsString(kSmallHtmlMetaTag, &small_html_meta_tag));
  ASSERT_TRUE(StringToStream(small_html_meta_tag, stream_with_meta.Receive()));
  EXPECT_HRESULT_SUCCEEDED(sniffer.ReadIntoCache(stream_with_meta, true));

  CComObjectStackEx<MockBindStatusCallbackImpl> mock;
  const CLIPFORMAT cf = 0xd0d0;
  const DWORD flags = BSCF_FIRSTDATANOTIFICATION | BSCF_LASTDATANOTIFICATION;
  const DWORD data_size = small_html_meta_tag.size();

  DWORD read_size1 = data_size * 2;
  scoped_array<char> read_buffer1(new char[read_size1]);
  ZeroMemory(read_buffer1.get(), read_size1);

  char read_buffer2[10] = {0};
  DWORD read_size2 = sizeof(read_buffer2);
  HRESULT ret1 = E_FAIL, ret2 = E_FAIL;

  EXPECT_CALL(mock, OnDataAvailable(flags, data_size,
                    testing::Field(&FORMATETC::cfFormat, cf),
                    testing::Field(&STGMEDIUM::tymed, TYMED_ISTREAM)))
      .WillOnce(testing::DoAll(
          ReadStream(read_buffer1.get(), &read_size1, &ret1),
          ReadStream(read_buffer2, &read_size2, &ret2),
          Return(S_OK)));

  EXPECT_HRESULT_SUCCEEDED(sniffer.DrainCache(&mock, flags, cf));

  EXPECT_HRESULT_SUCCEEDED(ret1);
  EXPECT_EQ(small_html_meta_tag, read_buffer1.get());
  EXPECT_EQ(data_size, read_size1);

  EXPECT_EQ(S_FALSE, ret2);
  EXPECT_STREQ("", read_buffer2);
  EXPECT_EQ(sizeof(read_buffer2), read_size2);
}

// case 2: callback reads data in 2 reads.
TEST_F(MonikerPatchTest, SniffDataPlayback2) {
  std::string small_html_meta_tag;
  base::win::ScopedComPtr<IStream> stream_with_meta;
  SniffData sniffer;

  EXPECT_HRESULT_SUCCEEDED(sniffer.InitializeCache(std::wstring()));
  ASSERT_TRUE(ReadFileAsString(kSmallHtmlMetaTag, &small_html_meta_tag));
  ASSERT_TRUE(StringToStream(small_html_meta_tag, stream_with_meta.Receive()));
  EXPECT_HRESULT_SUCCEEDED(sniffer.ReadIntoCache(stream_with_meta, true));

  CComObjectStackEx<MockBindStatusCallbackImpl> mock;
  const CLIPFORMAT cf = 0xd0d0;
  const DWORD flags = BSCF_FIRSTDATANOTIFICATION | BSCF_LASTDATANOTIFICATION;
  const DWORD data_size = small_html_meta_tag.size();

  DWORD read_size1 = data_size / 2; // First read is half the data.
  DWORD read_size2 = data_size; // Second read, try to read past data.
  scoped_array<char> read_buffer1(new char[read_size1]);
  scoped_array<char> read_buffer2(new char[read_size2]);
  ZeroMemory(read_buffer1.get(), read_size1);
  ZeroMemory(read_buffer2.get(), read_size2);

  char read_buffer3[10] = {0};
  DWORD read_size3 = sizeof(read_buffer3);
  HRESULT ret1 = E_FAIL, ret2 = E_FAIL, ret3 = E_FAIL;

  EXPECT_CALL(mock, OnDataAvailable(flags, data_size,
                    testing::Field(&FORMATETC::cfFormat, cf),
                    testing::Field(&STGMEDIUM::tymed, TYMED_ISTREAM)))
      .WillOnce(testing::DoAll(
          ReadStream(read_buffer1.get(), &read_size1, &ret1),
          ReadStream(read_buffer2.get(), &read_size2, &ret2),
          ReadStream(read_buffer3, &read_size3, &ret3),
          Return(S_OK)));

  EXPECT_HRESULT_SUCCEEDED(sniffer.DrainCache(&mock, flags, cf));

  EXPECT_HRESULT_SUCCEEDED(ret1);
  EXPECT_HRESULT_SUCCEEDED(ret2);
  EXPECT_EQ(data_size/2, read_size1);
  EXPECT_EQ(data_size - read_size1, read_size2);

  std::string data_read;
  data_read.append(read_buffer1.get(), read_size1);
  data_read.append(read_buffer2.get(), read_size2);
  EXPECT_EQ(small_html_meta_tag, data_read);

  EXPECT_EQ(S_FALSE, ret3);
  EXPECT_STREQ("", read_buffer3);
  EXPECT_EQ(sizeof(read_buffer3), read_size3);
}
