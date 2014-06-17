// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/settings_override_permission.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using extensions::api::manifest_types::ChromeSettingsOverrides;

namespace extensions {
namespace {

const char* kWwwPrefix = "www.";

scoped_ptr<GURL> CreateManifestURL(const std::string& url) {
  scoped_ptr<GURL> manifest_url(new GURL(url));
  if (!manifest_url->is_valid() ||
      !manifest_url->SchemeIsHTTPOrHTTPS())
    return scoped_ptr<GURL>();
  return manifest_url.Pass();
}

scoped_ptr<GURL> ParseHomepage(const ChromeSettingsOverrides& overrides,
                               base::string16* error) {
  if (!overrides.homepage)
    return scoped_ptr<GURL>();
  scoped_ptr<GURL> manifest_url = CreateManifestURL(*overrides.homepage);
  if (!manifest_url) {
    *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidHomepageOverrideURL, *overrides.homepage);
  }
  return manifest_url.Pass();
}

std::vector<GURL> ParseStartupPage(const ChromeSettingsOverrides& overrides,
                                   base::string16* error) {
  std::vector<GURL> urls;
  if (!overrides.startup_pages)
    return urls;

  for (std::vector<std::string>::const_iterator i =
       overrides.startup_pages->begin(); i != overrides.startup_pages->end();
       ++i) {
    scoped_ptr<GURL> manifest_url = CreateManifestURL(*i);
    if (!manifest_url) {
      *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
          manifest_errors::kInvalidStartupOverrideURL, *i);
    } else {
      urls.push_back(GURL());
      urls.back().Swap(manifest_url.get());
    }
  }
  return urls;
}

scoped_ptr<ChromeSettingsOverrides::Search_provider> ParseSearchEngine(
    ChromeSettingsOverrides* overrides,
    base::string16* error) {
  if (!overrides->search_provider)
    return scoped_ptr<ChromeSettingsOverrides::Search_provider>();
  if (!CreateManifestURL(overrides->search_provider->search_url)) {
    *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidSearchEngineURL,
        overrides->search_provider->search_url);
    return scoped_ptr<ChromeSettingsOverrides::Search_provider>();
  }
  if (overrides->search_provider->prepopulated_id)
    return overrides->search_provider.Pass();
  if (!overrides->search_provider->name ||
      !overrides->search_provider->keyword ||
      !overrides->search_provider->encoding ||
      !overrides->search_provider->favicon_url) {
    *error =
        base::ASCIIToUTF16(manifest_errors::kInvalidSearchEngineMissingKeys);
    return scoped_ptr<ChromeSettingsOverrides::Search_provider>();
  }
  if (!CreateManifestURL(*overrides->search_provider->favicon_url)) {
    *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidSearchEngineURL,
        *overrides->search_provider->favicon_url);
    return scoped_ptr<ChromeSettingsOverrides::Search_provider>();
  }
  return overrides->search_provider.Pass();
}

// A www. prefix is not informative and thus not worth the limited real estate
// in the permissions UI.
std::string RemoveWwwPrefix(const std::string& url) {
  if (StartsWithASCII(url, kWwwPrefix, false))
    return url.substr(strlen(kWwwPrefix));
  return url;
}

}  // namespace

SettingsOverrides::SettingsOverrides() {}

SettingsOverrides::~SettingsOverrides() {}

// static
const SettingsOverrides* SettingsOverrides::Get(
    const Extension* extension) {
  return static_cast<SettingsOverrides*>(
      extension->GetManifestData(manifest_keys::kSettingsOverride));
}

SettingsOverridesHandler::SettingsOverridesHandler() {}

SettingsOverridesHandler::~SettingsOverridesHandler() {}

bool SettingsOverridesHandler::Parse(Extension* extension,
                                     base::string16* error) {
  const base::Value* dict = NULL;
  CHECK(extension->manifest()->Get(manifest_keys::kSettingsOverride, &dict));
  scoped_ptr<ChromeSettingsOverrides> settings(
      ChromeSettingsOverrides::FromValue(*dict, error));
  if (!settings)
    return false;

  scoped_ptr<SettingsOverrides> info(new SettingsOverrides);
  info->homepage = ParseHomepage(*settings, error);
  info->search_engine = ParseSearchEngine(settings.get(), error);
  info->startup_pages = ParseStartupPage(*settings, error);
  if (!info->homepage && !info->search_engine && info->startup_pages.empty()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidEmptyDictionary,
        manifest_keys::kSettingsOverride);
    return false;
  }

  if (info->search_engine) {
    PermissionsParser::AddAPIPermission(
        extension,
        new SettingsOverrideAPIPermission(
            PermissionsInfo::GetInstance()->GetByID(
                APIPermission::kSearchProvider),
            RemoveWwwPrefix(CreateManifestURL(info->search_engine->search_url)
                                ->GetOrigin()
                                .host())));
  }
  if (!info->startup_pages.empty()) {
    PermissionsParser::AddAPIPermission(
        extension,
        new SettingsOverrideAPIPermission(
            PermissionsInfo::GetInstance()->GetByID(
                APIPermission::kStartupPages),
            // We only support one startup page even though the type of the
            // manifest
            // property is a list, only the first one is used.
            RemoveWwwPrefix(info->startup_pages[0].GetContent())));
  }
  if (info->homepage) {
    PermissionsParser::AddAPIPermission(
        extension,
        new SettingsOverrideAPIPermission(
            PermissionsInfo::GetInstance()->GetByID(APIPermission::kHomepage),
            RemoveWwwPrefix(info->homepage.get()->GetContent())));
  }
  extension->SetManifestData(manifest_keys::kSettingsOverride,
                             info.release());
  return true;
}

const std::vector<std::string> SettingsOverridesHandler::Keys() const {
  return SingleKey(manifest_keys::kSettingsOverride);
}

}  // namespace extensions
