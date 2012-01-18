// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_observer.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/plugin_finder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugin_installer.h"
#include "chrome/browser/plugin_installer_infobar_delegate.h"
#include "chrome/browser/plugin_installer_observer.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

using content::OpenURLParams;
using content::Referrer;
using content::UserMetricsAction;
using content::WebContents;

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
  OpenURLParams params(
      google_util::AppendGoogleLocaleParam(GURL(GetLearnMoreURL())), Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK,
      false);
  owner()->web_contents()->OpenURL(params);
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

BlockedPluginInfoBarDelegate::~BlockedPluginInfoBarDelegate() {
  content::RecordAction(UserMetricsAction("BlockedPluginInfobar.Closed"));
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
  content::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.AllowThisTime"));
  return PluginInfoBarDelegate::Cancel();
}

bool BlockedPluginInfoBarDelegate::Cancel() {
  content::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.AlwaysAllow"));
  content_settings_->AddExceptionForURL(owner()->web_contents()->GetURL(),
                                        owner()->web_contents()->GetURL(),
                                        CONTENT_SETTINGS_TYPE_PLUGINS,
                                        std::string(),
                                        CONTENT_SETTING_ALLOW);
  return PluginInfoBarDelegate::Cancel();
}

void BlockedPluginInfoBarDelegate::InfoBarDismissed() {
  content::RecordAction(
      UserMetricsAction("BlockedPluginInfobar.Dismissed"));
}

bool BlockedPluginInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  content::RecordAction(
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
  content::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Shown"));
  std::string name = UTF16ToUTF8(utf16_name);
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
  return l10n_util::GetStringFUTF16(IDS_PLUGIN_OUTDATED_PROMPT, name_);
}

string16 OutdatedPluginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PLUGIN_UPDATE : IDS_PLUGIN_ENABLE_TEMPORARILY);
}

bool OutdatedPluginInfoBarDelegate::Accept() {
  content::RecordAction(UserMetricsAction("OutdatedPluginInfobar.Update"));
  OpenURLParams params(
      update_url_, Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false);
  owner()->web_contents()->OpenURL(params);
  return false;
}

bool OutdatedPluginInfoBarDelegate::Cancel() {
  content::RecordAction(
      UserMetricsAction("OutdatedPluginInfobar.AllowThisTime"));
  return PluginInfoBarDelegate::Cancel();
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

#if defined(ENABLE_PLUGIN_INSTALLATION)
class ConfirmInstallDialogDelegate : public TabModalConfirmDialogDelegate,
                                     public PluginInstallerObserver {
 public:
  ConfirmInstallDialogDelegate(WebContents* web_contents,
                               PluginInstaller* installer);

  // TabModalConfirmDialogDelegate methods:
  virtual string16 GetTitle() OVERRIDE;
  virtual string16 GetMessage() OVERRIDE;
  virtual string16 GetAcceptButtonTitle() OVERRIDE;
  virtual void OnAccepted() OVERRIDE;
  virtual void OnCanceled() OVERRIDE;

  // PluginInstallerObserver methods:
  virtual void DidStartDownload() OVERRIDE;

 private:
  net::URLRequestContextGetter* request_context_;
};

ConfirmInstallDialogDelegate::ConfirmInstallDialogDelegate(
    WebContents* web_contents,
    PluginInstaller* installer)
    : TabModalConfirmDialogDelegate(web_contents),
      PluginInstallerObserver(installer),
      request_context_(web_contents->GetBrowserContext()->GetRequestContext()) {
}

string16 ConfirmInstallDialogDelegate::GetTitle() {
  return l10n_util::GetStringFUTF16(
      IDS_PLUGIN_CONFIRM_INSTALL_DIALOG_TITLE, installer()->name());
}

string16 ConfirmInstallDialogDelegate::GetMessage() {
  return l10n_util::GetStringFUTF16(IDS_PLUGIN_CONFIRM_INSTALL_DIALOG_MSG,
                                    installer()->name());
}

string16 ConfirmInstallDialogDelegate::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(
      IDS_PLUGIN_CONFIRM_INSTALL_DIALOG_ACCEPT_BUTTON);
}

void ConfirmInstallDialogDelegate::OnAccepted() {
  installer()->StartInstalling(request_context_);
}

void ConfirmInstallDialogDelegate::OnCanceled() {
}

void ConfirmInstallDialogDelegate::DidStartDownload() {
  Cancel();
}
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

}  // namespace

