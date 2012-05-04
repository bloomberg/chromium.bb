// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_screensaver.h"

#include "ash/screensaver/screensaver_view.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/sandboxed_extension_unpacker.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

typedef base::Callback<void(
    scoped_refptr<Extension>,
    Profile*,
    const FilePath&)> UnpackCallback;

class ScreensaverUnpackerClient : public SandboxedExtensionUnpackerClient {
 public:
  explicit ScreensaverUnpackerClient(const UnpackCallback& unpacker_callback)
      : unpack_callback_(unpacker_callback) {}

  void OnUnpackSuccess(const FilePath& temp_dir,
                       const FilePath& extension_root,
                       const base::DictionaryValue* original_manifest,
                       const Extension* extension) OVERRIDE;
  void OnUnpackFailure(const string16& error) OVERRIDE;

 private:
  void LoadScreensaverExtension(
      const FilePath& extension_base_path,
      const FilePath& screensaver_extension_path);

  UnpackCallback unpack_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScreensaverUnpackerClient);
};

void ScreensaverUnpackerClient::OnUnpackSuccess(
    const FilePath& temp_dir,
    const FilePath& extension_root,
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
}

void ScreensaverUnpackerClient::LoadScreensaverExtension(
    const FilePath& extension_base_path,
    const FilePath& screensaver_extension_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  std::string error;
  scoped_refptr<Extension> screensaver_extension =
      extension_file_util::LoadExtension(screensaver_extension_path,
                                         Extension::COMPONENT,
                                         Extension::NO_FLAGS,
                                         &error);
  if (!screensaver_extension) {
    LOG(ERROR) << "Could not load screensaver extension from: "
               << screensaver_extension_path.value() << " due to: " << error;
    return;
  }

  Profile* default_profile = ProfileManager::GetDefaultProfile();
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          unpack_callback_,
          screensaver_extension,
          default_profile,
          extension_base_path));
}

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
  chromeos::PowerManagerClient* power_manager =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
  if (power_manager->HasObserver(this))
    power_manager->RemoveObserver(this);
}

void KioskModeScreensaver::GetScreensaverCrxPath() {
  chromeos::KioskModeSettings::Get()->GetScreensaverPath(
      base::Bind(&KioskModeScreensaver::ScreensaverPathCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void KioskModeScreensaver::ScreensaverPathCallback(
    const FilePath& screensaver_crx) {
  if (screensaver_crx.empty())
    return;

  scoped_refptr<SandboxedExtensionUnpacker> screensaver_unpacker(
      new SandboxedExtensionUnpacker(
          screensaver_crx,
          true,
          Extension::COMPONENT,
          Extension::NO_FLAGS,
          new ScreensaverUnpackerClient(base::Bind(
              &KioskModeScreensaver::SetupScreensaver,
              weak_ptr_factory_.GetWeakPtr()))));

  // Fire off the unpacker on the file thread; don't need it to return.
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &SandboxedExtensionUnpacker::Start, screensaver_unpacker.get()));
}

void KioskModeScreensaver::SetupScreensaver(
    scoped_refptr<Extension> extension,
    Profile* default_profile,
    const FilePath& extension_base_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  extension_base_path_ = extension_base_path;

  // If the user is already logged in, don't need to display the screensaver.
  if (chromeos::UserManager::Get()->IsUserLoggedIn())
    return;

  registrar_.Add(this, chrome::NOTIFICATION_SESSION_STARTED,
                 content::NotificationService::AllSources());

  // We will register ourselves now and unregister if a user logs in.
  chromeos::PowerManagerClient* power_manager =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
  if (!power_manager->HasObserver(this))
    power_manager->AddObserver(this);

  // We need to disappear and login the demo user if we go active.
  chromeos::DBusThreadManager::Get()->
      GetPowerManagerClient()->RequestActiveNotification();

  // Add the extension to the extension service and display the screensaver.
  if (default_profile) {
    default_profile->GetExtensionService()->AddExtension(extension);
    ash::ShowScreensaver(extension->GetFullLaunchURL());
  } else {
    LOG(ERROR) << "Couldn't get default profile. Unable to load screensaver!";
  }
}

// NotificationObserver overrides:
void KioskModeScreensaver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_SESSION_STARTED);
  // User logged in, remove our observers, screensaver will be deactivated.
  chromeos::PowerManagerClient* power_manager =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
  if (power_manager->HasObserver(this))
    power_manager->RemoveObserver(this);

  ash::CloseScreensaver();
  ShutdownKioskModeScreensaver();
}

void KioskModeScreensaver::ActiveNotify() {
  // User is active, log us in.
  ExistingUserController* controller =
      ExistingUserController::current_controller();

  if (controller) {
    // Logging in will shut us down, removing the screen saver.
    controller->LoginAsDemoUser();
  } else {
    // Remove the screensaver so the user can at least use the underlying
    // login screen to be able to log in.
    ash::CloseScreensaver();
  }
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
