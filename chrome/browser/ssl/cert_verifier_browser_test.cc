// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/cert_verifier_browser_test.h"

#include "chrome/browser/profiles/profile_io_data.h"
#include "net/cert/mock_cert_verifier.h"

CertVerifierBrowserTest::CertVerifierBrowserTest()
    : InProcessBrowserTest(),
      mock_cert_verifier_(new net::MockCertVerifier()) {}

CertVerifierBrowserTest::~CertVerifierBrowserTest() {}

void CertVerifierBrowserTest::SetUpInProcessBrowserTestFixture() {
  ProfileIOData::SetCertVerifierForTesting(mock_cert_verifier_.get());
}

void CertVerifierBrowserTest::TearDownInProcessBrowserTestFixture() {
  ProfileIOData::SetCertVerifierForTesting(nullptr);
}

net::MockCertVerifier* CertVerifierBrowserTest::mock_cert_verifier() {
  return mock_cert_verifier_.get();
}
