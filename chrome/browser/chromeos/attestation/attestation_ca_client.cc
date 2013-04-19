// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"

#include <string>

#include "base/bind.h"
#include "base/message_loop.h"

namespace chromeos {
namespace attestation {

AttestationCAClient::AttestationCAClient() {}

AttestationCAClient::~AttestationCAClient() {}

void AttestationCAClient::SendEnrollRequest(const std::string& request,
                                            const DataCallback& on_response) {
  NOTIMPLEMENTED();
}

void AttestationCAClient::SendCertificateRequest(
    const std::string& request,
    const DataCallback& on_response) {
  NOTIMPLEMENTED();
}

}  // namespace attestation
}  // namespace chromeos
