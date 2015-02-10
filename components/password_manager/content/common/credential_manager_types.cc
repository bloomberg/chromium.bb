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

CredentialInfo::CredentialInfo() : type(CredentialType::CREDENTIAL_TYPE_EMPTY) {
}

CredentialInfo::CredentialInfo(const blink::WebCredential& credential)
    : id(credential.id()),
      name(credential.name()),
      avatar(credential.avatarURL()) {
  type = credential.isLocalCredential()
             ? CredentialType::CREDENTIAL_TYPE_LOCAL
             : CredentialType::CREDENTIAL_TYPE_FEDERATED;
  if (type == CredentialType::CREDENTIAL_TYPE_LOCAL) {
    DCHECK(credential.isLocalCredential());
    password =
        static_cast<const blink::WebLocalCredential&>(credential).password();
  } else {
    DCHECK(credential.isFederatedCredential());
    federation = static_cast<const blink::WebFederatedCredential&>(credential)
                     .federation();
  }
}

CredentialInfo::CredentialInfo(const autofill::PasswordForm& form,
                               CredentialType form_type)
    : type(form_type),
      id(form.username_value),
      name(form.display_name),
      avatar(form.avatar_url),
      password(form.password_value),
      federation(form.federation_url) {
  switch (form_type) {
    case CredentialType::CREDENTIAL_TYPE_EMPTY:
      password = base::string16();
      federation = GURL();
      break;
    case CredentialType::CREDENTIAL_TYPE_LOCAL:
      federation = GURL();
      break;
    case CredentialType::CREDENTIAL_TYPE_FEDERATED:
      password = base::string16();
      break;
  }
}

CredentialInfo::~CredentialInfo() {
}

scoped_ptr<autofill::PasswordForm> CreatePasswordFormFromCredentialInfo(
    const CredentialInfo& info,
    const GURL& origin) {
  scoped_ptr<autofill::PasswordForm> form;
  if (info.type == CredentialType::CREDENTIAL_TYPE_EMPTY)
    return form.Pass();

  form.reset(new autofill::PasswordForm);
  form->avatar_url = info.avatar;
  form->display_name = info.name;
  form->federation_url = info.federation;
  form->origin = origin;
  form->password_value = info.password;
  form->username_value = info.id;
  form->scheme = autofill::PasswordForm::SCHEME_HTML;

  form->signon_realm =
      info.type == CredentialType::CREDENTIAL_TYPE_LOCAL
          ? origin.spec()
          : "federation://" + origin.host() + "/" + info.federation.host();
  form->username_value = info.id;
  return form.Pass();
}

}  // namespace password_manager
