// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/test_signin_client_builder.h"

#include "chrome/browser/profiles/profile.h"
#include "components/signin/core/browser/test_signin_client.h"

class KeyedService;

namespace content {
class BrowserContext;
}

namespace signin {

KeyedService* BuildTestSigninClient(
    content::BrowserContext* context) {
  TestSigninClient* test_signin_client =
      new TestSigninClient(static_cast<Profile*>(context)->GetPrefs());
  return test_signin_client;
}

}  // namespace signin
