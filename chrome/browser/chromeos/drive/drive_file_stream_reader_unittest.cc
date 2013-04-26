// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_file_stream_reader.h"

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/drive/fake_drive_file_system.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/task_util.h"
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
namespace {

// Increments the |num_called|, when this method is invoked.
void IncrementCallback(int* num_called) {
  DCHECK(num_called);
  ++*num_called;
}

}  // namespace

TEST(LocalReaderProxyTest, Read) {
  // Prepare the test content.
  const base::FilePath kTestFile(
      google_apis::test_util::GetTestFilePath("chromeos/drive/applist.json"));
  std::string expected_content;
  ASSERT_TRUE(file_util::ReadFileToString(kTestFile, &expected_content));

  // The LocalReaderProxy should live on IO thread.
  MessageLoopForIO io_loop;
  content::TestBrowserThread io_thread(BrowserThread::IO, &io_loop);

  {
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

  // For graceful shutdown, we wait for that the FileStream used above is
  // actually closed.
  test_util::WaitForFileStreamClosed();
}

TEST(NetworkReaderProxyTest, EmptyFile) {
  // The NetworkReaderProxy should live on IO thread.
  MessageLoopForIO io_loop;
  content::TestBrowserThread io_thread(BrowserThread::IO, &io_loop);

  NetworkReaderProxy proxy(0, 0, base::Bind(&base::DoNothing));

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

  NetworkReaderProxy proxy(0, 10, base::Bind(&base::DoNothing));

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

TEST(NetworkReaderProxyTest, ReadWithLimit) {
  // The NetworkReaderProxy should live on IO thread.
  MessageLoopForIO io_loop;
  content::TestBrowserThread io_thread(BrowserThread::IO, &io_loop);

  NetworkReaderProxy proxy(10, 10, base::Bind(&base::DoNothing));

  net::TestCompletionCallback callback;
  const int kBufferSize = 3;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));

  // If no data is available yet, ERR_IO_PENDING should be returned.
  int result = proxy.Read(buffer.get(), kBufferSize, callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, result);

  // And when the data is supplied, the callback will be called.
  scoped_ptr<std::string> data(new std::string("abcde"));
  proxy.OnGetContent(data.Pass());
  data.reset(new std::string("fgh"));
  proxy.OnGetContent(data.Pass());
  data.reset(new std::string("ijklmno"));
  proxy.OnGetContent(data.Pass());

  // The returned data should be fit to the buffer size.
  result = callback.GetResult(result);
  EXPECT_EQ(3, result);
  EXPECT_EQ("klm", std::string(buffer->data(), result));

  // The next Read should return immediately because there is pending data
  result = proxy.Read(buffer.get(), kBufferSize, callback.callback());
  EXPECT_EQ(2, result);
  EXPECT_EQ("no", std::string(buffer->data(), result));

  // Supply the data before calling Read operation.
  data.reset(new std::string("pqrs"));
  proxy.OnGetContent(data.Pass());
  data.reset(new std::string("tuvwxyz"));
  proxy.OnGetContent(data.Pass());  // 't' is the 20-th byte.

  // The data should be concatenated if possible.
  result = proxy.Read(buffer.get(), kBufferSize, callback.callback());
  EXPECT_EQ(3, result);
  EXPECT_EQ("pqr", std::string(buffer->data(), result));

  result = proxy.Read(buffer.get(), kBufferSize, callback.callback());
  EXPECT_EQ(2, result);
  EXPECT_EQ("st", std::string(buffer->data(), result));

  // The whole data is read, so Read() should return 0 immediately by then.
  result = proxy.Read(buffer.get(), kBufferSize, callback.callback());
  EXPECT_EQ(0, result);
}

TEST(NetworkReaderProxyTest, ErrorWithPendingCallback) {
  // The NetworkReaderProxy should live on IO thread.
  MessageLoopForIO io_loop;
  content::TestBrowserThread io_thread(BrowserThread::IO, &io_loop);

  NetworkReaderProxy proxy(0, 10, base::Bind(&base::DoNothing));

  net::TestCompletionCallback callback;
  const int kBufferSize = 3;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));

  // Set pending callback.
  int result = proxy.Read(buffer.get(), kBufferSize, callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, result);

  // Emulate that an error is found. The callback should be called internally.
  proxy.OnCompleted(FILE_ERROR_FAILED);
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

  NetworkReaderProxy proxy(0, 10, base::Bind(&base::DoNothing));

  net::TestCompletionCallback callback;
  const int kBufferSize = 3;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));

  // Supply the data before an error.
  scoped_ptr<std::string> data(new std::string("abcde"));
  proxy.OnGetContent(data.Pass());

  // Emulate that an error is found.
  proxy.OnCompleted(FILE_ERROR_FAILED);

  // The next Read call should return the error code, even if there is
  // pending data (the pending data should be released in OnCompleted.
  EXPECT_EQ(net::ERR_FAILED,
            proxy.Read(buffer.get(), kBufferSize, callback.callback()));
}

