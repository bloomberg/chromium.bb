// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_MOCK_CERTIFICATE_IMPORTER_H_
#define CHROMEOS_NETWORK_ONC_MOCK_CERTIFICATE_IMPORTER_H_

#include "base/basictypes.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/onc/onc_certificate_importer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace onc {

class CHROMEOS_EXPORT MockCertificateImporter : public CertificateImporter {
 public:
  MockCertificateImporter();
  virtual ~MockCertificateImporter();
  MOCK_METHOD3(ImportCertificates, bool(const base::ListValue&,
                                        ::onc::ONCSource,
                                        net::CertificateList*));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockCertificateImporter);
};

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_MOCK_CERTIFICATE_IMPORTER_H_
