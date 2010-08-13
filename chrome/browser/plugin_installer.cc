// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_installer.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "webkit/glue/plugins/default_plugin_shared.h"

PluginInstaller::PluginInstaller(TabContents* tab_contents)
    : ConfirmInfoBarDelegate(tab_contents),
      tab_contents_(tab_contents) {
}

PluginInstaller::~PluginInstaller() {
  // Remove any InfoBars we may be showing.
  tab_contents_->RemoveInfoBar(this);
}

void PluginInstaller::OnMissingPluginStatus(int status) {
  switch (status) {
    case default_plugin::MISSING_PLUGIN_AVAILABLE: {
      tab_contents_->AddInfoBar(this);
      break;
    }
    case default_plugin::MISSING_PLUGIN_USER_STARTED_DOWNLOAD: {
      // Hide the InfoBar if user already started download/install of the
      // missing plugin.
      tab_contents_->RemoveInfoBar(this);
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

string16 PluginInstaller::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGININSTALLER_MISSINGPLUGIN_PROMPT);
}

SkBitmap* PluginInstaller::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_PLUGIN_INSTALL);
}

int PluginInstaller::GetButtons() const {
  return BUTTON_OK;
}

string16 PluginInstaller::GetButtonLabel(InfoBarButton button) const {
  if (button == BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_PLUGININSTALLER_INSTALLPLUGIN_BUTTON);
  return ConfirmInfoBarDelegate::GetButtonLabel(button);
}

bool PluginInstaller::Accept() {
  tab_contents_->render_view_host()->InstallMissingPlugin();
  return true;
}

string16 PluginInstaller::GetLinkText() {
  return l10n_util::GetStringUTF16(IDS_PLUGININSTALLER_PROBLEMSINSTALLING);
}

bool PluginInstaller::LinkClicked(WindowOpenDisposition disposition) {
  // Ignore the click dispostion and always open in a new top level tab.
  tab_contents_->OpenURL(
      GURL(l10n_util::GetStringUTF8(IDS_LEARN_MORE_PLUGININSTALLER_URL)),
      GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  return false;  // Do not dismiss the info bar.
}
