// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/credential_manager.h"

#import "base/mac/bind_objc_block.h"
#include "ios/chrome/browser/passwords/credential_manager_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::CredentialManagerError;
using password_manager::CredentialInfo;
using password_manager::CredentialType;
using password_manager::CredentialMediationRequirement;

namespace credential_manager {

CredentialManager::CredentialManager(
    password_manager::PasswordManagerClient* client)
    : impl_(client) {
  // TODO(crbug.com/435047): Register a script command callback for prefix
  // "credentials" and HandleScriptCommand method.
}

CredentialManager::~CredentialManager() {}

bool CredentialManager::HandleScriptCommand(const base::DictionaryValue& json,
                                            const GURL& origin_url,
                                            bool user_is_interacting) {
  // TODO(crbug.com/435047): Check if context is secure.
  std::string command;
  if (!json.GetString("command", &command)) {
    DLOG(ERROR) << "RECEIVED BAD json - NO VALID 'command' FIELD";
    return false;
  }

  int request_id;
  if (!json.GetInteger("requestId", &request_id)) {
    DLOG(ERROR) << "RECEIVED BAD json - NO VALID 'requestId' FIELD";
    return false;
  }

  if (command == "credentials.get") {
    CredentialMediationRequirement mediation;
    if (!ParseMediationRequirement(json, &mediation)) {
      // TODO(crbug.com/435047): Reject promise with a TypeError.
      return false;
    }
    bool include_passwords;
    if (!ParseIncludePasswords(json, &include_passwords)) {
      // TODO(crbug.com/435047): Reject promise with a TypeError.
      return false;
    }
    std::vector<GURL> federations;
    if (!ParseFederations(json, &federations)) {
      // TODO(crbug.com/435047): Reject promise with a TypeError.
      return false;
    }
    impl_.Get(mediation, include_passwords, federations,
              base::BindOnce(&CredentialManager::SendGetResponse,
                             base::Unretained(this), request_id));
    return true;
  }
  if (command == "credentials.store") {
    CredentialInfo credential;
    if (!ParseCredentialDictionary(json, &credential)) {
      // TODO(crbug.com/435047): Reject promise with a TypeError.
      return false;
    }
    impl_.Store(credential,
                base::BindOnce(&CredentialManager::SendStoreResponse,
                               base::Unretained(this), request_id));
    return true;
  }
  if (command == "credentials.preventSilentAccess") {
    impl_.PreventSilentAccess(
        base::BindOnce(&CredentialManager::SendPreventSilentAccessResponse,
                       base::Unretained(this), request_id));
    return true;
  }
  return false;
}

void CredentialManager::SendGetResponse(
    int request_id,
    CredentialManagerError error,
    const base::Optional<CredentialInfo>& info) {}

void CredentialManager::SendPreventSilentAccessResponse(int request_id) {}

void CredentialManager::SendStoreResponse(int request_id) {}

}  // namespace credential_manager
