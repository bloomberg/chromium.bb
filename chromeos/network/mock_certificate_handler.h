// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_MOCK_CERTIFICATE_HANDLER_H_
#define CHROMEOS_NETWORK_MOCK_CERTIFICATE_HANDLER_H_

#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/certificate_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class CHROMEOS_EXPORT MockCertificateHandler : public CertificateHandler {
 public:
  MockCertificateHandler();
  virtual ~MockCertificateHandler();
  MOCK_METHOD3(ImportCertificates, bool(const base::ListValue&,
                                        onc::ONCSource,
                                        net::CertificateList*));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockCertificateHandler);
};

}  // chromeos

#endif  // CHROMEOS_NETWORK_MOCK_CERTIFICATE_HANDLER_H_
