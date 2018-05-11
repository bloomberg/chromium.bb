// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_app_view_guest_delegate.h"

#include "extensions/shell/browser/shell_app_delegate.h"

namespace extensions {

ShellAppViewGuestDelegate::ShellAppViewGuestDelegate() {
}

ShellAppViewGuestDelegate::~ShellAppViewGuestDelegate() {
}

bool ShellAppViewGuestDelegate::HandleContextMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  // Eat the context menu request, as AppShell doesn't show context menus.
  return true;
}

AppDelegate* ShellAppViewGuestDelegate::CreateAppDelegate() {
  return new ShellAppDelegate();
}

}  // namespace extensions
