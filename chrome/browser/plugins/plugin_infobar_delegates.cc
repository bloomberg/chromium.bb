// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_infobar_delegates.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/google/core/browser/google_util.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#if defined(OS_WIN)
#include "base/win/metro.h"
#endif
#include "chrome/browser/plugins/plugin_installer.h"
#endif

#if defined(OS_WIN)
#include <shellapi.h>
#include "ui/base/win/shell.h"

#if defined(USE_AURA)
#include "ui/aura/remote_window_tree_host_win.h"
#endif
#endif

using base::UserMetricsAction;


// PluginInfoBarDelegate ------------------------------------------------------

PluginInfoBarDelegate::PluginInfoBarDelegate(const std::string& identifier)
    : ConfirmInfoBarDelegate(),
      identifier_(identifier) {
}

PluginInfoBarDelegate::~PluginInfoBarDelegate() {
}

bool PluginInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  InfoBarService::WebContentsFromInfoBar(infobar())->OpenURL(
      content::OpenURLParams(
          GURL(GetLearnMoreURL()), content::Referrer(),
          (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
          ui::PAGE_TRANSITION_LINK, false));
  return false;
}

void PluginInfoBarDelegate::LoadBlockedPlugins() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
      web_contents, true, identifier_);
}

int PluginInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_PLUGIN_INSTALL;
}

base::string16 PluginInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}


// UnauthorizedPluginInfoBarDelegate ------------------------------------------

// static
void UnauthorizedPluginInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    HostContentSettingsMap* content_settings,
    const base::string16& name,
    const std::string& identifier) {
  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new UnauthorizedPluginInfoBarDelegate(
          content_settings, name, identifier))));

  content::RecordAction(UserMetricsAction("BlockedPluginInfobar.Shown"));
  std::string utf8_name(base::UTF16ToUTF8(name));
  if (utf8_name == PluginMetadata::kJavaGroupName) {
    content::RecordAction(UserMetricsAction("BlockedPluginInfobar.Shown.Java"));
  } else if (utf8_name == PluginMetadata::kQuickTimeGroupName) {
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.QuickTime"));
  } else if (utf8_name == PluginMetadata::kShockwaveGroupName) {
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.Shockwave"));
  } else if (utf8_name == PluginMetadata::kRealPlayerGroupName) {
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.RealPlayer"));
  } else if (utf8_name == PluginMetadata::kWindowsMediaPlayerGroupName) {
    content::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.WindowsMediaPlayer"));
  }
}

UnauthorizedPluginInfoBarDelegate::UnauthorizedPluginInfoBarDelegate(
    HostContentSettingsMap* content_settings,
    const base::string16& name,
    const std::string& identifier)
    : PluginInfoBarDelegate(identifier),
      content_settings_(content_settings),
      name_(name) {
}

UnauthorizedPluginInfoBarDelegate::~UnauthorizedPluginInfoBarDelegate() {
  content::RecordAction(UserMetricsAction("BlockedPluginInfobar.Closed"));
}

std::string UnauthorizedPluginInfoBarDelegate::GetLearnMoreURL() const {
  return chrome::kBlockedPluginLearnMoreURL;
}

base::string16 UnauthorizedPluginInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_PLUGIN_NOT_AUTHORIZED, name_);
}

base::string16 UnauthorizedPluginInfoBarDelegate::GetButtonLabel(
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
  content::RecordAction(UserMetricsAction("BlockedPluginInfobar.AlwaysAllow"));
  const GURL& url = InfoBarService::WebContentsFromInfoBar(infobar())->GetURL();
  content_settings_->AddExceptionForURL(url, url, CONTENT_SETTINGS_TYPE_PLUGINS,
                                        CONTENT_SETTING_ALLOW);
  LoadBlockedPlugins();
  return true;
}

