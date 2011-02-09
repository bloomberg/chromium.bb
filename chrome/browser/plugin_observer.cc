// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_observer.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/plugin_installer_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/default_plugin_shared.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugininfo.h"

namespace {

// PluginInfoBarDelegate ------------------------------------------------------

class PluginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  PluginInfoBarDelegate(TabContents* tab_contents, const string16& name);

 protected:
  virtual ~PluginInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual void InfoBarClosed();
  virtual bool Cancel();
  virtual bool LinkClicked(WindowOpenDisposition disposition);

  string16 name_;
  TabContents* tab_contents_;

 private:
  // ConfirmInfoBarDelegate:
  virtual SkBitmap* GetIcon() const;
  virtual string16 GetLinkText();

  DISALLOW_COPY_AND_ASSIGN(PluginInfoBarDelegate);
};

PluginInfoBarDelegate::PluginInfoBarDelegate(TabContents* tab_contents,
                                             const string16& name)
    : ConfirmInfoBarDelegate(tab_contents),
      name_(name),
      tab_contents_(tab_contents) {
}

PluginInfoBarDelegate::~PluginInfoBarDelegate() {
}

void PluginInfoBarDelegate::InfoBarClosed() {
  delete this;
}

bool PluginInfoBarDelegate::Cancel() {
  tab_contents_->render_view_host()->LoadBlockedPlugins();
  return true;
}

bool PluginInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(chrome::kOutdatedPluginLearnMoreURL));
  tab_contents_->OpenURL(url, GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  return false;
}

SkBitmap* PluginInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_PLUGIN_INSTALL);
}

string16 PluginInfoBarDelegate::GetLinkText() {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}


// BlockedPluginInfoBarDelegate -----------------------------------------------

class BlockedPluginInfoBarDelegate : public PluginInfoBarDelegate {
 public:
  BlockedPluginInfoBarDelegate(TabContents* tab_contents,
                               const string16& name);

 private:
  virtual ~BlockedPluginInfoBarDelegate();

  // PluginInfoBarDelegate:
  virtual string16 GetMessageText() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual void InfoBarClosed();
  virtual bool LinkClicked(WindowOpenDisposition disposition);

  DISALLOW_COPY_AND_ASSIGN(BlockedPluginInfoBarDelegate);
};

BlockedPluginInfoBarDelegate::BlockedPluginInfoBarDelegate(
    TabContents* tab_contents,
    const string16& name)
    : PluginInfoBarDelegate(tab_contents, name) {
  UserMetrics::RecordAction(UserMetricsAction("BlockedPluginInfobar.Shown"));
}

BlockedPluginInfoBarDelegate::~BlockedPluginInfoBarDelegate() {
}

string16 BlockedPluginInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_PLUGIN_NOT_AUTHORIZED, name_);
}

string16 BlockedPluginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PLUGIN_ENABLE_ALWAYS : IDS_PLUGIN_ENABLE_TEMPORARILY);
}

bool BlockedPluginInfoBarDelegate::Accept() {
  UserMetrics::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.AlwaysAllow"));
  tab_contents_->profile()->GetHostContentSettingsMap()->AddExceptionForURL(
      tab_contents_->GetURL(), CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
      CONTENT_SETTING_ALLOW);
  tab_contents_->render_view_host()->LoadBlockedPlugins();
  return true;
}

bool BlockedPluginInfoBarDelegate::Cancel() {
  UserMetrics::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.AllowThisTime"));
  return PluginInfoBarDelegate::Cancel();
}

void BlockedPluginInfoBarDelegate::InfoBarClosed() {
  UserMetrics::RecordAction(UserMetricsAction("BlockedPluginInfobar.Closed"));
  PluginInfoBarDelegate::InfoBarClosed();
}

bool BlockedPluginInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.LearnMore"));
  return PluginInfoBarDelegate::LinkClicked(disposition);
}

// OutdatedPluginInfoBarDelegate ----------------------------------------------

class OutdatedPluginInfoBarDelegate : public PluginInfoBarDelegate {
 public:
  OutdatedPluginInfoBarDelegate(TabContents* tab_contents,
                                const string16& name,
                                const GURL& update_url);

 private:
  virtual ~OutdatedPluginInfoBarDelegate();

  // PluginInfoBarDelegate:
  virtual string16 GetMessageText() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual void InfoBarClosed();
  virtual bool LinkClicked(WindowOpenDisposition disposition);

  GURL update_url_;

  DISALLOW_COPY_AND_ASSIGN(OutdatedPluginInfoBarDelegate);
};

OutdatedPluginInfoBarDelegate::OutdatedPluginInfoBarDelegate(
    TabContents* tab_contents,
    const string16& name,
    const GURL& update_url)
    : PluginInfoBarDelegate(tab_contents, name),
      update_url_(update_url) {
  UserMetrics::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Shown"));
}

