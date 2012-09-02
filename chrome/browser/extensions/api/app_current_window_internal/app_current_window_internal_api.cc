// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/app_current_window_internal/app_current_window_internal_api.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/extensions/extension_error_utils.h"

namespace extensions {

namespace {

const char kNoAssociatedShellWindow[] =
    "The context from which the function was called did not have an "
    "associated shell window.";

}  // namespace

bool AppCurrentWindowInternalExtensionFunction::RunImpl() {
  ShellWindowRegistry* registry = ShellWindowRegistry::Get(profile());
  DCHECK(registry);
  content::RenderViewHost* rvh = render_view_host();
  if (!rvh)
    // No need to set an error, since we won't return to the caller anyway if
    // there's no RVH.
    return false;
  ShellWindow* window = registry->GetShellWindowForRenderViewHost(rvh);
  if (!window) {
    error_ = kNoAssociatedShellWindow;
    return false;
  }
  return RunWithWindow(window);
}

bool AppCurrentWindowInternalFocusFunction::RunWithWindow(ShellWindow* window) {
  window->GetBaseWindow()->Activate();
  return true;
}

bool AppCurrentWindowInternalMaximizeFunction::RunWithWindow(
    ShellWindow* window) {
  window->GetBaseWindow()->Maximize();
  return true;
}

bool AppCurrentWindowInternalMinimizeFunction::RunWithWindow(
    ShellWindow* window) {
  window->GetBaseWindow()->Minimize();
  return true;
}

bool AppCurrentWindowInternalRestoreFunction::RunWithWindow(
    ShellWindow* window) {
  window->GetBaseWindow()->Restore();
  return true;
}

}  // namespace extensions
