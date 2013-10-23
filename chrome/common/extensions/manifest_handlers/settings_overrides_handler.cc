// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"

using extensions::api::manifest_types::ChromeSettingsOverrides;

namespace extensions {
namespace {

scoped_ptr<GURL> CreateManifestURL(const std::string& url) {
  scoped_ptr<GURL> manifest_url(new GURL(url));
  if (!manifest_url->is_valid() ||
      !manifest_url->SchemeIsHTTPOrHTTPS())
    return scoped_ptr<GURL>();
  return manifest_url.Pass();
}

scoped_ptr<GURL> ParseHomepage(const ChromeSettingsOverrides& overrides,
                               string16* error) {
  if (!overrides.homepage)
    return scoped_ptr<GURL>();
  scoped_ptr<GURL> manifest_url = CreateManifestURL(*overrides.homepage);
  if (!manifest_url) {
    *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidHomepageOverrideURL, *overrides.homepage);
  }
  return manifest_url.Pass();
}

scoped_ptr<GURL> ParseStartupPage(const ChromeSettingsOverrides& overrides,
                                  string16* error) {
  if (!overrides.startup_page)
    return scoped_ptr<GURL>();
  scoped_ptr<GURL> manifest_url = CreateManifestURL(*overrides.startup_page);
  if (!manifest_url) {
    *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidStartupOverrideURL, *overrides.startup_page);
  }
  return manifest_url.Pass();
}

}  // namespace

SettingsOverride::SettingsOverride() {}

SettingsOverride::~SettingsOverride() {}

SettingsOverrideHandler::SettingsOverrideHandler() {}

SettingsOverrideHandler::~SettingsOverrideHandler() {}

bool SettingsOverrideHandler::Parse(Extension* extension, string16* error) {
  const base::Value* dict = NULL;
  CHECK(extension->manifest()->Get(manifest_keys::kSettingsOverride, &dict));
  scoped_ptr<ChromeSettingsOverrides> settings(
      ChromeSettingsOverrides::FromValue(*dict, error));
  if (!settings)
    return false;

  scoped_ptr<SettingsOverride> info(new SettingsOverride);
  info->homepage = ParseHomepage(*settings, error);
  info->search_engine = settings->search_provider.Pass();
  info->startup_page = ParseStartupPage(*settings, error);
  if (!info->homepage && !info->search_engine && !info->startup_page) {
    *error = ASCIIToUTF16(manifest_errors::kInvalidEmptySettingsOverrides);
    return false;
  }
  extension->SetManifestData(manifest_keys::kSettingsOverride,
                             info.release());
  return true;
}

const std::vector<std::string> SettingsOverrideHandler::Keys() const {
  return SingleKey(manifest_keys::kSettingsOverride);
}

}  // namespace extensions
