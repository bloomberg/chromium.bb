// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_url_request_job.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/drive/drive_file_stream_reader.h"
#include "chrome/browser/chromeos/drive/fake_file_system.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/drive/test_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/test_util.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace drive {
namespace {

// A simple URLRequestJobFactory implementation to create DriveURLRequestJob.
class TestURLRequestJobFactory : public net::URLRequestJobFactory {
 public:
  TestURLRequestJobFactory(
      const DriveURLRequestJob::FileSystemGetter& file_system_getter,
      base::SequencedTaskRunner* sequenced_task_runner)
      : file_system_getter_(file_system_getter),
        sequenced_task_runner_(sequenced_task_runner) {
  }

  virtual ~TestURLRequestJobFactory() {}

  // net::URLRequestJobFactory override:
  virtual net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    return new DriveURLRequestJob(file_system_getter_,
                                  sequenced_task_runner_.get(),
                                  request,
                                  network_delegate);
  }

  virtual bool IsHandledProtocol(const std::string& scheme) const OVERRIDE {
    return scheme == chrome::kDriveScheme;
  }

  virtual bool IsHandledURL(const GURL& url) const OVERRIDE {
    return url.is_valid() && IsHandledProtocol(url.scheme());
  }

  virtual bool IsSafeRedirectTarget(const GURL& location) const OVERRIDE {
    return true;
  }

 private:
  const DriveURLRequestJob::FileSystemGetter file_system_getter_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestJobFactory);
};

class TestDelegate : public net::TestDelegate {
 public:
  TestDelegate() {}

  const GURL& redirect_url() const { return redirect_url_; }

  // net::TestDelegate override.
  virtual void OnReceivedRedirect(net::URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE{
    redirect_url_ = new_url;
    net::TestDelegate::OnReceivedRedirect(request, new_url, defer_redirect);
  }

 private:
  GURL redirect_url_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

class DriveURLRequestJobTest : public testing::Test {
 protected:
  DriveURLRequestJobTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
  }

  virtual ~DriveURLRequestJobTest() {
  }

  virtual void SetUp() OVERRIDE {
    // Initialize FakeDriveService.
    fake_drive_service_.reset(new FakeDriveService);
    ASSERT_TRUE(test_util::SetUpTestEntries(fake_drive_service_.get()));

    // Initialize FakeFileSystem.
    fake_file_system_.reset(
        new test_util::FakeFileSystem(fake_drive_service_.get()));

    scoped_refptr<base::SequencedWorkerPool> blocking_pool =
        content::BrowserThread::GetBlockingPool();
    test_network_delegate_.reset(new net::TestNetworkDelegate);
    test_url_request_job_factory_.reset(new TestURLRequestJobFactory(
        base::Bind(&DriveURLRequestJobTest::GetFileSystem,
                   base::Unretained(this)),
        blocking_pool->GetSequencedTaskRunner(
            blocking_pool->GetSequenceToken()).get()));
    url_request_context_.reset(new net::URLRequestContext());
    url_request_context_->set_job_factory(test_url_request_job_factory_.get());
    url_request_context_->set_network_delegate(test_network_delegate_.get());
    test_delegate_.reset(new TestDelegate);
  }

  FileSystemInterface* GetFileSystem() {
    return fake_file_system_.get();
  }

  bool ReadDriveFileSync(
      const base::FilePath& file_path, std::string* out_content) {
    scoped_ptr<base::Thread> worker_thread(
        new base::Thread("ReadDriveFileSync"));
    if (!worker_thread->Start())
      return false;

    scoped_ptr<DriveFileStreamReader> reader(new DriveFileStreamReader(
        base::Bind(&DriveURLRequestJobTest::GetFileSystem,
                   base::Unretained(this)),
        worker_thread->message_loop_proxy().get()));
    int error = net::ERR_FAILED;
    scoped_ptr<ResourceEntry> entry;
    {
      base::RunLoop run_loop;
      reader->Initialize(
          file_path,
          net::HttpByteRange(),
          google_apis::test_util::CreateQuitCallback(
              &run_loop,
              google_apis::test_util::CreateCopyResultCallback(
                  &error, &entry)));
      run_loop.Run();
    }
    if (error != net::OK || !entry)
      return false;

    // Read data from the reader.
    std::string content;
    if (test_util::ReadAllData(reader.get(), &content) != net::OK)
      return false;

    if (static_cast<size_t>(entry->file_info().size()) != content.size())
      return false;

    *out_content = content;
    return true;
  }

  content::TestBrowserThreadBundle thread_bundle_;

  scoped_ptr<FakeDriveService> fake_drive_service_;
  scoped_ptr<test_util::FakeFileSystem> fake_file_system_;

  scoped_ptr<net::TestNetworkDelegate> test_network_delegate_;
  scoped_ptr<TestURLRequestJobFactory> test_url_request_job_factory_;
  scoped_ptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<TestDelegate> test_delegate_;
};

TEST_F(DriveURLRequestJobTest, NonGetMethod) {
  net::URLRequest request(GURL("drive:drive/root/File 1.txt"),
                          net::DEFAULT_PRIORITY,
                          test_delegate_.get(),
                          url_request_context_.get());
  request.set_method("POST");  // Set non "GET" method.
  request.Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_METHOD_NOT_SUPPORTED, request.status().error());
}

