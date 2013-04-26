// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_url_request_job.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive_file_stream_reader.h"
#include "chrome/browser/chromeos/drive/fake_drive_file_system.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/task_util.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/test_completion_callback.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace drive {
namespace {

// A simple URLRequestJobFactory implementation to create DriveURLReuqestJob.
class TestURLRequestJobFactory : public net::URLRequestJobFactory {
 public:
  TestURLRequestJobFactory(
      const DriveURLRequestJob::DriveFileSystemGetter& file_system_getter)
      : file_system_getter_(file_system_getter) {
  }

  virtual ~TestURLRequestJobFactory() {}

  // net::URLRequestJobFactory override:
  virtual net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    return new DriveURLRequestJob(file_system_getter_,
                                  request,
                                  network_delegate);
  }

  virtual bool IsHandledProtocol(const std::string& scheme) const OVERRIDE {
    return scheme == chrome::kDriveScheme;
  }

  virtual bool IsHandledURL(const GURL& url) const OVERRIDE {
    return url.is_valid() && IsHandledProtocol(url.scheme());
  }

 private:
  const DriveURLRequestJob::DriveFileSystemGetter file_system_getter_;

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
  DriveURLRequestJobTest() : ui_thread_(BrowserThread::UI),
                             io_thread_(BrowserThread::IO, &message_loop_) {
  }

  virtual ~DriveURLRequestJobTest() {
  }

  virtual void SetUp() OVERRIDE {
    ui_thread_.Start();

    BrowserThread::PostTaskAndReply(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DriveURLRequestJobTest::SetUpOnUIThread,
                   base::Unretained(this)),
        base::MessageLoop::QuitClosure());
    message_loop_.Run();

    test_network_delegate_.reset(new net::TestNetworkDelegate);
    test_url_request_job_factory_.reset(new TestURLRequestJobFactory(
        base::Bind(&DriveURLRequestJobTest::GetDriveFileSystem,
                   base::Unretained(this))));
    url_request_context_.reset(new net::URLRequestContext());
    url_request_context_->set_job_factory(test_url_request_job_factory_.get());
    url_request_context_->set_network_delegate(test_network_delegate_.get());
    test_delegate_.reset(new TestDelegate);
  }

  virtual void TearDown() OVERRIDE {
    test_delegate_.reset();
    url_request_context_.reset();
    test_url_request_job_factory_.reset();
    test_network_delegate_.reset();

    BrowserThread::PostTaskAndReply(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DriveURLRequestJobTest::TearDownOnUIThread,
                   base::Unretained(this)),
        base::MessageLoop::QuitClosure());
    message_loop_.Run();
  }

  void SetUpOnUIThread() {
    // Initialize FakeDriveService.
    fake_drive_service_.reset(new google_apis::FakeDriveService);
    ASSERT_TRUE(fake_drive_service_->LoadResourceListForWapi(
        "chromeos/gdata/root_feed.json"));
    ASSERT_TRUE(fake_drive_service_->LoadAccountMetadataForWapi(
        "chromeos/gdata/account_metadata.json"));
    ASSERT_TRUE(fake_drive_service_->LoadAppListForDriveApi(
        "chromeos/drive/applist.json"));

    // Initialize FakeDriveFileSystem.
    fake_drive_file_system_.reset(
        new test_util::FakeDriveFileSystem(fake_drive_service_.get()));
    ASSERT_TRUE(fake_drive_file_system_->InitializeForTesting());
  }

  void TearDownOnUIThread() {
    fake_drive_file_system_.reset();
    fake_drive_service_.reset();
  }

  DriveFileSystemInterface* GetDriveFileSystem() {
    return fake_drive_file_system_.get();
  }

  std::string ReadDriveFileSync(const base::FilePath& file_path) {
    net::TestCompletionCallback callback;
    const int kBufferSize = 100;
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));
    scoped_ptr<DriveFileStreamReader> reader(new DriveFileStreamReader(
        base::Bind(&DriveURLRequestJobTest::GetDriveFileSystem,
                   base::Unretained(this))));

    FileError error = FILE_ERROR_FAILED;
    scoped_ptr<DriveEntryProto> entry;
    reader->Initialize(
        file_path,
        google_apis::CreateComposedCallback(
            base::Bind(&google_apis::test_util::RunAndQuit),
            google_apis::test_util::CreateCopyResultCallback(
                &error, &entry)));
    message_loop_.Run();

    // Read data from the reader.
    size_t content_size = entry->file_info().size();
    std::string content;
    while (content.size() < content_size) {
      int result = reader->Read(buffer.get(), kBufferSize, callback.callback());
      result = callback.GetResult(result);
      content.append(buffer->data(), result);
    }

    return content;
  }

  MessageLoopForIO message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;

  scoped_ptr<google_apis::FakeDriveService> fake_drive_service_;
  scoped_ptr<test_util::FakeDriveFileSystem> fake_drive_file_system_;

  scoped_ptr<net::TestNetworkDelegate> test_network_delegate_;
  scoped_ptr<TestURLRequestJobFactory> test_url_request_job_factory_;
  scoped_ptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<TestDelegate> test_delegate_;
};

