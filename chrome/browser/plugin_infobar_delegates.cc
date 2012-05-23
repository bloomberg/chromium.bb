// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_infobar_delegates.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/plugin_observer.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/plugin_group.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugin_installer.h"
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

using content::OpenURLParams;
using content::Referrer;
using content::UserMetricsAction;

PluginInfoBarDelegate::PluginInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                             const string16& name,
                                             const std::string& identifier)
    : ConfirmInfoBarDelegate(infobar_helper),
      name_(name),
      identifier_(identifier) {
}

PluginInfoBarDelegate::~PluginInfoBarDelegate() {
}

bool PluginInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  OpenURLParams params(
      GURL(GetLearnMoreURL()), Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK,
      false);
  owner()->web_contents()->OpenURL(params);
  return false;
}

void PluginInfoBarDelegate::LoadBlockedPlugins() {
  owner()->Send(
      new ChromeViewMsg_LoadBlockedPlugins(owner()->routing_id(), identifier_));
}

gfx::Image* PluginInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_PLUGIN_INSTALL);
}

string16 PluginInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

// UnauthorizedPluginInfoBarDelegate ------------------------------------------

UnauthorizedPluginInfoBarDelegate::UnauthorizedPluginInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    HostContentSettingsMap* content_settings,
    const string16& utf16_name,
    const std::string& identifier)
    : PluginInfoBarDelegate(infobar_helper, utf16_name, identifier),
      content_settings_(content_settings) {
  content::RecordAction(UserMetricsAction("BlockedPluginInfobar.Shown"));
  std::string name = UTF16ToUTF8(utf16_name);
  if (name == webkit::npapi::PluginGroup::kJavaGroupName)
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.Java"));
  else if (name == webkit::npapi::PluginGroup::kQuickTimeGroupName)
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.QuickTime"));
  else if (name == webkit::npapi::PluginGroup::kShockwaveGroupName)
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.Shockwave"));
  else if (name == webkit::npapi::PluginGroup::kRealPlayerGroupName)
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.RealPlayer"));
  else if (name == webkit::npapi::PluginGroup::kWindowsMediaPlayerGroupName)
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.WindowsMediaPlayer"));
}

UnauthorizedPluginInfoBarDelegate::~UnauthorizedPluginInfoBarDelegate() {
  content::RecordAction(UserMetricsAction("BlockedPluginInfobar.Closed"));
}

std::string UnauthorizedPluginInfoBarDelegate::GetLearnMoreURL() const {
  return chrome::kBlockedPluginLearnMoreURL;
}

string16 UnauthorizedPluginInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_PLUGIN_NOT_AUTHORIZED, name_);
}

string16 UnauthorizedPluginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PLUGIN_ENABLE_TEMPORARILY : IDS_PLUGIN_ENABLE_ALWAYS);
}

bool UnauthorizedPluginInfoBarDelegate::Accept() {
  content::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.AllowThisTime"));
  LoadBlockedPlugins();
  return true;
}

bool UnauthorizedPluginInfoBarDelegate::Cancel() {
  content::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.AlwaysAllow"));
  content_settings_->AddExceptionForURL(owner()->web_contents()->GetURL(),
                                        owner()->web_contents()->GetURL(),
                                        CONTENT_SETTINGS_TYPE_PLUGINS,
                                        std::string(),
                                        CONTENT_SETTING_ALLOW);
  LoadBlockedPlugins();
  return true;
}

void UnauthorizedPluginInfoBarDelegate::InfoBarDismissed() {
  content::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.Dismissed"));
}

bool UnauthorizedPluginInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  content::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.LearnMore"));
  return PluginInfoBarDelegate::LinkClicked(disposition);
}

#if defined(ENABLE_PLUGIN_INSTALLATION)
// OutdatedPluginInfoBarDelegate ----------------------------------------------

InfoBarDelegate* OutdatedPluginInfoBarDelegate::Create(
    PluginObserver* observer,
    PluginInstaller* installer) {
  string16 message;
  switch (installer->state()) {
    case PluginInstaller::kStateIdle:
      message = l10n_util::GetStringFUTF16(IDS_PLUGIN_OUTDATED_PROMPT,
                                           installer->name());
      break;
    case PluginInstaller::kStateDownloading:
      message = l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOADING,
                                           installer->name());
      break;
  }
  return new OutdatedPluginInfoBarDelegate(
      observer, installer, message);
}

OutdatedPluginInfoBarDelegate::OutdatedPluginInfoBarDelegate(
    PluginObserver* observer,
    PluginInstaller* installer,
    const string16& message)
    : PluginInfoBarDelegate(
        observer->tab_contents_wrapper()->infobar_tab_helper(),
        installer->name(),
        installer->identifier()),
      WeakPluginInstallerObserver(installer),
      observer_(observer),
      message_(message) {
  content::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Shown"));
  std::string name = UTF16ToUTF8(installer->name());
  if (name == webkit::npapi::PluginGroup::kJavaGroupName)
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Java"));
  else if (name == webkit::npapi::PluginGroup::kQuickTimeGroupName)
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.QuickTime"));
  else if (name == webkit::npapi::PluginGroup::kShockwaveGroupName)
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Shockwave"));
  else if (name == webkit::npapi::PluginGroup::kRealPlayerGroupName)
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.RealPlayer"));
  else if (name == webkit::npapi::PluginGroup::kSilverlightGroupName)
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Silverlight"));
  else if (name == webkit::npapi::PluginGroup::kAdobeReaderGroupName)
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Reader"));
}

