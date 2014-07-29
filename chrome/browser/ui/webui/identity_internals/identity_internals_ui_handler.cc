// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/identity_internals/identity_internals_ui_handler.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/identity_internals/identity_internals_token_revoker.h"
#include "extensions/browser/extension_system.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

IdentityInternalsUIHandler::IdentityInternalsUIHandler(Profile* profile)
    : profile_(profile) {}

IdentityInternalsUIHandler::~IdentityInternalsUIHandler() {}

void IdentityInternalsUIHandler::OnTokenRevokerDone(
    IdentityInternalsTokenRevoker* token_revoker) {
  // Remove token from the cache.
  extensions::IdentityAPI::GetFactoryInstance()
      ->Get(profile_)
      ->EraseCachedToken(token_revoker->extension_id(),
                         token_revoker->access_token());

  // Update view about the token being removed.
  token_revoker->callback().Run();

  // Erase the revoker.
  ScopedVector<IdentityInternalsTokenRevoker>::iterator iter =
      std::find(token_revokers_.begin(), token_revokers_.end(), token_revoker);
  DCHECK(iter != token_revokers_.end());
  token_revokers_.erase(iter);
}

void IdentityInternalsUIHandler::GetTokens(
    const mojo::Callback<void(mojo::Array<IdentityTokenMojoPtr>)>& callback) {
  extensions::IdentityAPI::CachedTokens tokens =
      extensions::IdentityAPI::GetFactoryInstance()
          ->Get(profile_)->GetAllCachedTokens();
  callback.Run(ConvertCachedTokens(tokens).Pass());
}

void IdentityInternalsUIHandler::RevokeToken(
    const mojo::String& extension_id,
    const mojo::String& access_token,
    const mojo::Callback<void()>& callback) {
  token_revokers_.push_back(new IdentityInternalsTokenRevoker(
      extension_id, access_token, callback, profile_, this));
}

mojo::Array<IdentityTokenMojoPtr>
IdentityInternalsUIHandler::ConvertCachedTokens(
    const extensions::IdentityAPI::CachedTokens& tokens) {
  mojo::Array<IdentityTokenMojoPtr> array(tokens.size());
  size_t index = 0;
  for (extensions::IdentityAPI::CachedTokens::const_iterator
           it = tokens.begin(); it != tokens.end(); ++it, index++) {
    IdentityTokenMojoPtr item(IdentityTokenMojo::New());
    item->access_token = it->second.token();
    item->extension_name = GetExtensionName(it->first);
    item->extension_id = it->first.extension_id;
    item->token_status = GetStatus(it->second);
    item->expiration_time = GetExpirationTime(it->second);
    item->scopes = GetScopes(it->first).Pass();

    array[index] = item.Pass();
  }

  return array.Pass();
}

const std::string IdentityInternalsUIHandler::GetExtensionName(
    const extensions::ExtensionTokenKey& token_cache_key) {
  ExtensionService* extension_service = extensions::ExtensionSystem::Get(
      profile_)->extension_service();
  const extensions::Extension* extension =
      extension_service->extensions()->GetByID(token_cache_key.extension_id);
  if (!extension)
    return std::string();
  return extension->name();
}

mojo::Array<mojo::String> IdentityInternalsUIHandler::GetScopes(
    const extensions::ExtensionTokenKey& token_cache_key) {
  mojo::Array<mojo::String> array(token_cache_key.scopes.size());
  size_t index = 0;
  for (std::set<std::string>::const_iterator
           it = token_cache_key.scopes.begin();
       it != token_cache_key.scopes.end(); ++it, index++) {
    array[index] = mojo::String(*it);
  }
  return array.Pass();
}

const std::string IdentityInternalsUIHandler::GetStatus(
    const extensions::IdentityTokenCacheValue& token_cache_value) {
  switch (token_cache_value.status()) {
    case extensions::IdentityTokenCacheValue::CACHE_STATUS_ADVICE:
      // Fallthrough to NOT FOUND case, as ADVICE is short lived.
    case extensions::IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND:
      return l10n_util::GetStringUTF8(
          IDS_IDENTITY_INTERNALS_TOKEN_NOT_FOUND);
    case extensions::IdentityTokenCacheValue::CACHE_STATUS_TOKEN:
      return l10n_util::GetStringUTF8(
          IDS_IDENTITY_INTERNALS_TOKEN_PRESENT);
  }
  NOTREACHED();
  return std::string();
}

const std::string IdentityInternalsUIHandler::GetExpirationTime(
    const extensions::IdentityTokenCacheValue& token_cache_value) {
  return base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
      token_cache_value.expiration_time()));
}
