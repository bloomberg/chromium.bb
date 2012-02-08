// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_installer_infobar_delegate.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/plugin_installer.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::OpenURLParams;
using content::Referrer;

PluginInstallerInfoBarDelegate::PluginInstallerInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    PluginInstaller* installer,
    const base::Closure& callback,
    const string16& message)
    : ConfirmInfoBarDelegate(infobar_helper),
      WeakPluginInstallerObserver(installer),
      callback_(callback),
      message_(message) {
}

PluginInstallerInfoBarDelegate::~PluginInstallerInfoBarDelegate() {
}

InfoBarDelegate* PluginInstallerInfoBarDelegate::Create(
    InfoBarTabHelper* infobar_helper,
    PluginInstaller* installer,
    const base::Closure& callback) {
  string16 message;
  switch (installer->state()) {
    case PluginInstaller::kStateIdle:
      message = l10n_util::GetStringFUTF16(
          IDS_PLUGININSTALLER_INSTALLPLUGIN_PROMPT, installer->name());
      break;
    case PluginInstaller::kStateDownloading:
      message = l10n_util::GetStringUTF16(IDS_PLUGIN_DOWNLOADING);
      break;
  }
  return new PluginInstallerInfoBarDelegate(
      infobar_helper, installer, callback, message);
}

gfx::Image* PluginInstallerInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_PLUGIN_INSTALL);
}

string16 PluginInstallerInfoBarDelegate::GetMessageText() const {
  return message_;
}

int PluginInstallerInfoBarDelegate::GetButtons() const {
  return callback_.is_null() ? BUTTON_NONE : BUTTON_OK;
}

string16 PluginInstallerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_PLUGININSTALLER_INSTALLPLUGIN_BUTTON);
}

bool PluginInstallerInfoBarDelegate::Accept() {
  callback_.Run();
  return false;
}

string16 PluginInstallerInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGININSTALLER_PROBLEMSINSTALLING);
}

bool PluginInstallerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  GURL url(installer()->help_url());
  if (url.is_empty()) {
    url = google_util::AppendGoogleLocaleParam(GURL(
      "https://www.google.com/support/chrome/bin/answer.py?answer=142064"));
  }

  OpenURLParams params(
      url, Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK, false);
  owner()->web_contents()->OpenURL(params);
  return false;
}

void PluginInstallerInfoBarDelegate::DidStartDownload() {
  ReplaceWithInfoBar(l10n_util::GetStringUTF16(IDS_PLUGIN_DOWNLOADING));
}

void PluginInstallerInfoBarDelegate::DidFinishDownload() {
  ReplaceWithInfoBar(l10n_util::GetStringUTF16(IDS_PLUGIN_INSTALLING));
}

void PluginInstallerInfoBarDelegate::DownloadError(const std::string& message) {
  ReplaceWithInfoBar(
      l10n_util::GetStringUTF16(IDS_PLUGIN_DOWNLOAD_ERROR_SHORT));
}

void PluginInstallerInfoBarDelegate::OnlyWeakObserversLeft() {
  if (owner())
    owner()->RemoveInfoBar(this);
}

void PluginInstallerInfoBarDelegate::ReplaceWithInfoBar(
    const string16& message) {
  // Return early if the message doesn't change. This is important in case the
  // PluginInstaller is still iterating over its observers (otherwise we would
  // keep replacing infobar delegates infinitely).
  if (message_ == message)
    return;
  if (!owner())
    return;
  InfoBarDelegate* delegate = new PluginInstallerInfoBarDelegate(
      owner(), installer(), base::Closure(), message);
  owner()->ReplaceInfoBar(this, delegate);
}
