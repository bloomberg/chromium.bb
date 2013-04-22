// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_file_stream_reader.h"

#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace drive {
namespace internal {

TEST(LocalReaderProxyTest, Read) {
  // Prepare the test content.
  const base::FilePath kTestFile(
      google_apis::test_util::GetTestFilePath("chromeos/drive/applist.json"));
  std::string expected_content;
  ASSERT_TRUE(file_util::ReadFileToString(kTestFile, &expected_content));

  // The LocalReaderProxy should live on IO thread.
  MessageLoopForIO io_loop;
  content::TestBrowserThread io_thread(BrowserThread::IO, &io_loop);

  // Open the file first.
  scoped_ptr<net::FileStream> file_stream(new net::FileStream(NULL));
  net::TestCompletionCallback callback;
  int result = file_stream->Open(
      google_apis::test_util::GetTestFilePath("chromeos/drive/applist.json"),
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_ASYNC,
      callback.callback());
  ASSERT_EQ(net::OK, callback.GetResult(result));

  // Test instance.
  LocalReaderProxy proxy(file_stream.Pass());

  // Prepare the buffer, whose size is smaller than the whole data size.
  const int kBufferSize = 10;
  ASSERT_LE(static_cast<size_t>(kBufferSize), expected_content.size());
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));

  // Read repeatedly, until it is finished.
  std::string concatenated_content;
  while (concatenated_content.size() < expected_content.size()) {
    result = proxy.Read(buffer.get(), kBufferSize, callback.callback());
    result = callback.GetResult(result);

    // The read content size should be smaller than the buffer size.
    ASSERT_GT(result, 0);
    ASSERT_LE(result, kBufferSize);
    concatenated_content.append(buffer->data(), result);
  }

  // Make sure the read contant is as same as the file.
  EXPECT_EQ(expected_content, concatenated_content);
}

TEST(NetworkReaderProxyTest, EmptyFile) {
  // The NetworkReaderProxy should live on IO thread.
  MessageLoopForIO io_loop;
  content::TestBrowserThread io_thread(BrowserThread::IO, &io_loop);

  NetworkReaderProxy proxy(0);

  net::TestCompletionCallback callback;
  const int kBufferSize = 10;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));
  int result = proxy.Read(buffer.get(), kBufferSize, callback.callback());

  // For empty file, Read() should return 0 immediately.
  EXPECT_EQ(0, result);
}

TEST(NetworkReaderProxyTest, Read) {
  // The NetworkReaderProxy should live on IO thread.
  MessageLoopForIO io_loop;
  content::TestBrowserThread io_thread(BrowserThread::IO, &io_loop);

  NetworkReaderProxy proxy(10);

  net::TestCompletionCallback callback;
  const int kBufferSize = 3;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));

  // If no data is available yet, ERR_IO_PENDING should be returned.
  int result = proxy.Read(buffer.get(), kBufferSize, callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, result);

  // And when the data is supplied, the callback will be called.
  scoped_ptr<std::string> data(new std::string("abcde"));
  proxy.OnGetContent(data.Pass());

  // The returned data should be fit to the buffer size.
  result = callback.GetResult(result);
  EXPECT_EQ(3, result);
  EXPECT_EQ("abc", std::string(buffer->data(), result));

  // The next Read should return immediately because there is pending data
  result = proxy.Read(buffer.get(), kBufferSize, callback.callback());
  EXPECT_EQ(2, result);
  EXPECT_EQ("de", std::string(buffer->data(), result));

  // Supply the data before calling Read operation.
  data.reset(new std::string("fg"));
  proxy.OnGetContent(data.Pass());
  data.reset(new std::string("hij"));
  proxy.OnGetContent(data.Pass());  // Now 10 bytes are supplied.

  // The data should be concatenated if possible.
  result = proxy.Read(buffer.get(), kBufferSize, callback.callback());
  EXPECT_EQ(3, result);
  EXPECT_EQ("fgh", std::string(buffer->data(), result));

  result = proxy.Read(buffer.get(), kBufferSize, callback.callback());
  EXPECT_EQ(2, result);
  EXPECT_EQ("ij", std::string(buffer->data(), result));

  // The whole data is read, so Read() should return 0 immediately by then.
  result = proxy.Read(buffer.get(), kBufferSize, callback.callback());
  EXPECT_EQ(0, result);
}

TEST(NetworkReaderProxyTest, ErrorWithPendingCallback) {
  // The NetworkReaderProxy should live on IO thread.
  MessageLoopForIO io_loop;
  content::TestBrowserThread io_thread(BrowserThread::IO, &io_loop);

  NetworkReaderProxy proxy(10);

  net::TestCompletionCallback callback;
  const int kBufferSize = 3;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));

  // Set pending callback.
  int result = proxy.Read(buffer.get(), kBufferSize, callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, result);

  // Emulate that an error is found. The callback should be called internally.
  proxy.OnError(DRIVE_FILE_ERROR_FAILED);
  result = callback.GetResult(result);
  EXPECT_EQ(net::ERR_FAILED, result);

  // The next Read call should also return the same error code.
  EXPECT_EQ(net::ERR_FAILED,
            proxy.Read(buffer.get(), kBufferSize, callback.callback()));
}

TEST(NetworkReaderProxyTest, ErrorWithPendingData) {
  // The NetworkReaderProxy should live on IO thread.
  MessageLoopForIO io_loop;
  content::TestBrowserThread io_thread(BrowserThread::IO, &io_loop);

  NetworkReaderProxy proxy(10);

  net::TestCompletionCallback callback;
  const int kBufferSize = 3;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));

  // Supply the data before an error.
  scoped_ptr<std::string> data(new std::string("abcde"));
  proxy.OnGetContent(data.Pass());

  // Emulate that an error is found.
  proxy.OnError(DRIVE_FILE_ERROR_FAILED);

  // The next Read call should return the error code, even if there is
  // pending data (the pending data should be released in OnError.
  EXPECT_EQ(net::ERR_FAILED,
            proxy.Read(buffer.get(), kBufferSize, callback.callback()));
}

}  // namespace internal
}  // namespace drive
