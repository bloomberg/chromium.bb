// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_screensaver.h"

#include "ash/screensaver/screensaver_view.h"
#include "ash/shell.h"
#include "ash/wm/user_activity_detector.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_login_display_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/sandboxed_unpacker.h"
#include "chrome/browser/policy/app_pack_updater.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using extensions::Extension;
using extensions::SandboxedUnpacker;

namespace chromeos {

namespace {

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
                               const Extension* extension) OVERRIDE;
  virtual void OnUnpackFailure(const string16& error) OVERRIDE;

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
    const Extension* extension) {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ScreensaverUnpackerClient::LoadScreensaverExtension,
                 this,
                 temp_dir,
                 extension_root));
}

void ScreensaverUnpackerClient::OnUnpackFailure(const string16& error) {
  LOG(ERROR) << "Couldn't unpack screensaver extension. Error: " << error;
  NotifyAppPackOfDamagedFile();
}

void ScreensaverUnpackerClient::LoadScreensaverExtension(
    const base::FilePath& extension_base_path,
    const base::FilePath& screensaver_extension_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  std::string error;
  scoped_refptr<Extension> screensaver_extension =
      extension_file_util::LoadExtension(screensaver_extension_path,
                                         extensions::Manifest::COMPONENT,
                                         Extension::NO_FLAGS,
                                         &error);
  if (!screensaver_extension) {
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

  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  policy::AppPackUpdater* updater = connector->GetAppPackUpdater();
  if (updater)
    updater->OnDamagedFileDetected(crx_path_);
}

}  // namespace

KioskModeScreensaver::KioskModeScreensaver()
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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
  // If the extension was unpacked.
  if (!extension_base_path_.empty()) {
    // Delete it.
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE,
        FROM_HERE,
        base::Bind(
            &extension_file_util::DeleteFile, extension_base_path_, true));
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

  Profile* default_profile = ProfileManager::GetDefaultProfile();
  if (!default_profile)
    return;
  base::FilePath extensions_dir = extensions::ExtensionSystem::Get(
      default_profile)->extension_service()->install_directory();
  scoped_refptr<SandboxedUnpacker> screensaver_unpacker(
      new SandboxedUnpacker(
          screensaver_crx,
          true,
          extensions::Manifest::COMPONENT,
          Extension::NO_FLAGS,
          extensions_dir,
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::FILE),
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
  if (chromeos::UserManager::Get()->IsUserLoggedIn())
    return;

  ash::Shell::GetInstance()->user_activity_detector()->AddObserver(this);

  Profile* default_profile = ProfileManager::GetDefaultProfile();
  // Add the extension to the extension service and display the screensaver.
  if (default_profile) {
    extensions::ExtensionSystem::Get(default_profile)->extension_service()->
        AddExtension(extension);
    ash::ShowScreensaver(extension->GetFullLaunchURL());
  } else {
    LOG(ERROR) << "Couldn't get default profile. Unable to load screensaver!";
    ShutdownKioskModeScreensaver();
  }
}

void KioskModeScreensaver::OnUserActivity() {
  // We don't want to handle further user notifications; we'll either login
  // the user and close out or or at least close the screensaver.
  ash::Shell::GetInstance()->user_activity_detector()->RemoveObserver(this);

  // Find the retail mode login page.
  if (WebUILoginDisplayHost::default_host()) {
    WebUILoginDisplayHost* webui_host =
        static_cast<WebUILoginDisplayHost*>(
            WebUILoginDisplayHost::default_host());
    OobeUI* oobe_ui = webui_host->GetOobeUI();

    // Show the login spinner.
    if (oobe_ui)
      oobe_ui->ShowRetailModeLoginSpinner();

    // Close the screensaver, our login spinner is already showing.
    ash::CloseScreensaver();

    // Log us in.
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    if (controller && !chromeos::UserManager::Get()->IsUserLoggedIn())
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
