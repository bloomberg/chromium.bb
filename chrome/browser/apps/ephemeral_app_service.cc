// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/ephemeral_app_service.h"

#include "base/command_line.h"
#include "chrome/browser/apps/ephemeral_app_service_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/data_deleter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

using extensions::Extension;
using extensions::ExtensionInfo;
using extensions::ExtensionPrefs;
using extensions::ExtensionSet;
using extensions::ExtensionSystem;
using extensions::InstalledExtensionInfo;

namespace {

// The number of seconds after startup before performing garbage collection
// of ephemeral apps.
const int kGarbageCollectAppsStartupDelay = 60;

// The number of seconds after an ephemeral app has been installed before
// performing garbage collection.
const int kGarbageCollectAppsInstallDelay = 15;

// When the number of ephemeral apps reaches this count, trigger garbage
// collection to trim off the least-recently used apps in excess of
// kMaxEphemeralAppsCount.
const int kGarbageCollectAppsTriggerCount = 35;

// The number of seconds after startup before performing garbage collection
// of the data of evicted ephemeral apps.
const int kGarbageCollectDataStartupDelay = 120;

}  // namespace

const int EphemeralAppService::kAppInactiveThreshold = 10;
const int EphemeralAppService::kAppKeepThreshold = 1;
const int EphemeralAppService::kMaxEphemeralAppsCount = 30;
const int EphemeralAppService::kDataInactiveThreshold = 90;

// static
EphemeralAppService* EphemeralAppService::Get(Profile* profile) {
  return EphemeralAppServiceFactory::GetForProfile(profile);
}

EphemeralAppService::EphemeralAppService(Profile* profile)
    : profile_(profile),
      ephemeral_app_count_(-1) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableEphemeralApps))
    return;

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::Source<Profile>(profile_));
}

EphemeralAppService::~EphemeralAppService() {
}

void EphemeralAppService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSIONS_READY: {
      Init();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      const Extension* extension =
          content::Details<const InstalledExtensionInfo>(details)->extension;
      DCHECK(extension);
      if (extension->is_ephemeral()) {
        ++ephemeral_app_count_;
        if (ephemeral_app_count_ >= kGarbageCollectAppsTriggerCount)
          TriggerGarbageCollect(
              base::TimeDelta::FromSeconds(kGarbageCollectAppsInstallDelay));
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      DCHECK(extension);
      if (extension->is_ephemeral())
        --ephemeral_app_count_;
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      // Ideally we need to know when the extension system is shutting down.
      garbage_collect_apps_timer_.Stop();
      break;
    }
    default:
      NOTREACHED();
  };
}

void EphemeralAppService::Init() {
  InitEphemeralAppCount();
  TriggerGarbageCollect(
      base::TimeDelta::FromSeconds(kGarbageCollectAppsStartupDelay));

  garbage_collect_data_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kGarbageCollectDataStartupDelay),
      this,
      &EphemeralAppService::GarbageCollectData);
}

void EphemeralAppService::InitEphemeralAppCount() {
  scoped_ptr<ExtensionSet> extensions =
      extensions::ExtensionRegistry::Get(profile_)
          ->GenerateInstalledExtensionsSet();

  ephemeral_app_count_ = 0;
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const Extension* extension = *it;
    if (extension->is_ephemeral())
      ++ephemeral_app_count_;
  }
}

void EphemeralAppService::TriggerGarbageCollect(const base::TimeDelta& delay) {
  if (!garbage_collect_apps_timer_.IsRunning()) {
    garbage_collect_apps_timer_.Start(
        FROM_HERE,
        delay,
        this,
        &EphemeralAppService::GarbageCollectApps);
  }
}

