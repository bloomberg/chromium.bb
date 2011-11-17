// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_observer.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/webplugininfo.h"

namespace {

// PluginInfoBarDelegate ------------------------------------------------------

class PluginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  PluginInfoBarDelegate(InfoBarTabHelper* infobar_helper, const string16& name);

 protected:
  virtual ~PluginInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual bool Cancel() OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  virtual std::string GetLearnMoreURL() const = 0;

  string16 name_;

 private:
  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(PluginInfoBarDelegate);
};

PluginInfoBarDelegate::PluginInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                             const string16& name)
    : ConfirmInfoBarDelegate(infobar_helper),
      name_(name) {
}

PluginInfoBarDelegate::~PluginInfoBarDelegate() {
}

bool PluginInfoBarDelegate::Cancel() {
  owner()->Send(new ChromeViewMsg_LoadBlockedPlugins(owner()->routing_id()));
  return true;
}

bool PluginInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  owner()->tab_contents()->OpenURL(
      google_util::AppendGoogleLocaleParam(GURL(GetLearnMoreURL())), GURL(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK);
  return false;
}

gfx::Image* PluginInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_PLUGIN_INSTALL);
}

string16 PluginInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}


// BlockedPluginInfoBarDelegate -----------------------------------------------

class BlockedPluginInfoBarDelegate : public PluginInfoBarDelegate {
 public:
  BlockedPluginInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                               HostContentSettingsMap* content_settings,
                               const string16& name);

 private:
  virtual ~BlockedPluginInfoBarDelegate();

  // PluginInfoBarDelegate:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;
  virtual std::string GetLearnMoreURL() const OVERRIDE;

  HostContentSettingsMap* content_settings_;

  DISALLOW_COPY_AND_ASSIGN(BlockedPluginInfoBarDelegate);
};

BlockedPluginInfoBarDelegate::BlockedPluginInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    HostContentSettingsMap* content_settings,
    const string16& utf16_name)
    : PluginInfoBarDelegate(infobar_helper, utf16_name),
      content_settings_(content_settings) {
  UserMetrics::RecordAction(UserMetricsAction("BlockedPluginInfobar.Shown"));
  std::string name = UTF16ToUTF8(utf16_name);
  if (name == webkit::npapi::PluginGroup::kJavaGroupName)
    UserMetrics::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.Java"));
  else if (name == webkit::npapi::PluginGroup::kQuickTimeGroupName)
    UserMetrics::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.QuickTime"));
  else if (name == webkit::npapi::PluginGroup::kShockwaveGroupName)
    UserMetrics::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.Shockwave"));
  else if (name == webkit::npapi::PluginGroup::kRealPlayerGroupName)
    UserMetrics::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.RealPlayer"));
  else if (name == webkit::npapi::PluginGroup::kWindowsMediaPlayerGroupName)
    UserMetrics::RecordAction(
        UserMetricsAction("BlockedPluginInfobar.Shown.WindowsMediaPlayer"));
}

BlockedPluginInfoBarDelegate::~BlockedPluginInfoBarDelegate() {
  UserMetrics::RecordAction(UserMetricsAction("BlockedPluginInfobar.Closed"));
}

std::string BlockedPluginInfoBarDelegate::GetLearnMoreURL() const {
  return chrome::kBlockedPluginLearnMoreURL;
}

string16 BlockedPluginInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_PLUGIN_NOT_AUTHORIZED, name_);
}

string16 BlockedPluginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PLUGIN_ENABLE_TEMPORARILY : IDS_PLUGIN_ENABLE_ALWAYS);
}

bool BlockedPluginInfoBarDelegate::Accept() {
  UserMetrics::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.AllowThisTime"));
  return PluginInfoBarDelegate::Cancel();
}

bool BlockedPluginInfoBarDelegate::Cancel() {
  UserMetrics::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.AlwaysAllow"));
  content_settings_->AddExceptionForURL(owner()->tab_contents()->GetURL(),
                                        owner()->tab_contents()->GetURL(),
                                        CONTENT_SETTINGS_TYPE_PLUGINS,
                                        std::string(),
                                        CONTENT_SETTING_ALLOW);
  return PluginInfoBarDelegate::Cancel();
}

