// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/web_state/credential.h"

namespace web {

Credential::Credential() : type(CREDENTIAL_TYPE_EMPTY) {
}

Credential::~Credential() = default;

}  // namespace web
