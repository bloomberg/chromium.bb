// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"

#include "base/ref_counted.h"
#include "base/scoped_comptr_win.h"
#include "chrome_frame/urlmon_upload_data_stream.h"

TEST(UrlmonUploadDataStreamTest, TestBasicRead) {
  char random_string[] = "some random data, no really this totally random";
  int random_string_length = strlen(random_string);
  scoped_refptr<net::UploadData> upload_data = new net::UploadData();
  upload_data->AppendBytes(random_string, random_string_length);

  CComObject<UrlmonUploadDataStream>* upload_stream = NULL;
  HRESULT hr =
      CComObject<UrlmonUploadDataStream>::CreateInstance(&upload_stream);
  ASSERT_TRUE(SUCCEEDED(hr));

  upload_stream->Initialize(upload_data.get());
  ScopedComPtr<IStream> upload_istream(upload_stream);

  char buffer[500];
  memset(buffer, 0, 500);
  ULONG bytes_read = 0;
  hr = upload_istream->Read(buffer, 500, &bytes_read);

  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(bytes_read, random_string_length);
  EXPECT_TRUE(strcmp(buffer, random_string) == 0);

  char buffer2[500];
  memset(buffer2, 0, 500);
  ULONG bytes_read2 = 0;
  hr = upload_istream->Read(buffer2, 500, &bytes_read2);

  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(bytes_read2, 0);
  EXPECT_FALSE(strcmp(buffer2, random_string) == 0);
}

TEST(UrlmonUploadDataStreamTest, TestBigRead) {
  const size_t kBigBufferLength = 100000;
  char big_buffer[kBigBufferLength];
  memset(big_buffer, 'a', kBigBufferLength);

  scoped_refptr<net::UploadData> upload_data = new net::UploadData();
  upload_data->AppendBytes(big_buffer, kBigBufferLength);

  CComObject<UrlmonUploadDataStream>* upload_stream = NULL;
  HRESULT hr =
      CComObject<UrlmonUploadDataStream>::CreateInstance(&upload_stream);
  ASSERT_TRUE(SUCCEEDED(hr));

  upload_stream->Initialize(upload_data.get());
  ScopedComPtr<IStream> upload_istream(upload_stream);

  char big_rcv_buffer[kBigBufferLength];
  int write_pos = 0;
  ULONG bytes_read = 0;
  hr = E_UNEXPECTED;

  while ((hr = upload_istream->Read(&big_rcv_buffer[write_pos],
                                    kBigBufferLength,
                                    &bytes_read)) != S_FALSE) {
    EXPECT_TRUE(SUCCEEDED(hr));
    EXPECT_GT(bytes_read, static_cast<ULONG>(0));

    write_pos += bytes_read;
    bytes_read = 0;
  }

  EXPECT_EQ(S_FALSE, hr);
  EXPECT_TRUE((write_pos + bytes_read) == kBigBufferLength);
  EXPECT_EQ(0, memcmp(big_buffer, big_rcv_buffer, kBigBufferLength));
}

TEST(UrlmonUploadDataStreamTest, TestStat) {
  char random_string[] = "some random data, no really this totally random";
  int random_string_length = strlen(random_string);
  scoped_refptr<net::UploadData> upload_data = new net::UploadData();
  upload_data->AppendBytes(random_string, random_string_length);

  CComObject<UrlmonUploadDataStream>* upload_stream = NULL;
  HRESULT hr =
      CComObject<UrlmonUploadDataStream>::CreateInstance(&upload_stream);
  ASSERT_TRUE(SUCCEEDED(hr));

  upload_stream->Initialize(upload_data.get());
  ScopedComPtr<IStream> upload_istream(upload_stream);

  STATSTG statstg;
  hr = upload_stream->Stat(&statstg, STATFLAG_NONAME);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(static_cast<LONGLONG>(random_string_length),
            statstg.cbSize.QuadPart);
}

TEST(UrlmonUploadDataStreamTest, TestRepeatedRead) {
  char random_string[] = "some random data, no really this totally random";
  int random_string_length = strlen(random_string);
  scoped_refptr<net::UploadData> upload_data = new net::UploadData();
  upload_data->AppendBytes(random_string, random_string_length);

  CComObject<UrlmonUploadDataStream>* upload_stream = NULL;
  HRESULT hr =
      CComObject<UrlmonUploadDataStream>::CreateInstance(&upload_stream);
  ASSERT_TRUE(SUCCEEDED(hr));

  upload_stream->Initialize(upload_data.get());
  ScopedComPtr<IStream> upload_istream(upload_stream);

  char buffer[500];
  memset(buffer, 0, 500);
  ULONG bytes_read = 0;
  hr = upload_istream->Read(buffer, 500, &bytes_read);

  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_EQ(bytes_read, random_string_length);
  EXPECT_EQ(0, strcmp(buffer, random_string));

  char buffer2[500];
  memset(buffer2, 0, 500);
  ULONG bytes_read2 = 0;

  for (int i = 0; i < 10; i++) {
    hr = upload_istream->Read(buffer2, 500, &bytes_read2);
    EXPECT_EQ(S_FALSE, hr);
    EXPECT_EQ(bytes_read2, 0);
    EXPECT_NE(0, strcmp(buffer2, random_string));
  }
}

TEST(UrlmonUploadDataStreamTest, TestZeroRead) {
  char random_string[] = "some random data, no really this totally random";
  int random_string_length = strlen(random_string);
  scoped_refptr<net::UploadData> upload_data = new net::UploadData();
  upload_data->AppendBytes(random_string, random_string_length);

  CComObject<UrlmonUploadDataStream>* upload_stream = NULL;
  HRESULT hr =
      CComObject<UrlmonUploadDataStream>::CreateInstance(&upload_stream);
  ASSERT_TRUE(SUCCEEDED(hr));

  upload_stream->Initialize(upload_data.get());
  ScopedComPtr<IStream> upload_istream(upload_stream);

  char buffer[500];
  memset(buffer, 0, 500);
  ULONG bytes_read = 42;
  hr = upload_istream->Read(&buffer[0], 0, &bytes_read);

  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0, bytes_read);

  char buffer2[500];
  memset(&buffer2[0], 0, 500);
  EXPECT_EQ(0, memcmp(buffer, buffer2, 500));
}

