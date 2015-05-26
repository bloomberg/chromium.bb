// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/web_state/credential.h"

namespace web {

Credential::Credential() : type(CREDENTIAL_TYPE_EMPTY) {
}

Credential::~Credential() = default;

bool CredentialsEqual(const web::Credential& credential1,
                      const web::Credential& credential2) {
  return credential1.type == credential2.type &&
         credential1.id == credential2.id &&
         credential1.name == credential2.name &&
         credential1.avatar_url == credential2.avatar_url &&
         credential1.password == credential2.password &&
         credential1.federation_url == credential2.federation_url;
}

}  // namespace web
