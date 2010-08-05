// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/blocked_plugin_manager.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

BlockedPluginManager::BlockedPluginManager(TabContents* tab_contents)
    : ConfirmInfoBarDelegate(tab_contents),
      tab_contents_(tab_contents) { }

void BlockedPluginManager::OnNonSandboxedPluginBlocked(const string16& name) {
  name_ = name;
  tab_contents_->AddInfoBar(this);
}

void BlockedPluginManager::OnBlockedPluginLoaded() {
  tab_contents_->RemoveInfoBar(this);
}

int BlockedPluginManager::GetButtons() const {
  return BUTTON_OK;
}

std::wstring BlockedPluginManager::GetButtonLabel(InfoBarButton button) const {
  if (button == BUTTON_OK)
    return l10n_util::GetString(IDS_PLUGIN_LOAD);
  return ConfirmInfoBarDelegate::GetButtonLabel(button);
}

std::wstring BlockedPluginManager::GetMessageText() const {
  return UTF16ToWide(l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED_PROMPT,
                                                name_));
}

std::wstring BlockedPluginManager::GetLinkText() {
  return l10n_util::GetString(IDS_LEARN_MORE);
}

SkBitmap* BlockedPluginManager::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_PLUGIN_INSTALL);
}

bool BlockedPluginManager::Accept() {
  tab_contents_->render_view_host()->LoadBlockedPlugins();
  return true;
}

bool BlockedPluginManager::LinkClicked(WindowOpenDisposition disposition) {
  // TODO(bauerb): Navigate to a help page explaining why we blocked the plugin,
  // once we have one.
  return false;
}
