// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_observer.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/api/infobars/simple_alert_infobar_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_infobar_delegates.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugins/plugin_installer.h"
#include "chrome/browser/plugins/plugin_installer_observer.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

#if defined(OS_WIN)
#include "base/win/metro.h"
#endif

using content::OpenURLParams;
using content::PluginService;
using content::Referrer;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PluginObserver);

namespace {

#if defined(ENABLE_PLUGIN_INSTALLATION)

// ConfirmInstallDialogDelegate ------------------------------------------------

class ConfirmInstallDialogDelegate : public TabModalConfirmDialogDelegate,
                                     public WeakPluginInstallerObserver {
 public:
  ConfirmInstallDialogDelegate(content::WebContents* web_contents,
                               PluginInstaller* installer,
                               scoped_ptr<PluginMetadata> plugin_metadata);

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
  content::WebContents* web_contents_;
  scoped_ptr<PluginMetadata> plugin_metadata_;
};

ConfirmInstallDialogDelegate::ConfirmInstallDialogDelegate(
    content::WebContents* web_contents,
    PluginInstaller* installer,
    scoped_ptr<PluginMetadata> plugin_metadata)
    : TabModalConfirmDialogDelegate(web_contents),
      WeakPluginInstallerObserver(installer),
      web_contents_(web_contents),
      plugin_metadata_(plugin_metadata.Pass()) {
}

string16 ConfirmInstallDialogDelegate::GetTitle() {
  return l10n_util::GetStringFUTF16(
      IDS_PLUGIN_CONFIRM_INSTALL_DIALOG_TITLE, plugin_metadata_->name());
}

string16 ConfirmInstallDialogDelegate::GetMessage() {
  return l10n_util::GetStringFUTF16(IDS_PLUGIN_CONFIRM_INSTALL_DIALOG_MSG,
                                    plugin_metadata_->name());
}

string16 ConfirmInstallDialogDelegate::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(
      IDS_PLUGIN_CONFIRM_INSTALL_DIALOG_ACCEPT_BUTTON);
}

