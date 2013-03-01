// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/background_info.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/permissions/api_permission_set.h"
#include "extensions/common/error_utils.h"

using base::DictionaryValue;
namespace keys = extension_manifest_keys;
namespace values = extension_manifest_values;
namespace errors = extension_manifest_errors;

namespace extensions {

namespace {

const char kBackground[] = "background";

static base::LazyInstance<BackgroundInfo> g_empty_background_info =
    LAZY_INSTANCE_INITIALIZER;

const BackgroundInfo& GetBackgroundInfo(const Extension* extension) {
  BackgroundInfo* info = static_cast<BackgroundInfo*>(
      extension->GetManifestData(kBackground));
  if (!info)
    return g_empty_background_info.Get();
  return *info;
}

}  // namespace

BackgroundInfo::BackgroundInfo()
    : is_persistent_(true),
      allow_js_access_(true) {
}

BackgroundInfo::~BackgroundInfo() {
}

// static
GURL BackgroundInfo::GetBackgroundURL(const Extension* extension) {
  const BackgroundInfo& info = GetBackgroundInfo(extension);
  if (info.background_scripts_.empty())
    return info.background_url_;
  return extension->GetResourceURL(
      extension_filenames::kGeneratedBackgroundPageFilename);
}

// static
const std::vector<std::string>& BackgroundInfo::GetBackgroundScripts(
    const Extension* extension) {
  return GetBackgroundInfo(extension).background_scripts_;
}

// static
bool BackgroundInfo::HasBackgroundPage(const Extension* extension) {
  return GetBackgroundInfo(extension).has_background_page();
}

// static
bool BackgroundInfo::AllowJSAccess(const Extension* extension) {
  return GetBackgroundInfo(extension).allow_js_access_;
}

// static
bool BackgroundInfo::HasPersistentBackgroundPage(const Extension* extension)  {
  const BackgroundInfo& info = GetBackgroundInfo(extension);
  return info.has_background_page() && info.is_persistent_;
}

// static
bool BackgroundInfo::HasLazyBackgroundPage(const Extension* extension) {
  const BackgroundInfo& info = GetBackgroundInfo(extension);
  return info.has_background_page() && !info.is_persistent_;
}

bool BackgroundInfo::Parse(const Extension* extension, string16* error) {
  const std::string& bg_scripts_key = extension->is_platform_app() ?
      keys::kPlatformAppBackgroundScripts : keys::kBackgroundScripts;
  if (!LoadBackgroundScripts(extension, bg_scripts_key, error) ||
      !LoadBackgroundPage(extension, error) ||
      !LoadBackgroundPersistent(extension, error) ||
      !LoadAllowJSAccess(extension, error)) {
    return false;
  }
  return true;
}

bool BackgroundInfo::LoadBackgroundScripts(const Extension* extension,
                                           const std::string& key,
                                           string16* error) {
  const Value* background_scripts_value = NULL;
  if (!extension->manifest()->Get(key, &background_scripts_value))
    return true;

  CHECK(background_scripts_value);
  if (background_scripts_value->GetType() != Value::TYPE_LIST) {
    *error = ASCIIToUTF16(errors::kInvalidBackgroundScripts);
    return false;
  }

  const ListValue* background_scripts = NULL;
  background_scripts_value->GetAsList(&background_scripts);
  for (size_t i = 0; i < background_scripts->GetSize(); ++i) {
    std::string script;
    if (!background_scripts->GetString(i, &script)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidBackgroundScript, base::IntToString(i));
      return false;
    }
    background_scripts_.push_back(script);
  }

  return true;
}

bool BackgroundInfo::LoadBackgroundPage(const Extension* extension,
                                        const std::string& key,
                                        string16* error) {
  const base::Value* background_page_value = NULL;
  if (!extension->manifest()->Get(key, &background_page_value))
    return true;

  if (!background_scripts_.empty()) {
    *error = ASCIIToUTF16(errors::kInvalidBackgroundCombination);
    return false;
  }

  std::string background_str;
  if (!background_page_value->GetAsString(&background_str)) {
    *error = ASCIIToUTF16(errors::kInvalidBackground);
    return false;
  }

  if (extension->is_hosted_app()) {
    background_url_ = GURL(background_str);

    if (!extension->initial_api_permissions()->count(
            APIPermission::kBackground)) {
      *error = ASCIIToUTF16(errors::kBackgroundPermissionNeeded);
      return false;
    }
    // Hosted apps require an absolute URL.
    if (!background_url_.is_valid()) {
      *error = ASCIIToUTF16(errors::kInvalidBackgroundInHostedApp);
      return false;
    }

    if (!(background_url_.SchemeIs("https") ||
          (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAllowHTTPBackgroundPage) &&
           background_url_.SchemeIs("http")))) {
      *error = ASCIIToUTF16(errors::kInvalidBackgroundInHostedApp);
      return false;
    }
  } else {
    background_url_ = extension->GetResourceURL(background_str);
  }

  return true;
}

bool BackgroundInfo::LoadBackgroundPage(const Extension* extension,
                                        string16* error) {
  if (extension->is_platform_app()) {
    return LoadBackgroundPage(
        extension, keys::kPlatformAppBackgroundPage, error);
  }

  if (!LoadBackgroundPage(extension, keys::kBackgroundPage, error))
    return false;
  if (background_url_.is_empty())
    return LoadBackgroundPage(extension, keys::kBackgroundPageLegacy, error);
  return true;
}

bool BackgroundInfo::LoadBackgroundPersistent(const Extension* extension,
                                              string16* error) {
  if (extension->is_platform_app()) {
    is_persistent_ = false;
    return true;
  }

  const Value* background_persistent = NULL;
  if (!extension->manifest()->Get(keys::kBackgroundPersistent,
                                  &background_persistent))
    return true;

  if (!background_persistent->GetAsBoolean(&is_persistent_)) {
    *error = ASCIIToUTF16(errors::kInvalidBackgroundPersistent);
    return false;
  }

  if (!has_background_page()) {
    *error = ASCIIToUTF16(errors::kInvalidBackgroundPersistentNoPage);
    return false;
  }

  return true;
}

bool BackgroundInfo::LoadAllowJSAccess(const Extension* extension,
                                       string16* error) {
  const Value* allow_js_access = NULL;
  if (!extension->manifest()->Get(keys::kBackgroundAllowJsAccess,
                                  &allow_js_access))
    return true;

  if (!allow_js_access->IsType(Value::TYPE_BOOLEAN) ||
      !allow_js_access->GetAsBoolean(&allow_js_access_)) {
    *error = ASCIIToUTF16(errors::kInvalidBackgroundAllowJsAccess);
    return false;
  }

  return true;
}

BackgroundManifestHandler::BackgroundManifestHandler() {
}

BackgroundManifestHandler::~BackgroundManifestHandler() {
}

bool BackgroundManifestHandler::Parse(Extension* extension, string16* error) {
  scoped_ptr<BackgroundInfo> info(new BackgroundInfo);
  if (!info->Parse(extension, error))
    return false;
  extension->SetManifestData(kBackground, info.release());
  return true;
}

const std::vector<std::string> BackgroundManifestHandler::Keys() const {
  static const char* keys[] = {
    keys::kBackgroundAllowJsAccess,
    keys::kBackgroundPage,
    keys::kBackgroundPageLegacy,
    keys::kBackgroundPersistent,
    keys::kBackgroundScripts,
    keys::kPlatformAppBackgroundPage,
    keys::kPlatformAppBackgroundScripts
  };
  return std::vector<std::string>(keys, keys + arraysize(keys));
}

}  // extensions
