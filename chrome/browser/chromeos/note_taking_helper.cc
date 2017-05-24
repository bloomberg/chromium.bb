// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/note_taking_helper.h"

#include <algorithm>
#include <utility>

#include "apps/launcher.h"
#include "ash/system/palette/palette_utils.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/action_handlers_handler.h"
#include "url/gurl.h"

namespace app_runtime = extensions::api::app_runtime;

namespace chromeos {
namespace {

// Pointer to singleton instance.
NoteTakingHelper* g_helper = nullptr;

// Whitelisted Chrome note-taking apps.
const char* const kExtensionIds[] = {
    // TODO(jdufault): Remove dev version? See crbug.com/640828.
    NoteTakingHelper::kDevKeepExtensionId,
    NoteTakingHelper::kProdKeepExtensionId,
};

// Returns true if |app_id|, a value from prefs::kNoteTakingAppId, looks like
// it's probably an Android package name rather than a Chrome extension ID.
bool LooksLikeAndroidPackageName(const std::string& app_id) {
  // Android package names are required to contain at least one period (see
  // validateName() in PackageParser.java), while Chrome extension IDs contain
  // only characters in [a-p].
  return app_id.find(".") != std::string::npos;
}

// Creates a new Mojo IntentInfo struct for launching an Android note-taking app
// with an optional ClipData URI.
arc::mojom::IntentInfoPtr CreateIntentInfo(const GURL& clip_data_uri) {
  arc::mojom::IntentInfoPtr intent = arc::mojom::IntentInfo::New();
  intent->action = NoteTakingHelper::kIntentAction;
  if (!clip_data_uri.is_empty())
    intent->clip_data_uri = clip_data_uri.spec();
  return intent;
}

}  // namespace

const char NoteTakingHelper::kIntentAction[] =
    "org.chromium.arc.intent.action.CREATE_NOTE";
const char NoteTakingHelper::kDevKeepExtensionId[] =
    "ogfjaccbdfhecploibfbhighmebiffla";
const char NoteTakingHelper::kProdKeepExtensionId[] =
    "hmjkmjkepdijhoojdojkdfohbdgmmhki";
const char NoteTakingHelper::kPreferredLaunchResultHistogramName[] =
    "Apps.NoteTakingApp.PreferredLaunchResult";
const char NoteTakingHelper::kDefaultLaunchResultHistogramName[] =
    "Apps.NoteTakingApp.DefaultLaunchResult";

// static
void NoteTakingHelper::Initialize() {
  DCHECK(!g_helper);
  g_helper = new NoteTakingHelper();
}

// static
void NoteTakingHelper::Shutdown() {
  DCHECK(g_helper);
  delete g_helper;
  g_helper = nullptr;
}

// static
NoteTakingHelper* NoteTakingHelper::Get() {
  DCHECK(g_helper);
  return g_helper;
}

void NoteTakingHelper::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void NoteTakingHelper::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

NoteTakingAppInfos NoteTakingHelper::GetAvailableApps(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NoteTakingAppInfos infos;

  const std::vector<const extensions::Extension*> chrome_apps =
      GetChromeApps(profile);
  for (const auto* app : chrome_apps) {
    NoteTakingLockScreenSupport lock_screen_support =
        IsLockScreenEnabled(app) ? NoteTakingLockScreenSupport::kSupported
                                 : NoteTakingLockScreenSupport::kNotSupported;
    infos.push_back(
        NoteTakingAppInfo{app->name(), app->id(), false, lock_screen_support});
  }

  if (arc::IsArcAllowedForProfile(profile))
    infos.insert(infos.end(), android_apps_.begin(), android_apps_.end());

  // Determine which app, if any, is preferred and whether it is enabled on
  // lock screen.
  const std::string pref_app_id =
      profile->GetPrefs()->GetString(prefs::kNoteTakingAppId);
  for (auto& info : infos) {
    if (info.app_id == pref_app_id) {
      info.preferred = true;
      if (info.lock_screen_support == NoteTakingLockScreenSupport::kSupported &&
          profile->GetPrefs()->GetBoolean(
              prefs::kNoteTakingAppEnabledOnLockScreen)) {
        info.lock_screen_support = NoteTakingLockScreenSupport::kSelected;
      }
      break;
    }
  }

  return infos;
}

void NoteTakingHelper::SetPreferredApp(Profile* profile,
                                       const std::string& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          app_id, extensions::ExtensionRegistry::ENABLED);

