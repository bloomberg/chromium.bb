// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PERMISSIONS_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PERMISSIONS_API_H__
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/render_process_host.h"

class Extension;
class ExtensionPermissionSet;
class ExtensionService;

class ExtensionPermissionsManager {
 public:
  explicit ExtensionPermissionsManager(ExtensionService* extension_service);
  ~ExtensionPermissionsManager();

  // Adds the set of |permissions| to the |extension|'s active permission set
  // and sends the relevant messages and notifications. This method assumes the
  // user has already been prompted, if necessary, for the extra permissions.
  void AddPermissions(const Extension* extension,
                      const ExtensionPermissionSet* permissions);

  // Removes the set of |permissions| from the |extension|'s active permission
  // set and sends the relevant messages and notifications.
  void RemovePermissions(const Extension* extension,
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

  ExtensionService* extension_service_;
};


// chrome.permissions.contains
class ContainsPermissionsFunction : public SyncExtensionFunction {
  virtual ~ContainsPermissionsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("permissions.contains")
};

// chrome.permissions.getAll
class GetAllPermissionsFunction : public SyncExtensionFunction {
  virtual ~GetAllPermissionsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("permissions.getAll")
};

// chrome.permissions.remove
class RemovePermissionsFunction : public SyncExtensionFunction {
  virtual ~RemovePermissionsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("permissions.remove")
};

// chrome.permissions.request
class RequestPermissionsFunction : public AsyncExtensionFunction,
                                   public ExtensionInstallUI::Delegate {
 public:
  // FOR TESTS ONLY to bypass the confirmation UI.
  static void SetAutoConfirmForTests(bool should_proceed);
  static void SetIgnoreUserGestureForTests(bool ignore);

  RequestPermissionsFunction();

  // Implementing ExtensionInstallUI::Delegate interface.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

 protected:
  virtual ~RequestPermissionsFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  scoped_ptr<ExtensionInstallUI> install_ui_;
  scoped_refptr<ExtensionPermissionSet> requested_permissions_;
  const Extension* extension_;
  DECLARE_EXTENSION_FUNCTION_NAME("permissions.request")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PERMISSIONS_API_H__
