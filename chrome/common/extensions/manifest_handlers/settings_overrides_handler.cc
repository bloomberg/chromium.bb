// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/permissions/settings_override_permission.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_message.h"
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
  if (!CreateManifestURL(overrides->search_provider->favicon_url)) {
    *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidSearchEngineURL,
        overrides->search_provider->favicon_url);
    return scoped_ptr<ChromeSettingsOverrides::Search_provider>();
  }
  if (!CreateManifestURL(overrides->search_provider->search_url)) {
    *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidSearchEngineURL,
        overrides->search_provider->search_url);
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

// The manifest permission implementation supports a permission for overriding
// the bookmark UI.
class SettingsOverridesHandler::ManifestPermissionImpl
    : public ManifestPermission {
 public:
  explicit ManifestPermissionImpl(bool override_bookmarks_ui_permission)
      : override_bookmarks_ui_permission_(override_bookmarks_ui_permission) {}

  // extensions::ManifestPermission overrides.
  virtual std::string name() const OVERRIDE {
    return manifest_keys::kSettingsOverride;
  }

  virtual std::string id() const OVERRIDE {
    return name();
  }

  virtual bool HasMessages() const OVERRIDE {
    return override_bookmarks_ui_permission_;
  }

  virtual PermissionMessages GetMessages() const OVERRIDE {
    PermissionMessages result;
    if (override_bookmarks_ui_permission_) {
      result.push_back(PermissionMessage(
          PermissionMessage::kOverrideBookmarksUI,
          l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_OVERRIDE_BOOKMARKS_UI)));
    }
    return result;
  }

  virtual bool FromValue(const base::Value* value) OVERRIDE {
    return value && value->GetAsBoolean(&override_bookmarks_ui_permission_);
  }

  virtual scoped_ptr<base::Value> ToValue() const OVERRIDE {
    return scoped_ptr<base::Value>(
        new base::FundamentalValue(override_bookmarks_ui_permission_)).Pass();
  }

  virtual ManifestPermission* Clone() const OVERRIDE {
    return scoped_ptr<ManifestPermissionImpl>(
        new ManifestPermissionImpl(
            override_bookmarks_ui_permission_)).release();
  }

  virtual ManifestPermission* Diff(const ManifestPermission* rhs) const
      OVERRIDE {
    const ManifestPermissionImpl* other =
        static_cast<const ManifestPermissionImpl*>(rhs);

    return scoped_ptr<ManifestPermissionImpl>(new ManifestPermissionImpl(
        override_bookmarks_ui_permission_ &&
        !other->override_bookmarks_ui_permission_)).release();
  }

  virtual ManifestPermission* Union(const ManifestPermission* rhs) const
      OVERRIDE {
    const ManifestPermissionImpl* other =
        static_cast<const ManifestPermissionImpl*>(rhs);

    return scoped_ptr<ManifestPermissionImpl>(new ManifestPermissionImpl(
        override_bookmarks_ui_permission_ ||
        other->override_bookmarks_ui_permission_)).release();
  }

  virtual ManifestPermission* Intersect(const ManifestPermission* rhs) const
      OVERRIDE {
    const ManifestPermissionImpl* other =
        static_cast<const ManifestPermissionImpl*>(rhs);

    return scoped_ptr<ManifestPermissionImpl>(new ManifestPermissionImpl(
        override_bookmarks_ui_permission_ &&
        other->override_bookmarks_ui_permission_)).release();
  }

  virtual bool Contains(const ManifestPermission* rhs) const OVERRIDE {
    const ManifestPermissionImpl* other =
        static_cast<const ManifestPermissionImpl*>(rhs);

    return !other->override_bookmarks_ui_permission_ ||
        override_bookmarks_ui_permission_;
  }

  virtual bool Equal(const ManifestPermission* rhs) const OVERRIDE {
    const ManifestPermissionImpl* other =
        static_cast<const ManifestPermissionImpl*>(rhs);

    return override_bookmarks_ui_permission_ ==
        other->override_bookmarks_ui_permission_;
  }

  virtual void Write(IPC::Message* m) const OVERRIDE {
    IPC::WriteParam(m, override_bookmarks_ui_permission_);
  }

  virtual bool Read(const IPC::Message* m, PickleIterator* iter) OVERRIDE {
    return IPC::ReadParam(m, iter, &override_bookmarks_ui_permission_);
  }

  virtual void Log(std::string* log) const OVERRIDE {
    IPC::LogParam(override_bookmarks_ui_permission_, log);
  }

 private:
  bool override_bookmarks_ui_permission_;
};

