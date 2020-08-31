// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_REGISTRAR_H_

#include <map>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/common/web_application_info.h"
#include "third_party/skia/include/core/SkColor.h"

class GURL;
class Profile;

// Forward declared to support safe downcast;
namespace extensions {
class BookmarkAppRegistrar;
}

namespace web_app {

class AppRegistrarObserver;
class WebAppRegistrar;

enum class ExternalInstallSource;

class AppRegistrar {
 public:
  explicit AppRegistrar(Profile* profile);
  virtual ~AppRegistrar();

  // Returns whether the app with |app_id| is currently listed in the registry.
  // ie. we have data for web app manifest and icons, and this |app_id| can be
  // used in other registrar methods.
  virtual bool IsInstalled(const AppId& app_id) const = 0;

  // Returns whether the app with |app_id| is currently fully locally installed.
  // ie. app is not grey in chrome://apps UI surface and may have OS integration
  // like shortcuts. |IsLocallyInstalled| apps is a subset of |IsInstalled|
  // apps. On Chrome OS all apps are always locally installed.
  virtual bool IsLocallyInstalled(const AppId& app_id) const = 0;

  // Returns true if the app was installed by user, false if default installed.
  virtual bool WasInstalledByUser(const AppId& app_id) const = 0;

  // Returns the AppIds and URLs of apps externally installed from
  // |install_source|.
  virtual std::map<AppId, GURL> GetExternallyInstalledApps(
      ExternalInstallSource install_source) const;

  // Returns the app id for |install_url| if the AppRegistrar is aware of an
  // externally installed app for it. Note that the |install_url| is the URL
  // that the app was installed from, which may not necessarily match the app's
  // current start URL.
  virtual base::Optional<AppId> LookupExternalAppId(
      const GURL& install_url) const;

  // Returns whether the AppRegistrar has an externally installed app with
  // |app_id| from any |install_source|.
  virtual bool HasExternalApp(const AppId& app_id) const;

  // Returns whether the AppRegistrar has an externally installed app with
  // |app_id| from |install_source|.
  virtual bool HasExternalAppWithInstallSource(
      const AppId& app_id,
      ExternalInstallSource install_source) const;

  // Count a number of all apps which are installed by user (non-default).
  // Requires app registry to be in a ready state.
  virtual int CountUserInstalledApps() const = 0;

  // All names are UTF8 encoded.
  virtual std::string GetAppShortName(const AppId& app_id) const = 0;
  virtual std::string GetAppDescription(const AppId& app_id) const = 0;
  virtual base::Optional<SkColor> GetAppThemeColor(
      const AppId& app_id) const = 0;
  virtual const GURL& GetAppLaunchURL(const AppId& app_id) const = 0;

  // TODO(crbug.com/910016): Replace uses of this with GetAppScope().
  virtual base::Optional<GURL> GetAppScopeInternal(
      const AppId& app_id) const = 0;

  virtual DisplayMode GetAppDisplayMode(const AppId& app_id) const = 0;
  virtual DisplayMode GetAppUserDisplayMode(const AppId& app_id) const = 0;

  // Returns the "icons" field from the app manifest, use |AppIconManager| to
  // load icon bitmap data.
  virtual std::vector<WebApplicationIconInfo> GetAppIconInfos(
      const AppId& app_id) const = 0;

  // Represents which icon sizes we successfully downloaded from the IconInfos.
  virtual std::vector<SquareSizePx> GetAppDownloadedIconSizes(
      const AppId& app_id) const = 0;

  virtual std::vector<AppId> GetAppIds() const = 0;

  // Safe downcast.
  virtual WebAppRegistrar* AsWebAppRegistrar() = 0;
  virtual extensions::BookmarkAppRegistrar* AsBookmarkAppRegistrar();

  // Returns the "scope" field from the app manifest, or infers a scope from the
  // "start_url" field if unavailable. Returns an invalid GURL iff the |app_id|
  // does not refer to an installed web app.
  GURL GetAppScope(const AppId& app_id) const;

  // Returns the app id of an app in the registry with the longest scope that is
  // a prefix of |url|, if any.
  base::Optional<AppId> FindAppWithUrlInScope(const GURL& url) const;

  // Returns true if there exists at least one app installed under |scope|.
  bool DoesScopeContainAnyApp(const GURL& scope) const;

  // Finds all apps that are installed under |scope|.
  std::vector<AppId> FindAppsInScope(const GURL& scope) const;

  // Returns the app id of an installed app in the registry with the longest
  // scope that is a prefix of |url|, if any. If |window_only| is specified,
  // only apps that open in app windows will be considered.
  base::Optional<AppId> FindInstalledAppWithUrlInScope(
      const GURL& url,
      bool window_only = false) const;

  // Returns whether the app is a shortcut app (as opposed to a PWA).
  bool IsShortcutApp(const AppId& app_id) const;

  // Returns true if the app with the specified |start_url| is currently fully
  // locally installed. The provided |start_url| must exactly match the launch
  // URL for the app; this method does not consult the app scope or match URLs
  // that fall within the scope.
  bool IsLocallyInstalled(const GURL& start_url) const;

  // Returns whether the app is pending successful navigation in order to
  // complete installation via the PendingAppManager.
  bool IsPlaceholderApp(const AppId& app_id) const;

  DisplayMode GetAppEffectiveDisplayMode(const AppId& app_id) const;

  // TODO(crbug.com/897314): Finish experiment by legitimising it as a
  // DisplayMode or removing entirely.
  bool IsInExperimentalTabbedWindowMode(const AppId& app_id) const;

  void AddObserver(AppRegistrarObserver* observer);
  void RemoveObserver(AppRegistrarObserver* observer);

  void NotifyWebAppInstalled(const AppId& app_id);
  void NotifyWebAppUninstalled(const AppId& app_id);
  void NotifyWebAppLocallyInstalledStateChanged(const AppId& app_id,
                                                bool is_locally_installed);
  void NotifyWebAppDisabledStateChanged(const AppId& app_id, bool is_disabled);

 protected:
  Profile* profile() const { return profile_; }

  void NotifyWebAppProfileWillBeDeleted(const AppId& app_id);
  void NotifyAppRegistrarShutdown();

 private:
  Profile* const profile_;

  base::ObserverList<AppRegistrarObserver, /*check_empty=*/true> observers_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_REGISTRAR_H_
