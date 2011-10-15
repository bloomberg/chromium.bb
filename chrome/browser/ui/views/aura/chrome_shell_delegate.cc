// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/chrome_shell_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/aura_shell/launcher/launcher_types.h"

ChromeShellDelegate::ChromeShellDelegate() {
}

ChromeShellDelegate::~ChromeShellDelegate() {
}

void ChromeShellDelegate::CreateNewWindow() {
}

void ChromeShellDelegate::ShowApps() {
}

void ChromeShellDelegate::LauncherItemClicked(
    const aura_shell::LauncherItem& item) {
}

bool ChromeShellDelegate::ConfigureLauncherItem(
    aura_shell::LauncherItem* item) {
  BrowserView* view = BrowserView::GetBrowserViewForNativeWindow(item->window);
  if (!view)
    return false;
  item->type = (view->browser()->type() == Browser::TYPE_TABBED) ?
      aura_shell::TYPE_TABBED : aura_shell::TYPE_APP;
  return true;
}
