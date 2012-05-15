// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_CERT_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_CERT_LIBRARY_H_
#pragma once

#include "chrome/browser/chromeos/cros/cert_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockCertLibrary : public CertLibrary {
 public:
  MockCertLibrary();
  virtual ~MockCertLibrary();

  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
  MOCK_METHOD0(LoadKeyStore, void());
  MOCK_CONST_METHOD0(CertificatesLoading, bool());
  MOCK_CONST_METHOD0(CertificatesLoaded, bool());
  MOCK_CONST_METHOD0(IsHardwareBacked, bool());
  MOCK_CONST_METHOD0(GetTpmTokenName, const std::string&());
  MOCK_CONST_METHOD0(GetCertificates, const CertList&());
  MOCK_CONST_METHOD0(GetUserCertificates, const CertList&());
  MOCK_CONST_METHOD0(GetServerCertificates, const CertList&());
  MOCK_CONST_METHOD0(GetCACertificates, const CertList&());
  MOCK_METHOD1(EncryptToken, std::string(const std::string& token));
  MOCK_METHOD1(DecryptToken, std::string(const std::string& encrypted_token));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCertLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_CERT_LIBRARY_H_
