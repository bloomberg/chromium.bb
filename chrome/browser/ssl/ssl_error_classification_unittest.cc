// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_classification.h"

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

TEST(SSLErrorClassification, TestDateInvalidScore) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> expired_cert =
      net::ImportCertFromFile(certs_dir, "expired_cert.pem");
  base::Time time;

  {
    EXPECT_TRUE(base::Time::FromString("Wed, 03 Jan 2007 12:00:00 GMT", &time));
    SSLErrorClassification ssl_error(time, *expired_cert);
    EXPECT_FLOAT_EQ(0.2f, ssl_error.CalculateScoreTimePassedSinceExpiry());
  }

  {
    EXPECT_TRUE(base::Time::FromString("Sat, 06 Jan 2007 12:00:00 GMT", &time));
    SSLErrorClassification ssl_error(time, *expired_cert);
    EXPECT_FLOAT_EQ(0.3f, ssl_error.CalculateScoreTimePassedSinceExpiry());
  }

  {
    EXPECT_TRUE(base::Time::FromString("Mon, 08 Jan 2007 12:00:00 GMT", &time));
    SSLErrorClassification ssl_error(time, *expired_cert);
    EXPECT_FLOAT_EQ(0.4f, ssl_error.CalculateScoreTimePassedSinceExpiry());
  }

}
