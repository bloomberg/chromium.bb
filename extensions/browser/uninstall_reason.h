// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UNINSTALL_REASON_H_
#define EXTENSIONS_BROWSER_UNINSTALL_REASON_H_

namespace extensions {

enum UninstallReason {
  UNINSTALL_REASON_FOR_TESTING,         // Used for testing code only
  UNINSTALL_REASON_USER_INITIATED,      // User performed some UI gesture
  UNINSTALL_REASON_EXTENSION_DISABLED,  // Extension disabled due to error
  UNINSTALL_REASON_STORAGE_THRESHOLD_EXCEEDED,
  UNINSTALL_REASON_INSTALL_CANCELED,
  UNINSTALL_REASON_MANAGEMENT_API,
  UNINSTALL_REASON_SYNC,
  UNINSTALL_REASON_ORPHANED_THEME,
  UNINSTALL_REASON_ORPHANED_EPHEMERAL_EXTENSION,
  // The entries below imply bypassing checking user has permission to
  // uninstall the corresponding extension id.
  UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION,
  UNINSTALL_REASON_ORPHANED_SHARED_MODULE,
  UNINSTALL_REASON_INTERNAL_MANAGEMENT,  // Internal extensions (see usages)
  UNINSTALL_REASON_REINSTALL
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UNINSTALL_REASON_H_