TEST_F(DriveURLRequestJobTest, RegularFile) {
  const GURL kTestUrl("drive:drive/root/File 1.txt");
  const base::FilePath kTestFilePath("drive/root/File 1.txt");

  // For the first time, the file should be fetched from the server.
  {
    net::URLRequest request(kTestUrl,
                            net::DEFAULT_PRIORITY,
                            test_delegate_.get(),
                            url_request_context_.get());
    request.Start();

    base::RunLoop().Run();

    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request.status().status());
    // It looks weird, but the mime type for the "File 1.txt" is "audio/mpeg"
    // on the server.
    std::string mime_type;
    request.GetMimeType(&mime_type);
    EXPECT_EQ("audio/mpeg", mime_type);

    // Reading file must be done after |request| runs, otherwise
    // it'll create a local cache file, and we cannot test correctly.
    std::string expected_data;
    ASSERT_TRUE(ReadDriveFileSync(kTestFilePath, &expected_data));
    EXPECT_EQ(expected_data, test_delegate_->data_received());
  }

  // For the second time, the locally cached file should be used.
  // The caching emulation is done by FakeFileSystem.
  {
    test_delegate_.reset(new TestDelegate);
    net::URLRequest request(GURL("drive:drive/root/File 1.txt"),
                            net::DEFAULT_PRIORITY,
                            test_delegate_.get(),
                            url_request_context_.get());
    request.Start();

    base::RunLoop().Run();

    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request.status().status());
    std::string mime_type;
    request.GetMimeType(&mime_type);
    EXPECT_EQ("audio/mpeg", mime_type);

    std::string expected_data;
    ASSERT_TRUE(ReadDriveFileSync(kTestFilePath, &expected_data));
    EXPECT_EQ(expected_data, test_delegate_->data_received());
  }
}

TEST_F(DriveURLRequestJobTest, HostedDocument) {
  // Open a gdoc file.
  test_delegate_->set_quit_on_redirect(true);
  net::URLRequest request(
      GURL("drive:drive/root/Document 1 excludeDir-test.gdoc"),
      net::DEFAULT_PRIORITY,
      test_delegate_.get(),
      url_request_context_.get());
  request.Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request.status().status());
  // Make sure that a hosted document triggers redirection.
  EXPECT_TRUE(request.is_redirecting());
  EXPECT_TRUE(test_delegate_->redirect_url().is_valid());
}

TEST_F(DriveURLRequestJobTest, RootDirectory) {
  net::URLRequest request(GURL("drive:drive/root"),
                          net::DEFAULT_PRIORITY,
                          test_delegate_.get(),
                          url_request_context_.get());
  request.Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_FAILED, request.status().error());
}

TEST_F(DriveURLRequestJobTest, Directory) {
  net::URLRequest request(GURL("drive:drive/root/Directory 1"),
                          net::DEFAULT_PRIORITY,
                          test_delegate_.get(),
                          url_request_context_.get());
  request.Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_FAILED, request.status().error());
}

TEST_F(DriveURLRequestJobTest, NonExistingFile) {
  net::URLRequest request(GURL("drive:drive/root/non-existing-file.txt"),
                          net::DEFAULT_PRIORITY,
                          test_delegate_.get(),
                          url_request_context_.get());
  request.Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request.status().error());
}

TEST_F(DriveURLRequestJobTest, WrongFormat) {
  net::URLRequest request(GURL("drive:"),
                          net::DEFAULT_PRIORITY,
                          test_delegate_.get(),
                          url_request_context_.get());
  request.Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_INVALID_URL, request.status().error());
}

TEST_F(DriveURLRequestJobTest, Cancel) {
  net::URLRequest request(GURL("drive:drive/root/File 1.txt"),
                          net::DEFAULT_PRIORITY,
                          test_delegate_.get(),
                          url_request_context_.get());

  // Start the request, and cancel it immediately after it.
  request.Start();
  request.Cancel();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::CANCELED, request.status().status());
}

TEST_F(DriveURLRequestJobTest, RangeHeader) {
  const GURL kTestUrl("drive:drive/root/File 1.txt");
  const base::FilePath kTestFilePath("drive/root/File 1.txt");

  net::URLRequest request(kTestUrl,
                          net::DEFAULT_PRIORITY,
                          test_delegate_.get(),
                          url_request_context_.get());

  // Set range header.
  request.SetExtraRequestHeaderByName(
      "Range", "bytes=3-5", false /* overwrite */);
  request.Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request.status().status());

  // Reading file must be done after |request| runs, otherwise
  // it'll create a local cache file, and we cannot test correctly.
  std::string expected_data;
  ASSERT_TRUE(ReadDriveFileSync(kTestFilePath, &expected_data));
  EXPECT_EQ(expected_data.substr(3, 3), test_delegate_->data_received());
}

TEST_F(DriveURLRequestJobTest, WrongRangeHeader) {
  const GURL kTestUrl("drive:drive/root/File 1.txt");

  net::URLRequest request(kTestUrl,
                          net::DEFAULT_PRIORITY,
                          test_delegate_.get(),
                          url_request_context_.get());

  // Set range header.
  request.SetExtraRequestHeaderByName(
      "Range", "Wrong Range Header Value", false /* overwrite */);
  request.Start();

  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE, request.status().error());
}

}  // namespace drive
