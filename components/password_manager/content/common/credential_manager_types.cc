// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/common/credential_manager_types.h"

namespace password_manager {

CredentialInfo::CredentialInfo() : type(CREDENTIAL_TYPE_UNKNOWN) {
}

CredentialInfo::CredentialInfo(const base::string16& id,
                               const base::string16& name,
                               const GURL& avatar)
    : type(CREDENTIAL_TYPE_UNKNOWN),
      id(id),
      name(name),
      avatar(avatar) {
}

CredentialInfo::~CredentialInfo() {
}

}  // namespace password_manager