TEST_F(DriveURLRequestJobTest, NonGetMethod) {
  net::URLRequest request(
      GURL("drive:drive/root/File 1.txt"), test_delegate_.get(),
      url_request_context_.get(), test_network_delegate_.get());
  request.set_method("POST");  // Set non "GET" method.
  request.Start();

  MessageLoop::current()->Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_METHOD_NOT_SUPPORTED, request.status().error());
}

TEST_F(DriveURLRequestJobTest, RegularFile) {
  const GURL kTestUrl("drive:drive/root/File 1.txt");
  const base::FilePath kTestFilePath("drive/root/File 1.txt");

  // For the first time, the file should be fetched from the server.
  {
    net::URLRequest request(
        kTestUrl, test_delegate_.get(),
        url_request_context_.get(), test_network_delegate_.get());
    request.Start();

    MessageLoop::current()->Run();

    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request.status().status());
    // It looks weird, but the mime type for the "File 1.txt" is "audio/mpeg"
    // on the server.
    std::string mime_type;
    request.GetMimeType(&mime_type);
    EXPECT_EQ("audio/mpeg", mime_type);
    // Reading file must be done after |request| runs, otherwise
    // it'll create a local cache file, and we cannot test correctly.
    EXPECT_EQ(ReadDriveFileSync(kTestFilePath),
              test_delegate_->data_received());
  }

  // For the second time, the locally cached file should be used.
  // The caching emulation is done by FakeDriveFileSystem.
  {
    test_delegate_.reset(new TestDelegate);
    net::URLRequest request(
        GURL("drive:drive/root/File 1.txt"), test_delegate_.get(),
        url_request_context_.get(), test_network_delegate_.get());
    request.Start();

    MessageLoop::current()->Run();

    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request.status().status());
    std::string mime_type;
    request.GetMimeType(&mime_type);
    EXPECT_EQ("audio/mpeg", mime_type);
    EXPECT_EQ(ReadDriveFileSync(kTestFilePath),
              test_delegate_->data_received());
  }
}

TEST_F(DriveURLRequestJobTest, HostedDocument) {
  // Open a gdoc file.
  test_delegate_->set_quit_on_redirect(true);
  net::URLRequest request(
      GURL("drive:drive/root/Document 1 excludeDir-test.gdoc"),
      test_delegate_.get(),
      url_request_context_.get(), test_network_delegate_.get());
  request.Start();

  MessageLoop::current()->Run();

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request.status().status());
  // Make sure that a hosted document triggers redirection.
  EXPECT_TRUE(request.is_redirecting());
  EXPECT_EQ(GURL("https://3_document_alternate_link"),
            test_delegate_->redirect_url());
}

TEST_F(DriveURLRequestJobTest, RootDirectory) {
  net::URLRequest request(
      GURL("drive:drive/root"), test_delegate_.get(),
      url_request_context_.get(), test_network_delegate_.get());
  request.Start();

  MessageLoop::current()->Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_FAILED, request.status().error());
}

TEST_F(DriveURLRequestJobTest, Directory) {
  net::URLRequest request(
      GURL("drive:drive/root/Directory 1"), test_delegate_.get(),
      url_request_context_.get(), test_network_delegate_.get());
  request.Start();

  MessageLoop::current()->Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_FAILED, request.status().error());
}

TEST_F(DriveURLRequestJobTest, NonExistingFile) {
  net::URLRequest request(
      GURL("drive:drive/root/non-existing-file.txt"), test_delegate_.get(),
      url_request_context_.get(), test_network_delegate_.get());
  request.Start();

  MessageLoop::current()->Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request.status().error());
}

TEST_F(DriveURLRequestJobTest, WrongFormat) {
  net::URLRequest request(
      GURL("drive:"), test_delegate_.get(),
      url_request_context_.get(), test_network_delegate_.get());
  request.Start();

  MessageLoop::current()->Run();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_INVALID_URL, request.status().error());
}

TEST_F(DriveURLRequestJobTest, Cancel) {
  net::URLRequest request(
      GURL("drive:drive/root/File 1.txt"), test_delegate_.get(),
      url_request_context_.get(), test_network_delegate_.get());

  // Start the request, and cancel it immediately after it.
  request.Start();
  request.Cancel();

  MessageLoop::current()->Run();

  EXPECT_EQ(net::URLRequestStatus::CANCELED, request.status().status());
}

}  // namespace drive
