// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_observer.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/infobars/simple_alert_infobar_delegate.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/metrics_services_manager.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_infobar_delegates.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/webplugininfo.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#if defined(OS_WIN)
#include "base/win/metro.h"
#endif
#include "chrome/browser/plugins/plugin_installer.h"
#include "chrome/browser/plugins/plugin_installer_observer.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

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
  virtual base::string16 GetTitle() OVERRIDE;
  virtual base::string16 GetDialogMessage() OVERRIDE;
  virtual base::string16 GetAcceptButtonTitle() OVERRIDE;
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

base::string16 ConfirmInstallDialogDelegate::GetTitle() {
  return l10n_util::GetStringFUTF16(
      IDS_PLUGIN_CONFIRM_INSTALL_DIALOG_TITLE, plugin_metadata_->name());
}

base::string16 ConfirmInstallDialogDelegate::GetDialogMessage() {
  return l10n_util::GetStringFUTF16(IDS_PLUGIN_CONFIRM_INSTALL_DIALOG_MSG,
                                    plugin_metadata_->name());
}

base::string16 ConfirmInstallDialogDelegate::GetAcceptButtonTitle() {
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

// ReloadPluginInfoBarDelegate -------------------------------------------------

class ReloadPluginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static void Create(InfoBarService* infobar_service,
                     content::NavigationController* controller,
                     const base::string16& message);

 private:
  ReloadPluginInfoBarDelegate(content::NavigationController* controller,
                              const base::string16& message);
  virtual ~ReloadPluginInfoBarDelegate();

  // ConfirmInfobarDelegate:
  virtual int GetIconID() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  content::NavigationController* controller_;
  base::string16 message_;
};

// static
void ReloadPluginInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    content::NavigationController* controller,
    const base::string16& message) {
  infobar_service->AddInfoBar(
      ConfirmInfoBarDelegate::CreateInfoBar(scoped_ptr<ConfirmInfoBarDelegate>(
          new ReloadPluginInfoBarDelegate(controller, message))));
}

ReloadPluginInfoBarDelegate::ReloadPluginInfoBarDelegate(
    content::NavigationController* controller,
    const base::string16& message)
    : controller_(controller),
      message_(message) {}

ReloadPluginInfoBarDelegate::~ReloadPluginInfoBarDelegate(){ }

int ReloadPluginInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_PLUGIN_CRASHED;
}

base::string16 ReloadPluginInfoBarDelegate::GetMessageText() const {
  return message_;
}

int ReloadPluginInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 ReloadPluginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_RELOAD_PAGE_WITH_PLUGIN);
}

bool ReloadPluginInfoBarDelegate::Accept() {
  controller_->Reload(true);
  return true;
}

}  // namespace

// PluginObserver -------------------------------------------------------------

#if defined(ENABLE_PLUGIN_INSTALLATION)
class PluginObserver::PluginPlaceholderHost : public PluginInstallerObserver {
 public:
  PluginPlaceholderHost(PluginObserver* observer,
                        int routing_id,
                        base::string16 plugin_name,
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
      weak_ptr_factory_(this) {
}

PluginObserver::~PluginObserver() {
#if defined(ENABLE_PLUGIN_INSTALLATION)
  STLDeleteValues(&plugin_placeholders_);
#endif
}

void PluginObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
#if defined(OS_WIN)
  // If the window belongs to the Ash desktop, before we navigate we need
  // to tell the renderview that NPAPI plugins are not supported so it does
  // not try to instantiate them. The final decision is actually done in
  // the IO thread by PluginInfoMessageFilter of this proces,s but it's more
  // complex to manage a map of Ash views in PluginInfoMessageFilter than
  // just telling the renderer via IPC.

  // TODO(shrikant): Implement solution which will help associate
  // render_view_host/webcontents/view/window instance with host desktop.
  // Refer to issue http://crbug.com/317940.
  // When non-active tabs are restored they are not added in view/window parent
  // hierarchy (chrome::CreateRestoredTab/CreateParams). Normally we traverse
  // parent hierarchy to identify containing desktop (like in function
  // chrome::GetHostDesktopTypeForNativeView).
  // Possible issue with chrome::GetActiveDesktop, is that it's global
  // state, which remembers last active desktop, which may break in scenarios
  // where we have instances on both Ash and Native desktop.

  // We will do both tests. Both have some factor of unreliability.
  aura::Window* window = web_contents()->GetNativeView();
  if (chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_ASH ||
      chrome::GetHostDesktopTypeForNativeView(window) ==
      chrome::HOST_DESKTOP_TYPE_ASH) {
    int routing_id = render_frame_host->GetRoutingID();
    render_frame_host->Send(new ChromeViewMsg_NPAPINotSupported(routing_id));
  }
#endif
}

void PluginObserver::PluginCrashed(const base::FilePath& plugin_path,
                                   base::ProcessId plugin_pid) {
  DCHECK(!plugin_path.value().empty());

  base::string16 plugin_name =
      PluginService::GetInstance()->GetPluginDisplayNameByPath(plugin_path);
  base::string16 infobar_text;
#if defined(OS_WIN)
  // Find out whether the plugin process is still alive.
  // Note: Although the chances are slim, it is possible that after the plugin
  // process died, |plugin_pid| has been reused by a new process. The
  // consequence is that we will display |IDS_PLUGIN_DISCONNECTED_PROMPT| rather
  // than |IDS_PLUGIN_CRASHED_PROMPT| to the user, which seems acceptable.
  base::ProcessHandle plugin_handle = base::kNullProcessHandle;
  bool open_result = base::OpenProcessHandleWithAccess(
      plugin_pid, PROCESS_QUERY_INFORMATION | SYNCHRONIZE, &plugin_handle);
  bool is_running = false;
  if (open_result) {
    is_running = base::GetTerminationStatus(plugin_handle, NULL) ==
        base::TERMINATION_STATUS_STILL_RUNNING;
    base::CloseProcessHandle(plugin_handle);
  }

  if (is_running) {
    infobar_text = l10n_util::GetStringFUTF16(IDS_PLUGIN_DISCONNECTED_PROMPT,
                                              plugin_name);
    UMA_HISTOGRAM_COUNTS("Plugin.ShowDisconnectedInfobar", 1);
  } else {
    infobar_text = l10n_util::GetStringFUTF16(IDS_PLUGIN_CRASHED_PROMPT,
                                              plugin_name);
    UMA_HISTOGRAM_COUNTS("Plugin.ShowCrashedInfobar", 1);
  }
#else
  // Calling the POSIX version of base::GetTerminationStatus() may affect other
  // code which is interested in the process termination status. (Please see the
  // comment of the function.) Therefore, a better way is needed to distinguish
  // disconnections from crashes.
  infobar_text = l10n_util::GetStringFUTF16(IDS_PLUGIN_CRASHED_PROMPT,
                                            plugin_name);
  UMA_HISTOGRAM_COUNTS("Plugin.ShowCrashedInfobar", 1);
#endif

  ReloadPluginInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents()),
      &web_contents()->GetController(),
      infobar_text);
}

