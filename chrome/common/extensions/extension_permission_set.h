// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_PERMISSION_SET_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_PERMISSION_SET_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/string16.h"
#include "chrome/common/extensions/url_pattern_set.h"

class Extension;
class ExtensionPermissionsInfo;

// When prompting the user to install or approve permissions, we display
// messages describing the effects of the permissions rather than listing the
// permissions themselves. Each ExtensionPermissionMessage represents one of the
// messages shown to the user.
class ExtensionPermissionMessage {
 public:
  // Do not reorder this enumeration. If you need to add a new enum, add it just
  // prior to kEnumBoundary.
  enum ID {
    kUnknown,
    kNone,
    kBookmarks,
    kGeolocation,
    kBrowsingHistory,
    kTabs,
    kManagement,
    kDebugger,
    kHosts1,
    kHosts2,
    kHosts3,
    kHosts4OrMore,
    kHostsAll,
    kFullAccess,
    kClipboard,
    kTtsEngine,
    kContentSettings,
    kAllPageContent,
    kEnumBoundary
  };

  // Creates the corresponding permission message for a list of hosts. This is
  // simply a convenience method around the constructor, since the messages
  // change depending on what hosts are present.
  static ExtensionPermissionMessage CreateFromHostList(
      const std::set<std::string>& hosts);

  // Creates the corresponding permission message.
  ExtensionPermissionMessage(ID id, const string16& message);
  ~ExtensionPermissionMessage();

  // Gets the id of the permission message, which can be used in UMA
  // histograms.
  ID id() const { return id_; }

  // Gets a localized message describing this permission. Please note that
  // the message will be empty for message types TYPE_NONE and TYPE_UNKNOWN.
  const string16& message() const { return message_; }

  // Comparator to work with std::set.
  bool operator<(const ExtensionPermissionMessage& that) const {
    return id_ < that.id_;
  }

 private:
  ID id_;
  string16 message_;
};

typedef std::vector<ExtensionPermissionMessage> ExtensionPermissionMessages;

// The ExtensionAPIPermission is an immutable class that describes a single
// named permission (API permission).
class ExtensionAPIPermission {
 public:
  enum ID {
    // Error codes.
    kInvalid = -2,
    kUnknown = -1,

    // Real permissions.
    kBackground,
    kBookmark,
    kChromeAuthPrivate,
    kChromePrivate,
    kChromeosInfoPrivate,
    kClipboardRead,
    kClipboardWrite,
    kContentSettings,
    kContextMenus,
    kCookie,
    kDebugger,
    kDevtools,
    kExperimental,
    kFileBrowserHandler,
    kFileBrowserPrivate,
    kGeolocation,
    kHistory,
    kIdle,
    kInputMethodPrivate,
    kManagement,
    kMediaPlayerPrivate,
    kMetricsPrivate,
    kNotification,
    kPageCapture,
    kPlugin,
    kProxy,
    kSocket,
    kTab,
    kTts,
    kTtsEngine,
    kUnlimitedStorage,
    kWebNavigation,
    kWebRequest,
    kWebRequestBlocking,
    kWebSocketProxyPrivate,
    kWebstorePrivate,
    kEnumBoundary
  };

  enum Flag {
    kFlagNone = 0,

    // Indicates if the permission implies full access (native code).
    kFlagImpliesFullAccess = 1 << 0,

    // Indicates if the permission implies full URL access.
    kFlagImpliesFullURLAccess = 1 << 1,

    // Indicates that the permission is private to COMPONENT extensions.
    kFlagComponentOnly = 1 << 2,

    // Indicates that the permission supports the optional permissions API.
    kFlagSupportsOptional = 1 << 3,
  };

  // Flags for specifying what extension types can use the permission.
  enum TypeRestriction {
    kTypeNone = 0,

    // Extension::TYPE_EXTENSION and Extension::TYPE_USER_SCRIPT
    kTypeExtension = 1 << 0,

    // Extension::TYPE_HOSTED_APP
    kTypeHostedApp = 1 << 1,

    // Extension::TYPE_PACKAGED_APP
    kTypePackagedApp = 1 << 2,

    // Extension::TYPE_PLATFORM_APP
    kTypePlatformApp = 1 << 3,

    // Supports all types.
    kTypeAll = (1 << 4) - 1,

    // Convenience flag for all types except hosted apps.
    kTypeDefault = kTypeAll - kTypeHostedApp,
  };

  typedef std::set<ID> IDSet;

  ~ExtensionAPIPermission();

  // Returns the localized permission message associated with this api.
  ExtensionPermissionMessage GetMessage() const;

  int flags() const { return flags_; }

  int type_restrictions() const { return type_restrictions_; }

  ID id() const { return id_; }

  // Returns the message id associated with this permission.
  ExtensionPermissionMessage::ID message_id() const {
    return message_id_;
  }

  // Returns the name of this permission.
  const char* name() const { return name_; }

