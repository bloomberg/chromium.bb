// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PERMISSIONS_UPDATER_H__
#define CHROME_BROWSER_EXTENSIONS_PERMISSIONS_UPDATER_H__
#pragma once

#include <string>

#include "base/memory/ref_counted.h"

namespace base {
class DictionaryValue;
}
class Extension;
class ExtensionPermissionSet;
class ExtensionPrefs;
class Profile;

namespace extensions {

// Updates an Extension's active and granted permissions in persistent storage
// and notifies interested parties of the changes.
class PermissionsUpdater {
 public:
  explicit PermissionsUpdater(Profile* profile);
  ~PermissionsUpdater();

  // Adds the set of |permissions| to the |extension|'s active permission set
  // and sends the relevant messages and notifications. This method assumes the
  // user has already been prompted, if necessary, for the extra permissions.
  void AddPermissions(const Extension* extension,
                      const ExtensionPermissionSet* permissions);

  // Removes the set of |permissions| from the |extension|'s active permission
  // set and sends the relevant messages and notifications.
  void RemovePermissions(const Extension* extension,
                         const ExtensionPermissionSet* permissions);

  // Adds all permissions in the |extension|'s active permissions to its
  // granted permission set.
  void GrantActivePermissions(const Extension* extension);

  // Sets the |extension|'s active permissions to |permissions|.
  void UpdateActivePermissions(const Extension* extension,
                               const ExtensionPermissionSet* permissions);

 private:
  enum EventType {
    ADDED,
    REMOVED,
  };

  // Dispatches specified event to the extension.
  void DispatchEvent(const std::string& extension_id,
                     const char* event_name,
                     const ExtensionPermissionSet* changed_permissions);

  // Issues the relevant events, messages and notifications when the
  // |extension|'s permissions have |changed| (|changed| is the delta).
  // Specifically, this sends the EXTENSION_PERMISSIONS_UPDATED notification,
  // the ExtensionMsg_UpdatePermissions IPC message, and fires the
  // onAdded/onRemoved events in the extension.
  void NotifyPermissionsUpdated(EventType event_type,
                                const Extension* extension,
                                const ExtensionPermissionSet* changed);

  // Gets the ExtensionPrefs for the associated profile.
  ExtensionPrefs* GetExtensionPrefs();

  Profile* profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PERMISSIONS_UPDATER_H__
