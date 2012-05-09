// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_observer.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/plugin_finder.h"
#include "chrome/browser/plugin_infobar_delegates.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugin_installer.h"
#include "chrome/browser/plugin_installer_observer.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

using content::OpenURLParams;
using content::PluginService;
using content::Referrer;
using content::WebContents;

namespace {

#if defined(ENABLE_PLUGIN_INSTALLATION)

// ConfirmInstallDialogDelegate ------------------------------------------------

class ConfirmInstallDialogDelegate : public TabModalConfirmDialogDelegate,
                                     public WeakPluginInstallerObserver {
 public:
  ConfirmInstallDialogDelegate(TabContentsWrapper* wrapper,
                               PluginInstaller* installer);

  // TabModalConfirmDialogDelegate methods:
  virtual string16 GetTitle() OVERRIDE;
  virtual string16 GetMessage() OVERRIDE;
  virtual string16 GetAcceptButtonTitle() OVERRIDE;
  virtual void OnAccepted() OVERRIDE;
  virtual void OnCanceled() OVERRIDE;

  // WeakPluginInstallerObserver methods:
  virtual void DownloadStarted() OVERRIDE;
  virtual void OnlyWeakObserversLeft() OVERRIDE;

 private:
  TabContentsWrapper* wrapper_;
};

ConfirmInstallDialogDelegate::ConfirmInstallDialogDelegate(
    TabContentsWrapper* wrapper,
    PluginInstaller* installer)
    : TabModalConfirmDialogDelegate(wrapper->web_contents()),
      WeakPluginInstallerObserver(installer),
      wrapper_(wrapper) {
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
  installer()->StartInstalling(wrapper_);
}

void ConfirmInstallDialogDelegate::OnCanceled() {
}

void ConfirmInstallDialogDelegate::DownloadStarted() {
  Cancel();
}

void ConfirmInstallDialogDelegate::OnlyWeakObserversLeft() {
  Cancel();
}
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

}  // namespace

// PluginObserver -------------------------------------------------------------

#if defined(ENABLE_PLUGIN_INSTALLATION)
class PluginObserver::PluginPlaceholderHost : public PluginInstallerObserver {
 public:
  PluginPlaceholderHost(PluginObserver* observer,
                        int routing_id,
                        PluginInstaller* installer)
      : PluginInstallerObserver(installer),
        observer_(observer),
        routing_id_(routing_id) {
    DCHECK(installer);
    switch (installer->state()) {
      case PluginInstaller::kStateIdle: {
        observer->Send(new ChromeViewMsg_FoundMissingPlugin(routing_id_,
                                                            installer->name()));
        break;
      }
      case PluginInstaller::kStateDownloading: {
        DownloadStarted();
        break;
      }
    }
  }

  // PluginInstallerObserver methods:
  virtual void DownloadStarted() OVERRIDE {
    observer_->Send(new ChromeViewMsg_StartedDownloadingPlugin(routing_id_));
  }

  virtual void DownloadError(const std::string& msg) OVERRIDE {
    observer_->Send(new ChromeViewMsg_ErrorDownloadingPlugin(routing_id_, msg));
  }

  virtual void DownloadCancelled() OVERRIDE {
    observer_->Send(new ChromeViewMsg_CancelledDownloadingPlugin(routing_id_));
  }

  virtual void DownloadFinished() OVERRIDE {
    observer_->Send(new ChromeViewMsg_FinishedDownloadingPlugin(routing_id_));
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
#if defined(ENABLE_PLUGIN_INSTALLATION)
  STLDeleteValues(&plugin_placeholders_);
#endif
}

void PluginObserver::PluginCrashed(const FilePath& plugin_path) {
  DCHECK(!plugin_path.value().empty());

  string16 plugin_name =
      PluginService::GetInstance()->GetPluginDisplayNameByPath(plugin_path);
  gfx::Image* icon = &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_PLUGIN_CRASHED);
  InfoBarTabHelper* infobar_helper = tab_contents_->infobar_tab_helper();
  infobar_helper->AddInfoBar(
      new SimpleAlertInfoBarDelegate(
          infobar_helper,
          icon,
          l10n_util::GetStringFUTF16(IDS_PLUGIN_CRASHED_PROMPT, plugin_name),
          true));
}

bool PluginObserver::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PluginObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_BlockedOutdatedPlugin,
                        OnBlockedOutdatedPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_BlockedUnauthorizedPlugin,
                        OnBlockedUnauthorizedPlugin)
