// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_update_service.h"

#include "base/logging.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace chromeos {

namespace {

// How low to wait after an update is available before we force a restart.
const int kForceRestartWaitTimeMs = 24 * 3600 * 1000;  // 24 hours.

}  // namespace

KioskAppUpdateService::KioskAppUpdateService(Profile* profile)
    : profile_(profile) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (service)
    service->AddUpdateObserver(this);
}

KioskAppUpdateService::~KioskAppUpdateService() {
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

  StartRestartTimer();
}

void KioskAppUpdateService::StartRestartTimer() {
  if (restart_timer_.IsRunning())
    return;

  // Setup timer to force restart once the wait period expires.
  restart_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kForceRestartWaitTimeMs),
      this, &KioskAppUpdateService::ForceRestart);
}

void KioskAppUpdateService::ForceRestart() {
  // Force a chrome restart (not a logout or reboot) by closing all browsers.
  LOG(WARNING) << "Force closing all browsers to update kiosk app.";
  chrome::CloseAllBrowsers();
}

KioskAppUpdateServiceFactory::KioskAppUpdateServiceFactory()
    : ProfileKeyedServiceFactory("KioskAppUpdateService",
                                 ProfileDependencyManager::GetInstance()) {
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
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
KioskAppUpdateServiceFactory* KioskAppUpdateServiceFactory::GetInstance() {
  return Singleton<KioskAppUpdateServiceFactory>::get();
}

ProfileKeyedService* KioskAppUpdateServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new KioskAppUpdateService(profile);
}

}  // namespace chromeos
