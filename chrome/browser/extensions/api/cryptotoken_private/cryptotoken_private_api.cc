// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cryptotoken_private/cryptotoken_private_api.h"

#include <stddef.h>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "crypto/sha2.h"
#include "extensions/common/error_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kGoogleDotCom[] = "google.com";
constexpr const char* kGoogleGstaticAppIds[] = {
    "https://www.gstatic.com/securitykey/origins.json",
    "https://www.gstatic.com/securitykey/a/google.com/origins.json"};

// ContainsAppIdByHash returns true iff the SHA-256 hash of one of the
// elements of |list| equals |hash|.
bool ContainsAppIdByHash(const base::ListValue& list,
                         const std::vector<char>& hash) {
  if (hash.size() != crypto::kSHA256Length) {
    return false;
  }

  for (const auto& i : list) {
    const std::string& s = i.GetString();
    if (s.find('/') == std::string::npos) {
      // No slashes mean that this is a webauthn RP ID, not a U2F AppID.
      continue;
    }

    if (crypto::SHA256HashString(s).compare(0, crypto::kSHA256Length,
                                            hash.data(),
                                            crypto::kSHA256Length) == 0) {
      return true;
    }
  }

  return false;
}

// AttestationPermissionRequest is a delegate class that provides information
// and callbacks to the PermissionRequestManager.
//
// PermissionRequestManager has a reference to this object and so this object
// must outlive it. Since attestation requests are never canceled,
// PermissionRequestManager guarentees that |RequestFinished| will always,
// eventually, be called. This object uses that fact to delete itself during
// |RequestFinished| and thus owns itself.
class AttestationPermissionRequest : public PermissionRequest {
 public:
  AttestationPermissionRequest(const GURL& app_id,
                               base::OnceCallback<void(bool)> callback)
      : app_id_(app_id), callback_(std::move(callback)) {}

  PermissionRequest::IconId GetIconId() const override {
    return kUsbSecurityKeyIcon;
  }

  base::string16 GetMessageTextFragment() const override {
    return l10n_util::GetStringUTF16(
        IDS_SECURITY_KEY_ATTESTATION_PERMISSION_FRAGMENT);
  }
  GURL GetOrigin() const override { return app_id_; }
  void PermissionGranted() override { std::move(callback_).Run(true); }
  void PermissionDenied() override { std::move(callback_).Run(false); }
  void Cancelled() override { std::move(callback_).Run(false); }

  void RequestFinished() override {
    if (callback_)
      std::move(callback_).Run(false);
    delete this;
  }

  PermissionRequestType GetPermissionRequestType() const override {
    return PermissionRequestType::PERMISSION_SECURITY_KEY_ATTESTATION;
  }

 private:
  ~AttestationPermissionRequest() override = default;

  const GURL app_id_;
  base::OnceCallback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(AttestationPermissionRequest);
};

}  // namespace

namespace extensions {

namespace api {

void CryptotokenRegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kSecurityKeyPermitAttestation);
}

CryptotokenPrivateCanOriginAssertAppIdFunction::
    CryptotokenPrivateCanOriginAssertAppIdFunction() = default;

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
    return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
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
    return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
  }
  // For legacy purposes, allow google.com origins to assert certain
  // gstatic.com appIds.
  // TODO(juanlang): remove when legacy constraints are removed.
  if (origin_etldp1 == kGoogleDotCom) {
    for (const char* id : kGoogleGstaticAppIds) {
      if (params->app_id_url == id)
        return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
    }
  }
  return RespondNow(OneArgument(std::make_unique<base::Value>(false)));
}

CryptotokenPrivateIsAppIdHashInEnterpriseContextFunction::
    CryptotokenPrivateIsAppIdHashInEnterpriseContextFunction() {}

ExtensionFunction::ResponseAction
CryptotokenPrivateIsAppIdHashInEnterpriseContextFunction::Run() {
  std::unique_ptr<cryptotoken_private::IsAppIdHashInEnterpriseContext::Params>
      params(
          cryptotoken_private::IsAppIdHashInEnterpriseContext::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const PrefService* const prefs = profile->GetPrefs();
  const base::ListValue* const permit_attestation =
      prefs->GetList(prefs::kSecurityKeyPermitAttestation);

  return RespondNow(ArgumentList(
      cryptotoken_private::IsAppIdHashInEnterpriseContext::Results::Create(
          ContainsAppIdByHash(*permit_attestation, params->app_id_hash))));
}

CryptotokenPrivateCanAppIdGetAttestationFunction::
    CryptotokenPrivateCanAppIdGetAttestationFunction() {}

ExtensionFunction::ResponseAction
CryptotokenPrivateCanAppIdGetAttestationFunction::Run() {
  std::unique_ptr<cryptotoken_private::CanAppIdGetAttestation::Params> params =
      cryptotoken_private::CanAppIdGetAttestation::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  const std::string& app_id = params->options.app_id;

  // If the appId is permitted by the enterprise policy then no permission
  // prompt is shown.
  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const PrefService* const prefs = profile->GetPrefs();
  const base::ListValue* const permit_attestation =
      prefs->GetList(prefs::kSecurityKeyPermitAttestation);

  if (std::find_if(permit_attestation->begin(), permit_attestation->end(),
                   [&app_id](const base::Value& v) -> bool {
                     return v.GetString() == app_id;
                   }) != permit_attestation->end()) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
  }

  // If prompting is disabled, allow attestation because that is the historical
  // behavior.
  if (!base::FeatureList::IsEnabled(features::kSecurityKeyAttestationPrompt)) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
  }

  // Otherwise, show a permission prompt and pass the user's decision back.
  const GURL app_id_url(app_id);
  EXTENSION_FUNCTION_VALIDATE(app_id_url.is_valid());

  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(params->options.tab_id, browser_context(),
                                    true /* include incognito windows */,
                                    nullptr /* out_browser */,
                                    nullptr /* out_tab_strip */, &web_contents,
                                    nullptr /* out_tab_index */)) {
    return RespondNow(Error("cannot find specified tab"));
  }

  PermissionRequestManager* permission_request_manager =
      PermissionRequestManager::FromWebContents(web_contents);
  if (!permission_request_manager) {
    return RespondNow(Error("no PermissionRequestManager"));
  }

  // The created AttestationPermissionRequest deletes itself once complete.
  permission_request_manager->AddRequest(new AttestationPermissionRequest(
      app_id_url,
      base::BindOnce(
          &CryptotokenPrivateCanAppIdGetAttestationFunction::Complete, this)));
  return RespondLater();
}

void CryptotokenPrivateCanAppIdGetAttestationFunction::Complete(bool result) {
  Respond(OneArgument(std::make_unique<base::Value>(result)));
}

}  // namespace api
}  // namespace extensions
