// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_installer_infobar_delegate.h"

#include "chrome/browser/google/google_util.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/view_messages.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

PluginInstallerInfoBarDelegate::PluginInstallerInfoBarDelegate(
    TabContents* tab_contents)
    : ConfirmInfoBarDelegate(tab_contents),
      tab_contents_(tab_contents) {
}

PluginInstallerInfoBarDelegate::~PluginInstallerInfoBarDelegate() {
}

gfx::Image* PluginInstallerInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_PLUGIN_INSTALL);
}

PluginInstallerInfoBarDelegate*
    PluginInstallerInfoBarDelegate::AsPluginInstallerInfoBarDelegate() {
  return this;
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
  RenderViewHost* host = tab_contents_->render_view_host();
  host->Send(new ViewMsg_InstallMissingPlugin(host->routing_id()));
  return true;
}

string16 PluginInstallerInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGININSTALLER_PROBLEMSINSTALLING);
}

bool PluginInstallerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  tab_contents_->OpenURL(google_util::AppendGoogleLocaleParam(GURL(
      "http://www.google.com/support/chrome/bin/answer.py?answer=95697&topic="
      "14687")), GURL(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      PageTransition::LINK);
  return false;
}