void ConfirmInstallDialogDelegate::OnAccepted() {
  installer()->StartInstalling(plugin_metadata_->plugin_url(), web_contents_);
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
                        string16 plugin_name,
                        PluginInstaller* installer)
      : PluginInstallerObserver(installer),
        observer_(observer),
        routing_id_(routing_id) {
    DCHECK(installer);
    switch (installer->state()) {
      case PluginInstaller::INSTALLER_STATE_IDLE: {
        observer->Send(new ChromeViewMsg_FoundMissingPlugin(routing_id_,
                                                            plugin_name));
        break;
      }
      case PluginInstaller::INSTALLER_STATE_DOWNLOADING: {
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

PluginObserver::PluginObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
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
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  infobar_service->AddInfoBar(
      new SimpleAlertInfoBarDelegate(
          infobar_service,
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
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CouldNotLoadPlugin,
                        OnCouldNotLoadPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_NPAPINotSupported,
                        OnNPAPINotSupported)

    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()

  return true;
}

void PluginObserver::OnBlockedUnauthorizedPlugin(
    const string16& name,
    const std::string& identifier) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  infobar_service->AddInfoBar(
      new UnauthorizedPluginInfoBarDelegate(
          infobar_service,
          Profile::FromBrowserContext(web_contents()->GetBrowserContext())->
              GetHostContentSettingsMap(),
          name, identifier));
}

void PluginObserver::OnBlockedOutdatedPlugin(int placeholder_id,
                                             const std::string& identifier) {
#if defined(ENABLE_PLUGIN_INSTALLATION)
  PluginFinder* finder = PluginFinder::GetInstance();
  // Find plugin to update.
  PluginInstaller* installer = NULL;
  scoped_ptr<PluginMetadata> plugin;
  if (!finder->FindPluginWithIdentifier(identifier, &installer, &plugin)) {
    NOTREACHED();
    return;
  }

  plugin_placeholders_[placeholder_id] =
      new PluginPlaceholderHost(this, placeholder_id,
                                plugin->name(), installer);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  infobar_service->AddInfoBar(
      OutdatedPluginInfoBarDelegate::Create(web_contents(),
                                            installer, plugin.Pass()));
#else
  // If we don't support third-party plug-in installation, we shouldn't have
  // outdated plug-ins.
  NOTREACHED();
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)
}

#if defined(ENABLE_PLUGIN_INSTALLATION)
void PluginObserver::OnFindMissingPlugin(int placeholder_id,
                                         const std::string& mime_type) {
  std::string lang = "en-US";  // Oh yes.
  scoped_ptr<PluginMetadata> plugin_metadata;
  PluginInstaller* installer = NULL;
  bool found_plugin = PluginFinder::GetInstance()->FindPlugin(
      mime_type, lang, &installer, &plugin_metadata);
  if (!found_plugin) {
    Send(new ChromeViewMsg_DidNotFindMissingPlugin(placeholder_id));
    return;
  }
  DCHECK(installer);
  DCHECK(plugin_metadata.get());

  plugin_placeholders_[placeholder_id] =
      new PluginPlaceholderHost(this, placeholder_id,
                                plugin_metadata->name(),
                                installer);
  PluginInstallerInfoBarDelegate::InstallCallback callback =
      base::Bind(&PluginObserver::InstallMissingPlugin,
                 weak_ptr_factory_.GetWeakPtr(), installer);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  InfoBarDelegate* delegate;
#if !defined(OS_WIN)
  delegate = PluginInstallerInfoBarDelegate::Create(
      infobar_service, installer, plugin_metadata.Pass(), callback);
#else
  delegate = base::win::IsMetroProcess() ?
      new PluginMetroModeInfoBarDelegate(
          infobar_service,
          PluginMetroModeInfoBarDelegate::MISSING_PLUGIN,
          plugin_metadata->name()) :
      PluginInstallerInfoBarDelegate::Create(
          infobar_service, installer, plugin_metadata.Pass(), callback);
#endif
  infobar_service->AddInfoBar(delegate);
}

void PluginObserver::InstallMissingPlugin(
    PluginInstaller* installer,
    const PluginMetadata* plugin_metadata) {
  if (plugin_metadata->url_for_display()) {
    installer->OpenDownloadURL(plugin_metadata->plugin_url(), web_contents());
  } else {
    TabModalConfirmDialog::Create(
        new ConfirmInstallDialogDelegate(
            web_contents(), installer, plugin_metadata->Clone()),
        web_contents());
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
      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_AUTO_BOOKMARK, false));
}

void PluginObserver::OnCouldNotLoadPlugin(const FilePath& plugin_path) {
  g_browser_process->metrics_service()->LogPluginLoadingError(plugin_path);
  string16 plugin_name =
      PluginService::GetInstance()->GetPluginDisplayNameByPath(plugin_path);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  infobar_service->AddInfoBar(new SimpleAlertInfoBarDelegate(
      infobar_service,
      &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_INFOBAR_PLUGIN_CRASHED),
      l10n_util::GetStringFUTF16(IDS_PLUGIN_INITIALIZATION_ERROR_PROMPT,
                                 plugin_name),
      true  /* auto_expire */));
}

void PluginObserver::OnNPAPINotSupported(const std::string& identifier) {
#if defined(OS_WIN) && defined(ENABLE_PLUGIN_INSTALLATION)
  DCHECK(base::win::IsMetroProcess());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return;
  HostContentSettingsMap* content_settings =
      profile->GetHostContentSettingsMap();
  if (content_settings->GetContentSetting(
      web_contents()->GetURL(),
      web_contents()->GetURL(),
      CONTENT_SETTINGS_TYPE_METRO_SWITCH_TO_DESKTOP,
      std::string()) == CONTENT_SETTING_BLOCK)
    return;

  scoped_ptr<PluginMetadata> plugin;
  if (!PluginFinder::GetInstance()->FindPluginWithIdentifier(
          identifier, NULL, &plugin)) {
    NOTREACHED();
    return;
  }

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  infobar_service->AddInfoBar(
      new PluginMetroModeInfoBarDelegate(
          infobar_service,
          PluginMetroModeInfoBarDelegate::DESKTOP_MODE_REQUIRED,
          plugin->name()));
#else
  NOTREACHED();
#endif
}
