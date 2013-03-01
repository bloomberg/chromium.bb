// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "extensions/common/error_utils.h"

namespace keys = extension_manifest_keys;
namespace errors = extension_manifest_errors;

namespace {

// Manifest keys.
const char kClientId[] = "client_id";
const char kScopes[] = "scopes";

}  // namespace

namespace extensions {

OAuth2Info::OAuth2Info() {}
OAuth2Info::~OAuth2Info() {}

static base::LazyInstance<OAuth2Info> g_empty_oauth2_info =
    LAZY_INSTANCE_INITIALIZER;

// static
const OAuth2Info& OAuth2Info::GetOAuth2Info(const Extension* extension) {
  OAuth2Info* info = static_cast<OAuth2Info*>(
      extension->GetManifestData(keys::kOAuth2));
  return info ? *info : g_empty_oauth2_info.Get();
}

OAuth2ManifestHandler::OAuth2ManifestHandler() {
}

OAuth2ManifestHandler::~OAuth2ManifestHandler() {
}

bool OAuth2ManifestHandler::Parse(Extension* extension,
                                  string16* error) {
  scoped_ptr<OAuth2Info> info(new OAuth2Info);
  const DictionaryValue* dict = NULL;
  if (!extension->manifest()->GetDictionary(keys::kOAuth2, &dict) ||
      !dict->GetString(kClientId, &info->client_id) ||
      info->client_id.empty()) {
    *error = ASCIIToUTF16(errors::kInvalidOAuth2ClientId);
    return false;
  }

  const ListValue* list = NULL;
  if (!dict->GetList(kScopes, &list)) {
    *error = ASCIIToUTF16(errors::kInvalidOAuth2Scopes);
    return false;
  }

  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string scope;
    if (!list->GetString(i, &scope)) {
      *error = ASCIIToUTF16(errors::kInvalidOAuth2Scopes);
      return false;
    }
    info->scopes.push_back(scope);
  }

  extension->SetManifestData(keys::kOAuth2, info.release());
  return true;
}

const std::vector<std::string> OAuth2ManifestHandler::Keys() const {
  return SingleKey(keys::kOAuth2);
}

}  // namespace extensions