void EphemeralAppService::GarbageCollectApps() {
  scoped_ptr<ExtensionSet> extensions =
      extensions::ExtensionRegistry::Get(profile_)
          ->GenerateInstalledExtensionsSet();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);

  int app_count = 0;
  LaunchTimeAppMap app_launch_times;
  std::set<std::string> remove_app_ids;

  // Populate a list of ephemeral apps, ordered by their last launch time.
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const Extension* extension = *it;
    if (!extension->is_ephemeral())
      continue;

    ++app_count;

    // Do not remove ephemeral apps that are running.
    if (!extensions::util::IsExtensionIdle(extension->id(), profile_))
      continue;

    base::Time last_launch_time = prefs->GetLastLaunchTime(extension->id());

    // If the last launch time is invalid, this may be because it was just
    // installed. So use the install time. If this is also null for some reason,
    // the app will be removed.
    if (last_launch_time.is_null())
      last_launch_time = prefs->GetInstallTime(extension->id());

    app_launch_times.insert(std::make_pair(last_launch_time, extension->id()));
  }

  ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(service);
  // Execute the replacement policies and remove apps marked for deletion.
  if (!app_launch_times.empty()) {
    GetAppsToRemove(app_count, app_launch_times, &remove_app_ids);
    for (std::set<std::string>::const_iterator id = remove_app_ids.begin();
         id != remove_app_ids.end(); ++id) {
      if (service->UninstallExtension(*id, false, NULL))
        --app_count;
    }
  }

  ephemeral_app_count_ = app_count;
}

// static
void EphemeralAppService::GetAppsToRemove(
    int app_count,
    const LaunchTimeAppMap& app_launch_times,
    std::set<std::string>* remove_app_ids) {
  base::Time time_now = base::Time::Now();
  const base::Time inactive_threshold =
      time_now - base::TimeDelta::FromDays(kAppInactiveThreshold);
  const base::Time keep_threshold =
      time_now - base::TimeDelta::FromDays(kAppKeepThreshold);

  // Visit the apps in order of least recently to most recently launched.
  for (LaunchTimeAppMap::const_iterator it = app_launch_times.begin();
       it != app_launch_times.end(); ++it) {
    // Cannot remove apps that have been launched recently. So break when we
    // reach the new apps.
    if (it->first > keep_threshold)
        break;

    // Remove ephemeral apps that have been inactive for a while or if the cache
    // is larger than the desired size.
    if (it->first < inactive_threshold || app_count > kMaxEphemeralAppsCount) {
      remove_app_ids->insert(it->second);
      --app_count;
    } else {
      break;
    }
  }
}

void EphemeralAppService::GarbageCollectData() {
  ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(service);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  DCHECK(prefs);
  scoped_ptr<ExtensionPrefs::ExtensionsInfo> evicted_apps_info(
      prefs->GetEvictedEphemeralAppsInfo());

  base::Time time_now = base::Time::Now();
  const base::Time inactive_threshold =
      time_now - base::TimeDelta::FromDays(kDataInactiveThreshold);

  for (size_t i = 0; i < evicted_apps_info->size(); ++i) {
    ExtensionInfo* info = evicted_apps_info->at(i).get();
    base::Time last_launch_time = prefs->GetLastLaunchTime(info->extension_id);
    if (last_launch_time > inactive_threshold)
      continue;

    // Sanity check to ensure the app is not currently installed.
    if (service->GetInstalledExtension(info->extension_id)) {
      NOTREACHED();
      continue;
    }

    // Ensure the app is not waiting to be installed.
    scoped_ptr<ExtensionInfo> delayed_install(
        prefs->GetDelayedInstallInfo(info->extension_id));
    if (delayed_install.get())
      continue;

    if (info->extension_manifest.get()) {
      std::string error;
      scoped_refptr<const Extension> extension(Extension::Create(
          info->extension_path,
          info->extension_location,
          *info->extension_manifest,
          prefs->GetCreationFlags(info->extension_id),
          info->extension_id,
          &error));

      if (extension.get())
        extensions::DataDeleter::StartDeleting(profile_, extension.get());
    }

    prefs->RemoveEvictedEphemeralApp(info->extension_id);
  }
}
