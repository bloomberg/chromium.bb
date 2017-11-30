// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_task_impl.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#import "base/mac/bind_objc_block.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/testing/wait_util.h"
#import "ios/web/net/cookies/wk_cookie_util.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/test/fakes/crw_fake_nsurl_session_task.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::kWaitForDownloadTimeout;
using testing::kWaitForFileOperationTimeout;
using testing::WaitUntilConditionOrTimeout;
using base::BindBlockArc;

namespace web {

namespace {

const char kUrl[] = "chromium://download.test/";
const char kContentDisposition[] = "attachment; filename=file.test";
const char kMimeType[] = "application/pdf";

class MockDownloadTaskObserver : public DownloadTaskObserver {
 public:
  MOCK_METHOD1(OnDownloadUpdated, void(DownloadTask* task));
};

// Mocks DownloadTaskImpl::Delegate's OnTaskUpdated and OnTaskDestroyed methods
// and stubs DownloadTaskImpl::Delegate::CreateSession with session mock.
class FakeDownloadTaskImplDelegate : public DownloadTaskImpl::Delegate {
 public:
  FakeDownloadTaskImplDelegate()
      : configuration_([NSURLSessionConfiguration
            backgroundSessionConfigurationWithIdentifier:
                [NSUUID UUID].UUIDString]) {}

  MOCK_METHOD1(OnTaskDestroyed, void(DownloadTaskImpl* task));

  // Returns mock, which can be accessed via session() method.
  NSURLSession* CreateSession(NSString* identifier,
                              id<NSURLSessionDataDelegate> delegate,
                              NSOperationQueue* delegate_queue) {
    // Make sure this method is called only once.
    EXPECT_FALSE(session_);
    EXPECT_FALSE(session_delegate_);

    session_delegate_ = delegate;
    session_ = OCMStrictClassMock([NSURLSession class]);
    OCMStub([session_ configuration]).andReturn(configuration_);

    return session_;
  }

  // These methods return session objects injected into DownloadTaskImpl.
  NSURLSessionConfiguration* configuration() { return configuration_; }
  id session() { return session_; }
  id<NSURLSessionDataDelegate> session_delegate() { return session_delegate_; }

 private:
  id<NSURLSessionDataDelegate> session_delegate_;
  id configuration_;
  id session_;
};

}  //  namespace

// Test fixture for testing DownloadTaskImplTest class.
class DownloadTaskImplTest : public PlatformTest {
 protected:
  DownloadTaskImplTest()
      : task_(std::make_unique<DownloadTaskImpl>(
            &web_state_,
            GURL(kUrl),
            kContentDisposition,
            /*total_bytes=*/-1,
            kMimeType,
            task_delegate_.configuration().identifier,
            &task_delegate_)) {
    browser_state_.SetOffTheRecord(true);
    web_state_.SetBrowserState(&browser_state_);
    task_->AddObserver(&task_observer_);
  }

  ~DownloadTaskImplTest() {
    if (task_) {
      task_->RemoveObserver(&task_observer_);
    }
  }

  // Starts the download and return NSURLSessionDataTask fake for this task.
  CRWFakeNSURLSessionTask* Start(
      std::unique_ptr<net::URLFetcherResponseWriter> writer) {
    // Inject fake NSURLSessionDataTask into DownloadTaskImpl.
    NSURL* url = [NSURL URLWithString:@(kUrl)];
    CRWFakeNSURLSessionTask* session_task =
        [[CRWFakeNSURLSessionTask alloc] initWithURL:url];
    OCMExpect([task_delegate_.session() dataTaskWithURL:url])
        .andReturn(session_task);

    // Start the download.
    task_->Start(std::move(writer));
    bool success = WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
      base::RunLoop().RunUntilIdle();
      return session_task.state == NSURLSessionTaskStateRunning;
    });
    return success ? session_task : nil;
  }

  // Starts the download and return NSURLSessionDataTask fake for this task.
  // Same as above, but uses URLFetcherStringWriter as response writer.
  CRWFakeNSURLSessionTask* Start() {
    return Start(std::make_unique<net::URLFetcherStringWriter>());
  }

  // Sets cookie for the test browser state.
  bool SetCookie(NSHTTPCookie* cookie) WARN_UNUSED_RESULT
      API_AVAILABLE(ios(11.0)) {
    auto store = web::WKCookieStoreForBrowserState(&browser_state_);
    __block bool cookie_was_set = false;
    [store setCookie:cookie
        completionHandler:^{
          cookie_was_set = true;
        }];
    return WaitUntilConditionOrTimeout(testing::kWaitForCookiesTimeout, ^{
      return cookie_was_set;
    });
  }

  // Session and session delegate injected into DownloadTaskImpl for testing.
  NSURLSession* session() { return task_delegate_.session(); }
  id<NSURLSessionDataDelegate> session_delegate() {
    return task_delegate_.session_delegate();
  }

  // Updates NSURLSessionTask.countOfBytesReceived and calls
  // URLSession:dataTask:didReceiveData: callback. |data_str| is null terminated
  // C-string that represents the downloaded data.
  void SimulateDataDownload(CRWFakeNSURLSessionTask* session_task,
                            const char data_str[]) {
    session_task.countOfBytesReceived += strlen(data_str);
    NSData* data = [NSData dataWithBytes:data_str length:strlen(data_str)];
    [session_delegate() URLSession:session()
                          dataTask:session_task
                    didReceiveData:data];
  }

  // Sets NSURLSessionTask.state to NSURLSessionTaskStateCompleted and calls
  // URLSession:dataTask:didCompleteWithError: callback.
  void SimulateDownloadCompletion(CRWFakeNSURLSessionTask* session_task,
                                  NSError* error = nil) {
    session_task.state = NSURLSessionTaskStateCompleted;
    [session_delegate() URLSession:session()
                              task:session_task
              didCompleteWithError:error];
  }

  web::TestWebThreadBundle thread_bundle_;
  TestBrowserState browser_state_;
  TestWebState web_state_;
  testing::StrictMock<FakeDownloadTaskImplDelegate> task_delegate_;
  std::unique_ptr<DownloadTaskImpl> task_;
  MockDownloadTaskObserver task_observer_;
};