  // Returns true if this permission implies full access (e.g., native code).
  bool implies_full_access() const {
    return (flags_ & kFlagImpliesFullAccess) != 0;
  }

  // Returns true if this permission implies full URL access.
  bool implies_full_url_access() const {
    return (flags_ & kFlagImpliesFullURLAccess) != 0;
  }

  // Returns true if this permission can only be acquired by COMPONENT
  // extensions.
  bool is_component_only() const {
    return (flags_ & kFlagComponentOnly) != 0;
  }

  // Returns true if regular extensions can specify this permission.
  bool supports_extensions() const {
    return (type_restrictions_ & kTypeExtension) != 0;
  }

  // Returns true if hosted apps can specify this permission.
  bool supports_hosted_apps() const {
    return (type_restrictions_ & kTypeHostedApp) != 0;
  }

  // Returns true if packaged apps can specify this permission.
  bool supports_packaged_apps() const {
    return (type_restrictions_ & kTypePackagedApp) != 0;
  }

  // Returns true if platform apps can specify this permission.
  bool supports_platform_apps() const {
    return (type_restrictions_ & kTypePlatformApp) != 0;
  }

  // Returns true if this permission can be added and removed via the
  // optional permissions extension API.
  bool supports_optional() const {
    return (flags_ & kFlagSupportsOptional) != 0;
  }

  // Returns true if this permissions supports the specified |type|.
  bool supports_type(TypeRestriction type) const {
    return (type_restrictions_ & type) != 0;
  }

 private:
  // Instances should only be constructed from within ExtensionPermissionsInfo.
  friend class ExtensionPermissionsInfo;

  // Register ALL the permissions!
  static void RegisterAllPermissions(ExtensionPermissionsInfo* info);

  explicit ExtensionAPIPermission(
      ID id,
      const char* name,
      int l10n_message_id,
      ExtensionPermissionMessage::ID message_id,
      int flags,
      int type_restrictions);

  ID id_;
  const char* name_;
  int flags_;
  int type_restrictions_;
  int l10n_message_id_;
  ExtensionPermissionMessage::ID message_id_;
};

typedef std::set<ExtensionAPIPermission::ID> ExtensionAPIPermissionSet;

// Singleton that holds the extension permission instances and provides static
// methods for accessing them.
class ExtensionPermissionsInfo {
 public:
  // Returns a pointer to the singleton instance.
  static ExtensionPermissionsInfo* GetInstance();

  // Returns the permission with the given |id|, and NULL if it doesn't exist.
  ExtensionAPIPermission* GetByID(ExtensionAPIPermission::ID id);

  // Returns the permission with the given |name|, and NULL if none
  // exists.
  ExtensionAPIPermission* GetByName(std::string name);

  // Returns a set containing all valid api permission ids.
  ExtensionAPIPermissionSet GetAll();

  // Converts all the permission names in |permission_names| to permission ids.
  ExtensionAPIPermissionSet GetAllByName(
      const std::set<std::string>& permission_names);

  // Gets the total number of API permissions.
  size_t get_permission_count() { return permission_count_; }

 private:
  friend class ExtensionAPIPermission;

  ~ExtensionPermissionsInfo();
  ExtensionPermissionsInfo();

  // Registers an |alias| for a given permission |name|.
  void RegisterAlias(const char* name, const char* alias);

  // Registers a permission with the specified attributes and flags.
  void RegisterPermission(
      ExtensionAPIPermission::ID id,
      const char* name,
      int l10n_message_id,
      ExtensionPermissionMessage::ID message_id,
      int flags,
      int type_restrictions);

  // Maps permission ids to permissions.
  typedef std::map<ExtensionAPIPermission::ID, ExtensionAPIPermission*> IDMap;

  // Maps names and aliases to permissions.
  typedef std::map<std::string, ExtensionAPIPermission*> NameMap;

  IDMap id_map_;
  NameMap name_map_;

  size_t hosted_app_permission_count_;
  size_t permission_count_;

  friend struct DefaultSingletonTraits<ExtensionPermissionsInfo>;
  DISALLOW_COPY_AND_ASSIGN(ExtensionPermissionsInfo);
};