OutdatedPluginInfoBarDelegate::~OutdatedPluginInfoBarDelegate() {
}

string16 OutdatedPluginInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_PLUGIN_OUTDATED_PROMPT, name_);
}

string16 OutdatedPluginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PLUGIN_UPDATE : IDS_PLUGIN_ENABLE_TEMPORARILY);
}

bool OutdatedPluginInfoBarDelegate::Accept() {
  UserMetrics::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Update"));
  tab_contents_->OpenURL(update_url_, GURL(), NEW_FOREGROUND_TAB,
                         PageTransition::LINK);
  return false;
}

bool OutdatedPluginInfoBarDelegate::Cancel() {
  UserMetrics::RecordAction(
      UserMetricsAction("OutdatedPluginInfobar.AllowThisTime"));
  return PluginInfoBarDelegate::Cancel();
}

void OutdatedPluginInfoBarDelegate::InfoBarClosed() {
  UserMetrics::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Closed"));
  PluginInfoBarDelegate::InfoBarClosed();
}

bool OutdatedPluginInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(
      UserMetricsAction("OutdatedPluginInfobar.LearnMore"));
  return PluginInfoBarDelegate::LinkClicked(disposition);
}

}  // namespace


// PluginObserver -------------------------------------------------------------

PluginObserver::PluginObserver(TabContents* tab_contents)
    : tab_contents_(tab_contents) {
}

PluginObserver::~PluginObserver() {
}

bool PluginObserver::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PluginObserver, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MissingPluginStatus, OnMissingPluginStatus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CrashedPlugin, OnCrashedPlugin)
    IPC_MESSAGE_HANDLER(ViewHostMsg_BlockedOutdatedPlugin,
                        OnBlockedOutdatedPlugin)
    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()

  return true;
}

PluginInstallerInfoBarDelegate* PluginObserver::GetPluginInstaller() {
  if (plugin_installer_ == NULL)
    plugin_installer_.reset(new PluginInstallerInfoBarDelegate(tab_contents_));
  return plugin_installer_->AsPluginInstallerInfoBarDelegate();
}

void PluginObserver::OnMissingPluginStatus(int status) {
  // TODO(PORT): pull in when plug-ins work
#if defined(OS_WIN)
  if (status == webkit::npapi::default_plugin::MISSING_PLUGIN_AVAILABLE) {
    tab_contents_->AddInfoBar(
        new PluginInstallerInfoBarDelegate(tab_contents_));
    return;
  }

  DCHECK_EQ(webkit::npapi::default_plugin::MISSING_PLUGIN_USER_STARTED_DOWNLOAD,
            status);
  for (size_t i = 0; i < tab_contents_->infobar_count(); ++i) {
    InfoBarDelegate* delegate = tab_contents_->GetInfoBarDelegateAt(i);
    if (delegate->AsPluginInstallerInfoBarDelegate() != NULL) {
      tab_contents_->RemoveInfoBar(delegate);
      return;
    }
  }
#endif
}

void PluginObserver::OnCrashedPlugin(const FilePath& plugin_path) {
  DCHECK(!plugin_path.value().empty());

  string16 plugin_name = plugin_path.LossyDisplayName();
  webkit::npapi::WebPluginInfo plugin_info;
  if (webkit::npapi::PluginList::Singleton()->GetPluginInfoByPath(
          plugin_path, &plugin_info) &&
      !plugin_info.name.empty()) {
    plugin_name = plugin_info.name;
#if defined(OS_MACOSX)
    // Many plugins on the Mac have .plugin in the actual name, which looks
    // terrible, so look for that and strip it off if present.
    const std::string kPluginExtension = ".plugin";
    if (EndsWith(plugin_name, ASCIIToUTF16(kPluginExtension), true))
      plugin_name.erase(plugin_name.length() - kPluginExtension.length());
#endif  // OS_MACOSX
  }
  SkBitmap* crash_icon = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_PLUGIN_CRASHED);
  tab_contents_->AddInfoBar(new SimpleAlertInfoBarDelegate(tab_contents_,
      crash_icon,
      l10n_util::GetStringFUTF16(IDS_PLUGIN_CRASHED_PROMPT, plugin_name),
      true));
}

void PluginObserver::OnBlockedOutdatedPlugin(const string16& name,
                                             const GURL& update_url) {
  tab_contents_->AddInfoBar(update_url.is_empty() ?
      static_cast<InfoBarDelegate*>(new BlockedPluginInfoBarDelegate(
          tab_contents_, name)) :
      new OutdatedPluginInfoBarDelegate(tab_contents_, name, update_url));
}

