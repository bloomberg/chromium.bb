// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_screensaver.h"

#include "ash/screensaver/screensaver_view.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/policy/app_pack_updater.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_garbage_collector_chromeos.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/sandboxed_unpacker.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chromeos/login/login_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "ui/wm/core/user_activity_detector.h"

using extensions::Extension;
using extensions::ExtensionGarbageCollectorChromeOS;
using extensions::SandboxedUnpacker;

namespace chromeos {

namespace {

ExtensionService* GetDefaultExtensionService() {
  Profile* default_profile = ProfileHelper::GetSigninProfile();
  if (!default_profile)
    return NULL;
  return extensions::ExtensionSystem::Get(
      default_profile)->extension_service();
}

ExtensionGarbageCollectorChromeOS* GetDefaultExtensionGarbageCollector() {
  Profile* default_profile = ProfileHelper::GetSigninProfile();
  if (!default_profile)
    return NULL;
  return ExtensionGarbageCollectorChromeOS::Get(default_profile);
}

typedef base::Callback<void(
    scoped_refptr<Extension>,
    const base::FilePath&)> UnpackCallback;

class ScreensaverUnpackerClient
    : public extensions::SandboxedUnpackerClient {
 public:
  ScreensaverUnpackerClient(const base::FilePath& crx_path,
                            const UnpackCallback& unpacker_callback)
      : crx_path_(crx_path),
        unpack_callback_(unpacker_callback) {}

  virtual void OnUnpackSuccess(const base::FilePath& temp_dir,
                               const base::FilePath& extension_root,
                               const base::DictionaryValue* original_manifest,
                               const Extension* extension,
                               const SkBitmap& install_icon) OVERRIDE;
  virtual void OnUnpackFailure(const base::string16& error) OVERRIDE;

 protected:
  virtual ~ScreensaverUnpackerClient() {}

 private:
  void LoadScreensaverExtension(
      const base::FilePath& extension_base_path,
      const base::FilePath& screensaver_extension_path);

  void NotifyAppPackOfDamagedFile();

  base::FilePath crx_path_;
  UnpackCallback unpack_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScreensaverUnpackerClient);
};

void ScreensaverUnpackerClient::OnUnpackSuccess(
    const base::FilePath& temp_dir,
    const base::FilePath& extension_root,
    const base::DictionaryValue* original_manifest,
    const Extension* extension,
    const SkBitmap& install_icon) {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ScreensaverUnpackerClient::LoadScreensaverExtension,
                 this,
                 temp_dir,
                 extension_root));
}

void ScreensaverUnpackerClient::OnUnpackFailure(const base::string16& error) {
  LOG(ERROR) << "Couldn't unpack screensaver extension. Error: " << error;
  NotifyAppPackOfDamagedFile();
}

void ScreensaverUnpackerClient::LoadScreensaverExtension(
    const base::FilePath& extension_base_path,
    const base::FilePath& screensaver_extension_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  // TODO(rkc): This is a HACK, please remove this method from extension
  // service once this code is deprecated. See crbug.com/280363
  ExtensionGarbageCollectorChromeOS* gc = GetDefaultExtensionGarbageCollector();
  if (gc)
    gc->disable_garbage_collection();

  std::string error;
  scoped_refptr<Extension> screensaver_extension =
      extensions::file_util::LoadExtension(screensaver_extension_path,
                                           extensions::Manifest::COMPONENT,
                                           Extension::NO_FLAGS,
                                           &error);
  if (!screensaver_extension.get()) {
    LOG(ERROR) << "Could not load screensaver extension from: "
               << screensaver_extension_path.value() << " due to: " << error;
    NotifyAppPackOfDamagedFile();
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          unpack_callback_,
          screensaver_extension,
          extension_base_path));
}

void ScreensaverUnpackerClient::NotifyAppPackOfDamagedFile() {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&ScreensaverUnpackerClient::NotifyAppPackOfDamagedFile,
                   this));
    return;
  }

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::AppPackUpdater* updater = connector->GetAppPackUpdater();
  if (updater)
    updater->OnDamagedFileDetected(crx_path_);
}

}  // namespace

KioskModeScreensaver::KioskModeScreensaver()
    : weak_ptr_factory_(this) {
  chromeos::KioskModeSettings* kiosk_mode_settings =
      chromeos::KioskModeSettings::Get();

  if (kiosk_mode_settings->is_initialized()) {
    GetScreensaverCrxPath();
  } else {
    kiosk_mode_settings->Initialize(base::Bind(
        &KioskModeScreensaver::GetScreensaverCrxPath,
        weak_ptr_factory_.GetWeakPtr()));
  }
}

