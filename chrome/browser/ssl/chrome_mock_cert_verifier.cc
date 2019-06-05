// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_mock_cert_verifier.h"

#include "chrome/browser/profiles/profile_io_data.h"

ChromeMockCertVerifier::ChromeMockCertVerifier() = default;

ChromeMockCertVerifier::~ChromeMockCertVerifier() = default;

void ChromeMockCertVerifier::SetUpInProcessBrowserTestFixture() {
  ContentMockCertVerifier::SetUpInProcessBrowserTestFixture();
  ProfileIOData::SetCertVerifierForTesting(mock_cert_verifier_internal());
}

void ChromeMockCertVerifier::TearDownInProcessBrowserTestFixture() {
  ContentMockCertVerifier::TearDownInProcessBrowserTestFixture();
  ProfileIOData::SetCertVerifierForTesting(nullptr);
}
