// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_handler.h"

#include <utility>

#include "base/stl_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_handler_utils.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "chrome/services/app_service/public/cpp/app_service_proxy.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/identity/public/cpp/access_token_fetcher.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace {

// Returns the Arc package name for the specified app_id, which must
// be the AppID of an ARC app.  TODO(danan):  this functionality is
// also needed by KioskNext, so find a way to share the same code.
std::string AppIdToArcPackageName(const std::string& app_id, Profile* profile) {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile);

  // This should never return null, since we use the primary user profile
  // which should have ARC prefs.
  DCHECK(arc_prefs) << "Unable to retrieve ArcAppListPrefs";

  std::unique_ptr<ArcAppListPrefs::AppInfo> arc_info =
      arc_prefs->GetApp(app_id);

  if (!arc_info) {
    DLOG(ERROR) << "Couldn't retrieve ARC package name for AppID: " << app_id;
    return std::string();
  }
  return std::move(arc_info)->package_name;
}

std::string ArcPackageNameToAppId(const std::string& package_name,
                                  Profile* profile) {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile);

  // This should never return null, since we use the primary user profile
  // which should have ARC prefs.
  DCHECK(arc_prefs) << "Unable to retrieve ArcAppListPrefs";

  return arc_prefs->GetAppIdByPackageName(package_name);
}

// TODO(danan): This method should be removed once the stickyness
// attribute is available via the App Service instead.  This function
// will crash if app_id isn't installed.
// (https://crbug.com/948408).
bool IsArcAppSticky(const std::string& app_id, Profile* profile) {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile);

  // This should never return null, since we use the primary user profile
  // which should have arc prefs.
  DCHECK(arc_prefs) << "Unable to retrieve ArcAppListPrefs";

  std::unique_ptr<ArcAppListPrefs::AppInfo> arc_info =
      arc_prefs->GetApp(app_id);

  DCHECK(arc_info) << "Couldn't retrieve ARC package name for AppID: "
                   << app_id;
  return arc_info->sticky;
}

}  // namespace

AddSupervisionHandler::AddSupervisionHandler(
    add_supervision::mojom::AddSupervisionHandlerRequest request,
    content::WebUI* web_ui)
    : binding_(this, std::move(request)),
      web_ui_(web_ui),
      identity_manager_(
          IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui))) {}

AddSupervisionHandler::~AddSupervisionHandler() = default;

void AddSupervisionHandler::LogOut(LogOutCallback callback) {
  chrome::AttemptUserExit();
}

void AddSupervisionHandler::GetInstalledArcApps(
    GetInstalledArcAppsCallback callback) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  std::vector<std::string> installed_arc_apps;

  proxy->AppRegistryCache().ForEachApp([&installed_arc_apps, profile](
                                           const apps::AppUpdate& update) {
    // We don't include "sticky" ARC apps because they are system-required
    // apps that should not be offered for uninstallation.
    if (ShouldIncludeAppUpdate(update) &&
        !IsArcAppSticky(update.AppId(), profile)) {
      std::string package_name = AppIdToArcPackageName(update.AppId(), profile);
      if (!package_name.empty())
        installed_arc_apps.push_back(package_name);
    }
  });

  std::move(callback).Run(installed_arc_apps);
}

void AddSupervisionHandler::UninstallArcApps(
    const std::vector<std::string>& package_names,
    UninstallArcAppsCallback callback) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  for (const std::string& package_name : package_names) {
    proxy->AppRegistryCache().ForOneApp(
        ArcPackageNameToAppId(package_name, profile),
        [proxy, package_names, profile](const apps::AppUpdate& update) {
          // We don't include "sticky" ARC apps because they are
          // system-required apps that should not be uninstalled.
          if (ShouldIncludeAppUpdate(update) &&
              !IsArcAppSticky(update.AppId(), profile)) {
            proxy->Uninstall(update.AppId());
          }
        });
  }

  std::move(callback).Run();
}

void AddSupervisionHandler::GetOAuthToken(GetOAuthTokenCallback callback) {
  identity::ScopeSet scopes;
  scopes.insert(GaiaConstants::kKidFamilyOAuth2Scope);

  oauth2_access_token_fetcher_ =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          identity_manager_->GetPrimaryAccountId(), "add_supervision", scopes,
          base::BindOnce(&AddSupervisionHandler::OnAccessTokenFetchComplete,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          identity::AccessTokenFetcher::Mode::kImmediate);
}

void AddSupervisionHandler::OnAccessTokenFetchComplete(
    GetOAuthTokenCallback callback,
    GoogleServiceAuthError error,
    identity::AccessTokenInfo access_token_info) {
  oauth2_access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    DLOG(ERROR) << "AddSupervisionHandler: OAuth2 token request failed. "
                << error.state() << ": " << error.ToString();

    std::move(callback).Run(
        add_supervision::mojom::OAuthTokenFetchStatus::ERROR, "");

  } else {
    std::move(callback).Run(add_supervision::mojom::OAuthTokenFetchStatus::OK,
                            access_token_info.token);
  }
}