SettingsOverrides::SettingsOverrides() {}

SettingsOverrides::~SettingsOverrides() {}

const SettingsOverrides* SettingsOverrides::Get(
    const Extension* extension) {
  return static_cast<SettingsOverrides*>(
      extension->GetManifestData(manifest_keys::kSettingsOverride));
}

bool SettingsOverrides::RemovesBookmarkButton(
    const SettingsOverrides& settings_overrides) {
  return settings_overrides.bookmarks_ui &&
      settings_overrides.bookmarks_ui->remove_button &&
      *settings_overrides.bookmarks_ui->remove_button;
}

bool SettingsOverrides::RemovesBookmarkShortcut(
    const SettingsOverrides& settings_overrides) {
  return settings_overrides.bookmarks_ui &&
      settings_overrides.bookmarks_ui->remove_bookmark_shortcut &&
      *settings_overrides.bookmarks_ui->remove_bookmark_shortcut;
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
  info->bookmarks_ui.swap(settings->bookmarks_ui);
  // Support backward compatibility for deprecated key
  // chrome_settings_overrides.bookmarks_ui.hide_bookmark_button.
  if (info->bookmarks_ui && !info->bookmarks_ui->remove_button &&
      info->bookmarks_ui->hide_bookmark_button) {
    info->bookmarks_ui->remove_button.reset(
        new bool(*info->bookmarks_ui->hide_bookmark_button));
  }
  info->homepage = ParseHomepage(*settings, error);
  info->search_engine = ParseSearchEngine(settings.get(), error);
  info->startup_pages = ParseStartupPage(*settings, error);
  if (!info->bookmarks_ui && !info->homepage &&
      !info->search_engine && info->startup_pages.empty()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidEmptyDictionary,
        manifest_keys::kSettingsOverride);
    return false;
  }
  info->manifest_permission.reset(new ManifestPermissionImpl(
      SettingsOverrides::RemovesBookmarkButton(*info)));

  APIPermissionSet* permission_set =
      PermissionsData::GetInitialAPIPermissions(extension);
  DCHECK(permission_set);
  if (info->search_engine) {
    permission_set->insert(new SettingsOverrideAPIPermission(
        PermissionsInfo::GetInstance()->GetByID(APIPermission::kSearchProvider),
        RemoveWwwPrefix(CreateManifestURL(info->search_engine->search_url)->
            GetOrigin().host())));
  }
  if (!info->startup_pages.empty()) {
    permission_set->insert(new SettingsOverrideAPIPermission(
        PermissionsInfo::GetInstance()->GetByID(APIPermission::kStartupPages),
        // We only support one startup page even though the type of the manifest
        // property is a list, only the first one is used.
        RemoveWwwPrefix(info->startup_pages[0].GetContent())));
  }
  if (info->homepage) {
    permission_set->insert(new SettingsOverrideAPIPermission(
        PermissionsInfo::GetInstance()->GetByID(APIPermission::kHomepage),
        RemoveWwwPrefix(info->homepage.get()->GetContent())));
  }
  extension->SetManifestData(manifest_keys::kSettingsOverride,
                             info.release());
  return true;
}

bool SettingsOverridesHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  const SettingsOverrides* settings_overrides =
      SettingsOverrides::Get(extension);

  if (settings_overrides && settings_overrides->bookmarks_ui) {
    if (!FeatureSwitch::enable_override_bookmarks_ui()->IsEnabled()) {
      warnings->push_back(InstallWarning(
          ErrorUtils::FormatErrorMessage(
              manifest_errors::kUnrecognizedManifestKey,
              manifest_keys::kBookmarkUI)));
    } else if (settings_overrides->bookmarks_ui->hide_bookmark_button) {
      warnings->push_back(InstallWarning(
            ErrorUtils::FormatErrorMessage(
                manifest_errors::kKeyIsDeprecatedWithReplacement,
                manifest_keys::kHideBookmarkButton,
                manifest_keys::kRemoveButton)));
    }
  }

  return true;
}

ManifestPermission* SettingsOverridesHandler::CreatePermission() {
  return new ManifestPermissionImpl(false);
}

ManifestPermission* SettingsOverridesHandler::CreateInitialRequiredPermission(
    const Extension* extension) {
  const SettingsOverrides* data = SettingsOverrides::Get(extension);
  if (data)
    return data->manifest_permission->Clone();
  return NULL;
}
const std::vector<std::string> SettingsOverridesHandler::Keys() const {
  return SingleKey(manifest_keys::kSettingsOverride);
}

}  // namespace extensions
