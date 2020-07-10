// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gaia_credential.h"

#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {

CGaiaCredential::CGaiaCredential() = default;

CGaiaCredential::~CGaiaCredential() = default;

HRESULT CGaiaCredential::FinalConstruct() {
  LOGFN(INFO);
  return S_OK;
}

void CGaiaCredential::FinalRelease() {
  LOGFN(INFO);
}

}  // namespace credential_provider
