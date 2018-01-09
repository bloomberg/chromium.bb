// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/cert_verifier_browser_test.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/network_service_test_helper.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "services/service_manager/public/cpp/connector.h"

CertVerifierBrowserTest::CertVerifier::CertVerifier(
    net::MockCertVerifier* verifier)
    : verifier_(verifier) {
  if (!base::FeatureList::IsEnabled(features::kNetworkService))
    return;

  // Enable the MockCertVerifier in the network process via a switch. This is
  // because it's too early to call the service manager at this point (it's not
  // created yet), and by the time we can call the service manager in
  // SetUpOnMainThread the main profile has already been created.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kUseMockCertVerifierForTesting);
}

CertVerifierBrowserTest::CertVerifier::~CertVerifier() = default;

void CertVerifierBrowserTest::CertVerifier::set_default_result(
    int default_result) {
  verifier_->set_default_result(default_result);

  if (!base::FeatureList::IsEnabled(features::kNetworkService))
    return;

  EnsureNetworkServiceTestInitialized();
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  network_service_test_->MockCertVerifierSetDefaultResult(default_result);
}

void CertVerifierBrowserTest::CertVerifier::AddResultForCert(
    scoped_refptr<net::X509Certificate> cert,
    const net::CertVerifyResult& verify_result,
    int rv) {
  AddResultForCertAndHost(cert, "*", verify_result, rv);
}

void CertVerifierBrowserTest::CertVerifier::AddResultForCertAndHost(
    scoped_refptr<net::X509Certificate> cert,
    const std::string& host_pattern,
    const net::CertVerifyResult& verify_result,
    int rv) {
  verifier_->AddResultForCertAndHost(cert, host_pattern, verify_result, rv);

  if (!base::FeatureList::IsEnabled(features::kNetworkService))
    return;

  EnsureNetworkServiceTestInitialized();
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  network_service_test_->MockCertVerifierAddResultForCertAndHost(
      cert, host_pattern, verify_result, rv);
}

void CertVerifierBrowserTest::CertVerifier::
    EnsureNetworkServiceTestInitialized() {
  if (network_service_test_)
    return;

  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(content::mojom::kNetworkServiceName,
                      &network_service_test_);
}

CertVerifierBrowserTest::CertVerifierBrowserTest()
    : InProcessBrowserTest(),
      mock_cert_verifier_(new net::MockCertVerifier()),
      cert_verifier_(mock_cert_verifier_.get()) {}

CertVerifierBrowserTest::~CertVerifierBrowserTest() {}

void CertVerifierBrowserTest::SetUpInProcessBrowserTestFixture() {
  ProfileIOData::SetCertVerifierForTesting(mock_cert_verifier_.get());
}

void CertVerifierBrowserTest::TearDownInProcessBrowserTestFixture() {
  ProfileIOData::SetCertVerifierForTesting(nullptr);
}

CertVerifierBrowserTest::CertVerifier*
CertVerifierBrowserTest::mock_cert_verifier() {
  return &cert_verifier_;
}
