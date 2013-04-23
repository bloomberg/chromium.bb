// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_url_request_job.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/fake_drive_file_system.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
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
  DriveURLRequestJob::DriveFileSystemGetter file_system_getter_;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestJobFactory);
};

// URLRequest::Delegate for tests.
class TestDelegate : public net::URLRequest::Delegate {
 public:
  TestDelegate() {}

  virtual ~TestDelegate() {}

  const net::URLRequestStatus& status() const { return status_; }
  const GURL& redirect_url() const { return redirect_url_; }

  // net::URLRequest::Delegate override:
  virtual void OnReceivedRedirect(net::URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE {
    // Save the new URL and cancel the request.
    EXPECT_TRUE(redirect_url_.is_empty());
    redirect_url_ = new_url;

    request->Cancel();
  }

  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE {
    if (!request->status().is_success())
      CompleteRequest(request);
  }

  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE {}

 private:
  void CompleteRequest(net::URLRequest* request) {
    status_ = request->status();
    delete request;

    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::MessageLoop::QuitClosure());
  }

  net::URLRequestStatus status_;
  GURL redirect_url_;
};

}  // namespace

class DriveURLRequestJobTest : public testing::Test {
 protected:
  DriveURLRequestJobTest() : ui_thread_(BrowserThread::UI, &message_loop_),
                             io_thread_(BrowserThread::IO) {
  }

  virtual void SetUp() OVERRIDE {
    io_thread_.StartIOThread();

    // Initialize FakeDriveService.
    drive_service_.reset(new google_apis::FakeDriveService);
    drive_service_->LoadResourceListForWapi("chromeos/gdata/root_feed.json");
    drive_service_->LoadAccountMetadataForWapi(
        "chromeos/gdata/account_metadata.json");
    drive_service_->LoadAppListForDriveApi("chromeos/drive/applist.json");

    // Initialize FakeDriveFileSystem.
    drive_file_system_.reset(
        new test_util::FakeDriveFileSystem(drive_service_.get()));
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&DriveURLRequestJobTest::SetUpOnIOThread,
                   base::Unretained(this)),
        base::MessageLoop::QuitClosure());
    message_loop_.Run();
  }

  virtual void TearDown() OVERRIDE {
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&DriveURLRequestJobTest::TearDownOnIOThread,
                   base::Unretained(this)),
        base::MessageLoop::QuitClosure());
    message_loop_.Run();
  }

  void SetUpOnIOThread() {
    // Initialize URLRequest related objects.
    delegate_.reset(new TestDelegate);
    network_delegate_.reset(new net::TestNetworkDelegate);
    url_request_job_factory_.reset(new TestURLRequestJobFactory(
        base::Bind(&DriveURLRequestJobTest::GetDriveFileSystem,
                   base::Unretained(this))));
    url_request_context_.reset(new net::URLRequestContext);
    url_request_context_->set_job_factory(url_request_job_factory_.get());
    url_request_context_->set_network_delegate(network_delegate_.get());
  }

  void TearDownOnIOThread() {
    url_request_context_.reset();
    url_request_job_factory_.reset();
    network_delegate_.reset();
    delegate_.reset();
  }

  DriveFileSystemInterface* GetDriveFileSystem() const {
    return drive_file_system_.get();
  }

  void RunRequest(const GURL& url, const std::string& method) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&DriveURLRequestJobTest::StartRequestOnIOThread,
                   base::Unretained(this), url, method));
    message_loop_.Run();
  }

  void StartRequestOnIOThread(const GURL& url, const std::string& method) {
    // |delegate_| will delete the request.
    net::URLRequest* request = new net::URLRequest(url, delegate_.get(),
                                                   url_request_context_.get(),
                                                   network_delegate_.get());
    request->set_method(method);
    request->Start();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;

  scoped_ptr<google_apis::FakeDriveService> drive_service_;
  scoped_ptr<test_util::FakeDriveFileSystem> drive_file_system_;
  scoped_ptr<TestDelegate> delegate_;
  scoped_ptr<net::NetworkDelegate> network_delegate_;
  scoped_ptr<TestURLRequestJobFactory> url_request_job_factory_;
  scoped_ptr<net::URLRequestContext> url_request_context_;
};

TEST_F(DriveURLRequestJobTest, NonGetMethod) {
  RunRequest(util::FilePathToDriveURL(base::FilePath::FromUTF8Unsafe("file")),
             "POST");  // Set non "GET" method.

  EXPECT_EQ(net::URLRequestStatus::FAILED, delegate_->status().status());
  EXPECT_EQ(net::ERR_METHOD_NOT_SUPPORTED, delegate_->status().error());
}

TEST_F(DriveURLRequestJobTest, HostedDocument) {
  // Get a hosted document's path.
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProto> entry;
  base::FilePath file_path;
  drive_file_system_->GetEntryInfoByResourceId(
      "document:5_document_resource_id",
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(entry);
  ASSERT_TRUE(entry->file_specific_info().is_hosted_document());

  // Run request for the document.
  RunRequest(util::FilePathToDriveURL(file_path), "GET");

  // Request was cancelled by the delegate because of redirect.
  EXPECT_EQ(net::URLRequestStatus::CANCELED, delegate_->status().status());
  EXPECT_EQ(GURL(entry->file_specific_info().alternate_url()),
            delegate_->redirect_url());
}

}  // namespace drive
