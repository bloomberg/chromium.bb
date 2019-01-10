// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/bookmark_apps/bookmark_app_install_manager.h"
#include "chrome/browser/web_applications/components/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_tab_helper.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"
#include "chrome/browser/web_applications/external_web_apps.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_database.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/one_shot_event.h"

namespace web_app {

// static
WebAppProvider* WebAppProvider::Get(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

// static
WebAppProvider* WebAppProvider::GetForWebContents(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);
  return WebAppProvider::Get(profile);
}

WebAppProvider::WebAppProvider(Profile* profile) : profile_(profile) {}

WebAppProvider::~WebAppProvider() = default;

void WebAppProvider::CreateSubsystems() {
  audio_focus_id_map_ = std::make_unique<WebAppAudioFocusIdMap>();

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions))
    CreateWebAppsSubsystems(profile_);
  else
    CreateBookmarkAppsSubsystems(profile_);
}

void WebAppProvider::Init() {
  notification_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::Source<Profile>(profile_));

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions)) {
    if (AllowWebAppInstallation(profile_)) {
      registrar_->Init(base::BindOnce(&WebAppProvider::OnRegistryReady,
                                      weak_ptr_factory_.GetWeakPtr()));
    }
  } else {
    system_web_app_manager_->Init();

    web_app::ScanForExternalWebApps(
        profile_, base::BindOnce(&WebAppProvider::OnScanForExternalWebApps,
                                 weak_ptr_factory_.GetWeakPtr()));

    extensions::ExtensionSystem::Get(profile_)->ready().Post(
        FROM_HERE, base::BindRepeating(&WebAppProvider::OnRegistryReady,
                                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void WebAppProvider::CreateWebAppsSubsystems(Profile* profile) {
  if (!AllowWebAppInstallation(profile))
    return;

  database_factory_ = std::make_unique<WebAppDatabaseFactory>(profile);
  database_ = std::make_unique<WebAppDatabase>(database_factory_.get());
  registrar_ = std::make_unique<WebAppRegistrar>(database_.get());
  icon_manager_ = std::make_unique<WebAppIconManager>(
      profile, std::make_unique<FileUtilsWrapper>());

  auto install_finalizer = std::make_unique<WebAppInstallFinalizer>(
      registrar_.get(), icon_manager_.get());
  install_manager_ = std::make_unique<WebAppInstallManager>(
      profile, std::move(install_finalizer));
}

void WebAppProvider::CreateBookmarkAppsSubsystems(Profile* profile) {
  install_manager_ = std::make_unique<extensions::BookmarkAppInstallManager>();

  pending_app_manager_ =
      std::make_unique<extensions::PendingBookmarkAppManager>(profile);

  if (WebAppPolicyManager::ShouldEnableForProfile(profile)) {
    web_app_policy_manager_ = std::make_unique<WebAppPolicyManager>(
        profile, pending_app_manager_.get());
  }

  system_web_app_manager_ = std::make_unique<SystemWebAppManager>(
      profile, pending_app_manager_.get());
}

void WebAppProvider::OnRegistryReady() {
  DCHECK(!registry_is_ready_);
  registry_is_ready_ = true;

  if (registry_ready_callback_)
    std::move(registry_ready_callback_).Run();
}

// static
void WebAppProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ExtensionIdsMap::RegisterProfilePrefs(registry);
  WebAppPolicyManager::RegisterProfilePrefs(registry);
}

// static
WebAppTabHelperBase* WebAppProvider::CreateTabHelper(
    content::WebContents* web_contents) {
  WebAppProvider* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider)
    return nullptr;

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions))
    WebAppTabHelper::CreateForWebContents(web_contents);
  else
    extensions::BookmarkAppTabHelper::CreateForWebContents(web_contents);

  WebAppTabHelperBase* tab_helper =
      WebAppTabHelperBase::FromWebContents(web_contents);
  tab_helper->SetAudioFocusIdMap(provider->audio_focus_id_map_.get());

  return tab_helper;
}

// static
bool WebAppProvider::CanInstallWebApp(content::WebContents* web_contents) {
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider || !provider->install_manager_)
    return false;
  return provider->install_manager_->CanInstallWebApp(web_contents);
}

// static
void WebAppProvider::InstallWebApp(content::WebContents* web_contents,
                                   bool force_shortcut_app) {
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider || !provider->install_manager_)
    return;
  provider->install_manager_->InstallWebApp(web_contents, force_shortcut_app,
                                            base::DoNothing());
}

void WebAppProvider::Reset() {
  // TODO(loyso): Make it independent to the order of destruction via using two
  // end-to-end passes:
  // 1) Do Reset() for each subsystem to nullify pointers (detach subsystems).
  // 2) Destroy subsystems.

  // PendingAppManager is used by WebAppPolicyManager and therefore should be
  // deleted after it.
  web_app_policy_manager_.reset();
  system_web_app_manager_.reset();
  pending_app_manager_.reset();

  install_manager_.reset();
  icon_manager_.reset();
  registrar_.reset();
  database_.reset();
  database_factory_.reset();
  audio_focus_id_map_.reset();
}

void WebAppProvider::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& detals) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);

  // KeyedService::Shutdown() gets called when the profile is being destroyed,
  // but after DCHECK'ing that no RenderProcessHosts are being leaked. The
  // "chrome::NOTIFICATION_PROFILE_DESTROYED" notification gets sent before the
  // DCHECK so we use that to clean up RenderProcessHosts instead.
  Reset();
}

void WebAppProvider::SetRegistryReadyCallback(base::OnceClosure callback) {
  DCHECK(!registry_ready_callback_);
  if (registry_is_ready_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  } else {
    registry_ready_callback_ = std::move(callback);
  }
}

int WebAppProvider::CountUserInstalledApps() const {
  // TODO: Implement for new Web Apps system. crbug.com/918986.
  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions))
    return 0;

  return extensions::CountUserInstalledBookmarkApps(profile_);
}

void WebAppProvider::OnScanForExternalWebApps(
    std::vector<web_app::PendingAppManager::AppInfo> app_infos) {
  pending_app_manager_->SynchronizeInstalledApps(
      std::move(app_infos), InstallSource::kExternalDefault);
}

}  // namespace web_app
