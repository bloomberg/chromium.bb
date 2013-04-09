// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_url_request_job.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

class DriveFileSystem;

namespace {

DriveFileSystemInterface* GetNullDriveFileSystem() {
  return NULL;
}

}  // namespace

class DriveURLRequestJobTest : public testing::Test {
 protected:
  DriveURLRequestJobTest()
      : io_thread_(content::BrowserThread::IO, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    url_request_context_.reset(new net::TestURLRequestContext);
    delegate_.reset(new net::TestDelegate);
    network_delegate_.reset(new net::TestNetworkDelegate);

    // TODO(tedv): Using the NetworkDelegate with the URLRequestContext
    // with set_network_delegate() instead of a NULL delegate causes
    // unit test failures, which should be fixed.  This occurs because
    // the TestNetworkDelegate generates a failure if an failed request
    // is generated before the OnBeforeURLRequest() method is called,
    // and DriveURLRequestJob::Start() does not call OnBeforeURLRequest().
    // There is further discussion of this at:
    // https://codereview.chromium.org/13079008/
    //url_request_context_.set_network_delegate(network_delegate_.get());
  }

  MessageLoopForIO message_loop_;
  content::TestBrowserThread io_thread_;

  scoped_ptr<net::TestURLRequestContext> url_request_context_;
  scoped_ptr<net::TestDelegate> delegate_;
  scoped_ptr<net::TestNetworkDelegate> network_delegate_;
};

TEST_F(DriveURLRequestJobTest, NonGetMethod) {
  net::TestURLRequest request(
      util::FilePathToDriveURL(base::FilePath::FromUTF8Unsafe("file")),
      delegate_.get(), url_request_context_.get(), NULL);
  request.set_method("POST");  // Set non "GET" method.

  scoped_refptr<DriveURLRequestJob> job(
      new DriveURLRequestJob(
          base::Bind(&GetNullDriveFileSystem),
          &request, network_delegate_.get()));
  job->Start();
  MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(net::URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(net::ERR_METHOD_NOT_SUPPORTED, request.status().error());
}

}  // namespace drive