TEST(NetworkReaderProxyTest, CancelJob) {
  // The NetworkReaderProxy should live on IO thread.
  MessageLoopForIO io_loop;
  content::TestBrowserThread io_thread(BrowserThread::IO, &io_loop);

  int num_called = 0;
  {
    NetworkReaderProxy proxy(
        0, 0, base::Bind(&IncrementCallback, &num_called));
    proxy.OnCompleted(FILE_ERROR_OK);
    // Destroy the instance after the network operation is completed.
    // The cancelling callback shouldn't be called.
  }
  EXPECT_EQ(0, num_called);

  num_called = 0;
  {
    NetworkReaderProxy proxy(
        0, 0, base::Bind(&IncrementCallback, &num_called));
    // Destroy the instance before the network operation is completed.
    // The cancelling callback should be called.
  }
  EXPECT_EQ(1, num_called);
}

}  // namespace internal

class DriveFileStreamReaderTest : public ::testing::Test {
 protected:
  DriveFileStreamReaderTest()
      : ui_thread_(BrowserThread::UI),
        io_thread_(BrowserThread::IO, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    ui_thread_.Start();

    BrowserThread::PostTaskAndReply(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DriveFileStreamReaderTest::SetUpOnUIThread,
                   base::Unretained(this)),
        base::MessageLoop::QuitClosure());
    message_loop_.Run();
  }

  virtual void TearDown() OVERRIDE {
    BrowserThread::PostTaskAndReply(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DriveFileStreamReaderTest::TearDownOnUIThread,
                   base::Unretained(this)),
        base::MessageLoop::QuitClosure());
    message_loop_.Run();
  }

  void SetUpOnUIThread() {
    // Initialize FakeDriveService.
    fake_drive_service_.reset(new google_apis::FakeDriveService);
    fake_drive_service_->LoadResourceListForWapi(
        "chromeos/gdata/root_feed.json");
    fake_drive_service_->LoadAccountMetadataForWapi(
        "chromeos/gdata/account_metadata.json");
    fake_drive_service_->LoadAppListForDriveApi("chromeos/drive/applist.json");

    // Create a testee instance.
    fake_drive_file_system_.reset(
        new test_util::FakeDriveFileSystem(fake_drive_service_.get()));
    fake_drive_file_system_->Initialize();
  }

  void TearDownOnUIThread() {
    fake_drive_file_system_.reset();
    fake_drive_service_.reset();
  }

  DriveFileSystemInterface* GetDriveFileSystem() {
    return fake_drive_file_system_.get();
  }

  DriveFileStreamReader::DriveFileSystemGetter GetDriveFileSystemGetter() {
    return base::Bind(&DriveFileStreamReaderTest::GetDriveFileSystem,
                      base::Unretained(this));
  }

  MessageLoopForIO message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;

  scoped_ptr<google_apis::FakeDriveService> fake_drive_service_;
  scoped_ptr<test_util::FakeDriveFileSystem> fake_drive_file_system_;
};

TEST_F(DriveFileStreamReaderTest, Read) {
  const base::FilePath kDriveFile =
      util::GetDriveMyDriveRootPath().AppendASCII("File 1.txt");
  net::TestCompletionCallback callback;
  const int kBufferSize = 3;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));

  // Create the reader, and initialize it.
  // In this case, the file is not yet locally cached.
  scoped_ptr<DriveFileStreamReader> reader(
      new DriveFileStreamReader(GetDriveFileSystemGetter()));

  EXPECT_FALSE(reader->IsInitialized());

  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProto> entry;
  reader->Initialize(
      kDriveFile,
      google_apis::CreateComposedCallback(
          base::Bind(&google_apis::test_util::RunAndQuit),
                     google_apis::test_util::CreateCopyResultCallback(
                         &error, &entry)));
  message_loop_.Run();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(reader->IsInitialized());

  // Read data from the reader.
  size_t content_size = entry->file_info().size();
  std::string first_content;
  while (first_content.size() < content_size) {
    int result = reader->Read(buffer.get(), kBufferSize, callback.callback());
    result = callback.GetResult(result);
    ASSERT_GT(result, 0);
    first_content.append(buffer->data(), result);
  }

  EXPECT_EQ(content_size, first_content.size());

  // Create second instance and initialize it.
  // In this case, the file should be cached one.
  reader.reset(
      new DriveFileStreamReader(GetDriveFileSystemGetter()));
  EXPECT_FALSE(reader->IsInitialized());

  error = FILE_ERROR_FAILED;
  entry.reset();
  reader->Initialize(
      kDriveFile,
      google_apis::CreateComposedCallback(
          base::Bind(&google_apis::test_util::RunAndQuit),
                     google_apis::test_util::CreateCopyResultCallback(
                         &error, &entry)));
  message_loop_.Run();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(reader->IsInitialized());

  // The size should be same.
  EXPECT_EQ(content_size,
            static_cast<size_t>(entry->file_info().size()));

  // Read data from the reader, again.
  std::string second_content;
  while (second_content.size() < content_size) {
    int result = reader->Read(buffer.get(), kBufferSize, callback.callback());
    result = callback.GetResult(result);
    ASSERT_GT(result, 0);
    second_content.append(buffer->data(), result);
  }

  // The same content is expected.
  EXPECT_EQ(first_content, second_content);
}

}  // namespace drive
