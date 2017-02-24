// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_observer.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/plugins/flash_download_interception.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_infobar_delegates.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/common/features.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/component_updater/component_updater_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/webplugininfo.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugins/plugin_installer.h"
#include "chrome/browser/plugins/plugin_installer_observer.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#endif  // BUILDFLAG(ENABLE_PLUGIN_INSTALLATION)

using content::PluginService;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PluginObserver);

namespace {

#if BUILDFLAG(ENABLE_PLUGIN_INSTALLATION)

// ConfirmInstallDialogDelegate ------------------------------------------------

class ConfirmInstallDialogDelegate : public TabModalConfirmDialogDelegate,
                                     public WeakPluginInstallerObserver {
 public:
  ConfirmInstallDialogDelegate(content::WebContents* web_contents,
                               PluginInstaller* installer,
                               std::unique_ptr<PluginMetadata> plugin_metadata);

  // TabModalConfirmDialogDelegate methods:
  base::string16 GetTitle() override;
  base::string16 GetDialogMessage() override;
  base::string16 GetAcceptButtonTitle() override;
  void OnAccepted() override;
  void OnCanceled() override;

  // WeakPluginInstallerObserver methods:
  void DownloadStarted() override;
  void OnlyWeakObserversLeft() override;

 private:
  content::WebContents* web_contents_;
  std::unique_ptr<PluginMetadata> plugin_metadata_;
};

ConfirmInstallDialogDelegate::ConfirmInstallDialogDelegate(
    content::WebContents* web_contents,
    PluginInstaller* installer,
    std::unique_ptr<PluginMetadata> plugin_metadata)
    : TabModalConfirmDialogDelegate(web_contents),
      WeakPluginInstallerObserver(installer),
      web_contents_(web_contents),
      plugin_metadata_(std::move(plugin_metadata)) {}

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
#endif  // BUILDFLAG(ENABLE_PLUGIN_INSTALLATION)

// ReloadPluginInfoBarDelegate -------------------------------------------------

class ReloadPluginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static void Create(InfoBarService* infobar_service,
                     content::NavigationController* controller,
                     const base::string16& message);

 private:
  ReloadPluginInfoBarDelegate(content::NavigationController* controller,
                              const base::string16& message);
  ~ReloadPluginInfoBarDelegate() override;

  // ConfirmInfobarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

  content::NavigationController* controller_;
  base::string16 message_;
};

// static
void ReloadPluginInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    content::NavigationController* controller,
    const base::string16& message) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new ReloadPluginInfoBarDelegate(controller, message))));
}

ReloadPluginInfoBarDelegate::ReloadPluginInfoBarDelegate(
    content::NavigationController* controller,
    const base::string16& message)
    : controller_(controller),
      message_(message) {}

ReloadPluginInfoBarDelegate::~ReloadPluginInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
ReloadPluginInfoBarDelegate::GetIdentifier() const {
  return RELOAD_PLUGIN_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& ReloadPluginInfoBarDelegate::GetVectorIcon() const {
  return kExtensionCrashedIcon;
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
  controller_->Reload(content::ReloadType::NORMAL, true);
  return true;
}

}  // namespace

// PluginObserver -------------------------------------------------------------

#if BUILDFLAG(ENABLE_PLUGIN_INSTALLATION)
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
  void DownloadStarted() override {
    observer_->Send(new ChromeViewMsg_StartedDownloadingPlugin(routing_id_));
  }

  void DownloadError(const std::string& msg) override {
    observer_->Send(new ChromeViewMsg_ErrorDownloadingPlugin(routing_id_, msg));
  }

  void DownloadCancelled() override {
    observer_->Send(new ChromeViewMsg_CancelledDownloadingPlugin(routing_id_));
  }

  void DownloadFinished() override {
    observer_->Send(new ChromeViewMsg_FinishedDownloadingPlugin(routing_id_));
  }

 private:
  // Weak pointer; owns us.
  PluginObserver* observer_;

  int routing_id_;
};
#endif  // BUILDFLAG(ENABLE_PLUGIN_INSTALLATION)

