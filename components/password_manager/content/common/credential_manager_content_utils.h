// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_COMMON_CREDENTIAL_MANAGER_CONTENT_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_COMMON_CREDENTIAL_MANAGER_CONTENT_UTILS_H_

#include "components/password_manager/core/common/credential_manager_types.h"

namespace blink {
class WebCredential;
};

namespace password_manager {

// Returns a CredentialInfo struct populated from |credential|.
CredentialInfo WebCredentialToCredentialInfo(
    const blink::WebCredential& credential);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_COMMON_CREDENTIAL_MANAGER_CONTENT_UTILS_H_
