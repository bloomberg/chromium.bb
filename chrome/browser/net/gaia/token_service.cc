// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/gaia/token_service.h"

void TokenService::SetClientLoginResult(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  credentials_ = credentials;
}

bool TokenService::HasLsid() {
  return !credentials_.lsid.empty();
}

const std::string& TokenService::GetLsid() {
  return credentials_.lsid;
}
