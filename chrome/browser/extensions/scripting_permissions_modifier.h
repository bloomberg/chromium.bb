// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCRIPTING_PERMISSIONS_MODIFIER_H_
#define CHROME_BROWSER_EXTENSIONS_SCRIPTING_PERMISSIONS_MODIFIER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class PermissionSet;

// Responsible for managing the majority of click-to-script features, including
// granting, withholding, and querying host permissions, and determining if an
// extension has been affected by the click-to-script project.
class ScriptingPermissionsModifier {
 public:
  ScriptingPermissionsModifier(content::BrowserContext* browser_context,
                               const scoped_refptr<const Extension>& extension);
  ~ScriptingPermissionsModifier();

  // Returns true if the --scripts-require-action flag would possibly affect
  // the given extension and |permissions|. We pass in the |permissions|
  // explicitly, as we may need to check with permissions other than the ones
  // that are currently on the extension's PermissionsData.
  bool CanAffectExtension(const PermissionSet& permissions) const;

  // Returns true if the extension has been affected by the scripts-require-
  // action flag.
  bool HasAffectedExtension() const;

  // Grants the extension permission to run on the origin of |url|.
  void GrantHostPermission(const GURL& url) const;

  // Returns true if the extension has been explicitly granted permission to run
  // on the origin of |url|.
  bool HasGrantedHostPermission(const GURL& url) const;

  // Revokes permission to run on the origin of |url|. DCHECKs if |url| has not
  // been granted.
  void RemoveGrantedHostPermission(const GURL& url) const;

  // Takes in a set of permissions and withholds any permissions that should not
  // be granted, populating |granted_permissions_out| with the set of all
  // permissions that can be granted, and |withheld_permissions_out| with the
  // set of all withheld permissions.
  // If |use_initial_state| is true, this will treat the extension as though it
  // was just installed, not taking into account extra granted preferences.
  void WithholdPermissions(
      const PermissionSet& permissions,
      scoped_ptr<const PermissionSet>* granted_permissions_out,
      scoped_ptr<const PermissionSet>* withheld_permissions_out,
      bool use_initial_state) const;

  // Grants any withheld all-hosts (or all-hosts-like) permissions.
  void GrantWithheldImpliedAllHosts() const;

  // Revokes any granted all-hosts (or all-hosts-like) permissions.
  void WithholdImpliedAllHosts() const;

 private:
  content::BrowserContext* browser_context_;

  scoped_refptr<const Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(ScriptingPermissionsModifier);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCRIPTING_PERMISSIONS_MODIFIER_H_