void UnauthorizedPluginInfoBarDelegate::InfoBarDismissed() {
  content::RecordAction(UserMetricsAction("BlockedPluginInfobar.Dismissed"));
}

bool UnauthorizedPluginInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("BlockedPluginInfobar.LearnMore"));
  return PluginInfoBarDelegate::LinkClicked(disposition);
}


#if defined(ENABLE_PLUGIN_INSTALLATION)

// OutdatedPluginInfoBarDelegate ----------------------------------------------

void OutdatedPluginInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PluginInstaller* installer,
    scoped_ptr<PluginMetadata> plugin_metadata) {
  // Copy the name out of |plugin_metadata| now, since the Pass() call below
  // will make it impossible to get at.
  base::string16 name(plugin_metadata->name());
  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new OutdatedPluginInfoBarDelegate(
          installer, plugin_metadata.Pass(), l10n_util::GetStringFUTF16(
              (installer->state() == PluginInstaller::INSTALLER_STATE_IDLE) ?
                  IDS_PLUGIN_OUTDATED_PROMPT : IDS_PLUGIN_DOWNLOADING,
              name)))));
}

OutdatedPluginInfoBarDelegate::OutdatedPluginInfoBarDelegate(
    PluginInstaller* installer,
    scoped_ptr<PluginMetadata> plugin_metadata,
    const base::string16& message)
    : PluginInfoBarDelegate(plugin_metadata->identifier()),
      WeakPluginInstallerObserver(installer),
      plugin_metadata_(plugin_metadata.Pass()),
      message_(message) {
  content::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Shown"));
  std::string name = base::UTF16ToUTF8(plugin_metadata_->name());
  if (name == PluginMetadata::kJavaGroupName) {
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Java"));
  } else if (name == PluginMetadata::kQuickTimeGroupName) {
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.QuickTime"));
  } else if (name == PluginMetadata::kShockwaveGroupName) {
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Shockwave"));
  } else if (name == PluginMetadata::kRealPlayerGroupName) {
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.RealPlayer"));
  } else if (name == PluginMetadata::kSilverlightGroupName) {
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Silverlight"));
  } else if (name == PluginMetadata::kAdobeReaderGroupName) {
    content::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Reader"));
  }
}

OutdatedPluginInfoBarDelegate::~OutdatedPluginInfoBarDelegate() {
  content::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Closed"));
}

std::string OutdatedPluginInfoBarDelegate::GetLearnMoreURL() const {
  return chrome::kOutdatedPluginLearnMoreURL;
}

base::string16 OutdatedPluginInfoBarDelegate::GetMessageText() const {
  return message_;
}

base::string16 OutdatedPluginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PLUGIN_UPDATE : IDS_PLUGIN_ENABLE_TEMPORARILY);
}

bool OutdatedPluginInfoBarDelegate::Accept() {
  DCHECK_EQ(PluginInstaller::INSTALLER_STATE_IDLE, installer()->state());
  content::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Update"));
  // A call to any of |OpenDownloadURL()| or |StartInstalling()| will
  // result in deleting ourselves. Accordingly, we make sure to
  // not pass a reference to an object that can go away.
  GURL plugin_url(plugin_metadata_->plugin_url());
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  if (plugin_metadata_->url_for_display())
    installer()->OpenDownloadURL(plugin_url, web_contents);
  else
    installer()->StartInstalling(plugin_url, web_contents);
  return false;
}

bool OutdatedPluginInfoBarDelegate::Cancel() {
  content::RecordAction(
      UserMetricsAction("OutdatedPluginInfobar.AllowThisTime"));
  LoadBlockedPlugins();
  return true;
}

void OutdatedPluginInfoBarDelegate::InfoBarDismissed() {
  content::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Dismissed"));
}

bool OutdatedPluginInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("OutdatedPluginInfobar.LearnMore"));
  return PluginInfoBarDelegate::LinkClicked(disposition);
}

