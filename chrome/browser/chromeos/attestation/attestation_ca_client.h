// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_ATTESTATION_CA_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_ATTESTATION_CA_CLIENT_H_

#include <string>

#include "base/basictypes.h"

#include "chromeos/attestation/attestation_flow.h"

namespace chromeos {
namespace attestation {

// This class is a ServerProxy implementation for the Chrome OS attestation
// flow.  It sends all requests to an Attestation CA via HTTPS.
class AttestationCAClient : public ServerProxy {
 public:
  AttestationCAClient();
  virtual ~AttestationCAClient();

  // chromeos::attestation::ServerProxy:
  virtual void SendEnrollRequest(const std::string& request,
                                 const DataCallback& on_response) OVERRIDE;
  virtual void SendCertificateRequest(const std::string& request,
                                      const DataCallback& on_response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AttestationCAClient);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_ATTESTATION_CA_CLIENT_H_
