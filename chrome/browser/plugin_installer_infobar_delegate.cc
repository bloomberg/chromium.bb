// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_installer_infobar_delegate.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "webkit/plugins/npapi/default_plugin_shared.h"

PluginInstallerInfoBarDelegate::PluginInstallerInfoBarDelegate(
    TabContents* tab_contents)
    : ConfirmInfoBarDelegate(tab_contents),
      tab_contents_(tab_contents) {
}

PluginInstallerInfoBarDelegate::~PluginInstallerInfoBarDelegate() {
  // Remove any InfoBars we may be showing.
  tab_contents_->RemoveInfoBar(this);
}

void PluginInstallerInfoBarDelegate::OnMissingPluginStatus(int status) {
  switch (status) {
    case webkit::npapi::default_plugin::MISSING_PLUGIN_AVAILABLE: {
      tab_contents_->AddInfoBar(this);
      break;
    }
    case webkit::npapi::default_plugin::MISSING_PLUGIN_USER_STARTED_DOWNLOAD: {
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

SkBitmap* PluginInstallerInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_PLUGIN_INSTALL);
}

string16 PluginInstallerInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGININSTALLER_MISSINGPLUGIN_PROMPT);
}

int PluginInstallerInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

string16 PluginInstallerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_PLUGININSTALLER_INSTALLPLUGIN_BUTTON);
}

bool PluginInstallerInfoBarDelegate::Accept() {
  tab_contents_->render_view_host()->InstallMissingPlugin();
  return true;
}

string16 PluginInstallerInfoBarDelegate::GetLinkText() {
  return l10n_util::GetStringUTF16(IDS_PLUGININSTALLER_PROBLEMSINSTALLING);
}

bool PluginInstallerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  // Ignore the click dispostion and always open in a new top level tab.
  static const char kLearnMorePluginInstallerUrl[] = "http://www.google.com/"
      "support/chrome/bin/answer.py?answer=95697&amp;topic=14687";
  tab_contents_->OpenURL(GURL(kLearnMorePluginInstallerUrl), GURL(),
                         NEW_FOREGROUND_TAB, PageTransition::LINK);
  return false;
}