KioskModeScreensaver::~KioskModeScreensaver() {
  // If we are shutting down the system might already be gone and we shouldn't
  // do anything (see crbug.com/288216).
  if (!g_browser_process || g_browser_process->IsShuttingDown())
    return;

  // If the extension was unpacked.
  if (!extension_base_path_.empty()) {
    // TODO(rkc): This is a HACK, please remove this method from extension
    // service once this code is deprecated. See crbug.com/280363
    ExtensionGarbageCollectorChromeOS* gc =
        GetDefaultExtensionGarbageCollector();
    if (gc)
      gc->enable_garbage_collection();

    // Delete it.
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE,
        FROM_HERE,
        base::Bind(
            &extensions::file_util::DeleteFile, extension_base_path_, true));
  }

  // In case we're shutting down without ever triggering the active
  // notification and/or logging in.
  if (ash::Shell::GetInstance() &&
      ash::Shell::GetInstance()->user_activity_detector() &&
      ash::Shell::GetInstance()->user_activity_detector()->HasObserver(this))
    ash::Shell::GetInstance()->user_activity_detector()->RemoveObserver(this);
}

void KioskModeScreensaver::GetScreensaverCrxPath() {
  chromeos::KioskModeSettings::Get()->GetScreensaverPath(
      base::Bind(&KioskModeScreensaver::ScreensaverPathCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void KioskModeScreensaver::ScreensaverPathCallback(
    const base::FilePath& screensaver_crx) {
  if (screensaver_crx.empty())
    return;

  ExtensionService* extension_service = GetDefaultExtensionService();
  if (!extension_service)
    return;
  base::FilePath extensions_dir = extension_service->install_directory();
  scoped_refptr<SandboxedUnpacker> screensaver_unpacker(
      new SandboxedUnpacker(
          screensaver_crx,
          extensions::Manifest::COMPONENT,
          Extension::NO_FLAGS,
          extensions_dir,
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::FILE).get(),
          new ScreensaverUnpackerClient(
              screensaver_crx,
              base::Bind(
                  &KioskModeScreensaver::SetupScreensaver,
                  weak_ptr_factory_.GetWeakPtr()))));

  // Fire off the unpacker on the file thread; don't need it to return.
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &SandboxedUnpacker::Start, screensaver_unpacker.get()));
}

void KioskModeScreensaver::SetupScreensaver(
    scoped_refptr<Extension> extension,
    const base::FilePath& extension_base_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  extension_base_path_ = extension_base_path;

  // If the user is already logged in, don't need to display the screensaver.
  if (chromeos::LoginState::Get()->IsUserLoggedIn())
    return;

  ash::Shell::GetInstance()->user_activity_detector()->AddObserver(this);

  ExtensionService* extension_service = GetDefaultExtensionService();
  // Add the extension to the extension service and display the screensaver.
  if (extension_service) {
    extension_service->AddExtension(extension.get());
    ash::ShowScreensaver(
        extensions::AppLaunchInfo::GetFullLaunchURL(extension.get()));
  } else {
    LOG(ERROR) << "Couldn't get extension system. Unable to load screensaver!";
    ShutdownKioskModeScreensaver();
  }
}

void KioskModeScreensaver::OnUserActivity(const ui::Event* event) {
  // We don't want to handle further user notifications; we'll either login
  // the user and close out or or at least close the screensaver.
  ash::Shell::GetInstance()->user_activity_detector()->RemoveObserver(this);

  // Find the retail mode login page.
  if (LoginDisplayHostImpl::default_host()) {
    LoginDisplayHostImpl* webui_host =
        static_cast<LoginDisplayHostImpl*>(
            LoginDisplayHostImpl::default_host());
    OobeUI* oobe_ui = webui_host->GetOobeUI();

    // Show the login spinner.
    if (oobe_ui)
      oobe_ui->ShowRetailModeLoginSpinner();

    // Close the screensaver, our login spinner is already showing.
    ash::CloseScreensaver();

    // Log us in.
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    if (controller && !chromeos::LoginState::Get()->IsUserLoggedIn())
      controller->LoginAsRetailModeUser();
  } else {
    // No default host for the WebUiLoginDisplay means that we're already in the
    // process of logging in - shut down screensaver and do nothing else.
    ash::CloseScreensaver();
  }

  ShutdownKioskModeScreensaver();
}

static KioskModeScreensaver* g_kiosk_mode_screensaver = NULL;

void InitializeKioskModeScreensaver() {
  if (g_kiosk_mode_screensaver) {
    LOG(WARNING) << "Screensaver was already initialized";
    return;
  }

  g_kiosk_mode_screensaver = new KioskModeScreensaver();
}

void ShutdownKioskModeScreensaver() {
  delete g_kiosk_mode_screensaver;
  g_kiosk_mode_screensaver = NULL;
}

}  // namespace chromeos