// The ExtensionPermissionSet is an immutable class that encapsulates an
// extension's permissions. The class exposes set operations for combining and
// manipulating the permissions.
class ExtensionPermissionSet
    : public base::RefCountedThreadSafe<ExtensionPermissionSet> {
 public:
  // Creates an empty permission set (e.g. default permissions).
  ExtensionPermissionSet();

  // Creates a new permission set based on the |extension| manifest data, and
  // the api and host permissions (|apis| and |hosts|). The effective hosts
  // of the newly created permission set will be inferred from the |extension|
  // manifest, |apis| and |hosts|.
  ExtensionPermissionSet(const Extension* extension,
                         const ExtensionAPIPermissionSet& apis,
                         const URLPatternSet& explicit_hosts);

  // Creates a new permission set based on the specified data.
  ExtensionPermissionSet(const ExtensionAPIPermissionSet& apis,
                         const URLPatternSet& explicit_hosts,
                         const URLPatternSet& scriptable_hosts);

  ~ExtensionPermissionSet();

  // Creates a new permission set equal to |set1| - |set2|, passing ownership of
  // the new set to the caller.
  static ExtensionPermissionSet* CreateDifference(
      const ExtensionPermissionSet* set1, const ExtensionPermissionSet* set2);

  // Creates a new permission set equal to the intersection of |set1| and
  // |set2|, passing ownership of the new set to the caller.
  static ExtensionPermissionSet* CreateIntersection(
      const ExtensionPermissionSet* set1, const ExtensionPermissionSet* set2);

  // Creates a new permission set equal to the union of |set1| and |set2|.
  // Passes ownership of the new set to the caller.
  static ExtensionPermissionSet* CreateUnion(
      const ExtensionPermissionSet* set1, const ExtensionPermissionSet* set2);

  bool operator==(const ExtensionPermissionSet& rhs) const;

  // Returns true if |set| is a subset of this.
  bool Contains(const ExtensionPermissionSet& set) const;

  // Gets the API permissions in this set as a set of strings.
  std::set<std::string> GetAPIsAsStrings() const;

  // Gets a list of the distinct hosts for displaying to the user.
  // NOTE: do not use this for comparing permissions, since this disgards some
  // information.
  std::set<std::string> GetDistinctHostsForDisplay() const;

  // Gets the localized permission messages that represent this set.
  ExtensionPermissionMessages GetPermissionMessages() const;

  // Gets the localized permission messages that represent this set (represented
  // as strings).
  std::vector<string16> GetWarningMessages() const;

  // Returns true if this is an empty set (e.g., the default permission set).
  bool IsEmpty() const;

  // Returns true if the set has the specified API permission.
  bool HasAPIPermission(ExtensionAPIPermission::ID permission) const;

  // Returns true if the permissions in this set grant access to the specified
  // |function_name|.
  bool HasAccessToFunction(const std::string& function_name) const;

  // Returns true if this includes permission to access |origin|.
  bool HasExplicitAccessToOrigin(const GURL& origin) const;

  // Returns true if this permission set includes access to script |url|.
  bool HasScriptableAccessToURL(const GURL& url) const;

  // Returns true if this permission set includes effective access to all
  // origins.
  bool HasEffectiveAccessToAllHosts() const;

  // Returns true if this permission set includes effective access to |url|.
  bool HasEffectiveAccessToURL(const GURL& url) const;

  // Returns ture if this permission set effectively represents full access
  // (e.g. native code).
  bool HasEffectiveFullAccess() const;

  // Returns true if this permission set includes permissions that are
  // restricted to internal extensions.
  bool HasPrivatePermissions() const;

  // Returns true if |permissions| has a greater privilege level than this
  // permission set (e.g., this permission set has less permissions).
  bool HasLessPrivilegesThan(const ExtensionPermissionSet* permissions) const;

  const ExtensionAPIPermissionSet& apis() const { return apis_; }

  const URLPatternSet& effective_hosts() const { return effective_hosts_; }

  const URLPatternSet& explicit_hosts() const { return explicit_hosts_; }

  const URLPatternSet& scriptable_hosts() const { return scriptable_hosts_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionPermissionSetTest,
                           HasLessHostPrivilegesThan);

  friend class base::RefCountedThreadSafe<ExtensionPermissionSet>;

  static std::set<std::string> GetDistinctHosts(
      const URLPatternSet& host_patterns,
      bool include_rcd,
      bool exclude_file_scheme);

  // Initializes the set based on |extension|'s manifest data.
  void InitImplicitExtensionPermissions(const Extension* extension);

  // Initializes the effective host permission based on the data in this set.
  void InitEffectiveHosts();

  // Gets the permission messages for the API permissions.
  std::set<ExtensionPermissionMessage> GetSimplePermissionMessages() const;

  // Returns true if |permissions| has an elevated API privilege level than
  // this set.
  bool HasLessAPIPrivilegesThan(
      const ExtensionPermissionSet* permissions) const;

  // Returns true if |permissions| has more host permissions compared to this
  // set.
  bool HasLessHostPrivilegesThan(
      const ExtensionPermissionSet* permissions) const;

  // The api list is used when deciding if an extension can access certain
  // extension APIs and features.
  ExtensionAPIPermissionSet apis_;

  // The list of hosts that can be accessed directly from the extension.
  // TODO(jstritar): Rename to "hosts_"?
  URLPatternSet explicit_hosts_;

  // The list of hosts that can be scripted by content scripts.
  // TODO(jstritar): Rename to "user_script_hosts_"?
  URLPatternSet scriptable_hosts_;

  // The list of hosts this effectively grants access to.
  URLPatternSet effective_hosts_;
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_PERMISSION_SET_H_