  if (!app || !IsLockScreenEnabled(app)) {
    profile->GetPrefs()->SetBoolean(prefs::kNoteTakingAppEnabledOnLockScreen,
                                    false);
  }

  profile->GetPrefs()->SetString(prefs::kNoteTakingAppId, app_id);
}

bool NoteTakingHelper::IsAppAvailable(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  return ash::palette_utils::HasStylusInput() &&
         !GetAvailableApps(profile).empty();
}

void NoteTakingHelper::LaunchAppForNewNote(Profile* profile,
                                           const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  LaunchResult result = LaunchResult::NO_APP_SPECIFIED;
  std::string app_id = profile->GetPrefs()->GetString(prefs::kNoteTakingAppId);
  if (!app_id.empty())
    result = LaunchAppInternal(profile, app_id, path);
  UMA_HISTOGRAM_ENUMERATION(kPreferredLaunchResultHistogramName,
                            static_cast<int>(result),
                            static_cast<int>(LaunchResult::MAX));
  if (result == LaunchResult::CHROME_SUCCESS ||
      result == LaunchResult::ANDROID_SUCCESS) {
    return;
  }

  // If the user hasn't chosen an app or we were unable to launch the one that
  // they've chosen, just launch the first one we see.
  result = LaunchResult::NO_APPS_AVAILABLE;
  NoteTakingAppInfos infos = GetAvailableApps(profile);
  if (infos.empty())
    LOG(WARNING) << "Unable to launch note-taking app; none available";
  else
    result = LaunchAppInternal(profile, infos[0].app_id, path);
  UMA_HISTOGRAM_ENUMERATION(kDefaultLaunchResultHistogramName,
                            static_cast<int>(result),
                            static_cast<int>(LaunchResult::MAX));
}

void NoteTakingHelper::OnIntentFiltersUpdated() {
  if (play_store_enabled_)
    UpdateAndroidApps();
}

void NoteTakingHelper::OnArcPlayStoreEnabledChanged(bool enabled) {
  play_store_enabled_ = enabled;
  if (!enabled) {
    android_apps_.clear();
    android_apps_received_ = false;
  }
  for (auto& observer : observers_)
    observer.OnAvailableNoteTakingAppsUpdated();
}

NoteTakingHelper::NoteTakingHelper()
    : launch_chrome_app_callback_(
          base::Bind(&apps::LaunchPlatformAppWithAction)),
      extension_registry_observer_(this),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kNoteTakingAppIds);
  if (!switch_value.empty()) {
    whitelisted_chrome_app_ids_ = base::SplitString(
        switch_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }
  whitelisted_chrome_app_ids_.insert(whitelisted_chrome_app_ids_.end(),
                                     kExtensionIds,
                                     kExtensionIds + arraysize(kExtensionIds));

  // Track profiles so we can observe their extension registries.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_ADDED,
                 content::NotificationService::AllBrowserContextsAndSources());
  play_store_enabled_ = false;
  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    extension_registry_observer_.Add(
        extensions::ExtensionRegistry::Get(profile));
    // Check if the profile has already enabled Google Play Store.
    // IsArcPlayStoreEnabledForProfile() can return true only for the primary
    // profile.
    play_store_enabled_ |= arc::IsArcPlayStoreEnabledForProfile(profile);
  }

  // Watch for changes of Google Play Store enabled state.
  auto* session_manager = arc::ArcSessionManager::Get();
  session_manager->AddObserver(this);

  // ArcIntentHelperBridge will notify us about changes to the list of available
  // Android apps.
  auto* intent_helper_bridge =
      arc::ArcServiceManager::GetGlobalService<arc::ArcIntentHelperBridge>();
  if (intent_helper_bridge)
    intent_helper_bridge->AddObserver(this);

  // If the ARC intent helper is ready, get the Android apps. Otherwise,
  // UpdateAndroidApps() will be called when ArcServiceManager calls
  // OnIntentFiltersUpdated().
  if (play_store_enabled_ && arc::ArcServiceManager::Get()
                                 ->arc_bridge_service()
                                 ->intent_helper()
                                 ->has_instance()) {
    UpdateAndroidApps();
  }
}

