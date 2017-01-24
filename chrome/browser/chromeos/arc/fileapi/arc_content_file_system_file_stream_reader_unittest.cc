// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_file_stream_reader.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/file_system/test/fake_arc_file_system_operation_runner.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/edk/embedder/embedder.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace arc {

namespace {

// URL which returns a file descriptor of a regular file.
constexpr char kArcUrlFile[] = "content://org.chromium.foo/file";

// URL which returns a file descriptor of a pipe's read end.
constexpr char kArcUrlPipe[] = "content://org.chromium.foo/pipe";

constexpr char kData[] = "abcdefghijklmnopqrstuvwxyz";

// Reads data from the reader to fill the buffer.
bool ReadData(ArcContentFileSystemFileStreamReader* reader,
              net::IOBufferWithSize* buffer) {
  auto drainable_buffer =
      make_scoped_refptr(new net::DrainableIOBuffer(buffer, buffer->size()));
  while (drainable_buffer->BytesRemaining()) {
    net::TestCompletionCallback callback;
    int result = callback.GetResult(
        reader->Read(drainable_buffer.get(), drainable_buffer->BytesRemaining(),
                     callback.callback()));
    if (result < 0) {
      LOG(ERROR) << "Read failed: " << result;
      return false;
    }
    drainable_buffer->DidConsume(result);
  }
  return true;
}

// TODO(crbug.com/683049): Use a generic FakeArcFileSystemOperationRunner.
class ArcFileSystemOperationRunnerForTest
    : public FakeArcFileSystemOperationRunner {
 public:
  ArcFileSystemOperationRunnerForTest(ArcBridgeService* bridge_service,
                                      const base::FilePath& file_path)
      : FakeArcFileSystemOperationRunner(bridge_service),
        file_path_(file_path) {}
  ~ArcFileSystemOperationRunnerForTest() override = default;

  void GetFileSize(const GURL& url,
                   const GetFileSizeCallback& callback) override {
    EXPECT_EQ(kArcUrlFile, url.spec());
    base::File::Info info;
    EXPECT_TRUE(base::GetFileInfo(file_path_, &info));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, info.size));
  }

  void OpenFileToRead(const GURL& url,
                      const OpenFileToReadCallback& callback) override {
    base::ScopedFD fd;
    if (url.spec() == kArcUrlFile) {
      fd = OpenRegularFileToRead();
    } else if (url.spec() == kArcUrlPipe) {
      fd = OpenPipeToRead();
    } else {
      LOG(ERROR) << "Unknown URL: " << url.spec();
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(callback, base::Passed(mojo::ScopedHandle())));
      return;
    }
    mojo::edk::ScopedPlatformHandle platform_handle(
        mojo::edk::PlatformHandle(fd.release()));
    MojoHandle wrapped_handle;
    EXPECT_EQ(MOJO_RESULT_OK, mojo::edk::CreatePlatformHandleWrapper(
                                  std::move(platform_handle), &wrapped_handle));

    mojo::ScopedHandle scoped_handle{mojo::Handle(wrapped_handle)};
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::Passed(std::move(scoped_handle))));
  }

 private:
  // Returns a ScopedFD to read |file_path_|.
  base::ScopedFD OpenRegularFileToRead() {
    base::File file(file_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);
    EXPECT_TRUE(file.IsValid());
    return base::ScopedFD(file.TakePlatformFile());
  }

  // Returns a pipe's read end with |file_path_|'s contents written.
  base::ScopedFD OpenPipeToRead() {
    // Create a new pipe.
    int fds[2];
    if (pipe(fds) != 0) {
      LOG(ERROR) << "pipe() failed.";
      return base::ScopedFD();
    }
    base::ScopedFD fd_read(fds[0]);
    base::ScopedFD fd_write(fds[1]);
    // Put the file's contents to the pipe.
    std::string contents;
    if (!base::ReadFileToString(file_path_, &contents) ||
        !base::WriteFileDescriptor(fd_write.get(), contents.data(),
                                   contents.length())) {
      LOG(ERROR) << "Failed to write the file contents to the pipe";
      return base::ScopedFD();
    }
    // Return the read end.
    return fd_read;
  }

  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(ArcFileSystemOperationRunnerForTest);
};

class ArcContentFileSystemFileStreamReaderTest : public testing::Test {
 public:
  ArcContentFileSystemFileStreamReaderTest() = default;

  ~ArcContentFileSystemFileStreamReaderTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    base::FilePath path = temp_dir_.GetPath().AppendASCII("bar");
    ASSERT_TRUE(base::WriteFile(path, kData, arraysize(kData)));

    arc_service_manager_ = base::MakeUnique<ArcServiceManager>(nullptr);
    arc_service_manager_->AddService(
        base::MakeUnique<ArcFileSystemOperationRunnerForTest>(
            arc_service_manager_->arc_bridge_service(), path));
  }

 private:
  base::ScopedTempDir temp_dir_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;

  DISALLOW_COPY_AND_ASSIGN(ArcContentFileSystemFileStreamReaderTest);
};

}  // namespace

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadRegularFile) {
  ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlFile), 0);
  auto buffer = make_scoped_refptr(new net::IOBufferWithSize(arraysize(kData)));
  EXPECT_TRUE(ReadData(&reader, buffer.get()));
  EXPECT_EQ(base::StringPiece(kData, arraysize(kData)),
            base::StringPiece(buffer->data(), buffer->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadRegularFileWithOffset) {
  constexpr size_t kOffset = 10;
  ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlFile), kOffset);
  auto buffer =
      make_scoped_refptr(new net::IOBufferWithSize(arraysize(kData) - kOffset));
  EXPECT_TRUE(ReadData(&reader, buffer.get()));
  EXPECT_EQ(base::StringPiece(kData + kOffset, arraysize(kData) - kOffset),
            base::StringPiece(buffer->data(), buffer->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadPipe) {
  ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlPipe), 0);
  auto buffer = make_scoped_refptr(new net::IOBufferWithSize(arraysize(kData)));
  EXPECT_TRUE(ReadData(&reader, buffer.get()));
  EXPECT_EQ(base::StringPiece(kData, arraysize(kData)),
            base::StringPiece(buffer->data(), buffer->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadPipeWithOffset) {
  constexpr size_t kOffset = 10;
  ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlPipe), kOffset);
  auto buffer =
      make_scoped_refptr(new net::IOBufferWithSize(arraysize(kData) - kOffset));
  EXPECT_TRUE(ReadData(&reader, buffer.get()));
  EXPECT_EQ(base::StringPiece(kData + kOffset, arraysize(kData) - kOffset),
            base::StringPiece(buffer->data(), buffer->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, GetLength) {
  ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlFile), 0);

  net::TestInt64CompletionCallback callback;
  EXPECT_EQ(static_cast<int64_t>(arraysize(kData)),
            callback.GetResult(reader.GetLength(callback.callback())));
}

}  // namespace arc
