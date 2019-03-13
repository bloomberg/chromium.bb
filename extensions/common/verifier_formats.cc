// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/crx_verifier.h"

namespace extensions {

crx_file::VerifierFormat GetWebstoreVerifierFormat() {
  // TODO(waffles@chromium.org): This should be CRX3_WITH_PUBLISHER_PROOF, but
  // we have not decided how to sign the test data yet.
  return crx_file::VerifierFormat::CRX3;
}

crx_file::VerifierFormat GetPolicyVerifierFormat(
    bool insecure_updates_enabled) {
  // TODO(crbug.com/740715): Eliminate CRX2.
  if (insecure_updates_enabled)
    return crx_file::VerifierFormat::CRX2_OR_CRX3;
  return crx_file::VerifierFormat::CRX3;
}

crx_file::VerifierFormat GetExternalVerifierFormat() {
  return crx_file::VerifierFormat::CRX3;
}

crx_file::VerifierFormat GetTestVerifierFormat() {
  return crx_file::VerifierFormat::CRX3;
}

}  // namespace extensions