NoteTakingHelper::~NoteTakingHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // ArcSessionManagerTest shuts down ARC before NoteTakingHelper.
  auto* intent_helper_bridge =
      arc::ArcServiceManager::GetGlobalService<arc::ArcIntentHelperBridge>();
  if (intent_helper_bridge)
    intent_helper_bridge->RemoveObserver(this);
  if (arc::ArcSessionManager::Get())
    arc::ArcSessionManager::Get()->RemoveObserver(this);
}

bool NoteTakingHelper::IsWhitelistedChromeApp(
    const extensions::Extension* extension) const {
  DCHECK(extension);
  return base::ContainsValue(whitelisted_chrome_app_ids_, extension->id());
}

std::vector<const extensions::Extension*> NoteTakingHelper::GetChromeApps(
    Profile* profile) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::ExtensionSet& enabled_extensions =
      extension_registry->enabled_extensions();

  std::vector<const extensions::Extension*> extensions;
  for (const auto& id : whitelisted_chrome_app_ids_) {
    if (enabled_extensions.Contains(id)) {
      extensions.push_back(extension_registry->GetExtensionById(
          id, extensions::ExtensionRegistry::ENABLED));
    }
  }

  // Add any extensions which have a "note" action in their manifest
  // "action_handler" entry.
  for (const auto& extension : enabled_extensions) {
    if (base::ContainsValue(extensions, extension.get()))
      continue;

    if (extensions::ActionHandlersInfo::HasActionHandler(
            extension.get(), app_runtime::ACTION_TYPE_NEW_NOTE)) {
      extensions.push_back(extension.get());
    }
  }

  return extensions;
}

bool NoteTakingHelper::IsLockScreenEnabled(const extensions::Extension* app) {
  if (!lock_screen_apps::StateController::IsEnabled())
    return false;

  if (!IsWhitelistedChromeApp(app))
    return false;

  return extensions::ActionHandlersInfo::HasLockScreenActionHandler(
      app, app_runtime::ACTION_TYPE_NEW_NOTE);
}

void NoteTakingHelper::UpdateAndroidApps() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* helper = ARC_GET_INSTANCE_FOR_METHOD(
      arc::ArcServiceManager::Get()->arc_bridge_service()->intent_helper(),
      RequestIntentHandlerList);
  if (!helper)
    return;
  helper->RequestIntentHandlerList(
      CreateIntentInfo(GURL()), base::Bind(&NoteTakingHelper::OnGotAndroidApps,
                                           weak_ptr_factory_.GetWeakPtr()));
}

void NoteTakingHelper::OnGotAndroidApps(
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!play_store_enabled_)
    return;

  android_apps_.clear();
  android_apps_.reserve(handlers.size());
  for (const auto& it : handlers) {
    android_apps_.emplace_back(
        NoteTakingAppInfo{it->name, it->package_name, false,
                          NoteTakingLockScreenSupport::kNotSupported});
  }
  android_apps_received_ = true;

  for (auto& observer : observers_)
    observer.OnAvailableNoteTakingAppsUpdated();
}

