// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/in_memory_download.h"

#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
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
    blob_storage_context_ = std::make_unique<storage::BlobStorageContext>();
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

 private:
  // IO thread used by network and blob IO tasks.
  std::unique_ptr<base::Thread> io_thread_;

  // Message loop for the main thread.
  base::MessageLoop main_loop;

  std::unique_ptr<InMemoryDownloadImpl> download_;
  MockDelegate mock_delegate_;

  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
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

  base::FilePath path = GetTestDataDirectory().AppendASCII("text_data.json");
  std::string data;
  EXPECT_TRUE(ReadFileToString(path, &data));
  EXPECT_EQ(InMemoryDownload::State::COMPLETE, download()->state());
  // TODO(xingliu): Read the blob and verify data.
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

  base::FilePath path = GetTestDataDirectory().AppendASCII("text_data.json");
  EXPECT_EQ(InMemoryDownload::State::COMPLETE, download()->state());
  // TODO(xingliu): Read the blob and verify data.
}

}  // namespace

}  // namespace download
