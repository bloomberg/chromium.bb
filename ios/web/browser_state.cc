// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/browser_state.h"

namespace web {
namespace {
// Private key used for safe conversion of base::SupportsUserData to
// web::BrowserState in web::BrowserState::FromSupportsUserData.
const char kBrowserStateIdentifierKey[] = "BrowserStateIdentifierKey";
}

BrowserState::BrowserState() {
  // (Refcounted)?BrowserStateKeyedServiceFactories needs to be able to convert
  // a base::SupportsUserData to a BrowserState. Moreover, since the factories
  // may be passed a content::BrowserContext instead of a BrowserState, attach
  // an empty object to this via a private key.
  SetUserData(kBrowserStateIdentifierKey, new SupportsUserData::Data);
}

BrowserState::~BrowserState() {
}

// static
BrowserState* BrowserState::FromSupportsUserData(
    base::SupportsUserData* supports_user_data) {
  if (!supports_user_data ||
      !supports_user_data->GetUserData(kBrowserStateIdentifierKey)) {
    return nullptr;
  }
  return static_cast<BrowserState*>(supports_user_data);
}
}  // namespace web
