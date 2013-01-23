// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_infobar_delegates.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_WIN)
#include <shellapi.h>
#include "ui/base/win/shell.h"
#endif

#if defined(ENABLE_PLUGIN_INSTALLATION)
#if defined(OS_WIN)
#include "base/win/metro.h"
#endif
#include "chrome/browser/plugins/plugin_installer.h"
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

using content::OpenURLParams;
using content::Referrer;
using content::UserMetricsAction;

PluginInfoBarDelegate::PluginInfoBarDelegate(InfoBarService* infobar_service,
                                             const string16& name,
                                             const std::string& identifier)
    : ConfirmInfoBarDelegate(infobar_service),
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
  owner()->GetWebContents()->OpenURL(params);
  return false;
}

void PluginInfoBarDelegate::LoadBlockedPlugins() {
  content::WebContents* web_contents = owner()->GetWebContents();
  if (web_contents) {
    web_contents->Send(new ChromeViewMsg_LoadBlockedPlugins(
        web_contents->GetRoutingID(), identifier_));
  }
}

gfx::Image* PluginInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_PLUGIN_INSTALL);
}

string16 PluginInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

// UnauthorizedPluginInfoBarDelegate ------------------------------------------

// static
void UnauthorizedPluginInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    HostContentSettingsMap* content_settings,
    const string16& utf16_name,
    const std::string& identifier) {
  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new UnauthorizedPluginInfoBarDelegate(infobar_service, content_settings,
                                            utf16_name, identifier)));

  content::RecordAction(UserMetricsAction("BlockedPluginInfobar.Shown"));
  std::string name(UTF16ToUTF8(utf16_name));
  if (name == PluginMetadata::kJavaGroupName)
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.Java"));
  else if (name == PluginMetadata::kQuickTimeGroupName)
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.QuickTime"));
  else if (name == PluginMetadata::kShockwaveGroupName)
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.Shockwave"));
  else if (name == PluginMetadata::kRealPlayerGroupName)
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.RealPlayer"));
  else if (name == PluginMetadata::kWindowsMediaPlayerGroupName)
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.WindowsMediaPlayer"));
}

UnauthorizedPluginInfoBarDelegate::UnauthorizedPluginInfoBarDelegate(
    InfoBarService* infobar_service,
    HostContentSettingsMap* content_settings,
    const string16& utf16_name,
    const std::string& identifier)
    : PluginInfoBarDelegate(infobar_service, utf16_name, identifier),
      content_settings_(content_settings) {
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
  content_settings_->AddExceptionForURL(owner()->GetWebContents()->GetURL(),
                                        owner()->GetWebContents()->GetURL(),
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

void OutdatedPluginInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PluginInstaller* installer,
    scoped_ptr<PluginMetadata> plugin_metadata) {
  string16 message;
  switch (installer->state()) {
    case PluginInstaller::INSTALLER_STATE_IDLE:
      message = l10n_util::GetStringFUTF16(IDS_PLUGIN_OUTDATED_PROMPT,
                                           plugin_metadata->name());
      break;
    case PluginInstaller::INSTALLER_STATE_DOWNLOADING:
      message = l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOADING,
                                           plugin_metadata->name());
      break;
  }
  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new OutdatedPluginInfoBarDelegate(
          infobar_service, installer, plugin_metadata.Pass(), message)));
}

