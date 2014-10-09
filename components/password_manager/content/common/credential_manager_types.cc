// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/common/credential_manager_types.h"

#include "base/logging.h"
#include "components/autofill/core/common/password_form.h"
#include "third_party/WebKit/public/platform/WebCredential.h"
#include "third_party/WebKit/public/platform/WebFederatedCredential.h"
#include "third_party/WebKit/public/platform/WebLocalCredential.h"

namespace password_manager {

CredentialInfo::CredentialInfo() : type(CREDENTIAL_TYPE_EMPTY) {
}

CredentialInfo::CredentialInfo(const blink::WebCredential& credential)
    : id(credential.id()),
      name(credential.name()),
      avatar(credential.avatarURL()) {
  type = credential.isLocalCredential() ? CREDENTIAL_TYPE_LOCAL
                                        : CREDENTIAL_TYPE_FEDERATED;
  if (type == CREDENTIAL_TYPE_LOCAL) {
    DCHECK(credential.isLocalCredential());
    password = static_cast<const blink::WebLocalCredential&>(
        credential).password();
  } else {
    DCHECK(credential.isFederatedCredential());
    federation = static_cast<const blink::WebFederatedCredential&>(
        credential).federation();
  }
}

CredentialInfo::CredentialInfo(const autofill::PasswordForm& form)
    : id(form.username_value),
      name(form.display_name),
      avatar(form.avatar_url),
      password(form.password_value),
      federation(form.federation_url) {
  DCHECK(!password.empty() || !federation.is_empty());
  type = password.empty() ? CREDENTIAL_TYPE_FEDERATED : CREDENTIAL_TYPE_LOCAL;
}

CredentialInfo::~CredentialInfo() {
}

scoped_ptr<autofill::PasswordForm> CreatePasswordFormFromCredentialInfo(
    const CredentialInfo& info,
    const GURL& origin) {
  scoped_ptr<autofill::PasswordForm> form(new autofill::PasswordForm);
  form->avatar_url = info.avatar;
  form->display_name = info.name;
  form->federation_url = info.federation;
  form->origin = origin;
  form->password_value = info.password;
  form->scheme = autofill::PasswordForm::SCHEME_HTML;
  form->signon_realm = origin.spec();
  form->username_value = info.id;
  return form.Pass();
}

}  // namespace password_manager
