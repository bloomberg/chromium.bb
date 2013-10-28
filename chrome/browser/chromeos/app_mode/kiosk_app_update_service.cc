// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_update_service.h"

#include "base/logging.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/system/automatic_reboot_manager.h"
#include "chrome/browser/extensions/api/runtime/runtime_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace chromeos {

namespace {

// How low to wait after an update is available before we force a restart.
const int kForceRestartWaitTimeMs = 24 * 3600 * 1000;  // 24 hours.

}  // namespace

KioskAppUpdateService::KioskAppUpdateService(
    Profile* profile,
    system::AutomaticRebootManager* automatic_reboot_manager)
    : profile_(profile),
      automatic_reboot_manager_(automatic_reboot_manager) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (service)
    service->AddUpdateObserver(this);

  if (automatic_reboot_manager_)
    automatic_reboot_manager_->AddObserver(this);
}

KioskAppUpdateService::~KioskAppUpdateService() {
}

void KioskAppUpdateService::StartAppUpdateRestartTimer() {
  if (restart_timer_.IsRunning())
    return;

  // Setup timer to force restart once the wait period expires.
  restart_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kForceRestartWaitTimeMs),
      this, &KioskAppUpdateService::ForceAppUpdateRestart);
}

void KioskAppUpdateService::ForceAppUpdateRestart() {
  // Force a chrome restart (not a logout or reboot) by closing all browsers.
  LOG(WARNING) << "Force closing all browsers to update kiosk app.";
  chrome::CloseAllBrowsersAndQuit();
}

void KioskAppUpdateService::Shutdown() {
  ExtensionService* service = profile_->GetExtensionService();
  if (service)
    service->RemoveUpdateObserver(this);
}

void KioskAppUpdateService::OnAppUpdateAvailable(const std::string& app_id) {
  DCHECK(!app_id_.empty());
  if (app_id != app_id_)
    return;

  extensions::RuntimeEventRouter::DispatchOnRestartRequiredEvent(
      profile_,
      app_id_,
      extensions::api::runtime::OnRestartRequired::REASON_APP_UPDATE);

  StartAppUpdateRestartTimer();
}

void KioskAppUpdateService::OnRebootScheduled(Reason reason) {
  extensions::api::runtime::OnRestartRequired::Reason restart_reason =
      extensions::api::runtime::OnRestartRequired::REASON_NONE;
  switch (reason) {
    case REBOOT_REASON_OS_UPDATE:
      restart_reason =
          extensions::api::runtime::OnRestartRequired::REASON_OS_UPDATE;
      break;
    case REBOOT_REASON_PERIODIC:
      restart_reason =
          extensions::api::runtime::OnRestartRequired::REASON_PERIODIC;
      break;
    default:
      NOTREACHED() << "Unknown reboot reason=" << reason;
      return;
  }

  extensions::RuntimeEventRouter::DispatchOnRestartRequiredEvent(
      profile_, app_id_, restart_reason);
}

void KioskAppUpdateService::WillDestroyAutomaticRebootManager() {
  automatic_reboot_manager_->RemoveObserver(this);
  automatic_reboot_manager_ = NULL;
}

KioskAppUpdateServiceFactory::KioskAppUpdateServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "KioskAppUpdateService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

KioskAppUpdateServiceFactory::~KioskAppUpdateServiceFactory() {
}

// static
KioskAppUpdateService* KioskAppUpdateServiceFactory::GetForProfile(
    Profile* profile) {
  // This should never be called unless we are running in forced app mode.
  DCHECK(chrome::IsRunningInForcedAppMode());
  if (!chrome::IsRunningInForcedAppMode())
    return NULL;

  return static_cast<KioskAppUpdateService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
KioskAppUpdateServiceFactory* KioskAppUpdateServiceFactory::GetInstance() {
  return Singleton<KioskAppUpdateServiceFactory>::get();
}

BrowserContextKeyedService*
KioskAppUpdateServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new KioskAppUpdateService(
      Profile::FromBrowserContext(context),
      g_browser_process->platform_part()->automatic_reboot_manager());
}

}  // namespace chromeos