OutdatedPluginInfoBarDelegate::OutdatedPluginInfoBarDelegate(
    InfoBarService* infobar_service,
    PluginInstaller* installer,
    scoped_ptr<PluginMetadata> plugin_metadata,
    const string16& message)
    : PluginInfoBarDelegate(
          infobar_service,
          plugin_metadata->name(),
          plugin_metadata->identifier()),
      WeakPluginInstallerObserver(installer),
      plugin_metadata_(plugin_metadata.Pass()),
      message_(message) {
  content::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Shown"));
  std::string name = UTF16ToUTF8(plugin_metadata_->name());
  if (name == PluginMetadata::kJavaGroupName)
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Java"));
  else if (name == PluginMetadata::kQuickTimeGroupName)
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.QuickTime"));
  else if (name == PluginMetadata::kShockwaveGroupName)
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Shockwave"));
  else if (name == PluginMetadata::kRealPlayerGroupName)
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.RealPlayer"));
  else if (name == PluginMetadata::kSilverlightGroupName)
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Silverlight"));
  else if (name == PluginMetadata::kAdobeReaderGroupName)
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
  if (installer()->state() != PluginInstaller::INSTALLER_STATE_IDLE) {
    NOTREACHED();
    return false;
  }

  content::WebContents* web_contents = owner()->GetWebContents();
  // A call to any of |OpenDownloadURL()| or |StartInstalling()| will
  // result in deleting ourselves. Accordingly, we make sure to
  // not pass a reference to an object that can go away.
  // http://crbug.com/54167
  GURL plugin_url(plugin_metadata_->plugin_url());
  if (plugin_metadata_->url_for_display()) {
    installer()->OpenDownloadURL(plugin_url, web_contents);
  } else {
    installer()->StartInstalling(plugin_url, web_contents);
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
                                                plugin_metadata_->name()));
}

void OutdatedPluginInfoBarDelegate::DownloadError(const std::string& message) {
  ReplaceWithInfoBar(
      l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_ERROR_SHORT,
                                 plugin_metadata_->name()));
}

void OutdatedPluginInfoBarDelegate::DownloadCancelled() {
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_CANCELLED,
                                                plugin_metadata_->name()));
}

void OutdatedPluginInfoBarDelegate::DownloadFinished() {
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(IDS_PLUGIN_UPDATING,
                                                plugin_metadata_->name()));
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
  PluginInstallerInfoBarDelegate::Replace(
      this, installer(), plugin_metadata_->Clone(), false, message);
}

// PluginInstallerInfoBarDelegate ---------------------------------------------

void PluginInstallerInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PluginInstaller* installer,
    scoped_ptr<PluginMetadata> plugin_metadata,
    const InstallCallback& callback) {
  string16 name(plugin_metadata->name());
#if defined(OS_WIN)
  if (base::win::IsMetroProcess()) {
    PluginMetroModeInfoBarDelegate::Create(
        infobar_service, PluginMetroModeInfoBarDelegate::MISSING_PLUGIN, name);
    return;
  }
#endif
  string16 message;
  switch (installer->state()) {
    case PluginInstaller::INSTALLER_STATE_IDLE:
      message = l10n_util::GetStringFUTF16(
          IDS_PLUGININSTALLER_INSTALLPLUGIN_PROMPT, name);
      break;
    case PluginInstaller::INSTALLER_STATE_DOWNLOADING:
      message = l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOADING, name);
      break;
  }
  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new PluginInstallerInfoBarDelegate(
          infobar_service, installer, plugin_metadata.Pass(), callback, true,
          message)));
}

void PluginInstallerInfoBarDelegate::Replace(
    InfoBarDelegate* infobar,
    PluginInstaller* installer,
    scoped_ptr<PluginMetadata> plugin_metadata,
    bool new_install,
    const string16& message) {
  DCHECK(infobar->owner());
  infobar->owner()->ReplaceInfoBar(infobar, scoped_ptr<InfoBarDelegate>(
      new PluginInstallerInfoBarDelegate(
          infobar->owner(), installer, plugin_metadata.Pass(),
          PluginInstallerInfoBarDelegate::InstallCallback(), new_install,
          message)));
}

PluginInstallerInfoBarDelegate::PluginInstallerInfoBarDelegate(
    InfoBarService* infobar_service,
    PluginInstaller* installer,
    scoped_ptr<PluginMetadata> plugin_metadata,
    const InstallCallback& callback,
    bool new_install,
    const string16& message)
    : ConfirmInfoBarDelegate(infobar_service),
      WeakPluginInstallerObserver(installer),
      plugin_metadata_(plugin_metadata.Pass()),
      callback_(callback),
      new_install_(new_install),
      message_(message) {
}

PluginInstallerInfoBarDelegate::~PluginInstallerInfoBarDelegate() {
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
  callback_.Run(plugin_metadata_.get());
  return false;
}

