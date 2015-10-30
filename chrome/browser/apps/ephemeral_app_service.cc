// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/ephemeral_app_service.h"

#include "apps/app_lifetime_monitor_factory.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/apps/ephemeral_app_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/one_shot_event.h"

using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using extensions::ExtensionSet;
using extensions::ExtensionSystem;

namespace {

// The number of seconds after an app has stopped running before it will be
// disabled.
const int kDefaultDisableAppDelay = 1;

}  // namespace

// static
EphemeralAppService* EphemeralAppService::Get(Profile* profile) {
  return EphemeralAppServiceFactory::GetForProfile(profile);
}

EphemeralAppService::EphemeralAppService(Profile* profile)
    : profile_(profile),
      extension_registry_observer_(this),
      app_lifetime_monitor_observer_(this),
      disable_idle_app_delay_(kDefaultDisableAppDelay),
      weak_ptr_factory_(this) {
  ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE,
      base::Bind(&EphemeralAppService::Init, weak_ptr_factory_.GetWeakPtr()));
}

EphemeralAppService::~EphemeralAppService() {
}

void EphemeralAppService::ClearCachedApps() {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  DCHECK(registry);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  DCHECK(prefs);
  ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(service);

  scoped_ptr<ExtensionSet> extensions =
      registry->GenerateInstalledExtensionsSet();

  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end();
       ++it) {
    std::string extension_id = (*it)->id();
    if (!prefs->IsEphemeralApp(extension_id))
      continue;

    DCHECK(registry->GetExtensionById(extension_id,
                                      ExtensionRegistry::EVERYTHING));
    service->UninstallExtension(
        extension_id,
        extensions::UNINSTALL_REASON_ORPHANED_EPHEMERAL_EXTENSION,
        base::Bind(&base::DoNothing),
        NULL);
  }
}

void EphemeralAppService::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update,
    bool from_ephemeral,
    const std::string& old_name) {
  if (from_ephemeral)
    HandleEphemeralAppPromoted(extension);
}

void EphemeralAppService::OnAppStop(Profile* profile,
                                    const std::string& app_id) {
  if (!extensions::util::IsEphemeralApp(app_id, profile_))
    return;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&EphemeralAppService::DisableEphemeralApp,
                            weak_ptr_factory_.GetWeakPtr(), app_id),
      base::TimeDelta::FromSeconds(disable_idle_app_delay_));
}

void EphemeralAppService::OnChromeTerminating() {
  extension_registry_observer_.RemoveAll();
  app_lifetime_monitor_observer_.RemoveAll();
}

void EphemeralAppService::Init() {
  // Start observing.
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
  app_lifetime_monitor_observer_.Add(
      apps::AppLifetimeMonitorFactory::GetForProfile(profile_));

  // Execute startup clean up tasks (except during tests).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType))
    return;

  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE, content::BrowserThread::GetMessageLoopProxyForThread(
                     content::BrowserThread::UI),
      base::Bind(&EphemeralAppService::ClearCachedApps,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EphemeralAppService::DisableEphemeralApp(const std::string& app_id) {
  if (!extensions::util::IsEphemeralApp(app_id, profile_) ||
      !extensions::util::IsExtensionIdle(app_id, profile_)) {
    return;
  }

  // After an ephemeral app has stopped running, unload it from extension
  // system and disable it to prevent all background activity.
  ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(service);
  service->DisableExtension(app_id, Extension::DISABLE_INACTIVE_EPHEMERAL_APP);
}

void EphemeralAppService::HandleEphemeralAppPromoted(const Extension* app) {
  // When ephemeral apps are promoted to regular install apps, remove the
  // DISABLE_INACTIVE_EPHEMERAL_APP reason and enable the app if there are no
  // other reasons.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  DCHECK(prefs);

  int disable_reasons = prefs->GetDisableReasons(app->id());
  if (disable_reasons & Extension::DISABLE_INACTIVE_EPHEMERAL_APP) {
    if (disable_reasons == Extension::DISABLE_INACTIVE_EPHEMERAL_APP) {
      // This will also clear disable reasons.
      prefs->SetExtensionEnabled(app->id());
    } else {
      prefs->RemoveDisableReason(app->id(),
                                 Extension::DISABLE_INACTIVE_EPHEMERAL_APP);
    }
  }
}
