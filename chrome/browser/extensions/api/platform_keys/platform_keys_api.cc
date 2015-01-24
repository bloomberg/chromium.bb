// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/common/extensions/api/platform_keys_internal.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"

namespace extensions {

namespace api_pki = api::platform_keys_internal;

namespace platform_keys {

const char kErrorInvalidToken[] = "The token is not valid.";
const char kErrorAlgorithmNotSupported[] = "Algorithm not supported.";
const char kTokenIdUser[] = "user";
const char kTokenIdSystem[] = "system";

// Returns whether |token_id| references a known Token.
bool ValidateToken(const std::string& token_id,
                   std::string* platform_keys_token_id) {
  platform_keys_token_id->clear();
  if (token_id == kTokenIdUser) {
    *platform_keys_token_id = chromeos::platform_keys::kTokenIdUser;
    return true;
  }
  if (token_id == kTokenIdSystem) {
    *platform_keys_token_id = chromeos::platform_keys::kTokenIdSystem;
    return true;
  }
  return false;
}

std::string PlatformKeysTokenIdToApiId(
    const std::string& platform_keys_token_id) {
  if (platform_keys_token_id == chromeos::platform_keys::kTokenIdUser)
    return kTokenIdUser;
  if (platform_keys_token_id == chromeos::platform_keys::kTokenIdSystem)
    return kTokenIdSystem;

  return std::string();
}

}  // namespace platform_keys

PlatformKeysInternalSignFunction::~PlatformKeysInternalSignFunction() {
}

ExtensionFunction::ResponseAction PlatformKeysInternalSignFunction::Run() {
  scoped_ptr<api_pki::Sign::Params> params(
      api_pki::Sign::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string platform_keys_token_id;
  if (!platform_keys::ValidateToken(params->token_id, &platform_keys_token_id))
    return RespondNow(Error(platform_keys::kErrorInvalidToken));

  chromeos::platform_keys::HashAlgorithm hash_algorithm;
  if (params->hash_algorithm_name == "SHA-1")
    hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA1;
  else if (params->hash_algorithm_name == "SHA-256")
    hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA256;
  else if (params->hash_algorithm_name == "SHA-384")
    hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA384;
  else if (params->hash_algorithm_name == "SHA-512")
    hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA512;
  else
    return RespondNow(Error(platform_keys::kErrorAlgorithmNotSupported));

  chromeos::PlatformKeysService* service =
      chromeos::PlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  service->Sign(
      platform_keys_token_id,
      std::string(params->public_key.begin(), params->public_key.end()),
      hash_algorithm, std::string(params->data.begin(), params->data.end()),
      extension_id(),
      base::Bind(&PlatformKeysInternalSignFunction::OnSigned, this));
  return RespondLater();
}

void PlatformKeysInternalSignFunction::OnSigned(
    const std::string& signature,
    const std::string& error_message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (error_message.empty())
    Respond(ArgumentList(api_pki::Sign::Results::Create(
        std::vector<char>(signature.begin(), signature.end()))));
  else
    Respond(Error(error_message));
}

}  // namespace extensions
