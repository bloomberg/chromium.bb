// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions_test_util.h"

#include "base/run_loop.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_set.h"

namespace extensions {
namespace permissions_test_util {

void GrantOptionalPermissionsAndWaitForCompletion(
    content::BrowserContext* browser_context,
    const Extension& extension,
    const PermissionSet& permissions) {
  base::RunLoop run_loop;
  PermissionsUpdater(browser_context)
      .GrantOptionalPermissions(extension, permissions, run_loop.QuitClosure());
  run_loop.Run();
}

void GrantRuntimePermissionsAndWaitForCompletion(
    content::BrowserContext* browser_context,
    const Extension& extension,
    const PermissionSet& permissions) {
  base::RunLoop run_loop;
  PermissionsUpdater(browser_context)
      .GrantRuntimePermissions(extension, permissions, run_loop.QuitClosure());
  run_loop.Run();
}

void RevokeOptionalPermissionsAndWaitForCompletion(
    content::BrowserContext* browser_context,
    const Extension& extension,
    const PermissionSet& permissions,
    PermissionsUpdater::RemoveType remove_type) {
  base::RunLoop run_loop;
  PermissionsUpdater(browser_context)
      .RevokeOptionalPermissions(extension, permissions, remove_type,
                                 run_loop.QuitClosure());
  run_loop.Run();
}

void RevokeRuntimePermissionsAndWaitForCompletion(
    content::BrowserContext* browser_context,
    const Extension& extension,
    const PermissionSet& permissions) {
  base::RunLoop run_loop;
  PermissionsUpdater(browser_context)
      .RevokeRuntimePermissions(extension, permissions, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace browsertest_util
}  // namespace extensions
