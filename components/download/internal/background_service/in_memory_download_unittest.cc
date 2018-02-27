// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/in_memory_download.h"

#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {
namespace {

// Posts a dummy task on |task_runner| and wait for its callback, to drain all
// previous tasks on |task_runner|.
void DrainPreviousTasks(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  base::RunLoop run_loop;

  auto dummy_task = []() {};
  task_runner->PostTaskAndReply(FROM_HERE, base::BindRepeating(dummy_task),
                                run_loop.QuitClosure());

  run_loop.Run();
}

// Dummy callback used for IO_PENDING state in blob operations, this is not
// called when the blob operation is done, but called when chained with other
// IO operations that might return IO_PENDING.
template <typename T>
void SetValue(T* address, T value) {
  *address = value;
}

class MockDelegate : public InMemoryDownload::Delegate {
 public:
  MockDelegate() = default;

  void WaitForCompletion() {
    DCHECK(!run_loop_.running());
    run_loop_.Run();
  }

  // InMemoryDownload::Delegate implementation.
  MOCK_METHOD1(OnDownloadProgress, void(InMemoryDownload*));
  void OnDownloadComplete(InMemoryDownload* download) {
    if (run_loop_.running())
      run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

// Must run on IO thread task runner.
base::WeakPtr<storage::BlobStorageContext> BlobStorageContextGetter(
    storage::BlobStorageContext* blob_context) {
  DCHECK(blob_context);
  return blob_context->AsWeakPtr();
}

class InMemoryDownloadTest : public testing::Test {
 public:
  InMemoryDownloadTest() = default;
  ~InMemoryDownloadTest() override = default;

  void SetUp() override {
    test_server_.ServeFilesFromDirectory(GetTestDataDirectory());
    ASSERT_TRUE(test_server_.Start());

    io_thread_.reset(new base::Thread("Network and Blob IO thread"));
    base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
    io_thread_->StartWithOptions(options);
    request_context_getter_ =
        new net::TestURLRequestContextGetter(io_thread_->task_runner());
    base::RunLoop loop;
    io_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          blob_storage_context_ =
              std::make_unique<storage::BlobStorageContext>();
          loop.Quit();
        }));
    loop.Run();
  }

  void TearDown() override {
    // Say goodbye to |blob_storage_context_| on IO thread.
    io_thread_->task_runner()->DeleteSoon(FROM_HERE,
                                          blob_storage_context_.release());
  }

 protected:
  base::FilePath GetTestDataDirectory() {
    base::FilePath test_data_dir;
    EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    return test_data_dir.AppendASCII("components/test/data/download");
  }

  // Helper method to create a download with request_params.
  void CreateDownload(const RequestParams& request_params) {
    download_ = std::make_unique<InMemoryDownloadImpl>(
        base::GenerateGUID(), request_params, TRAFFIC_ANNOTATION_FOR_TESTS,
        delegate(), request_context_getter_,
        base::BindRepeating(&BlobStorageContextGetter,
                            blob_storage_context_.get()),
        io_thread_->task_runner());
  }

  InMemoryDownload* download() { return download_.get(); }
  MockDelegate* delegate() { return &mock_delegate_; }
  net::EmbeddedTestServer* test_server() { return &test_server_; }
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter() {
    return request_context_getter_;
  }

  // Verifies if data read from |blob| is identical as |expected|.
  void VerifyBlobData(const std::string& expected,
                      storage::BlobDataHandle* blob) {
    base::RunLoop run_loop;
    // BlobReader needs to work on IO thread of BlobStorageContext.
    io_thread_->task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&InMemoryDownloadTest::VerifyBlobDataOnIO,
                       base::Unretained(this), expected, blob),
        run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  void VerifyBlobDataOnIO(const std::string& expected,
                          storage::BlobDataHandle* blob) {
    DCHECK(blob);
    int bytes_read = 0;
    int async_bytes_read = 0;
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(expected.size()));

    auto blob_reader = blob->CreateReader();

    int blob_size = 0;
    blob_reader->CalculateSize(base::BindRepeating(&SetValue<int>, &blob_size));
    EXPECT_EQ(blob_size, 0) << "In memory blob read data synchronously.";
    EXPECT_FALSE(blob->IsBeingBuilt())
        << "InMemoryDownload ensures blob construction completed.";
    storage::BlobReader::Status status = blob_reader->Read(
        buffer.get(), expected.size(), &bytes_read,
        base::BindRepeating(&SetValue<int>, &async_bytes_read));
    EXPECT_EQ(storage::BlobReader::Status::DONE, status);
    EXPECT_EQ(bytes_read, static_cast<int>(expected.size()));
    EXPECT_EQ(async_bytes_read, 0);
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(expected[i], buffer->data()[i]);
    }
  }

  // IO thread used by network and blob IO tasks.
  std::unique_ptr<base::Thread> io_thread_;

  // Created before other objects to provide test environment.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<InMemoryDownloadImpl> download_;
  MockDelegate mock_delegate_;

  // Used by URLFetcher network backend.
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;

  // Memory backed blob storage that can never page to disk.
  std::unique_ptr<storage::BlobStorageContext> blob_storage_context_;

  net::EmbeddedTestServer test_server_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryDownloadTest);
};

TEST_F(InMemoryDownloadTest, DownloadTest) {
  RequestParams request_params;
  request_params.url = test_server()->GetURL("/text_data.json");
  CreateDownload(request_params);
  download()->Start();
  delegate()->WaitForCompletion();

  EXPECT_EQ(InMemoryDownload::State::COMPLETE, download()->state());
  auto blob = download()->ResultAsBlob();

  std::string expected;
  EXPECT_TRUE(ReadFileToString(
      GetTestDataDirectory().AppendASCII("text_data.json"), &expected));
  VerifyBlobData(expected, blob.get());
}

TEST_F(InMemoryDownloadTest, PauseResume) {
  RequestParams request_params;
  request_params.url = test_server()->GetURL("/text_data.json");
  CreateDownload(request_params);

  // Pause before sending request.
  download()->Pause();
  download()->Start();

  // Force to return ERR_IO_PENDING on network thread in
  // InMemoryDownloadImpl::ResponseWriter::Write.
  DrainPreviousTasks(request_context_getter()->GetNetworkTaskRunner());

  download()->Resume();
  delegate()->WaitForCompletion();

  EXPECT_EQ(InMemoryDownload::State::COMPLETE, download()->state());
  auto blob = download()->ResultAsBlob();

  std::string expected;
  EXPECT_TRUE(ReadFileToString(
      GetTestDataDirectory().AppendASCII("text_data.json"), &expected));
  VerifyBlobData(expected, blob.get());
}

}  // namespace

}  // namespace download
