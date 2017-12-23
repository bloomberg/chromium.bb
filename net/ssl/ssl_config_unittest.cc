// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config.h"

#include "net/cert/cert_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void CheckCertVerifyFlags(SSLConfig* ssl_config,
                          bool rev_checking_enabled,
                          bool verify_ev_cert,
                          bool cert_io_enabled,
                          bool rev_checking_required_local_anchors,
                          bool symantec_enforcement_disabled) {
  ssl_config->rev_checking_enabled = rev_checking_enabled;
  ssl_config->verify_ev_cert = verify_ev_cert;
  ssl_config->cert_io_enabled = cert_io_enabled;
  ssl_config->rev_checking_required_local_anchors =
      rev_checking_required_local_anchors;
  ssl_config->symantec_enforcement_disabled = symantec_enforcement_disabled;

  int flags = ssl_config->GetCertVerifyFlags();
  EXPECT_EQ(rev_checking_enabled,
            !!(flags & CertVerifier::VERIFY_REV_CHECKING_ENABLED));
  EXPECT_EQ(verify_ev_cert, !!(flags & CertVerifier::VERIFY_EV_CERT));
  EXPECT_EQ(cert_io_enabled, !!(flags & CertVerifier::VERIFY_CERT_IO_ENABLED));
  EXPECT_EQ(
      rev_checking_required_local_anchors,
      !!(flags & CertVerifier::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS));
  EXPECT_EQ(symantec_enforcement_disabled,
            !!(flags & CertVerifier::VERIFY_DISABLE_SYMANTEC_ENFORCEMENT));
}

}  // namespace

TEST(SSLConfigTest, GetCertVerifyFlags) {
  SSLConfig ssl_config;
  CheckCertVerifyFlags(&ssl_config,
                       /*rev_checking_enabled=*/true,
                       /*verify_ev_cert=*/true,
                       /*cert_io_enabled=*/true,
                       /*rev_checking_required_local_anchors=*/true,
                       /*symantec_enforcement_disabled=*/true);

  CheckCertVerifyFlags(&ssl_config,
                       /*rev_checking_enabled=*/false,
                       /*verify_ev_cert=*/false,
                       /*cert_io_enabled=*/false,
                       /*rev_checking_required_local_anchors=*/false,
                       /*symantec_enforcement_disabled=*/false);

  CheckCertVerifyFlags(&ssl_config,
                       /*rev_checking_enabled=*/true,
                       /*verify_ev_cert=*/false,
                       /*cert_io_enabled=*/false,
                       /*rev_checking_required_local_anchors=*/false,
                       /*symantec_enforcement_disabled=*/false);

  CheckCertVerifyFlags(&ssl_config,
                       /*rev_checking_enabled=*/false,
                       /*verify_ev_cert=*/true,
                       /*cert_io_enabled=*/false,
                       /*rev_checking_required_local_anchors=*/false,
                       /*symantec_enforcement_disabled=*/false);

  CheckCertVerifyFlags(&ssl_config,
                       /*rev_checking_enabled=*/false,
                       /*verify_ev_cert=*/false,
                       /*cert_io_enabled=*/true,
                       /*rev_checking_required_local_anchors=*/false,
                       /*symantec_enforcement_disabled=*/false);

  CheckCertVerifyFlags(&ssl_config,
                       /*rev_checking_enabled=*/false,
                       /*verify_ev_cert=*/false,
                       /*cert_io_enabled=*/false,
                       /*rev_checking_required_local_anchors=*/true,
                       /*symantec_enforcement_disabled=*/false);

  CheckCertVerifyFlags(&ssl_config,
                       /*rev_checking_enabled=*/false,
                       /*verify_ev_cert=*/false,
                       /*cert_io_enabled=*/true,
                       /*rev_checking_required_local_anchors=*/false,
                       /*symantec_enforcement_disabled=*/true);
}

}  // namespace net
