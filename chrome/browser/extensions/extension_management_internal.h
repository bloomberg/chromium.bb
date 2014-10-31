// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_INTERNAL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_INTERNAL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/extensions/extension_management.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/api_permission_set.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace extensions {

class URLPatternSet;

namespace internal {

// Class to hold extension management settings for one or a group of
// extensions. Settings can be applied to an individual extension identified
// by an ID, a group of extensions with specific |update_url| or all
// extensions at once.
struct IndividualSettings {
  enum ParsingScope {
    // Parses the default settings.
    SCOPE_DEFAULT = 0,
    // Parses the settings for an extension with specified extension ID.
    SCOPE_INDIVIDUAL,
  };

  IndividualSettings();
  ~IndividualSettings();

  void Reset();

  // Parses the individual settings. |dict| is the a sub-dictionary in extension
  // management preference and |scope| represents the applicable range of the
  // settings, a single extension, a group of extensions or default settings.
  // Note that in case of parsing errors, |this| will NOT be left untouched.
  // This method is required to be called in order of ParsingScope, i.e. first
  // SCOPE_DEFAULT, then SCOPE_INDIVIDUAL.
  bool Parse(const base::DictionaryValue* dict, ParsingScope scope);

  // Extension installation mode. Setting this to INSTALLATION_FORCED or
  // INSTALLATION_RECOMMENDED will enable extension auto-loading (only
  // applicable to single extension), and in this case the |update_url| must
  // be specified, containing the update URL for this extension.
  // Note that |update_url| will be ignored for INSTALLATION_ALLOWED and
  // INSTALLATION_BLOCKED installation mode.
  // These settings will override the default settings, and unspecified
  // settings will take value from default settings.
  ExtensionManagement::InstallationMode installation_mode;
  std::string update_url;

  // Permissions settings for extensions. These settings won't grant permissions
  // to extensions automatically. Instead, these settings will provide a list of
  // blocked permissions for each extension. That is, if an extension requires a
  // permission which has been blacklisted, this extension will not be allowed
  // to load. And if it contains a blocked permission as optional requirement,
  // it will be allowed to load (of course, with permission granted from user if
  // necessary), but conflicting permissions will be dropped. These settings
  // will merge from the default settings, and unspecified settings will take
  // value from default settings.
  APIPermissionSet blocked_permissions;

 private:
  DISALLOW_ASSIGN(IndividualSettings);
};

// Global extension management settings, applicable to all extensions.
struct GlobalSettings {
  GlobalSettings();
  ~GlobalSettings();

  void Reset();

  // Settings specifying which URLs are allowed to install extensions, will be
  // enforced only if |has_restricted_install_sources| is set to true.
  URLPatternSet install_sources;
  bool has_restricted_install_sources;

  // Settings specifying all allowed app/extension types, will be enforced
  // only of |has_restricted_allowed_types| is set to true.
  std::vector<Manifest::Type> allowed_types;
  bool has_restricted_allowed_types;

 private:
  DISALLOW_COPY_AND_ASSIGN(GlobalSettings);
};

}  // namespace internal

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_INTERNAL_H_
