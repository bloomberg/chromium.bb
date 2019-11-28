// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/background_info.h"

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/switches.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;

namespace extensions {

namespace keys = manifest_keys;
namespace values = manifest_values;
namespace errors = manifest_errors;

namespace {

const char kBackground[] = "background";

static base::LazyInstance<BackgroundInfo>::DestructorAtExit
    g_empty_background_info = LAZY_INSTANCE_INITIALIZER;

const BackgroundInfo& GetBackgroundInfo(const Extension* extension) {
  BackgroundInfo* info = static_cast<BackgroundInfo*>(
      extension->GetManifestData(kBackground));
  if (!info)
    return g_empty_background_info.Get();
  return *info;
}

// Checks features that are restricted to manifest v2, populating |error| if
// the |extension|'s manifest version is too high.
// TODO(devlin): It's unfortunate that the features system doesn't handle this
// automatically, but it only adds a warning (rather than an error). Depending
// on how many keys we want to error on, it may make sense to change that.
bool CheckManifestV2RestrictedFeatures(const Extension* extension,
                                       base::string16* error) {
  if (extension->manifest_version() < 3) {
    // No special restrictions for manifest v2 extensions (or v1, if the legacy
    // commandline flag is being used).
    return true;
  }

  auto check_path = [error, extension](const char* path) {
    if (extension->manifest()->HasPath(path)) {
      *error = base::UTF8ToUTF16(ErrorUtils::FormatErrorMessage(
          errors::kBackgroundSpecificationInvalidForManifestV3, path));
      return false;
    }
    return true;
  };

  if (!check_path(keys::kBackgroundPage) ||
      !check_path(keys::kBackgroundScripts) ||
      !check_path(keys::kBackgroundPersistent)) {
    return false;
  }

  return true;
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
  return extension->GetResourceURL(kGeneratedBackgroundPageFilename);
}

// static
const std::string& BackgroundInfo::GetBackgroundServiceWorkerScript(
    const Extension* extension) {
  const BackgroundInfo& info = GetBackgroundInfo(extension);
  DCHECK(info.background_service_worker_script_.has_value());
  return *info.background_service_worker_script_;
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
bool BackgroundInfo::HasPersistentBackgroundPage(const Extension* extension)  {
  return GetBackgroundInfo(extension).has_persistent_background_page();
}

// static
bool BackgroundInfo::HasLazyBackgroundPage(const Extension* extension) {
  return GetBackgroundInfo(extension).has_lazy_background_page();
}

// static
bool BackgroundInfo::HasGeneratedBackgroundPage(const Extension* extension) {
  const BackgroundInfo& info = GetBackgroundInfo(extension);
  return !info.background_scripts_.empty();
}

// static
bool BackgroundInfo::AllowJSAccess(const Extension* extension) {
  return GetBackgroundInfo(extension).allow_js_access_;
}

// static
bool BackgroundInfo::IsServiceWorkerBased(const Extension* extension) {
  return GetBackgroundInfo(extension)
      .background_service_worker_script_.has_value();
}

bool BackgroundInfo::Parse(const Extension* extension, base::string16* error) {
  const std::string& bg_scripts_key = extension->is_platform_app() ?
      keys::kPlatformAppBackgroundScripts : keys::kBackgroundScripts;
  if (!CheckManifestV2RestrictedFeatures(extension, error) ||
      !LoadBackgroundScripts(extension, bg_scripts_key, error) ||
      !LoadBackgroundPage(extension, error) ||
      !LoadBackgroundServiceWorkerScript(extension, error) ||
      !LoadBackgroundPersistent(extension, error) ||
      !LoadAllowJSAccess(extension, error)) {
    return false;
  }

  int background_solution_sum =
      (background_url_.is_valid() ? 1 : 0) +
      (!background_scripts_.empty() ? 1 : 0) +
      (background_service_worker_script_.has_value() ? 1 : 0);
  if (background_solution_sum > 1) {
    *error = ASCIIToUTF16(errors::kInvalidBackgroundCombination);
    return false;
  }

  return true;
}

bool BackgroundInfo::LoadBackgroundScripts(const Extension* extension,
                                           const std::string& key,
                                           base::string16* error) {
  const base::Value* background_scripts_value = nullptr;
  if (!extension->manifest()->Get(key, &background_scripts_value))
    return true;

  CHECK(background_scripts_value);
  if (!background_scripts_value->is_list()) {
    *error = ASCIIToUTF16(errors::kInvalidBackgroundScripts);
    return false;
  }

  base::Value::ConstListView background_scripts =
      background_scripts_value->GetList();
  for (size_t i = 0; i < background_scripts.size(); ++i) {
    if (!background_scripts[i].is_string()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidBackgroundScript, base::NumberToString(i));
      return false;
    }
    background_scripts_.push_back(background_scripts[i].GetString());
  }

  return true;
}

bool BackgroundInfo::LoadBackgroundPage(const Extension* extension,
                                        const std::string& key,
                                        base::string16* error) {
  const base::Value* background_page_value = nullptr;
  if (!extension->manifest()->Get(key, &background_page_value))
    return true;

  if (!background_page_value->is_string()) {
    *error = ASCIIToUTF16(errors::kInvalidBackground);
    return false;
  }
  const std::string& background_str = background_page_value->GetString();

  if (extension->is_hosted_app()) {
    background_url_ = GURL(background_str);

    if (!PermissionsParser::HasAPIPermission(extension,
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
          (base::CommandLine::ForCurrentProcess()->HasSwitch(
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

bool BackgroundInfo::LoadBackgroundServiceWorkerScript(
    const Extension* extension,
    base::string16* error) {
  const base::Value* scripts_value = nullptr;
  if (!extension->manifest()->Get(keys::kBackgroundServiceWorkerScript,
                                  &scripts_value)) {
    return true;
  }

  DCHECK(scripts_value);
  if (!scripts_value->is_string()) {
    *error = ASCIIToUTF16(errors::kInvalidBackgroundServiceWorkerScript);
    return false;
  }

  background_service_worker_script_ = scripts_value->GetString();

  return true;
}

bool BackgroundInfo::LoadBackgroundPage(const Extension* extension,
                                        base::string16* error) {
  const char* key = extension->is_platform_app()
                        ? keys::kPlatformAppBackgroundPage
                        : keys::kBackgroundPage;
  return LoadBackgroundPage(extension, key, error);
}

bool BackgroundInfo::LoadBackgroundPersistent(const Extension* extension,
                                              base::string16* error) {
  if (extension->is_platform_app()) {
    is_persistent_ = false;
    return true;
  }

  const base::Value* background_persistent = NULL;
  if (!extension->manifest()->Get(keys::kBackgroundPersistent,
                                  &background_persistent)) {
    return true;
  }

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
                                       base::string16* error) {
  const base::Value* allow_js_access = NULL;
  if (!extension->manifest()->Get(keys::kBackgroundAllowJsAccess,
                                  &allow_js_access))
    return true;

  if (!allow_js_access->is_bool() ||
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

bool BackgroundManifestHandler::Parse(Extension* extension,
                                      base::string16* error) {
  std::unique_ptr<BackgroundInfo> info(new BackgroundInfo);
  if (!info->Parse(extension, error))
    return false;

  // Platform apps must have background pages.
  if (extension->is_platform_app() && !info->has_background_page()) {
    *error = ASCIIToUTF16(errors::kBackgroundRequiredForPlatformApps);
    return false;
  }
  // Lazy background pages are incompatible with the webRequest API.
  if (info->has_lazy_background_page() &&
      PermissionsParser::HasAPIPermission(extension,
                                          APIPermission::kWebRequest)) {
    *error = ASCIIToUTF16(errors::kWebRequestConflictsWithLazyBackground);
    return false;
  }

  if (!info->has_lazy_background_page() &&
      PermissionsParser::HasAPIPermission(
          extension, APIPermission::kTransientBackground)) {
    *error = ASCIIToUTF16(
        errors::kTransientBackgroundConflictsWithPersistentBackground);
    return false;
  }

  extension->SetManifestData(kBackground, std::move(info));
  return true;
}

bool BackgroundManifestHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  // Validate that background scripts exist.
  const std::vector<std::string>& background_scripts =
      BackgroundInfo::GetBackgroundScripts(extension);
  for (size_t i = 0; i < background_scripts.size(); ++i) {
    if (!base::PathExists(
            extension->GetResource(background_scripts[i]).GetFilePath())) {
      *error = l10n_util::GetStringFUTF8(
          IDS_EXTENSION_LOAD_BACKGROUND_SCRIPT_FAILED,
          base::UTF8ToUTF16(background_scripts[i]));
      return false;
    }
  }

  if (BackgroundInfo::IsServiceWorkerBased(extension)) {
    DCHECK(extension->is_extension());
    const std::string& background_service_worker_script =
        BackgroundInfo::GetBackgroundServiceWorkerScript(extension);
    if (!base::PathExists(
            extension->GetResource(background_service_worker_script)
                .GetFilePath())) {
      *error = l10n_util::GetStringFUTF8(
          IDS_EXTENSION_LOAD_BACKGROUND_SCRIPT_FAILED,
          base::UTF8ToUTF16(background_service_worker_script));
      return false;
    }
  }

  // Validate background page location, except for hosted apps, which should use
  // an external URL. Background page for hosted apps are verified when the
  // extension is created (in Extension::InitFromValue)
  if (BackgroundInfo::HasBackgroundPage(extension) &&
      !extension->is_hosted_app() && background_scripts.empty()) {
    base::FilePath page_path = file_util::ExtensionURLToRelativeFilePath(
        BackgroundInfo::GetBackgroundURL(extension));
    const base::FilePath path = extension->GetResource(page_path).GetFilePath();
    if (path.empty() || !base::PathExists(path)) {
      *error =
          l10n_util::GetStringFUTF8(IDS_EXTENSION_LOAD_BACKGROUND_PAGE_FAILED,
                                    page_path.LossyDisplayName());
      return false;
    }
  }

  if (extension->is_platform_app()) {
    const std::string manifest_key =
        std::string(keys::kPlatformAppBackground) + ".persistent";
    bool is_persistent = false;
    // Validate that packaged apps do not use a persistent background page.
    if (extension->manifest()->GetBoolean(manifest_key, &is_persistent) &&
        is_persistent) {
      warnings->push_back(
          InstallWarning(errors::kInvalidBackgroundPersistentInPlatformApp));
    }
    // Validate that packaged apps do not use the key 'background.persistent'.
    // Use the dictionary directly to prevent an access check as
    // 'background.persistent' is not available for packaged apps.
    if (extension->manifest()->value()->Get(keys::kBackgroundPersistent,
                                            NULL)) {
      warnings->push_back(
          InstallWarning(errors::kBackgroundPersistentInvalidForPlatformApps));
    }
  }

  return true;
}

bool BackgroundManifestHandler::AlwaysParseForType(Manifest::Type type) const {
  return type == Manifest::TYPE_PLATFORM_APP;
}

base::span<const char* const> BackgroundManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {
      keys::kBackgroundAllowJsAccess,       keys::kBackgroundPage,
      keys::kBackgroundPersistent,          keys::kBackgroundScripts,
      keys::kBackgroundServiceWorkerScript, keys::kPlatformAppBackgroundPage,
      keys::kPlatformAppBackgroundScripts};
  return kKeys;
}

}  // namespace extensions