// PluginObserver -------------------------------------------------------------

#if defined(ENABLE_PLUGIN_INSTALLATION)
class PluginObserver::MissingPluginHost : public PluginInstallerObserver {
 public:
  MissingPluginHost(PluginObserver* observer,
                    int routing_id,
                    PluginInstaller* installer)
      : PluginInstallerObserver(installer),
        observer_(observer),
        routing_id_(routing_id) {
    switch (installer->state()) {
      case PluginInstaller::kStateIdle: {
        observer->Send(new ChromeViewMsg_FoundMissingPlugin(routing_id_,
                                                            installer->name()));
        observer->ShowPluginInstallationInfoBar(installer);
        break;
      }
      case PluginInstaller::kStateDownloading: {
        DidStartDownload();
        break;
      }
    }
  }

  // PluginInstallerObserver methods:
  virtual void DidStartDownload() OVERRIDE {
    observer_->Send(new ChromeViewMsg_StartedDownloadingPlugin(routing_id_));
  }

  virtual void DidFinishDownload() OVERRIDE {
    observer_->Send(new ChromeViewMsg_FinishedDownloadingPlugin(routing_id_));
  }

  virtual void DownloadError(const std::string& msg) OVERRIDE {
    observer_->Send(new ChromeViewMsg_ErrorDownloadingPlugin(routing_id_, msg));
  }

 private:
  // Weak pointer; owns us.
  PluginObserver* observer_;

  int routing_id_;
};
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

PluginObserver::PluginObserver(TabContentsWrapper* tab_contents)
    : content::WebContentsObserver(tab_contents->web_contents()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      tab_contents_(tab_contents) {
}

PluginObserver::~PluginObserver() {
}

bool PluginObserver::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PluginObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_BlockedOutdatedPlugin,
                        OnBlockedOutdatedPlugin)
#if defined(ENABLE_PLUGIN_INSTALLATION)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FindMissingPlugin,
                        OnFindMissingPlugin)
#endif
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_OpenAboutPlugins,
                        OnOpenAboutPlugins)

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

#if defined(ENABLE_PLUGIN_INSTALLATION)
void PluginObserver::OnFindMissingPlugin(int placeholder_id,
                                         const std::string& mime_type) {
  PluginFinder* plugin_finder = PluginFinder::GetInstance();
  std::string lang = "en-US";  // Oh yes.
  plugin_finder->FindPlugin(
      mime_type, lang,
      base::Bind(&PluginObserver::FoundMissingPlugin,
                 weak_ptr_factory_.GetWeakPtr(), placeholder_id, mime_type),
      base::Bind(&PluginObserver::DidNotFindMissingPlugin,
                 weak_ptr_factory_.GetWeakPtr(), placeholder_id));
}

void PluginObserver::FoundMissingPlugin(int placeholder_id,
                                        const std::string& mime_type,
                                        PluginInstaller* installer) {
  missing_plugins_.push_back(
      new MissingPluginHost(this, placeholder_id, installer));
}

void PluginObserver::ShowPluginInstallationInfoBar(PluginInstaller* installer) {
  InfoBarTabHelper* infobar_helper = tab_contents_->infobar_tab_helper();
  infobar_helper->AddInfoBar(new PluginInstallerInfoBarDelegate(
      installer,
      infobar_helper,
      installer->name(),
      installer->help_url(),
      base::Bind(&PluginObserver::InstallMissingPlugin,
                 weak_ptr_factory_.GetWeakPtr(), installer)));
}

void PluginObserver::DidNotFindMissingPlugin(int placeholder_id) {
  Send(new ChromeViewMsg_DidNotFindMissingPlugin(placeholder_id));
}

void PluginObserver::InstallMissingPlugin(PluginInstaller* installer) {
  if (installer->url_for_display()) {
    web_contents()->OpenURL(OpenURLParams(
        installer->plugin_url(),
        content::Referrer(web_contents()->GetURL(),
                          WebKit::WebReferrerPolicyDefault),
        NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_TYPED, false));
  } else {
    browser::ShowTabModalConfirmDialog(
        new ConfirmInstallDialogDelegate(web_contents(), installer),
        tab_contents_);
  }
}
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

void PluginObserver::OnOpenAboutPlugins() {
    web_contents()->OpenURL(OpenURLParams(
        GURL(chrome::kAboutPluginsURL),
        content::Referrer(web_contents()->GetURL(),
                          WebKit::WebReferrerPolicyDefault),
        NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_TYPED, false));
}
