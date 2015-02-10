// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cryptotoken_private/cryptotoken_private_api.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "extensions/common/error_utils.h"
#include "grit/theme_resources.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {
namespace api {

const char kGoogleDotCom[] = "google.com";
const char* kGoogleGstaticAppIds[] = {
  "https://www.gstatic.com/securitykey/origins.json",
  "https://www.gstatic.com/securitykey/a/google.com/origins.json"
};

namespace {

class CryptotokenPermissionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  typedef base::Callback<void(cryptotoken_private::PermissionResult)>
      InfoBarCallback;

  static void Create(InfoBarService* infobar_service,
                     const base::string16& message,
                     const InfoBarCallback& callback) {
    infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
        scoped_ptr<ConfirmInfoBarDelegate>(
            new CryptotokenPermissionInfoBarDelegate(message, callback))));
  }

 private:
  CryptotokenPermissionInfoBarDelegate(const base::string16& message,
                                       const InfoBarCallback& callback)
      : message_(message), callback_(callback), answered_(false) {}

  ~CryptotokenPermissionInfoBarDelegate() override {
    if (!answered_)
      callback_.Run(cryptotoken_private::PERMISSION_RESULT_DISMISSED);
  }

  // ConfirmInfoBarDelegate:
  base::string16 GetMessageText() const override { return message_; }
  base::string16 GetButtonLabel(InfoBarButton button) const override {
    return l10n_util::GetStringUTF16((button == BUTTON_OK)
                                         ? IDS_CRYPTOTOKEN_ALLOW_BUTTON
                                         : IDS_CRYPTOTOKEN_DENY_BUTTON);
  }

  bool Accept() override {
    answered_ = true;
    callback_.Run(cryptotoken_private::PERMISSION_RESULT_ALLOWED);
    return true;
  }
  bool Cancel() override {
    answered_ = true;
    callback_.Run(cryptotoken_private::PERMISSION_RESULT_DENIED);
    return true;
  }

  // InfoBarDelegate:
  Type GetInfoBarType() const override { return PAGE_ACTION_TYPE; }
  int GetIconID() const override { return IDR_INFOBAR_CRYPTOTOKEN; }

  base::string16 message_;
  InfoBarCallback callback_;
  bool answered_;
};

}  // namespace

CryptotokenPrivateRequestPermissionFunction::
    CryptotokenPrivateRequestPermissionFunction()
    : chrome_details_(this) {
}

ExtensionFunction::ResponseAction
CryptotokenPrivateRequestPermissionFunction::Run() {
  scoped_ptr<cryptotoken_private::RequestPermission::Params> params =
      cryptotoken_private::RequestPermission::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  content::WebContents* web_contents = NULL;
  if (!extensions::ExtensionTabUtil::GetTabById(
          params->tab_id, chrome_details_.GetProfile(), true, NULL, NULL,
          &web_contents, NULL)) {
    return RespondNow(Error(extensions::ErrorUtils::FormatErrorMessage(
        extensions::tabs_constants::kTabNotFoundError,
        base::IntToString(params->tab_id))));
  }
  DCHECK(web_contents);

  // Fetch the eTLD+1, for display purposes only
  const GURL origin_url(params->security_origin);
  if (!origin_url.is_valid()) {
    return RespondNow(Error(extensions::ErrorUtils::FormatErrorMessage(
        "Security origin * is not a valid URL", params->security_origin)));
  }
  const std::string etldp1 =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (etldp1.empty()) {
    return RespondNow(Error(extensions::ErrorUtils::FormatErrorMessage(
        "Could not find an eTLD for origin *", params->security_origin)));
  }

  CryptotokenPermissionInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents),
      l10n_util::GetStringFUTF16(IDS_CRYPTOTOKEN_INFOBAR_QUESTION,
                                 base::UTF8ToUTF16(etldp1)),
      base::Bind(
          &CryptotokenPrivateRequestPermissionFunction::OnInfobarResponse,
          this));

  return RespondLater();
}

void CryptotokenPrivateRequestPermissionFunction::OnInfobarResponse(
    cryptotoken_private::PermissionResult result) {
  Respond(ArgumentList(
      cryptotoken_private::RequestPermission::Results::Create(result)));
}

CryptotokenPrivateCanOriginAssertAppIdFunction::
    CryptotokenPrivateCanOriginAssertAppIdFunction()
    : chrome_details_(this) {
}

ExtensionFunction::ResponseAction
CryptotokenPrivateCanOriginAssertAppIdFunction::Run() {
  scoped_ptr<cryptotoken_private::CanOriginAssertAppId::Params> params =
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
    return RespondNow(OneArgument(new base::FundamentalValue(true)));
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
    return RespondNow(OneArgument(new base::FundamentalValue(true)));
  }
  // For legacy purposes, allow google.com origins to assert certain
  // gstatic.com appIds.
  // TODO(juanlang): remove when legacy constraints are removed.
  if (origin_etldp1 == kGoogleDotCom) {
    for (size_t i = 0;
         i < sizeof(kGoogleGstaticAppIds) / sizeof(kGoogleGstaticAppIds[0]);
         i++) {
      if (params->app_id_url == kGoogleGstaticAppIds[i]) {
        return RespondNow(OneArgument(new base::FundamentalValue(true)));
      }
    }
  }
  return RespondNow(OneArgument(new base::FundamentalValue(false)));
}

}  // namespace api
}  // namespace extensions
