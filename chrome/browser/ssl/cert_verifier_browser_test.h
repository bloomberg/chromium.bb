// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CERT_VERIFIER_BROWSER_TEST_H_
#define CHROME_BROWSER_SSL_CERT_VERIFIER_BROWSER_TEST_H_

#include <memory>

#include "chrome/test/base/in_process_browser_test.h"

namespace net {
class MockCertVerifier;
}  // namespace net

// CertVerifierBrowserTest allows tests to force certificate
// verification results for requests made with any profile's main
// request context (such as navigations). To do so, tests can use the
// MockCertVerifier exposed via
// CertVerifierBrowserTest::mock_cert_verifier().
class CertVerifierBrowserTest : public InProcessBrowserTest {
 public:
  CertVerifierBrowserTest();
  ~CertVerifierBrowserTest() override;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;

  // Returns a pointer to the MockCertVerifier used by all profiles in
  // this test.
  net::MockCertVerifier* mock_cert_verifier();

 private:
  std::unique_ptr<net::MockCertVerifier> mock_cert_verifier_;
};

#endif  // CHROME_BROWSER_SSL_CERT_VERIFIER_BROWSER_TEST_H_
