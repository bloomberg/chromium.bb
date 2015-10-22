// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/net/clients/crn_network_client_protocol.h"
#import "ios/web/net/clients/crw_passkit_delegate.h"
#import "ios/web/net/clients/crw_passkit_network_client.h"
#include "ios/web/net/request_tracker_impl.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class CRWPassKitNetworkClientTest : public testing::Test {
 public:
  CRWPassKitNetworkClientTest() {}

  void SetUp() override {
    // The mock passkit delegate will be called when the Pass is loaded.
    mockCRWPassKitDelegate_.reset([[OCMockObject
        niceMockForProtocol:@protocol(CRWPassKitDelegate)] retain]);

    // The request tracker needs to be set up with a unique tab id.

    static int gcount = 0;
    request_group_id_.reset(
        [[NSString stringWithFormat:@"passkittest%d", gcount++] retain]);
    request_tracker_ = web::RequestTrackerImpl::CreateTrackerForRequestGroupID(
        request_group_id_,
        &browser_state_,
        browser_state_.GetRequestContext(),
        nil /* CRWRequestTrackerDelegate */);

    // Set up mock original UIWebView proxy.
    OCMockObject* mockProxy_ = [[OCMockObject
        niceMockForProtocol:@protocol(CRNNetworkClientProtocol)] retain];
    mockWebProxy_.reset(mockProxy_);

    // Link all the mock objects into the PassKit network client.
    passkit_client_.reset([[CRWPassKitNetworkClient alloc]
        initWithTracker:request_tracker_->GetWeakPtr()
               delegate:static_cast<id<CRWPassKitDelegate> >(
                            mockCRWPassKitDelegate_.get())]);
    [passkit_client_
        setUnderlyingClient:(id<CRNNetworkClientProtocol>)mockWebProxy_];
  }

  void TearDown() override { request_tracker_->Close(); }

 protected:
  web::TestWebThreadBundle thread_bundle_;
  base::scoped_nsobject<CRWPassKitNetworkClient> passkit_client_;
  scoped_refptr<web::RequestTrackerImpl> request_tracker_;
  // Holds a mock CRNNetworkClientProtocol object.
  base::scoped_nsobject<OCMockObject> mockWebProxy_;
  // Holds a mock CRWPassKitDelegate object.
  base::scoped_nsobject<OCMockObject> mockCRWPassKitDelegate_;
  base::scoped_nsobject<NSString> request_group_id_;
  web::TestBrowserState browser_state_;
};

}  // namespace

TEST_F(CRWPassKitNetworkClientTest, GoodPassKitObjectHandled) {
  // This error is expected to be passed to the web proxy at the end of the
  // PassKit loading sequence, successful or not.
  [[mockWebProxy_ expect] didFailWithNSErrorCode:NSURLErrorCancelled
                                    netErrorCode:net::ERR_ABORTED];

  // Set the data in PassKitProtocolHandler to point to a good PassKit object.
  base::FilePath pass_path;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &pass_path));
  pass_path = pass_path.AppendASCII("ios/web/test/data/testpass.pkpass");
  NSData* passKitObject = [NSData dataWithContentsOfFile:
      base::SysUTF8ToNSString(pass_path.value())];
  EXPECT_TRUE(passKitObject);

  // Load the PassKit object and trigger loading finished methods.
  [passkit_client_ didLoadData:passKitObject];
  [passkit_client_ didFinishLoading];

  EXPECT_OCMOCK_VERIFY(mockWebProxy_);
}

TEST_F(CRWPassKitNetworkClientTest, BadPassKitObjectHandled) {
  // This error is expected to be passed to the web proxy at the end of the
  // PassKit loading sequence, successful or not.
  [[mockWebProxy_ expect] didFailWithNSErrorCode:NSURLErrorCancelled
                                    netErrorCode:net::ERR_ABORTED];

  // Set the data in PassKitProtocolHandler to point to a bad PassKit object.
  base::FilePath pass_path;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &pass_path));
  pass_path = pass_path.AppendASCII("ios/web/test/data/testbadpass.pkpass");
  NSData* passKitObject = [NSData dataWithContentsOfFile:
      base::SysUTF8ToNSString(pass_path.value())];
  EXPECT_TRUE(passKitObject);

  // Load the PassKit object and trigger error method.
  [passkit_client_ didLoadData:passKitObject];
  [passkit_client_ didFailWithNSErrorCode:NSURLErrorCancelled
                             netErrorCode:net::ERR_ABORTED];

  EXPECT_OCMOCK_VERIFY(mockWebProxy_);
}
