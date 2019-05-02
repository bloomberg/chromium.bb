// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_MOCK_CERTIFICATE_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_MOCK_CERTIFICATE_UPLOADER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/attestation/certificate_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace attestation {

class MockCertificateUploader : public CertificateUploader {
 public:
  MockCertificateUploader();
  ~MockCertificateUploader();

  MOCK_METHOD1(UploadCertificateIfNeeded, void(UploadCallback));
  MOCK_METHOD1(RefreshAndUploadCertificate, void(UploadCallback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCertificateUploader);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_MOCK_CERTIFICATE_UPLOADER_H_
