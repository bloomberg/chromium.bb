// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cryptotoken_private/cryptotoken_private_api.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "extensions/common/error_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace {

const char kGoogleDotCom[] = "google.com";
constexpr const char* kGoogleGstaticAppIds[] = {
    "https://www.gstatic.com/securitykey/origins.json",
    "https://www.gstatic.com/securitykey/a/google.com/origins.json"};

}  // namespace

namespace extensions {
namespace api {

CryptotokenPrivateCanOriginAssertAppIdFunction::
    CryptotokenPrivateCanOriginAssertAppIdFunction()
    : chrome_details_(this) {
}

ExtensionFunction::ResponseAction
CryptotokenPrivateCanOriginAssertAppIdFunction::Run() {
  std::unique_ptr<cryptotoken_private::CanOriginAssertAppId::Params> params =
      cryptotoken_private::CanOriginAssertAppId::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  const GURL origin_url(params->security_origin);
  if (!origin_url.is_valid()) {
    return RespondNow(Error(extensions::ErrorUtils::FormatErrorMessage(
        "Security origin * is not a valid URL", params->security_origin)));
  }
  const GURL app_id_url(params->app_id_url);
  if (!app_id_url.is_valid()) {
    return RespondNow(Error(extensions::ErrorUtils::FormatErrorMessage(
        "appId * is not a valid URL", params->app_id_url)));
  }

  if (origin_url == app_id_url) {
    return RespondNow(OneArgument(base::MakeUnique<base::Value>(true)));
  }

  // Fetch the eTLD+1 of both.
  const std::string origin_etldp1 =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (origin_etldp1.empty()) {
    return RespondNow(Error(extensions::ErrorUtils::FormatErrorMessage(
        "Could not find an eTLD for origin *", params->security_origin)));
  }
  const std::string app_id_etldp1 =
      net::registry_controlled_domains::GetDomainAndRegistry(
          app_id_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (app_id_etldp1.empty()) {
    return RespondNow(Error(extensions::ErrorUtils::FormatErrorMessage(
        "Could not find an eTLD for appId *", params->app_id_url)));
  }
  if (origin_etldp1 == app_id_etldp1) {
    return RespondNow(OneArgument(base::MakeUnique<base::Value>(true)));
  }
  // For legacy purposes, allow google.com origins to assert certain
  // gstatic.com appIds.
  // TODO(juanlang): remove when legacy constraints are removed.
  if (origin_etldp1 == kGoogleDotCom) {
    for (const char* id : kGoogleGstaticAppIds) {
      if (params->app_id_url == id)
        return RespondNow(OneArgument(base::MakeUnique<base::Value>(true)));
    }
  }
  return RespondNow(OneArgument(base::MakeUnique<base::Value>(false)));
}

}  // namespace api
}  // namespace extensions
