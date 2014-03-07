// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_UI_OVERRIDES_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_UI_OVERRIDES_HANDLER_H_

#include "chrome/common/extensions/api/manifest_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

class ManifestPermission;

// SettingsOverride is associated with "chrome_settings_overrides" manifest key.
// An extension can add a search engine as default or non-default, overwrite the
// homepage and append a startup page to the list.
struct SettingsOverrides : public Extension::ManifestData {
  SettingsOverrides();
  virtual ~SettingsOverrides();

  static const SettingsOverrides* Get(const Extension* extension);

  static bool RemovesBookmarkButton(
      const SettingsOverrides& settings_overrides);
  static bool RemovesBookmarkShortcut(
      const SettingsOverrides& settings_overrides);

  scoped_ptr<api::manifest_types::ChromeSettingsOverrides::Bookmarks_ui>
      bookmarks_ui;
  scoped_ptr<api::manifest_types::ChromeSettingsOverrides::Search_provider>
      search_engine;
  scoped_ptr<GURL> homepage;
  std::vector<GURL> startup_pages;

  scoped_ptr<ManifestPermission> manifest_permission;

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingsOverrides);
};

class SettingsOverridesHandler : public ManifestHandler {
 public:
  SettingsOverridesHandler();
  virtual ~SettingsOverridesHandler();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;
  virtual bool Validate(const Extension* extension,
                        std::string* error,
                        std::vector<InstallWarning>* warnings) const OVERRIDE;

  virtual ManifestPermission* CreatePermission() OVERRIDE;
  virtual ManifestPermission* CreateInitialRequiredPermission(
      const Extension* extension) OVERRIDE;

 private:
  class ManifestPermissionImpl;

  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SettingsOverridesHandler);
};

}  // namespace extensions
#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_UI_OVERRIDES_HANDLER_H_
