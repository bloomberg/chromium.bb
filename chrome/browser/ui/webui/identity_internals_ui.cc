// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/identity_internals_ui.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kExtensionId[] = "extensionId";
const char kExtensionName[] = "extensionName";
const char kScopes[] = "scopes";
const char kStatus[] = "status";
const char kTokenExpirationTime[] = "expirationTime";
const char kTokenId[] = "tokenId";
const char kGetTokensCallback[] = "identity_internals.returnTokens";

class IdentityInternalsUIMessageHandler : public content::WebUIMessageHandler {
 public:
  IdentityInternalsUIMessageHandler();
  virtual ~IdentityInternalsUIMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  const std::string GetExtensionName(
      const extensions::IdentityAPI::TokenCacheKey& token_cache_key);

  ListValue* GetScopes(
      const extensions::IdentityAPI::TokenCacheKey& token_cache_key);

  const base::string16 GetStatus(
      const extensions::IdentityTokenCacheValue& token_cache_value);

  const std::string GetExpirationTime(
      const extensions::IdentityTokenCacheValue& token_cache_value);

  DictionaryValue* GetInfoForToken(
      const extensions::IdentityAPI::TokenCacheKey& token_cache_key,
      const extensions::IdentityTokenCacheValue& token_cache_value);

  void GetInfoForAllTokens(const ListValue* args);
};

IdentityInternalsUIMessageHandler::IdentityInternalsUIMessageHandler() {}

IdentityInternalsUIMessageHandler::~IdentityInternalsUIMessageHandler() {}

const std::string IdentityInternalsUIMessageHandler::GetExtensionName(
    const extensions::IdentityAPI::TokenCacheKey& token_cache_key) {
  ExtensionService* extension_service = extensions::ExtensionSystem::Get(
      Profile::FromWebUI(web_ui()))->extension_service();
  const extensions::Extension* extension =
      extension_service->extensions()->GetByID(token_cache_key.extension_id);
  if (!extension)
    return std::string();
  return extension->name();
}

ListValue* IdentityInternalsUIMessageHandler::GetScopes(
    const extensions::IdentityAPI::TokenCacheKey& token_cache_key) {
  ListValue* scopes_value = new ListValue();
  for (std::set<std::string>::const_iterator
           iter = token_cache_key.scopes.begin();
       iter != token_cache_key.scopes.end(); ++iter) {
    scopes_value->AppendString(*iter);
  }
  return scopes_value;
}

const base::string16 IdentityInternalsUIMessageHandler::GetStatus(
    const extensions::IdentityTokenCacheValue& token_cache_value) {
  switch (token_cache_value.status()) {
    case extensions::IdentityTokenCacheValue::CACHE_STATUS_ADVICE:
      // Fallthrough to NOT FOUND case, as ADVICE is short lived.
    case extensions::IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND:
      return l10n_util::GetStringUTF16(
          IDS_IDENTITY_INTERNALS_TOKEN_NOT_FOUND);
    case extensions::IdentityTokenCacheValue::CACHE_STATUS_TOKEN:
      return l10n_util::GetStringUTF16(
          IDS_IDENTITY_INTERNALS_TOKEN_PRESENT);
  }
  NOTREACHED();
  return base::string16();
}

const std::string IdentityInternalsUIMessageHandler::GetExpirationTime(
    const extensions::IdentityTokenCacheValue& token_cache_value) {
  return UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
      token_cache_value.expiration_time()));
}

DictionaryValue* IdentityInternalsUIMessageHandler::GetInfoForToken(
    const extensions::IdentityAPI::TokenCacheKey& token_cache_key,
    const extensions::IdentityTokenCacheValue& token_cache_value) {
  DictionaryValue* token_data = new DictionaryValue();
  token_data->SetString(kExtensionId, token_cache_key.extension_id);
  token_data->SetString(kExtensionName, GetExtensionName(token_cache_key));
  token_data->Set(kScopes, GetScopes(token_cache_key));
  token_data->SetString(kStatus, GetStatus(token_cache_value));
  token_data->SetString(kTokenId, token_cache_value.token());
  token_data->SetString(kTokenExpirationTime,
                        GetExpirationTime(token_cache_value));
  return token_data;
}

void IdentityInternalsUIMessageHandler::GetInfoForAllTokens(
    const ListValue* args) {
  ListValue results;
  extensions::IdentityAPI::CachedTokens tokens =
      extensions::IdentityAPI::GetFactoryInstance()->GetForProfile(
          Profile::FromWebUI(web_ui()))->GetAllCachedTokens();
  for (extensions::IdentityAPI::CachedTokens::const_iterator
           iter = tokens.begin(); iter != tokens.end(); ++iter) {
    results.Append(GetInfoForToken(iter->first, iter->second));
  }

  web_ui()->CallJavascriptFunction(kGetTokensCallback, results);
}

void IdentityInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("identityInternalsGetTokens",
      base::Bind(&IdentityInternalsUIMessageHandler::GetInfoForAllTokens,
                 base::Unretained(this)));
}

}  // namespace

IdentityInternalsUI::IdentityInternalsUI(content::WebUI* web_ui)
  : content::WebUIController(web_ui) {
  // chrome://identity-internals source.
  content::WebUIDataSource* html_source =
    content::WebUIDataSource::Create(chrome::kChromeUIIdentityInternalsHost);
  html_source->SetUseJsonJSFormatV2();

  // Localized strings
  html_source->AddLocalizedString("tokenCacheHeader",
      IDS_IDENTITY_INTERNALS_TOKEN_CACHE_TEXT);
  html_source->AddLocalizedString("tokenId",
      IDS_IDENTITY_INTERNALS_TOKEN_ID);
  html_source->AddLocalizedString("extensionName",
      IDS_IDENTITY_INTERNALS_EXTENSION_NAME);
  html_source->AddLocalizedString("extensionId",
      IDS_IDENTITY_INTERNALS_EXTENSION_ID);
  html_source->AddLocalizedString("tokenStatus",
      IDS_IDENTITY_INTERNALS_TOKEN_STATUS);
  html_source->AddLocalizedString("expirationTime",
      IDS_IDENTITY_INTERNALS_EXPIRATION_TIME);
  html_source->AddLocalizedString("scopes",
      IDS_IDENTITY_INTERNALS_SCOPES);
  html_source->AddLocalizedString("revoke",
      IDS_IDENTITY_INTERNALS_REVOKE);
  html_source->SetJsonPath("strings.js");

  // Required resources
  html_source->AddResourcePath("identity_internals.css",
      IDR_IDENTITY_INTERNALS_CSS);
  html_source->AddResourcePath("identity_internals.js",
      IDR_IDENTITY_INTERNALS_JS);
  html_source->SetDefaultResource(IDR_IDENTITY_INTERNALS_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);

  web_ui->AddMessageHandler(new IdentityInternalsUIMessageHandler());
}

IdentityInternalsUI::~IdentityInternalsUI() {}