void OutdatedPluginInfoBarDelegate::DownloadStarted() {
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOADING,
                                                plugin_metadata_->name()));
}

void OutdatedPluginInfoBarDelegate::DownloadError(const std::string& message) {
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_ERROR_SHORT,
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
  infobar()->RemoveSelf();
}

void OutdatedPluginInfoBarDelegate::ReplaceWithInfoBar(
    const base::string16& message) {
  // Return early if the message doesn't change. This is important in case the
  // PluginInstaller is still iterating over its observers (otherwise we would
  // keep replacing infobar delegates infinitely).
  if ((message_ == message) || !infobar()->owner())
    return;
  PluginInstallerInfoBarDelegate::Replace(
      infobar(), installer(), plugin_metadata_->Clone(), false, message);
}


// PluginInstallerInfoBarDelegate ---------------------------------------------

void PluginInstallerInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PluginInstaller* installer,
    scoped_ptr<PluginMetadata> plugin_metadata,
    const InstallCallback& callback) {
  base::string16 name(plugin_metadata->name());
#if defined(OS_WIN)
  if (base::win::IsMetroProcess()) {
    PluginMetroModeInfoBarDelegate::Create(
        infobar_service, PluginMetroModeInfoBarDelegate::MISSING_PLUGIN, name);
    return;
  }
#endif
  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new PluginInstallerInfoBarDelegate(
          installer, plugin_metadata.Pass(), callback, true,
          l10n_util::GetStringFUTF16(
              (installer->state() == PluginInstaller::INSTALLER_STATE_IDLE) ?
                  IDS_PLUGININSTALLER_INSTALLPLUGIN_PROMPT :
                  IDS_PLUGIN_DOWNLOADING,
              name)))));
}

void PluginInstallerInfoBarDelegate::Replace(
    infobars::InfoBar* infobar,
    PluginInstaller* installer,
    scoped_ptr<PluginMetadata> plugin_metadata,
    bool new_install,
    const base::string16& message) {
  DCHECK(infobar->owner());
  infobar->owner()->ReplaceInfoBar(infobar,
      ConfirmInfoBarDelegate::CreateInfoBar(scoped_ptr<ConfirmInfoBarDelegate>(
          new PluginInstallerInfoBarDelegate(
              installer, plugin_metadata.Pass(),
              PluginInstallerInfoBarDelegate::InstallCallback(), new_install,
              message))));
}

PluginInstallerInfoBarDelegate::PluginInstallerInfoBarDelegate(
    PluginInstaller* installer,
    scoped_ptr<PluginMetadata> plugin_metadata,
    const InstallCallback& callback,
    bool new_install,
    const base::string16& message)
    : ConfirmInfoBarDelegate(),
      WeakPluginInstallerObserver(installer),
      plugin_metadata_(plugin_metadata.Pass()),
      callback_(callback),
      new_install_(new_install),
      message_(message) {
}

PluginInstallerInfoBarDelegate::~PluginInstallerInfoBarDelegate() {
}

int PluginInstallerInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_PLUGIN_INSTALL;
}

base::string16 PluginInstallerInfoBarDelegate::GetMessageText() const {
  return message_;
}

int PluginInstallerInfoBarDelegate::GetButtons() const {
  return callback_.is_null() ? BUTTON_NONE : BUTTON_OK;
}

base::string16 PluginInstallerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_PLUGININSTALLER_INSTALLPLUGIN_BUTTON);
}

bool PluginInstallerInfoBarDelegate::Accept() {
  callback_.Run(plugin_metadata_.get());
  return false;
}

base::string16 PluginInstallerInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(new_install_ ?
      IDS_PLUGININSTALLER_PROBLEMSINSTALLING :
      IDS_PLUGININSTALLER_PROBLEMSUPDATING);
}

