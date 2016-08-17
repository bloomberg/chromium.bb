// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCRIPTING_PERMISSIONS_MODIFIER_H_
#define CHROME_BROWSER_EXTENSIONS_SCRIPTING_PERMISSIONS_MODIFIER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ExtensionPrefs;
class PermissionSet;

// Responsible for managing the majority of click-to-script features, including
// granting, withholding, and querying host permissions, and determining if an
// extension has been affected by the click-to-script project.
class ScriptingPermissionsModifier {
 public:
  ScriptingPermissionsModifier(content::BrowserContext* browser_context,
                               const scoped_refptr<const Extension>& extension);
  ~ScriptingPermissionsModifier();

  // Sets whether or not the extension is allowed on all urls and handles the
  // case of the extension not being present in the registry (which can happen
  // if we sync the data before downloading the extension). For cases other than
  // sync, SetAllowedOnAllUrls() should be used.
  static void SetAllowedOnAllUrlsForSync(bool allowed,
                                         content::BrowserContext* context,
                                         const std::string& id);

  // Returns the default value for being allowed to script on all urls.
  static bool DefaultAllowedOnAllUrls();

  // Sets whether the extension should be allowed to execute on all urls without
  // explicit user consent. Used when the FeatureSwitch::scripts_require_action
  // switch is enabled.
  void SetAllowedOnAllUrls(bool allowed);

  // Returns whether the extension is allowed to execute scripts on all urls
  // without user consent.
  bool IsAllowedOnAllUrls();

  // Returns whether the user has set an explicit preference for the extension's
  // ability to execute scripts without consent.
  bool HasSetAllowedOnAllUrls() const;

  // Returns true if the --scripts-require-action flag would possibly affect
  // the given extension and |permissions|. We pass in the |permissions|
  // explicitly, as we may need to check with permissions other than the ones
  // that are currently on the extension's PermissionsData.
  bool CanAffectExtension(const PermissionSet& permissions) const;

  // Returns true if the extension has been affected by the scripts-require-
  // action flag.
  bool HasAffectedExtension() const;

  // Grants the extension permission to run on the origin of |url|.
  void GrantHostPermission(const GURL& url);

  // Returns true if the extension has been explicitly granted permission to run
  // on the origin of |url|.
  bool HasGrantedHostPermission(const GURL& url);

  // Revokes permission to run on the origin of |url|. DCHECKs if |url| has not
  // been granted.
  void RemoveGrantedHostPermission(const GURL& url);

  // Takes in a set of permissions and withholds any permissions that should not
  // be granted, populating |granted_permissions_out| with the set of all
  // permissions that can be granted, and |withheld_permissions_out| with the
  // set of all withheld permissions.
  // If |use_initial_state| is true, this will treat the extension as though it
  // was just installed, not taking into account extra granted preferences.
  void WithholdPermissions(
      const PermissionSet& permissions,
      std::unique_ptr<const PermissionSet>* granted_permissions_out,
      std::unique_ptr<const PermissionSet>* withheld_permissions_out,
      bool use_initial_state);

 private:
  // Grants any withheld all-hosts (or all-hosts-like) permissions.
  void GrantWithheldImpliedAllHosts();

  // Revokes any granted all-hosts (or all-hosts-like) permissions.
  void WithholdImpliedAllHosts();

  // Updates extension prefs in the case of improper values being found for
  // an extension.
  void CleanUpPrefsIfNecessary();

  content::BrowserContext* browser_context_;

  scoped_refptr<const Extension> extension_;

  ExtensionPrefs* extension_prefs_;

  DISALLOW_COPY_AND_ASSIGN(ScriptingPermissionsModifier);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCRIPTING_PERMISSIONS_MODIFIER_H_