NoteTakingHelper::LaunchResult NoteTakingHelper::LaunchAppInternal(
    Profile* profile,
    const std::string& app_id,
    const base::FilePath& path) {
  DCHECK(profile);

  if (LooksLikeAndroidPackageName(app_id)) {
    // Android app.
    if (!arc::IsArcAllowedForProfile(profile)) {
      LOG(WARNING) << "Can't launch Android app " << app_id << " for profile";
      return LaunchResult::ANDROID_NOT_SUPPORTED_BY_PROFILE;
    }
    auto* helper = ARC_GET_INSTANCE_FOR_METHOD(
        arc::ArcServiceManager::Get()->arc_bridge_service()->intent_helper(),
        HandleIntent);
    if (!helper)
      return LaunchResult::ANDROID_NOT_RUNNING;

    GURL clip_data_uri;
    if (!path.empty()) {
      if (!file_manager::util::ConvertPathToArcUrl(path, &clip_data_uri) ||
          !clip_data_uri.is_valid()) {
        LOG(WARNING) << "Failed to convert " << path.value() << " to ARC URI";
        return LaunchResult::ANDROID_FAILED_TO_CONVERT_PATH;
      }
    }

    // Only set the package name: leaving the activity name unset enables the
    // app to rename its activities.
    arc::mojom::ActivityNamePtr activity = arc::mojom::ActivityName::New();
    activity->package_name = app_id;

    // TODO(derat): Is there some way to detect whether this fails due to the
    // package no longer being available?
    helper->HandleIntent(CreateIntentInfo(clip_data_uri), std::move(activity));
    return LaunchResult::ANDROID_SUCCESS;
  } else {
    // Chrome app.
    const extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(profile);
    const extensions::Extension* app = extension_registry->GetExtensionById(
        app_id, extensions::ExtensionRegistry::ENABLED);
    if (!app) {
      LOG(WARNING) << "Failed to find Chrome note-taking app " << app_id;
      return LaunchResult::CHROME_APP_MISSING;
    }
    auto action_data = base::MakeUnique<app_runtime::ActionData>();
    action_data->action_type = app_runtime::ActionType::ACTION_TYPE_NEW_NOTE;
    launch_chrome_app_callback_.Run(profile, app, std::move(action_data), path);
    return LaunchResult::CHROME_SUCCESS;
  }
  NOTREACHED();
}

void NoteTakingHelper::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_PROFILE_ADDED);
  Profile* profile = content::Source<Profile>(source).ptr();
  DCHECK(profile);

  auto* registry = extensions::ExtensionRegistry::Get(profile);
  DCHECK(!extension_registry_observer_.IsObserving(registry));
  extension_registry_observer_.Add(registry);

  // TODO(derat): Remove this once OnArcPlayStoreEnabledChanged() is always
  // called after an ARC-enabled user logs in: http://b/36655474
  if (!play_store_enabled_ && arc::IsArcPlayStoreEnabledForProfile(profile)) {
    play_store_enabled_ = true;
    for (auto& observer : observers_)
      observer.OnAvailableNoteTakingAppsUpdated();
  }
}

void NoteTakingHelper::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (IsWhitelistedChromeApp(extension) ||
      extensions::ActionHandlersInfo::HasActionHandler(
          extension, app_runtime::ACTION_TYPE_NEW_NOTE)) {
    for (auto& observer : observers_)
      observer.OnAvailableNoteTakingAppsUpdated();
  }
}

void NoteTakingHelper::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (IsWhitelistedChromeApp(extension) ||
      extensions::ActionHandlersInfo::HasActionHandler(
          extension, app_runtime::ACTION_TYPE_NEW_NOTE)) {
    for (auto& observer : observers_)
      observer.OnAvailableNoteTakingAppsUpdated();
  }
}

void NoteTakingHelper::OnShutdown(extensions::ExtensionRegistry* registry) {
  extension_registry_observer_.Remove(registry);
}

}  // namespace chromeos
