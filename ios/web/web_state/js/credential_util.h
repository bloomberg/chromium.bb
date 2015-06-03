// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_JS_CREDENTIAL_UTIL_H_
#define IOS_WEB_WEB_STATE_JS_CREDENTIAL_UTIL_H_

namespace base {
class DictionaryValue;
}  // namespace base

namespace web {

struct Credential;

// Populates |credential| from |value|, returning true if successful and false
// otherwise. |value| must contain the following string->string key/value
// pairs:
//
//     "type": one of "PasswordCredential" of "FederatedCredential"
//     "id": a string (possibly empty)
//
// The following pairs are optional:
//
//     "name": a string (possibly empty)
//     "avatarURL": a valid URL as a string
//
// If "type" is "PasswordCredential", then |value| must contain
//
//     "password" a string (possibly empty)
//
// If "type" is "FederatedCredential", then |value| must contain
//
//     "federation": a valid URL as a string
//
// If passed a |value| that doesn't meet these restrictions, |credential| will
// not be modified and false will be returned. |credential| must not be null.
bool DictionaryValueToCredential(const base::DictionaryValue& value,
                                 Credential* credential);

// Serializes |credential| to |value|.
void CredentialToDictionaryValue(const Credential& credential,
                                 base::DictionaryValue* value);

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_JS_CREDENTIAL_UTIL_H_