// Tests DownloadTaskImpl default state after construction.
TEST_F(DownloadTaskImplTest, DefaultState) {
  EXPECT_FALSE(task_->GetResponseWriter());
  EXPECT_NSEQ(task_delegate_.configuration().identifier,
              task_->GetIndentifier());
  EXPECT_EQ(kUrl, task_->GetOriginalUrl());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(-1, task_->GetTotalBytes());
  EXPECT_EQ(-1, task_->GetPercentComplete());
  EXPECT_EQ(kContentDisposition, task_->GetContentDisposition());
  EXPECT_EQ(kMimeType, task_->GetMimeType());
  EXPECT_EQ("file.test", base::UTF16ToUTF8(task_->GetSuggestedFilename()));

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests sucessfull download of response without content.
// (No URLSession:dataTask:didReceiveData: callback).
TEST_F(DownloadTaskImplTest, EmptyContentDownload) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // Download has finished.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  SimulateDownloadCompletion(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(0, task_->GetTotalBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests sucessfull download of response with only one
// URLSession:dataTask:didReceiveData: callback.
TEST_F(DownloadTaskImplTest, SmallResponseDownload) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // The response has arrived.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  const char kData[] = "foo";
  int64_t kDataSize = strlen(kData);
  session_task.countOfBytesExpectedToReceive = kDataSize;
  SimulateDataDownload(session_task, kData);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kDataSize, task_->GetTotalBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_EQ(kData, task_->GetResponseWriter()->AsStringWriter()->data());

  // Download has finished.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  SimulateDownloadCompletion(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kDataSize, task_->GetTotalBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_EQ(kData, task_->GetResponseWriter()->AsStringWriter()->data());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests sucessfull download of response with multiple
// URLSession:dataTask:didReceiveData: callbacks.
TEST_F(DownloadTaskImplTest, LargeResponseDownload) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // The first part of the response has arrived.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  const char kData1[] = "foo";
  const char kData2[] = "buzz";
  int64_t kData1Size = strlen(kData1);
  int64_t kData2Size = strlen(kData2);
  session_task.countOfBytesExpectedToReceive = kData1Size + kData2Size;
  SimulateDataDownload(session_task, kData1);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kData1Size + kData2Size, task_->GetTotalBytes());
  EXPECT_EQ(42, task_->GetPercentComplete());
  net::URLFetcherStringWriter* writer =
      task_->GetResponseWriter()->AsStringWriter();
  EXPECT_EQ(kData1, writer->data());

  // The second part of the response has arrived.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  SimulateDataDownload(session_task, kData2);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kData1Size + kData2Size, task_->GetTotalBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_EQ(std::string(kData1) + kData2, writer->data());

  // Download has finished.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  SimulateDownloadCompletion(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kData1Size + kData2Size, task_->GetTotalBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_EQ(std::string(kData1) + kData2, writer->data());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests failed download when URLSession:dataTask:didReceiveData: callback was