bool PluginInstallerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  GURL url(plugin_metadata_->help_url());
  if (url.is_empty()) {
    url = GURL(
        "https://www.google.com/support/chrome/bin/answer.py?answer=142064");
  }
  InfoBarService::WebContentsFromInfoBar(infobar())->OpenURL(
      content::OpenURLParams(
          url, content::Referrer(),
          (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
          ui::PAGE_TRANSITION_LINK, false));
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
  ReplaceWithInfoBar(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_ERROR_SHORT,
                                                plugin_metadata_->name()));
}

void PluginInstallerInfoBarDelegate::DownloadFinished() {
  ReplaceWithInfoBar(
      l10n_util::GetStringFUTF16(
          new_install_ ? IDS_PLUGIN_INSTALLING : IDS_PLUGIN_UPDATING,
          plugin_metadata_->name()));
}

void PluginInstallerInfoBarDelegate::OnlyWeakObserversLeft() {
  infobar()->RemoveSelf();
}

void PluginInstallerInfoBarDelegate::ReplaceWithInfoBar(
    const base::string16& message) {
  // Return early if the message doesn't change. This is important in case the
  // PluginInstaller is still iterating over its observers (otherwise we would
  // keep replacing infobar delegates infinitely).
  if ((message_ == message) || !infobar()->owner())
    return;
  Replace(infobar(), installer(), plugin_metadata_->Clone(), new_install_,
          message);
}


#if defined(OS_WIN)

// PluginMetroModeInfoBarDelegate ---------------------------------------------

// static
void PluginMetroModeInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PluginMetroModeInfoBarDelegate::Mode mode,
    const base::string16& name) {
  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(
          new PluginMetroModeInfoBarDelegate(mode, name))));
}

PluginMetroModeInfoBarDelegate::PluginMetroModeInfoBarDelegate(
    PluginMetroModeInfoBarDelegate::Mode mode,
    const base::string16& name)
    : ConfirmInfoBarDelegate(),
      mode_(mode),
      name_(name) {
}

PluginMetroModeInfoBarDelegate::~PluginMetroModeInfoBarDelegate() {
}

int PluginMetroModeInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_PLUGIN_INSTALL;
}

base::string16 PluginMetroModeInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16((mode_ == MISSING_PLUGIN) ?
      IDS_METRO_MISSING_PLUGIN_PROMPT : IDS_METRO_NPAPI_PLUGIN_PROMPT, name_);
}

int PluginMetroModeInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 PluginMetroModeInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_WIN_DESKTOP_RESTART);
}

void LaunchDesktopInstanceHelper(const base::string16& url) {
  base::FilePath exe_path;
  if (!PathService::Get(base::FILE_EXE, &exe_path))
    return;
  base::FilePath shortcut_path(
      ShellIntegration::GetStartMenuShortcut(exe_path));

  // Actually launching the process needs to happen in the metro viewer,
  // otherwise it won't automatically transition to desktop.  So we have
  // to send an IPC to the viewer to do the ShellExecute.
  aura::RemoteWindowTreeHostWin::Instance()->HandleOpenURLOnDesktop(
      shortcut_path, url);
}

bool PluginMetroModeInfoBarDelegate::Accept() {
  chrome::AttemptRestartToDesktopMode();
  return true;
}

base::string16 PluginMetroModeInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool PluginMetroModeInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  // TODO(shrikant): We may need to change language a little at following
  // support URLs. With new approach we will just restart for both missing
  // and not missing mode.
  InfoBarService::WebContentsFromInfoBar(infobar())->OpenURL(
      content::OpenURLParams(
          GURL((mode_ == MISSING_PLUGIN) ?
              "https://support.google.com/chrome/?p=ib_display_in_desktop" :
              "https://support.google.com/chrome/?p=ib_redirect_to_desktop"),
          content::Referrer(),
          (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
          ui::PAGE_TRANSITION_LINK, false));
  return false;
}

#endif  // defined(OS_WIN)

#endif  // defined(ENABLE_PLUGIN_INSTALLATION)
