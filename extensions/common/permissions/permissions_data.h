// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_DATA_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_DATA_H_

#include <map>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_message.h"

class GURL;

namespace extensions {

class PermissionSet;
class APIPermissionSet;
class Extension;
class ManifestPermissionSet;
class URLPatternSet;
class UserScript;

// A container for the permissions data of the extension; also responsible for
// parsing the "permissions" and "optional_permissions" manifest keys. This
// class also contains the active (runtime) permissions for the extension.
class PermissionsData {
 public:
  PermissionsData();
  ~PermissionsData();

  // Delegate class to allow different contexts (e.g. browser vs renderer) to
  // have control over policy decisions.
  class PolicyDelegate {
   public:
    virtual ~PolicyDelegate() {}

    // Returns false if script access should be blocked on this page.
    // Otherwise, default policy should decide.
    virtual bool CanExecuteScriptOnPage(const Extension* extension,
                                        const GURL& document_url,
                                        const GURL& top_document_url,
                                        int tab_id,
                                        const UserScript* script,
                                        int process_id,
                                        std::string* error) = 0;
  };

  static void SetPolicyDelegate(PolicyDelegate* delegate);

  // Return the optional or required permission set for the given |extension|.
  static const PermissionSet* GetOptionalPermissions(
      const Extension* extension);
  static const PermissionSet* GetRequiredPermissions(
      const Extension* extension);

  // Return the temporary API permission set which is used during extension
  // initialization. Once initialization completes, this is NULL.
  static const APIPermissionSet* GetInitialAPIPermissions(
      const Extension* extension);
  static APIPermissionSet* GetInitialAPIPermissions(Extension* extension);

  // Set the scriptable hosts for the given |extension| during initialization.
  static void SetInitialScriptableHosts(Extension* extension,
                                        const URLPatternSet& scriptable_hosts);

  // Return the active (runtime) permissions for the given |extension|.
  static scoped_refptr<const PermissionSet> GetActivePermissions(
      const Extension* extension);
  // Sets the runtime permissions of the given |extension| to |permissions|.
  static void SetActivePermissions(const Extension* extension,
                                   const PermissionSet* active);

  // Gets the tab-specific host permissions of |tab_id|, or NULL if there
  // aren't any.
  static scoped_refptr<const PermissionSet> GetTabSpecificPermissions(
      const Extension* extension,
      int tab_id);
  // Updates the tab-specific permissions of |tab_id| to include those from
  // |permissions|.
  static void UpdateTabSpecificPermissions(
      const Extension* extension,
      int tab_id,
      scoped_refptr<const PermissionSet> permissions);
  // Clears the tab-specific permissions of |tab_id|.
  static void ClearTabSpecificPermissions(const Extension* extension,
                                          int tab_id);

  // Returns true if the |extension| has the given |permission|. Prefer
  // IsExtensionWithPermissionOrSuggestInConsole when developers may be using an
  // api that requires a permission they didn't know about, e.g. open web apis.
  // Note this does not include APIs with no corresponding permission, like
  // "runtime" or "browserAction".
  // TODO(mpcomplete): drop the "API" from these names, it's confusing.
  static bool HasAPIPermission(const Extension* extension,
                               APIPermission::ID permission);
  static bool HasAPIPermission(const Extension* extension,
                               const std::string& permission_name);
  static bool HasAPIPermissionForTab(const Extension* extension,
                                     int tab_id,
                                     APIPermission::ID permission);

  static bool CheckAPIPermissionWithParam(
      const Extension* extension,
      APIPermission::ID permission,
      const APIPermission::CheckParam* param);

  static const URLPatternSet& GetEffectiveHostPermissions(
      const Extension* extension);

  // Returns true if the |extension| can silently increase its permission level.
  // Users must approve permissions for unpacked and packed extensions in the
  // following situations:
  //  - when installing or upgrading packed extensions
  //  - when installing unpacked extensions that have NPAPI plugins
  //  - when either type of extension requests optional permissions
  static bool CanSilentlyIncreasePermissions(const Extension* extension);

  // Returns true if the extension does not require permission warnings
  // to be displayed at install time.
  static bool ShouldSkipPermissionWarnings(const Extension* extension);

  // Whether the |extension| has access to the given |url|.
  static bool HasHostPermission(const Extension* extension, const GURL& url);

  // Whether the |extension| has effective access to all hosts. This is true if
  // there is a content script that matches all hosts, if there is a host
  // permission grants access to all hosts (like <all_urls>) or an api
  // permission that effectively grants access to all hosts (e.g. proxy,
  // network, etc.)
  static bool HasEffectiveAccessToAllHosts(const Extension* extension);

  // Returns the full list of permission messages that the given |extension|
  // should display at install time.
  static PermissionMessages GetPermissionMessages(const Extension* extension);
  // Returns the full list of permission messages that the given |extension|
  // should display at install time. The messages are returned as strings
  // for convenience.
  static std::vector<string16> GetPermissionMessageStrings(
      const Extension* extension);

  // Returns the full list of permission details for messages that the given
  // |extension| should display at install time. The messages are returned as
  // strings for convenience.
  static std::vector<string16> GetPermissionMessageDetailsStrings(
      const Extension* extension);

  // Returns true if the given |extension| can execute script on a page. If a
  // UserScript object is passed, permission to run that specific script is
  // checked (using its matches list). Otherwise, permission to execute script
  // programmatically is checked (using the extension's host permission).
  //
  // This method is also aware of certain special pages that extensions are
  // usually not allowed to run script on.
  static bool CanExecuteScriptOnPage(const Extension* extension,
                                     const GURL& document_url,
                                     const GURL& top_document_url,
                                     int tab_id,
                                     const UserScript* script,
                                     int process_id,
                                     std::string* error);

  // Returns true if the given |extension| is a COMPONENT extension, or if it is
  // on the whitelist of extensions that can script all pages.
  static bool CanExecuteScriptEverywhere(const Extension* extension);

  // Returns true if the |extension| is allowed to obtain the contents of a
  // page as an image.  Since a page may contain sensitive information, this
  // is restricted to the extension's host permissions as well as the
  // extension page itself.
  static bool CanCaptureVisiblePage(const Extension* extension,
                                    const GURL& page_url,
                                    int tab_id,
                                    std::string* error);

  // Parse the permissions of a given extension in the initialization process.
  bool ParsePermissions(Extension* extension, string16* error);

  // Ensure manifest handlers provide their custom manifest permissions.
  void InitializeManifestPermissions(Extension* extension);

  // Finalize permissions after the initialization process completes.
  void FinalizePermissions(Extension* extension);

 private:
  struct InitialPermissions;
  typedef std::map<int, scoped_refptr<const PermissionSet> > TabPermissionsMap;

  // Temporary permissions during the initialization process; NULL after
  // initialization completes.
  scoped_ptr<InitialPermissions> initial_required_permissions_;
  scoped_ptr<InitialPermissions> initial_optional_permissions_;

  // The set of permissions the extension can request at runtime.
  scoped_refptr<const PermissionSet> optional_permission_set_;

  // The extension's required / default set of permissions.
  scoped_refptr<const PermissionSet> required_permission_set_;

  mutable base::Lock runtime_lock_;

  // The permission's which are currently active on the extension during
  // runtime.
  mutable scoped_refptr<const PermissionSet> active_permissions_;

  mutable TabPermissionsMap tab_specific_permissions_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsData);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_DATA_H_