string16 PluginInstallerInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(
      new_install_ ? IDS_PLUGININSTALLER_PROBLEMSINSTALLING
                   : IDS_PLUGININSTALLER_PROBLEMSUPDATING);
}

bool PluginInstallerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  GURL url(plugin_metadata_->help_url());
  if (url.is_empty()) {
    url = google_util::AppendGoogleLocaleParam(GURL(
      "https://www.google.com/support/chrome/bin/answer.py?answer=142064"));
  }

  OpenURLParams params(
      url, Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK, false);
  owner()->GetWebContents()->OpenURL(params);
  return false;
}

void PluginInstallerInfoBarDelegate::DownloadStarted() {
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOADING,
                                                plugin_metadata_->name()));
}

void PluginInstallerInfoBarDelegate::DownloadCancelled() {
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_CANCELLED,
                                                plugin_metadata_->name()));
}

void PluginInstallerInfoBarDelegate::DownloadError(const std::string& message) {
  ReplaceWithInfoBar(
      l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_ERROR_SHORT,
                                 plugin_metadata_->name()));
}

void PluginInstallerInfoBarDelegate::DownloadFinished() {
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(
      new_install_ ? IDS_PLUGIN_INSTALLING : IDS_PLUGIN_UPDATING,
          plugin_metadata_->name()));
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
  Replace(this, installer(), plugin_metadata_->Clone(), new_install_, message);
}

// PluginMetroModeInfoBarDelegate ---------------------------------------------
#if defined(OS_WIN)
// static
void PluginMetroModeInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PluginMetroModeInfoBarDelegate::Mode mode,
    const string16& name) {
  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new PluginMetroModeInfoBarDelegate(infobar_service, mode, name)));
}

PluginMetroModeInfoBarDelegate::PluginMetroModeInfoBarDelegate(
    InfoBarService* infobar_service,
    PluginMetroModeInfoBarDelegate::Mode mode,
    const string16& name)
    : ConfirmInfoBarDelegate(infobar_service),
      mode_(mode),
      name_(name) {
}

PluginMetroModeInfoBarDelegate::~PluginMetroModeInfoBarDelegate() {
}

gfx::Image* PluginMetroModeInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_PLUGIN_INSTALL);
}

string16 PluginMetroModeInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16((mode_ == MISSING_PLUGIN) ?
      IDS_METRO_MISSING_PLUGIN_PROMPT : IDS_METRO_NPAPI_PLUGIN_PROMPT, name_);
}

int PluginMetroModeInfoBarDelegate::GetButtons() const {
  return (mode_ == MISSING_PLUGIN) ? BUTTON_OK : (BUTTON_OK | BUTTON_CANCEL);
}

string16 PluginMetroModeInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_CANCEL)
    return l10n_util::GetStringUTF16(IDS_DONT_ASK_AGAIN_INFOBAR_BUTTON_LABEL);
  return l10n_util::GetStringUTF16((mode_ == MISSING_PLUGIN) ?
      IDS_WIN8_DESKTOP_RESTART : IDS_WIN8_RESTART);
}

bool PluginMetroModeInfoBarDelegate::Accept() {
  browser::AttemptRestartWithModeSwitch();
  return true;
}

bool PluginMetroModeInfoBarDelegate::Cancel() {
  DCHECK_EQ(DESKTOP_MODE_REQUIRED, mode_);
  content::WebContents* web_contents = owner()->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* content_settings =
      profile->GetHostContentSettingsMap();
  GURL url = web_contents->GetURL();
  content_settings->SetContentSetting(
      ContentSettingsPattern::FromURL(url),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_METRO_SWITCH_TO_DESKTOP,
      std::string(),
      CONTENT_SETTING_BLOCK);
  return true;
}

string16 PluginMetroModeInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool PluginMetroModeInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  OpenURLParams params(
      GURL((mode_ == MISSING_PLUGIN) ?
          "https://support.google.com/chrome/?p=ib_display_in_desktop" :
          "https://support.google.com/chrome/?p=ib_redirect_to_desktop"),
      Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK, false);
  owner()->GetWebContents()->OpenURL(params);
  return false;
}
#endif  // defined(OS_WIN)
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)