// not even called.
TEST_F(DownloadTaskImplTest, FailureInTheBeginning) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // Download has failed.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorNotConnectedToInternet
                                   userInfo:nil];
  SimulateDownloadCompletion(session_task, error);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_TRUE(task_->GetErrorCode() == net::ERR_INTERNET_DISCONNECTED);
  EXPECT_EQ(0, task_->GetTotalBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests failed download when URLSession:dataTask:didReceiveData: callback was
// called once.
TEST_F(DownloadTaskImplTest, FailureInTheMiddle) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // A part of the response has arrived.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  const char kReceivedData[] = "foo";
  int64_t kReceivedDataSize = strlen(kReceivedData);
  int64_t kExpectedDataSize = kReceivedDataSize + 10;
  session_task.countOfBytesExpectedToReceive = kExpectedDataSize;
  SimulateDataDownload(session_task, kReceivedData);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kExpectedDataSize, task_->GetTotalBytes());
  EXPECT_EQ(23, task_->GetPercentComplete());
  net::URLFetcherStringWriter* writer =
      task_->GetResponseWriter()->AsStringWriter();
  EXPECT_EQ(kReceivedData, writer->data());

  // Download has failed.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorNotConnectedToInternet
                                   userInfo:nil];
  SimulateDownloadCompletion(session_task, error);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_TRUE(task_->GetErrorCode() == net::ERR_INTERNET_DISCONNECTED);
  EXPECT_EQ(kExpectedDataSize, task_->GetTotalBytes());
  EXPECT_EQ(23, task_->GetPercentComplete());
  EXPECT_EQ(kReceivedData, writer->data());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests that NSURLSessionConfiguration contains up to date cookie from browser
// state before the download started.
TEST_F(DownloadTaskImplTest, Cookie) {
  if (@available(iOS 11, *)) {
    // Remove all cookies from the session configuration.
    auto storage = task_delegate_.configuration().HTTPCookieStorage;
    for (NSHTTPCookie* cookie in storage.cookies)
      [storage deleteCookie:cookie];

    // Add a cookie to BrowserState.
    NSURL* cookie_url = [NSURL URLWithString:@(kUrl)];
    NSHTTPCookie* cookie = [NSHTTPCookie cookieWithProperties:@{
      NSHTTPCookieName : @"name",
      NSHTTPCookieValue : @"value",
      NSHTTPCookiePath : cookie_url.path,
      NSHTTPCookieDomain : cookie_url.host,
      NSHTTPCookieVersion : @1,
    }];
    ASSERT_TRUE(SetCookie(cookie));

    // Start the download and make sure that all cookie from BrowserState were
    // picked up.
    EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
    ASSERT_TRUE(Start());
    EXPECT_EQ(1U, storage.cookies.count);
    EXPECT_NSEQ(cookie, storage.cookies.firstObject);
  }

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests that URLFetcherFileWriter deletes the file if download has failed with
// error.
TEST_F(DownloadTaskImplTest, FileDeletion) {
  // Create URLFetcherFileWriter.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file = temp_dir.GetPath().AppendASCII("DownloadTaskImpl");
  base::DeleteFile(temp_file, false);
  ASSERT_FALSE(base::PathExists(temp_file));
  std::unique_ptr<net::URLFetcherResponseWriter> writer =
      std::make_unique<net::URLFetcherFileWriter>(
          base::ThreadTaskRunnerHandle::Get(), temp_file);
  __block bool initialized_file_writer = false;
  ASSERT_EQ(net::ERR_IO_PENDING, writer->Initialize(BindBlockArc(^(int error) {
    ASSERT_FALSE(error);
    initialized_file_writer = true;
  })));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(1.0, ^{
    base::RunLoop().RunUntilIdle();
    return initialized_file_writer;
  }));

  // Start the download.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start(std::move(writer));
  ASSERT_TRUE(session_task);

  // Deliver the response and verify that download file exists.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  const char kReceivedData[] = "foo";
  SimulateDataDownload(session_task, kReceivedData);
  ASSERT_TRUE(base::PathExists(temp_file));

  // Fail the download and verify that the file was deleted.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorNotConnectedToInternet
                                   userInfo:nil];
  SimulateDownloadCompletion(session_task, error);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !base::PathExists(temp_file);
  }));

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests changing MIME type during the download.
TEST_F(DownloadTaskImplTest, MimeTypeChange) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // Download has finished with a different MIME type.
  ASSERT_EQ(kMimeType, task_->GetMimeType());
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  const char kOtherMimeType[] = "application/foo";
  session_task.response =
      [[NSURLResponse alloc] initWithURL:[NSURL URLWithString:@(kUrl)]
                                MIMEType:@(kOtherMimeType)
                   expectedContentLength:0
                        textEncodingName:nil];
  SimulateDownloadCompletion(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(kOtherMimeType, task_->GetMimeType());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests that destructing DownloadTaskImpl calls -[NSURLSessionDataTask cancel]
// and OnTaskDestroyed().
TEST_F(DownloadTaskImplTest, DownloadTaskDestruction) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
  task_->RemoveObserver(&task_observer_);
  task_ = nullptr;  // Destruct DownloadTaskImpl.
  EXPECT_TRUE(session_task.state = NSURLSessionTaskStateCanceling);
}

// Tests that shutting down DownloadTaskImpl calls
// -[NSURLSessionDataTask cancel], but does not call OnTaskDestroyed().
TEST_F(DownloadTaskImplTest, DownloadTaskShutdown) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  task_->ShutDown();
  EXPECT_TRUE(session_task.state = NSURLSessionTaskStateCanceling);
}

}  // namespace web
