// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_CREDENTIAL_H_
#define IOS_WEB_PUBLIC_WEB_STATE_CREDENTIAL_H_

#include "base/strings/string16.h"
#include "url/gurl.h"

namespace web {

// Indicates the specific type of a Credential object.
enum CredentialType {
  CREDENTIAL_TYPE_EMPTY = 0,
  CREDENTIAL_TYPE_PASSWORD,
  CREDENTIAL_TYPE_FEDERATED,
  CREDENTIAL_TYPE_LAST = CREDENTIAL_TYPE_FEDERATED
};

// Represents an instance of the JavaScript Credential type.
struct Credential {
  Credential();
  ~Credential();

  // The specific type of this credential.
  CredentialType type;

  // An identifier for the credential.
  base::string16 id;

  // A human-understandable name corresponding to the credential.
  base::string16 name;

  // The URL of the user's avatar.
  GURL avatar_url;

  // The password for a local credential.
  base::string16 password;

  // The federation URL for a federated credential.
  GURL federation_url;
};

// Determines whether two credentials are equal.
bool CredentialsEqual(const web::Credential& credential1,
                      const web::Credential& credential2);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_CREDENTIAL_H_
