// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_observer.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/plugin_installer_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/render_messages.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugininfo.h"

namespace {

// PluginInfoBar --------------------------------------------------------------

class PluginInfoBar : public ConfirmInfoBarDelegate {
 public:
  PluginInfoBar(TabContents* tab_contents, const string16& name);

  // ConfirmInfoBarDelegate:
  virtual SkBitmap* GetIcon() const;
  virtual int GetButtons() const;
  virtual string16 GetLinkText();

 protected:
  virtual ~PluginInfoBar();

  void CommonCancel();
  void CommonClose();
  void CommonLearnMore(WindowOpenDisposition disposition);

  string16 name_;
  TabContents* tab_contents_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginInfoBar);
};

PluginInfoBar::PluginInfoBar(TabContents* tab_contents, const string16& name)
    : ConfirmInfoBarDelegate(tab_contents),
      name_(name),
      tab_contents_(tab_contents) {
}

PluginInfoBar::~PluginInfoBar() {
}

void PluginInfoBar::CommonClose() {
  delete this;
}

SkBitmap* PluginInfoBar::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_PLUGIN_INSTALL);
}

int PluginInfoBar::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

void PluginInfoBar::CommonCancel() {
  tab_contents_->render_view_host()->LoadBlockedPlugins();
}

string16 PluginInfoBar::GetLinkText() {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

void PluginInfoBar::CommonLearnMore(WindowOpenDisposition disposition) {
  // TODO(bauerb): Navigate to a help page explaining why we disabled
  // or blocked the plugin, once we have one.
}

// BlockedPluginInfoBar -------------------------------------------------------

class BlockedPluginInfoBar : public PluginInfoBar {
 public:
  BlockedPluginInfoBar(TabContents* tab_contents,
                       const string16& name);

  // ConfirmInfoBarDelegate:
  virtual string16 GetMessageText() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual void InfoBarClosed();
  virtual bool LinkClicked(WindowOpenDisposition disposition);

 protected:
  virtual ~BlockedPluginInfoBar();

 private:
  DISALLOW_COPY_AND_ASSIGN(BlockedPluginInfoBar);
};

BlockedPluginInfoBar::BlockedPluginInfoBar(TabContents* tab_contents,
                                           const string16& name)
    : PluginInfoBar(tab_contents, name) {
  tab_contents->AddInfoBar(this);
  UserMetrics::RecordAction(UserMetricsAction("BlockedPluginInfobar.Shown"));
}

BlockedPluginInfoBar::~BlockedPluginInfoBar() {
}

string16 BlockedPluginInfoBar::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_PLUGIN_NOT_AUTHORIZED, name_);
}

string16 BlockedPluginInfoBar::GetButtonLabel(InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PLUGIN_ENABLE_ALWAYS : IDS_PLUGIN_ENABLE_TEMPORARILY);
}

bool BlockedPluginInfoBar::Accept() {
  UserMetrics::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.AlwaysAllow"));
  tab_contents_->profile()->GetHostContentSettingsMap()->AddExceptionForURL(
      tab_contents_->GetURL(), CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
      CONTENT_SETTING_ALLOW);
  tab_contents_->render_view_host()->LoadBlockedPlugins();
  return true;
}

bool BlockedPluginInfoBar::Cancel() {
  UserMetrics::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.AllowThisTime"));
  CommonCancel();
  return true;
}

void BlockedPluginInfoBar::InfoBarClosed() {
  UserMetrics::RecordAction(UserMetricsAction("BlockedPluginInfobar.Closed"));
  CommonClose();
}

bool BlockedPluginInfoBar::LinkClicked(WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.LearnMore"));
  CommonLearnMore(disposition);
  return false;
}

// OutdatedPluginInfoBar ------------------------------------------------------

class OutdatedPluginInfoBar : public PluginInfoBar {
 public:
  OutdatedPluginInfoBar(TabContents* tab_contents,
                        const string16& name,
                        const GURL& update_url);

  // ConfirmInfoBarDelegate:
  virtual string16 GetMessageText() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual void InfoBarClosed();
  virtual bool LinkClicked(WindowOpenDisposition disposition);

 protected:
  virtual ~OutdatedPluginInfoBar();

 private:
  GURL update_url_;

  DISALLOW_COPY_AND_ASSIGN(OutdatedPluginInfoBar);
};

OutdatedPluginInfoBar::OutdatedPluginInfoBar(TabContents* tab_contents,
                                             const string16& name,
                                             const GURL& update_url)
    : PluginInfoBar(tab_contents, name), update_url_(update_url) {
  tab_contents->AddInfoBar(this);
  UserMetrics::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Shown"));
}

OutdatedPluginInfoBar::~OutdatedPluginInfoBar() {
}

string16 OutdatedPluginInfoBar::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_PLUGIN_OUTDATED_PROMPT, name_);
}

string16 OutdatedPluginInfoBar::GetButtonLabel(InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PLUGIN_UPDATE : IDS_PLUGIN_ENABLE_TEMPORARILY);
}

bool OutdatedPluginInfoBar::Accept() {
  UserMetrics::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Update"));
  tab_contents_->OpenURL(update_url_, GURL(), NEW_FOREGROUND_TAB,
                         PageTransition::LINK);
  return false;
}

bool OutdatedPluginInfoBar::Cancel() {
  UserMetrics::RecordAction(
      UserMetricsAction("OutdatedPluginInfobar.AllowThisTime"));
  CommonCancel();
  return true;
}

void OutdatedPluginInfoBar::InfoBarClosed() {
  UserMetrics::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Closed"));
  CommonClose();
}

bool OutdatedPluginInfoBar::LinkClicked(WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(
      UserMetricsAction("OutdatedPluginInfobar.LearnMore"));
  CommonLearnMore(disposition);
  return false;
}

}  // namespace

PluginObserver::PluginObserver(TabContents* tab_contents)
    : tab_contents_(tab_contents) { }

PluginObserver::~PluginObserver() { }

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
  if (plugin_installer_.get() == NULL)
    plugin_installer_.reset(new PluginInstallerInfoBarDelegate(tab_contents_));
  return plugin_installer_.get();
}

void PluginObserver::OnMissingPluginStatus(int status) {
#if defined(OS_WIN)
// TODO(PORT): pull in when plug-ins work
  GetPluginInstaller()->OnMissingPluginStatus(status);
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
  tab_contents_->AddInfoBar(new SimpleAlertInfoBarDelegate(
      tab_contents_, crash_icon,
      l10n_util::GetStringFUTF16(IDS_PLUGIN_CRASHED_PROMPT, plugin_name),
      true));
}

void PluginObserver::OnBlockedOutdatedPlugin(const string16& name,
                                                  const GURL& update_url) {
  if (!update_url.is_empty())
    new OutdatedPluginInfoBar(tab_contents_, name, update_url);
  else
    new BlockedPluginInfoBar(tab_contents_, name);
}