bool PluginObserver::OnMessageReceived(
      const IPC::Message& message,
      content::RenderFrameHost* render_frame_host) {
  IPC_BEGIN_MESSAGE_MAP(PluginObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_BlockedOutdatedPlugin,
                        OnBlockedOutdatedPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_BlockedUnauthorizedPlugin,
                        OnBlockedUnauthorizedPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_NPAPINotSupported,
                        OnNPAPINotSupported)
#if defined(ENABLE_PLUGIN_INSTALLATION)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FindMissingPlugin,
                        OnFindMissingPlugin)
#endif

    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()

  return true;
}

bool PluginObserver::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PluginObserver, message)
#if defined(ENABLE_PLUGIN_INSTALLATION)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RemovePluginPlaceholderHost,
                        OnRemovePluginPlaceholderHost)
#endif
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_OpenAboutPlugins,
                        OnOpenAboutPlugins)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CouldNotLoadPlugin,
                        OnCouldNotLoadPlugin)

    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()

  return true;
}

void PluginObserver::OnBlockedUnauthorizedPlugin(
    const base::string16& name,
    const std::string& identifier) {
  UnauthorizedPluginInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents()),
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())->
          GetHostContentSettingsMap(),
      name, identifier);
}

void PluginObserver::OnBlockedOutdatedPlugin(int placeholder_id,
                                             const std::string& identifier) {
#if defined(ENABLE_PLUGIN_INSTALLATION)
  PluginFinder* finder = PluginFinder::GetInstance();
  // Find plugin to update.
  PluginInstaller* installer = NULL;
  scoped_ptr<PluginMetadata> plugin;
  if (finder->FindPluginWithIdentifier(identifier, &installer, &plugin)) {
    plugin_placeholders_[placeholder_id] = new PluginPlaceholderHost(
        this, placeholder_id, plugin->name(), installer);
    OutdatedPluginInfoBarDelegate::Create(InfoBarService::FromWebContents(
        web_contents()), installer, plugin.Pass());
  } else {
    NOTREACHED();
  }
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
      new PluginPlaceholderHost(this, placeholder_id, plugin_metadata->name(),
                                installer);
  PluginInstallerInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents()), installer,
      plugin_metadata.Pass(),
      base::Bind(&PluginObserver::InstallMissingPlugin,
                 weak_ptr_factory_.GetWeakPtr(), installer));
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
      GURL(chrome::kChromeUIPluginsURL),
      content::Referrer(web_contents()->GetURL(),
                        blink::WebReferrerPolicyDefault),
      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_AUTO_BOOKMARK, false));
}

void PluginObserver::OnCouldNotLoadPlugin(const base::FilePath& plugin_path) {
  g_browser_process->GetMetricsServicesManager()->OnPluginLoadingError(
      plugin_path);
  base::string16 plugin_name =
      PluginService::GetInstance()->GetPluginDisplayNameByPath(plugin_path);
  SimpleAlertInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents()),
      IDR_INFOBAR_PLUGIN_CRASHED,
      l10n_util::GetStringFUTF16(IDS_PLUGIN_INITIALIZATION_ERROR_PROMPT,
                                 plugin_name),
      true);
}

void PluginObserver::OnNPAPINotSupported(const std::string& identifier) {
#if defined(OS_WIN) && defined(ENABLE_PLUGIN_INSTALLATION)
#if !defined(USE_AURA)
  DCHECK(base::win::IsMetroProcess());
#endif

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
  bool ret = PluginFinder::GetInstance()->FindPluginWithIdentifier(
      identifier, NULL, &plugin);
  DCHECK(ret);

  PluginMetroModeInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents()),
      PluginMetroModeInfoBarDelegate::DESKTOP_MODE_REQUIRED, plugin->name());
#endif
}
