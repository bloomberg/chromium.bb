// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/common/credential_manager_content_utils.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebCredential.h"
#include "third_party/WebKit/public/platform/WebFederatedCredential.h"
#include "third_party/WebKit/public/platform/WebPasswordCredential.h"

namespace password_manager {

CredentialInfo WebCredentialToCredentialInfo(
    const blink::WebCredential& credential) {
  CredentialInfo credential_info;
  credential_info.id = credential.id();
  credential_info.name = credential.name();
  credential_info.icon = credential.iconURL();
  credential_info.type = credential.isPasswordCredential()
                             ? CredentialType::CREDENTIAL_TYPE_PASSWORD
                             : CredentialType::CREDENTIAL_TYPE_FEDERATED;
  if (credential_info.type == CredentialType::CREDENTIAL_TYPE_PASSWORD) {
    DCHECK(credential.isPasswordCredential());
    credential_info.password =
        static_cast<const blink::WebPasswordCredential&>(credential).password();
  } else {
    DCHECK(credential.isFederatedCredential());
    credential_info.federation =
        static_cast<const blink::WebFederatedCredential&>(credential)
            .federation();
  }
  return credential_info;
}

}  // namespace password_manager