#if defined(ENABLE_PLUGIN_INSTALLATION)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FindMissingPlugin,
                        OnFindMissingPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RemovePluginPlaceholderHost,
                        OnRemovePluginPlaceholderHost)
#endif
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_OpenAboutPlugins,
                        OnOpenAboutPlugins)

    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()

  return true;
}

void PluginObserver::OnBlockedUnauthorizedPlugin(
    const string16& name,
    const std::string& identifier) {
  InfoBarTabHelper* infobar_helper = tab_contents_->infobar_tab_helper();
  infobar_helper->AddInfoBar(
      new UnauthorizedPluginInfoBarDelegate(
          infobar_helper,
          tab_contents_->profile()->GetHostContentSettingsMap(),
          name, identifier));
}

void PluginObserver::OnBlockedOutdatedPlugin(int placeholder_id,
                                             const std::string& identifier) {
#if defined(ENABLE_PLUGIN_INSTALLATION)
  PluginFinder::Get(base::Bind(&PluginObserver::FindPluginToUpdate,
                               weak_ptr_factory_.GetWeakPtr(),
                               placeholder_id, identifier));
#else
  // If we don't support third-party plug-in installation, we shouldn't have
  // outdated plug-ins.
  NOTREACHED();
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)
}

#if defined(ENABLE_PLUGIN_INSTALLATION)
void PluginObserver::FindPluginToUpdate(int placeholder_id,
                                        const std::string& identifier,
                                        PluginFinder* plugin_finder) {
  PluginInstaller* installer =
      plugin_finder->FindPluginWithIdentifier(identifier);
  DCHECK(installer) << "Couldn't find PluginInstaller for identifier "
                    << identifier;
  plugin_placeholders_[placeholder_id] =
      new PluginPlaceholderHost(this, placeholder_id, installer);
  InfoBarTabHelper* infobar_helper = tab_contents_->infobar_tab_helper();
  infobar_helper->AddInfoBar(
      OutdatedPluginInfoBarDelegate::Create(this, installer));
}

void PluginObserver::OnFindMissingPlugin(int placeholder_id,
                                         const std::string& mime_type) {
PluginFinder::Get(base::Bind(&PluginObserver::FindMissingPlugin,
                             weak_ptr_factory_.GetWeakPtr(),
                             placeholder_id, mime_type));
}

void PluginObserver::FindMissingPlugin(int placeholder_id,
                                       const std::string& mime_type,
                                       PluginFinder* plugin_finder) {
  std::string lang = "en-US";  // Oh yes.
  PluginInstaller* installer = plugin_finder->FindPlugin(mime_type, lang);
  if (!installer) {
    Send(new ChromeViewMsg_DidNotFindMissingPlugin(placeholder_id));
    return;
  }

  plugin_placeholders_[placeholder_id] =
      new PluginPlaceholderHost(this, placeholder_id, installer);
  InfoBarTabHelper* infobar_helper = tab_contents_->infobar_tab_helper();
  InfoBarDelegate* delegate = PluginInstallerInfoBarDelegate::Create(
      infobar_helper, installer,
      base::Bind(&PluginObserver::InstallMissingPlugin,
                 weak_ptr_factory_.GetWeakPtr(), installer));
  infobar_helper->AddInfoBar(delegate);
}

void PluginObserver::InstallMissingPlugin(PluginInstaller* installer) {
  if (installer->url_for_display()) {
    installer->OpenDownloadURL(web_contents());
  } else {
    browser::ShowTabModalConfirmDialog(
        new ConfirmInstallDialogDelegate(tab_contents_, installer),
        tab_contents_);
  }
}

void PluginObserver::OnRemovePluginPlaceholderHost(int placeholder_id) {
  std::map<int, PluginPlaceholderHost*>::iterator it =
      plugin_placeholders_.find(placeholder_id);
  if (it == plugin_placeholders_.end()) {
    NOTREACHED();
    return;
  }
  delete it->second;
  plugin_placeholders_.erase(it);
}
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

void PluginObserver::OnOpenAboutPlugins() {
    web_contents()->OpenURL(OpenURLParams(
        GURL(chrome::kAboutPluginsURL),
        content::Referrer(web_contents()->GetURL(),
                          WebKit::WebReferrerPolicyDefault),
        NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_TYPED, false));
}
