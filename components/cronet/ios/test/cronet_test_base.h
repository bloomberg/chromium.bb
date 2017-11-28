// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_IOS_TEST_CRONET_TEST_BASE_H_
#define COMPONENTS_CRONET_IOS_TEST_CRONET_TEST_BASE_H_

#include <Cronet/Cronet.h>
#include "net/cert/cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

#pragma mark

namespace base {
class SingleThreadTaskRunner;
class Thread;
}

// Exposes private test-only methods of the Cronet class.
@interface Cronet (ExposedForTesting)
+ (void)shutdownForTesting;
+ (void)setMockCertVerifierForTesting:
    (std::unique_ptr<net::CertVerifier>)certVerifier;
+ (void)setEnablePublicKeyPinningBypassForLocalTrustAnchors:(BOOL)enable;
+ (base::SingleThreadTaskRunner*)getFileThreadRunnerForTesting;
@end

// NSURLSessionDataDelegate delegate implementation used by the tests to
// wait for a response and check its status.
@interface TestDelegate : NSObject<NSURLSessionDataDelegate>

// Error the request this delegate is attached to failed with, if any.
@property(retain, atomic)
    NSMutableDictionary<NSURLSessionTask*, NSError*>* errorPerTask;

// Contains total amount of received data.
@property(readonly) NSMutableDictionary<NSURLSessionDataTask*, NSNumber*>*
    totalBytesReceivedPerTask;

@property(readonly) NSMutableDictionary<NSURLSessionDataTask*, NSNumber*>*
    expectedContentLengthPerTask;

// Resets the delegate, so it can be used again for another request.
- (void)reset;

// Contains the response body.
- (NSString*)responseBody:(NSURLSessionDataTask*)task;

/// Waits for a single request to complete.

/// @return  |NO| if the request didn't complete and the method timed-out.
- (BOOL)waitForDone:(NSURLSessionDataTask*)task
        withTimeout:(int64_t)deadline_ns;

// Convenience functions for single-task delegates
- (NSError*)error;
- (long)totalBytesReceived;
- (long)expectedContentLength;
- (NSString*)responseBody;

@end

// Forward declaration.
namespace net {
class MockCertVerifier;
}

namespace cronet {

// A base class that should be extended by all other Cronet tests.
// The class automatically starts and stops the test QUIC server.
class CronetTestBase : public ::testing::Test {
 protected:
  static bool CalculatePublicKeySha256(const net::X509Certificate& cert,
                                       net::HashValue* out_hash_value);

  void SetUp() override;
  void TearDown() override;
  bool StartDataTaskAndWaitForCompletion(NSURLSessionDataTask* task,
                                         int64_t deadline_ns = 15 *
                                                               NSEC_PER_SEC);
  std::unique_ptr<net::MockCertVerifier> CreateMockCertVerifier(
      const std::vector<std::string>& certs,
      bool known_root);

  ::testing::AssertionResult IsResponseSuccessful();
  ::testing::AssertionResult IsResponseCanceled();

  TestDelegate* delegate_;
};  // class CronetTestBase

}  // namespace cronet

#endif  // COMPONENTS_CRONET_IOS_TEST_CRONET_TEST_BASE_H_
