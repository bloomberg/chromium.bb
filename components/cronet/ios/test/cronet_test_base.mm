// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cronet_test_base.h"

#include "components/grpc_support/test/quic_test_server.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"

#pragma mark

@implementation TestDelegate {
  // Completion semaphore for this TestDelegate. When the request this delegate
  // is attached to finishes (either successfully or with an error), this
  // delegate signals this semaphore.
  dispatch_semaphore_t _semaphore;
}

@synthesize error = _error;
@synthesize totalBytesReceived = _totalBytesReceived;

NSMutableArray<NSData*>* _responseData;

- (id)init {
  if (self = [super init]) {
    _semaphore = dispatch_semaphore_create(0);
  }
  return self;
}

- (void)reset {
  _responseData = nil;
  _error = nil;
  _totalBytesReceived = 0;
}

- (NSString*)responseBody {
  if (_responseData == nil) {
    return nil;
  }
  NSMutableString* body = [NSMutableString string];
  for (NSData* data in _responseData) {
    [body appendString:[[NSString alloc] initWithData:data
                                             encoding:NSUTF8StringEncoding]];
  }
  VLOG(3) << "responseBody size:" << [body length]
          << " chunks:" << [_responseData count];
  return body;
}

- (BOOL)waitForDone {
  int64_t deadline_ns = 20 * NSEC_PER_SEC;
  return dispatch_semaphore_wait(
             _semaphore, dispatch_time(DISPATCH_TIME_NOW, deadline_ns)) == 0;
}

- (void)URLSession:(NSURLSession*)session
    didBecomeInvalidWithError:(NSError*)error {
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  [self setError:error];
  dispatch_semaphore_signal(_semaphore);
}

- (void)URLSession:(NSURLSession*)session
                   task:(NSURLSessionTask*)task
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:
          (void (^)(NSURLSessionAuthChallengeDisposition disp,
                    NSURLCredential* credential))completionHandler {
  completionHandler(NSURLSessionAuthChallengeUseCredential, nil);
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))
                           completionHandler {
  completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveData:(NSData*)data {
  _totalBytesReceived += [data length];
  if (_responseData == nil) {
    _responseData = [[NSMutableArray alloc] init];
  }
  [_responseData addObject:data];
}

- (void)URLSession:(NSURLSession*)session
             dataTask:(NSURLSessionDataTask*)dataTask
    willCacheResponse:(NSCachedURLResponse*)proposedResponse
    completionHandler:
        (void (^)(NSCachedURLResponse* cachedResponse))completionHandler {
  completionHandler(proposedResponse);
}

@end

namespace cronet {

void CronetTestBase::SetUp() {
  ::testing::Test::SetUp();
  grpc_support::StartQuicTestServer();
  delegate_ = [[TestDelegate alloc] init];
}

void CronetTestBase::TearDown() {
  grpc_support::ShutdownQuicTestServer();
  ::testing::Test::TearDown();
}

// Launches the supplied |task| and blocks until it completes, with a timeout
// of 1 second.
void CronetTestBase::StartDataTaskAndWaitForCompletion(
    NSURLSessionDataTask* task) {
  [delegate_ reset];
  [task resume];
  CHECK([delegate_ waitForDone]);
}

::testing::AssertionResult CronetTestBase::IsResponseSuccessful() {
  if ([delegate_ error])
    return ::testing::AssertionFailure() << "error in response: " <<
           [[[delegate_ error] description]
               cStringUsingEncoding:NSUTF8StringEncoding];
  else
    return ::testing::AssertionSuccess() << "no errors in response";
}

std::unique_ptr<net::MockCertVerifier> CronetTestBase::CreateMockCertVerifier(
    const std::vector<std::string>& certs,
    bool known_root) {
  std::unique_ptr<net::MockCertVerifier> mock_cert_verifier(
      new net::MockCertVerifier());
  for (const auto& cert : certs) {
    net::CertVerifyResult verify_result;
    verify_result.verified_cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), cert);

    // By default, HPKP verification is enabled for known trust roots only.
    verify_result.is_issued_by_known_root = known_root;

    // Calculate the public key hash and add it to the verify_result.
    net::HashValue hashValue;
    CHECK(CalculatePublicKeySha256(*verify_result.verified_cert.get(),
                                   &hashValue));
    verify_result.public_key_hashes.push_back(hashValue);

    mock_cert_verifier->AddResultForCert(verify_result.verified_cert.get(),
                                         verify_result, net::OK);
  }
  return mock_cert_verifier;
}

bool CronetTestBase::CalculatePublicKeySha256(const net::X509Certificate& cert,
                                              net::HashValue* out_hash_value) {
  // Convert the cert to DER encoded bytes.
  std::string der_cert_bytes;
  net::X509Certificate::OSCertHandle cert_handle = cert.os_cert_handle();
  if (!net::X509Certificate::GetDEREncoded(cert_handle, &der_cert_bytes)) {
    LOG(INFO) << "Unable to convert the given cert to DER encoding";
    return false;
  }
  // Extract the public key from the cert.
  base::StringPiece spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(der_cert_bytes, &spki_bytes)) {
    LOG(INFO) << "Unable to retrieve the public key from the DER cert";
    return false;
  }
  // Calculate SHA256 hash of public key bytes.
  out_hash_value->tag = net::HASH_VALUE_SHA256;
  crypto::SHA256HashString(spki_bytes, out_hash_value->data(),
                           crypto::kSHA256Length);
  return true;
}

}  // namespace cronet