class PluginObserver::ComponentObserver
    : public update_client::UpdateClient::Observer {
 public:
  using Events = update_client::UpdateClient::Observer::Events;
  ComponentObserver(PluginObserver* observer,
                    int routing_id,
                    const std::string& component_id)
      : observer_(observer),
        routing_id_(routing_id),
        component_id_(component_id) {
    g_browser_process->component_updater()->AddObserver(this);
  }

  ~ComponentObserver() override {
    g_browser_process->component_updater()->RemoveObserver(this);
  }

  void OnEvent(Events event, const std::string& id) override {
    if (id != component_id_)
      return;
    switch (event) {
      case Events::COMPONENT_UPDATED:
        observer_->Send(
            new ChromeViewMsg_PluginComponentUpdateSuccess(routing_id_));
        observer_->RemoveComponentObserver(routing_id_);
        break;
      case Events::COMPONENT_UPDATE_FOUND:
        observer_->Send(
            new ChromeViewMsg_PluginComponentUpdateDownloading(routing_id_));
        break;
      case Events::COMPONENT_NOT_UPDATED:
        observer_->Send(
            new ChromeViewMsg_PluginComponentUpdateFailure(routing_id_));
        observer_->RemoveComponentObserver(routing_id_);
        break;
      default:
        // No message to send.
        break;
    }
  }

 private:
  PluginObserver* observer_;
  int routing_id_;
  std::string component_id_;
  DISALLOW_COPY_AND_ASSIGN(ComponentObserver);
};

PluginObserver::PluginObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      weak_ptr_factory_(this) {
}

PluginObserver::~PluginObserver() {
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
  base::Process plugin_process =
      base::Process::OpenWithAccess(plugin_pid,
                                    PROCESS_QUERY_INFORMATION | SYNCHRONIZE);
  bool is_running = false;
  if (plugin_process.IsValid()) {
    is_running =
        base::GetTerminationStatus(plugin_process.Handle(), NULL) ==
            base::TERMINATION_STATUS_STILL_RUNNING;
    plugin_process.Close();
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
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_BlockedComponentUpdatedPlugin,
                        OnBlockedComponentUpdatedPlugin)
#if BUILDFLAG(ENABLE_PLUGIN_INSTALLATION)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RemovePluginPlaceholderHost,
                        OnRemovePluginPlaceholderHost)
#endif
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ShowFlashPermissionBubble,
                        OnShowFlashPermissionBubble)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CouldNotLoadPlugin,
                        OnCouldNotLoadPlugin)

    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()

  return true;
}

void PluginObserver::OnBlockedOutdatedPlugin(int placeholder_id,
                                             const std::string& identifier) {
#if BUILDFLAG(ENABLE_PLUGIN_INSTALLATION)
  PluginFinder* finder = PluginFinder::GetInstance();
  // Find plugin to update.
  PluginInstaller* installer = NULL;
  std::unique_ptr<PluginMetadata> plugin;
  if (finder->FindPluginWithIdentifier(identifier, &installer, &plugin)) {
    plugin_placeholders_[placeholder_id] =
        base::MakeUnique<PluginPlaceholderHost>(this, placeholder_id,
                                                plugin->name(), installer);
    OutdatedPluginInfoBarDelegate::Create(
        InfoBarService::FromWebContents(web_contents()), installer,
        std::move(plugin));
  } else {
    NOTREACHED();
  }
#else
  // If we don't support third-party plugin installation, we shouldn't have
  // outdated plugins.
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_PLUGIN_INSTALLATION)
}

void PluginObserver::OnBlockedComponentUpdatedPlugin(
    int placeholder_id,
    const std::string& identifier) {
  component_observers_[placeholder_id] =
      base::MakeUnique<ComponentObserver>(this, placeholder_id, identifier);
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      identifier, component_updater::Callback());
}

void PluginObserver::RemoveComponentObserver(int placeholder_id) {
  auto it = component_observers_.find(placeholder_id);
  DCHECK(it != component_observers_.end());
  component_observers_.erase(it);
}

#if BUILDFLAG(ENABLE_PLUGIN_INSTALLATION)
void PluginObserver::OnRemovePluginPlaceholderHost(int placeholder_id) {
  auto it = plugin_placeholders_.find(placeholder_id);
  if (it == plugin_placeholders_.end()) {
    NOTREACHED();
    return;
  }
  plugin_placeholders_.erase(it);
}
#endif  // BUILDFLAG(ENABLE_PLUGIN_INSTALLATION)

void PluginObserver::OnShowFlashPermissionBubble() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  FlashDownloadInterception::InterceptFlashDownloadNavigation(
      web_contents(), web_contents()->GetLastCommittedURL());
}

void PluginObserver::OnCouldNotLoadPlugin(const base::FilePath& plugin_path) {
  g_browser_process->GetMetricsServicesManager()->OnPluginLoadingError(
      plugin_path);
  base::string16 plugin_name =
      PluginService::GetInstance()->GetPluginDisplayNameByPath(plugin_path);
  SimpleAlertInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents()),
      infobars::InfoBarDelegate::PLUGIN_OBSERVER, &kExtensionCrashedIcon,
      l10n_util::GetStringFUTF16(IDS_PLUGIN_INITIALIZATION_ERROR_PROMPT,
                                 plugin_name),
      true);
}