OutdatedPluginInfoBarDelegate::~OutdatedPluginInfoBarDelegate() {
  content::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Closed"));
}

std::string OutdatedPluginInfoBarDelegate::GetLearnMoreURL() const {
  return chrome::kOutdatedPluginLearnMoreURL;
}

string16 OutdatedPluginInfoBarDelegate::GetMessageText() const {
  return message_;
}

string16 OutdatedPluginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PLUGIN_UPDATE : IDS_PLUGIN_ENABLE_TEMPORARILY);
}

bool OutdatedPluginInfoBarDelegate::Accept() {
  content::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Update"));
  if (installer()->state() != PluginInstaller::kStateIdle) {
    NOTREACHED();
    return false;
  }

  content::WebContents* web_contents = owner()->web_contents();
  if (installer()->url_for_display()) {
    installer()->OpenDownloadURL(web_contents);
  } else {
    installer()->StartInstalling(observer_->tab_contents_wrapper());
  }
  return false;
}

bool OutdatedPluginInfoBarDelegate::Cancel() {
  content::RecordAction(
      UserMetricsAction("OutdatedPluginInfobar.AllowThisTime"));
  LoadBlockedPlugins();
  return true;
}

void OutdatedPluginInfoBarDelegate::InfoBarDismissed() {
  content::RecordAction(
      UserMetricsAction("OutdatedPluginInfobar.Dismissed"));
}

bool OutdatedPluginInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  content::RecordAction(
      UserMetricsAction("OutdatedPluginInfobar.LearnMore"));
  return PluginInfoBarDelegate::LinkClicked(disposition);
}

void OutdatedPluginInfoBarDelegate::DownloadStarted() {
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOADING,
                                                installer()->name()));
}

void OutdatedPluginInfoBarDelegate::DownloadError(const std::string& message) {
  ReplaceWithInfoBar(
      l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_ERROR_SHORT,
                                 installer()->name()));
}

void OutdatedPluginInfoBarDelegate::DownloadCancelled() {
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_CANCELLED,
                                                installer()->name()));
}

void OutdatedPluginInfoBarDelegate::DownloadFinished() {
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(IDS_PLUGIN_UPDATING,
                                                installer()->name()));
}

void OutdatedPluginInfoBarDelegate::OnlyWeakObserversLeft() {
  if (owner())
    owner()->RemoveInfoBar(this);
}

void OutdatedPluginInfoBarDelegate::ReplaceWithInfoBar(
    const string16& message) {
  // Return early if the message doesn't change. This is important in case the
  // PluginInstaller is still iterating over its observers (otherwise we would
  // keep replacing infobar delegates infinitely).
  if (message_ == message)
    return;
  if (!owner())
    return;
  InfoBarDelegate* delegate = new PluginInstallerInfoBarDelegate(
      owner(), installer(), base::Closure(), false, message);
  owner()->ReplaceInfoBar(this, delegate);
}

// PluginInstallerInfoBarDelegate ---------------------------------------------

PluginInstallerInfoBarDelegate::PluginInstallerInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    PluginInstaller* installer,
    const base::Closure& callback,
    bool new_install,
    const string16& message)
    : ConfirmInfoBarDelegate(infobar_helper),
      WeakPluginInstallerObserver(installer),
      callback_(callback),
      new_install_(new_install),
      message_(message) {
}

PluginInstallerInfoBarDelegate::~PluginInstallerInfoBarDelegate() {
}

InfoBarDelegate* PluginInstallerInfoBarDelegate::Create(
    InfoBarTabHelper* infobar_helper,
    PluginInstaller* installer,
    const base::Closure& callback) {
  string16 message;
  const string16& plugin_name = installer->name();
  switch (installer->state()) {
    case PluginInstaller::kStateIdle:
      message = l10n_util::GetStringFUTF16(
          IDS_PLUGININSTALLER_INSTALLPLUGIN_PROMPT, plugin_name);
      break;
    case PluginInstaller::kStateDownloading:
      message = l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOADING, plugin_name);
      break;
  }
  return new PluginInstallerInfoBarDelegate(
      infobar_helper, installer, callback, true, message);
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
  return l10n_util::GetStringUTF16(
      new_install_ ? IDS_PLUGININSTALLER_PROBLEMSINSTALLING
                   : IDS_PLUGININSTALLER_PROBLEMSUPDATING);
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

void PluginInstallerInfoBarDelegate::DownloadStarted() {
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOADING,
                                                installer()->name()));
}

void PluginInstallerInfoBarDelegate::DownloadCancelled() {
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_CANCELLED,
                                                installer()->name()));
}

void PluginInstallerInfoBarDelegate::DownloadError(const std::string& message) {
  ReplaceWithInfoBar(
      l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_ERROR_SHORT,
                                 installer()->name()));
}

void PluginInstallerInfoBarDelegate::DownloadFinished() {
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(
      new_install_ ? IDS_PLUGIN_INSTALLING : IDS_PLUGIN_UPDATING,
      installer()->name()));
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
      owner(), installer(), base::Closure(), new_install_, message);
  owner()->ReplaceInfoBar(this, delegate);
}
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)