void BlockedPluginInfoBarDelegate::InfoBarDismissed() {
  UserMetrics::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.Dismissed"));
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
  OutdatedPluginInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                const string16& name,
                                const GURL& update_url);

 private:
  virtual ~OutdatedPluginInfoBarDelegate();

  // PluginInfoBarDelegate:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;
  virtual std::string GetLearnMoreURL() const OVERRIDE;

  GURL update_url_;

  DISALLOW_COPY_AND_ASSIGN(OutdatedPluginInfoBarDelegate);
};

OutdatedPluginInfoBarDelegate::OutdatedPluginInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    const string16& utf16_name,
    const GURL& update_url)
    : PluginInfoBarDelegate(infobar_helper, utf16_name),
      update_url_(update_url) {
  UserMetrics::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Shown"));
  std::string name = UTF16ToUTF8(utf16_name);
  if (name == webkit::npapi::PluginGroup::kJavaGroupName)
    UserMetrics::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Java"));
  else if (name == webkit::npapi::PluginGroup::kQuickTimeGroupName)
    UserMetrics::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.QuickTime"));
  else if (name == webkit::npapi::PluginGroup::kShockwaveGroupName)
    UserMetrics::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Shockwave"));
  else if (name == webkit::npapi::PluginGroup::kRealPlayerGroupName)
    UserMetrics::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.RealPlayer"));
  else if (name == webkit::npapi::PluginGroup::kSilverlightGroupName)
    UserMetrics::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Silverlight"));
  else if (name == webkit::npapi::PluginGroup::kAdobeReaderGroupName)
    UserMetrics::RecordAction(
        UserMetricsAction("OutdatedPluginInfobar.Shown.Reader"));
}

OutdatedPluginInfoBarDelegate::~OutdatedPluginInfoBarDelegate() {
  UserMetrics::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Closed"));
}

std::string OutdatedPluginInfoBarDelegate::GetLearnMoreURL() const {
  return chrome::kOutdatedPluginLearnMoreURL;
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
  owner()->tab_contents()->OpenURL(update_url_, GURL(), NEW_FOREGROUND_TAB,
                                   content::PAGE_TRANSITION_LINK);
  return false;
}

bool OutdatedPluginInfoBarDelegate::Cancel() {
  UserMetrics::RecordAction(
      UserMetricsAction("OutdatedPluginInfobar.AllowThisTime"));
  return PluginInfoBarDelegate::Cancel();
}

void OutdatedPluginInfoBarDelegate::InfoBarDismissed() {
  UserMetrics::RecordAction(
      UserMetricsAction("OutdatedPluginInfobar.Dismissed"));
}

bool OutdatedPluginInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(
      UserMetricsAction("OutdatedPluginInfobar.LearnMore"));
  return PluginInfoBarDelegate::LinkClicked(disposition);
}

}  // namespace


// PluginObserver -------------------------------------------------------------

PluginObserver::PluginObserver(TabContentsWrapper* tab_contents)
    : TabContentsObserver(tab_contents->tab_contents()),
      tab_contents_(tab_contents) {
}

PluginObserver::~PluginObserver() {
}

bool PluginObserver::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PluginObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_BlockedOutdatedPlugin,
                        OnBlockedOutdatedPlugin)
    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()

  return true;
}

void PluginObserver::OnBlockedOutdatedPlugin(const string16& name,
                                             const GURL& update_url) {
  InfoBarTabHelper* infobar_helper = tab_contents_->infobar_tab_helper();
  infobar_helper->AddInfoBar(update_url.is_empty() ?
      static_cast<InfoBarDelegate*>(new BlockedPluginInfoBarDelegate(
          infobar_helper,
          tab_contents_->profile()->GetHostContentSettingsMap(),
          name)) :
      new OutdatedPluginInfoBarDelegate(infobar_helper, name, update_url));
}
